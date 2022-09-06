// Copyright 2021 Mobilinkd LLC.

#pragma once

#include <QDebug>
#include <QString>

#include "M17Randomizer.h"
#include "PolynomialInterleaver.h"
#include "Trellis.h"
#include "Viterbi.h"
#include "CRC16.h"
#include "LinkSetupFrame.h"
#include "Golay24.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <iostream>
#include <iomanip>

namespace modemm17
{


template <typename C, size_t N>
QString dump(const std::array<C,N>& data, char header = 'D')
{
    QString s(header);
    s += "=";

    for (auto c : data) {
        s += QString("%1 ").arg((int) c, 2, 16, QChar('0'));
    }

    return s;
}

struct M17FrameDecoder
{
    static const size_t MAX_LICH_FRAGMENT = 5;

    M17Randomizer derandomize_;
    PolynomialInterleaver<45, 92, 368> interleaver_;
    Trellis<4,2> trellis_{makeTrellis<4, 2>({031,027})};
    Viterbi<decltype(trellis_), 4> viterbi_{trellis_};
    CRC16 crc_;

    enum class State { LSF, STREAM, BASIC_PACKET, FULL_PACKET, BERT };
    enum class SyncWordType { LSF, STREAM, PACKET, BERT };
    enum class DecodeResult { FAIL, OK, EOS, INCOMPLETE, PACKET_INCOMPLETE };
    enum class FrameType { LSF, LICH, STREAM, BASIC_PACKET, FULL_PACKET, BERT };

    State state_ = State::LSF;

    using input_buffer_t = std::array<int8_t, 368>;

    using lsf_conv_buffer_t = std::array<uint8_t, 46>;
    using audio_conv_buffer_t = std::array<uint8_t, 34>;

    using lsf_buffer_t = std::array<uint8_t, 30>;
    using lich_buffer_t = std::array<uint8_t, 6>;
    using audio_buffer_t = std::array<uint8_t, 18>;
    using packet_buffer_t = std::array<uint8_t, 26>;
    using bert_buffer_t = std::array<uint8_t, 25>;

    using output_buffer_t = struct {
        FrameType type;
        union {
            lich_buffer_t lich;
            audio_buffer_t stream;
            packet_buffer_t packet;
            bert_buffer_t bert;
        };
        lsf_buffer_t lsf;
    };

    using depunctured_buffer_t = union {
        std::array<int8_t, 488> lsf;
        std::array<int8_t, 296> stream;
        std::array<int8_t, 420> packet;
        std::array<int8_t, 402> bert;
    };

    using decode_buffer_t = union {
        std::array<uint8_t, 240> lsf;
        std::array<uint8_t, 144> stream;
        std::array<uint8_t, 206> packet;
        std::array<uint8_t, 197> bert;
    };

    /**
     * Callback function for frame types.  The caller is expected to return
     * true if the data was good or unknown and false if the data is known
     * to be bad.
     */
    using callback_t = std::function<bool(const output_buffer_t&, int)>;

    callback_t callback_;

    output_buffer_t output_buffer;
    decode_buffer_t decode_buffer;
    uint16_t frame_number = 0;

    uint8_t lich_segments{0};       ///< one bit per received LICH fragment.

    M17FrameDecoder(callback_t callback) :
        crc_(0x5935, 0xFFFF),
        callback_(callback)
    {}

    void update_state(std::array<uint8_t, 240>& lsf_output)
    {
        if (lsf_output[111]) // LSF type bit 0
        {
            if (lsf_output[109] != 0) {
                state_ = State::STREAM;
            }
        }
        else    // packet frame comes next.
        {
            uint8_t packet_type = (lsf_output[109] << 1) | lsf_output[110];
            switch (packet_type)
            {
            case 1: // RAW -- ignore LSF.
                state_ = State::BASIC_PACKET;
                break;
            case 2: // ENCAPSULATED
                state_ = State::FULL_PACKET;
                break;
            default:
                state_ = State::FULL_PACKET;
            }
        }
    }

    void reset()
    {
        state_ = State::LSF;
        frame_number = 0;
    }

    /**
     * Decode the LSF and, if it is valid, transition to the next state.
     *
     * The LSF is returned for STREAM mode, dropped for BASIC_PACKET mode,
     * and captured for FULL_PACKET mode.
     *
     * @param buffer
     * @param viterbi_cost
     * @return
     */
    DecodeResult decode_lsf(input_buffer_t& buffer, int& viterbi_cost)
    {
        depunctured_buffer_t depuncture_buffer;
        depuncture<368, 488, 61>(buffer, depuncture_buffer.lsf, P1);
        viterbi_cost = viterbi_.decode(depuncture_buffer.lsf, decode_buffer.lsf);
        to_byte_array(decode_buffer.lsf, output_buffer.lsf);

        // qDebug() << "modemm17::M17FrameDecoder::decode_lsf: vierbi:" << viterbi_cost <<dump(output_buffer.lsf);

        crc_.reset();
        for (auto c : output_buffer.lsf) crc_(c);
        auto checksum = crc_.get();

        if (checksum == 0)
        {
            update_state(decode_buffer.lsf);
            output_buffer.type = FrameType::LSF;
            callback_(output_buffer, viterbi_cost);
            return DecodeResult::OK;
        }
        else
        {
            qDebug() << "modemm17::M17FrameDecoder::decode_lsf: bad CRC:" << dump(output_buffer.lsf);
        }

        lich_segments = 0;
        output_buffer.lsf.fill(0);
        return DecodeResult::FAIL;
    }

    // Unpack  & decode LICH fragments into tmp_buffer.
    bool unpack_lich(input_buffer_t& buffer)
    {
        size_t index = 0;
        // Read the 4 24-bit codewords from LICH
        for (size_t i = 0; i != 4; ++i) // for each codeword
        {
            uint32_t codeword = 0;
            for (size_t j = 0; j != 24; ++j) // for each bit in codeword
            {
                codeword <<= 1;
                codeword |= (buffer[i * 24 + j] > 0);
            }
            uint32_t decoded = 0;
            if (!Golay24::decode(codeword, decoded))
            {
                return false;
            }
            decoded >>= 12; // Remove check bits and parity.
            // append codeword.
            if (i & 1)
            {
                output_buffer.lich[index++] |= (decoded >> 8);     // upper 4 bits
                output_buffer.lich[index++] = (decoded & 0xFF);    // lower 8 bits
            }
            else
            {
                output_buffer.lich[index++] |= (decoded >> 4);     // upper 8 bits
                output_buffer.lich[index] = (decoded & 0x0F) << 4; // lower 4 bits
            }
        }
        return true;
    }

    DecodeResult decode_lich(input_buffer_t& buffer, int& viterbi_cost)
    {
        output_buffer.lich.fill(0);
        // Read the 4 12-bit codewords from LICH into buffers.lich.
        if (!unpack_lich(buffer)) return DecodeResult::FAIL;

        output_buffer.type = FrameType::LICH;
        callback_(output_buffer, 0);

        uint8_t fragment_number = output_buffer.lich[5];   // Get fragment number.
        fragment_number = (fragment_number >> 5) & 7;

        if (fragment_number > MAX_LICH_FRAGMENT)
        {
            viterbi_cost = -1;
            return DecodeResult::INCOMPLETE;    // More to go...
        }

        // Copy decoded LICH to superframe buffer.
        std::copy(output_buffer.lich.begin(), output_buffer.lich.begin() + 5,
            output_buffer.lsf.begin() + (fragment_number * 5));

        lich_segments |= (1 << fragment_number);        // Indicate segment received.
        if ((lich_segments & 0x3F) != 0x3F)
        {
            viterbi_cost = -1;
            return DecodeResult::INCOMPLETE;        // More to go...
        }

        crc_.reset();
        for (auto c : output_buffer.lsf) crc_(c);
        auto checksum = crc_.get();

        if (checksum == 0)
        {
        	lich_segments = 0;
            state_ = State::STREAM;
            viterbi_cost = 0;
            output_buffer.type = FrameType::LSF;
            callback_(output_buffer, viterbi_cost);
            return DecodeResult::OK;
        }

        // Failed CRC... try again.
        // lich_segments = 0;
        // output_buffer.lsf.fill(0);
        viterbi_cost = 128;
        return DecodeResult::INCOMPLETE;
    }

    DecodeResult decode_bert(input_buffer_t& buffer, int& viterbi_cost)
    {
        depunctured_buffer_t depuncture_buffer;
        depuncture<368, 402, 12>(buffer, depuncture_buffer.bert, P2);
        viterbi_cost = viterbi_.decode(depuncture_buffer.bert, decode_buffer.bert);
        to_byte_array(decode_buffer.bert, output_buffer.bert);

        output_buffer.type = FrameType::BERT;
        callback_(output_buffer, viterbi_cost);

        return DecodeResult::OK;
    }

    DecodeResult decode_stream(input_buffer_t& buffer, int& viterbi_cost)
    {
        std::array<int8_t, 272> tmp;
        std::copy(buffer.begin() + 96, buffer.end(), tmp.begin());
        depunctured_buffer_t depuncture_buffer;

        depuncture<272, 296, 12>(tmp, depuncture_buffer.stream, P2);
        viterbi_cost = viterbi_.decode(depuncture_buffer.stream, decode_buffer.stream);
        to_byte_array(decode_buffer.stream, output_buffer.stream);

        if ((viterbi_cost < 60) && (output_buffer.stream[0] & 0x80))
        {
            // fputs("\nEOS\n", stderr);
            state_ = State::LSF;
        }

        output_buffer.type = FrameType::STREAM;
        callback_(output_buffer, viterbi_cost);

        return state_ == State::LSF ? DecodeResult::EOS : DecodeResult::OK;
    }

    /**
     * Capture packet frames until an EOF bit is found.

     * @param buffer the demodulated M17 symbols in LLR format.
     * @param viterbi_cost the cost of traversing the trellis.
     * @param frame_type is either BASIC_PACKET or FULL_PACKET.
     * @return the result of decoding the packet frame.
     */
    DecodeResult decode_packet(input_buffer_t& buffer, int& viterbi_cost, FrameType type)
    {
        depunctured_buffer_t depuncture_buffer;
        depuncture<368, 420, 8>(buffer, depuncture_buffer.packet, P3);
        viterbi_cost = viterbi_.decode(depuncture_buffer.packet, decode_buffer.packet);
        to_byte_array(decode_buffer.packet, output_buffer.packet);

        output_buffer.type = type;
        auto result = callback_(output_buffer, viterbi_cost);

        if (output_buffer.packet[25] & 0x80) // last packet;
        {
            state_ = State::LSF;
            return result ? DecodeResult::OK : DecodeResult::FAIL;
        }

        return DecodeResult::PACKET_INCOMPLETE;
    }

    /**
     * Decode M17 frames.  The decoder uses the sync word to determine frame
     * type and to update its state machine.
     *
     * The decoder receives M17 frame type indicator (based on sync word) and
     * frames from the M17 demodulator.
     *
     * If the frame is an LSF, the state immediately changes to LSF. When
     * in LSF mode, the state machine can transition to:
     *
     *  - LSF if the CRC is bad.
     *  - STREAM if the LSF type field indicates Stream.
     *  - BASIC_PACKET if the LSF type field indicates Packet and the packet
     *    type is RAW.
     *  - FULL_PACKET if the LSF type field indicates Packet and the packet
     *    type is ENCAPSULATED or RESERVED.
     *
     * When in LSF mode, if an LSF frame is received it is parsed as an LSF.
     * When a STREAM frame is received, it attempts to recover an LSF from
     * the LICH.  PACKET frame types are ignored when state is LSF.
     *
     * When in STREAM mode, the state machine can transition to either:
     *
     *  - STREAM when a any stream frame is received.
     *  - LSF when the EOS indicator is set, or when a packet frame is received.
     *
     * When in BASIC_PACKET mode, the state machine can transition to either:
     *
     *  - BASIC_PACKET when any packet frame is received.
     *  - LSF when the EOS indicator is set, or when a stream frame is received.
     *
     * When in FULL_PACKET mode, the state machine can transition to either:
     *
     *  - FULL_PACKET when any packet frame is received.
     *  - LSF when the EOS indicator is set, or when a stream frame is received.
     */
    DecodeResult operator()(SyncWordType frame_type, input_buffer_t& buffer, int& viterbi_cost)
    {
        derandomize_(buffer);
        interleaver_.deinterleave(buffer);

        // This is out state machined.
        switch(frame_type)
        {
        case SyncWordType::LSF:
            state_ = State::LSF;
            return decode_lsf(buffer, viterbi_cost);
        case SyncWordType::STREAM:
            switch (state_)
            {
            case State::LSF:
                return decode_lich(buffer, viterbi_cost);
            case State::STREAM:
                return decode_stream(buffer, viterbi_cost);
            default:
                state_ = State::LSF;
            }
            break;
        case SyncWordType::PACKET:
            switch (state_)
            {
            case State::BASIC_PACKET:
                return decode_packet(buffer, viterbi_cost, FrameType::BASIC_PACKET);
            case State::FULL_PACKET:
                return decode_packet(buffer, viterbi_cost, FrameType::FULL_PACKET);
            default:
                state_ = State::LSF;
            }
            break;
        case SyncWordType::BERT:
            state_ = State::BERT;
            return decode_bert(buffer, viterbi_cost);
        }

        return DecodeResult::FAIL;
    }

    State state() const { return state_; }
};

} // modemm17