#pragma once

#include <QDebug>
#include <QString>

#include "FirFilter.h"
#include "LinkSetupFrame.h"
#include "CRC16.h"
#include "Convolution.h"
#include "PolynomialInterleaver.h"
#include "M17Randomizer.h"
#include "Util.h"
#include "Golay24.h"
#include "Trellis.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>

#include "export.h"

namespace modemm17
{

/**
 * Common routines extracted from the original asynchronous M17 modulator.
 * It is used to produce the various symbol sequences but modulation is handled at
 * upper level.
 */
struct MODEMM17_API M17Modulator
{
public:
    using symbols_t = std::array<int8_t, 192>;      // One frame of symbols.
    using baseband_t = std::array<int16_t, 1920>;   // One frame of baseband data @ 48ksps
    using bitstream_t = std::array<uint8_t, 48>;    // M17 frame of bits (in bytes).
    using lsf_t = std::array<uint8_t, 30>;          // Link setup frame bytes.
    using lich_segment_t = std::array<uint8_t, 96>; // Golay-encoded LICH bits.
    using lich_t = std::array<lich_segment_t, 6>;   // All LICH segments.
    using audio_frame_t = std::array<int16_t, 320>;
    using codec_frame_t = std::array<uint8_t, 16>;
    using payload_t = std::array<uint8_t, 34>;      // Bytes in the payload of a data frame.
    using frame_t = std::array<uint8_t, 46>;        // M17 frame (without sync word).
    using packet_t = std::array<uint8_t, 25>;       // Packet payload

    static const std::array<uint8_t, 2> SYNC_WORD;
    static const std::array<uint8_t, 2> LSF_SYNC_WORD;
    static const std::array<uint8_t, 2> STREAM_SYNC_WORD;
    static const std::array<uint8_t, 2> PACKET_SYNC_WORD;
    static const std::array<uint8_t, 2> BERT_SYNC_WORD;
    static const std::array<uint8_t, 2> EOT_SYNC;

    static int8_t bits_to_symbol(uint8_t bits)
    {
        switch (bits)
        {
        case 0: return 1;
        case 1: return 3;
        case 2: return -1;
        case 3: return -3;
        }

        return 0;
    }

    template <typename T, size_t N>
    static std::array<int8_t, N / 2> bits_to_symbols(const std::array<T, N>& bits)
    {
        std::array<int8_t, N / 2> result;
        size_t index = 0;
        for (size_t i = 0; i != N; i += 2)
        {
            result[index++] = bits_to_symbol((bits[i] << 1) | bits[i + 1]);
        }
        return result;
    }

    template <typename T, size_t N>
    static std::array<int8_t, N * 4> bytes_to_symbols(const std::array<T, N>& bytes)
    {
        std::array<int8_t, N * 4> result;
        size_t index = 0;
        for (auto b : bytes)
        {
            for (size_t i = 0; i != 4; ++i)
            {
                result[index++] = bits_to_symbol(b >> 6);
                b <<= 2;
            }
        }
        return result;
    }

    /*
    * Converts a suite of 192 symbols (from the 384 bits of a frame) into 1920 16 bit integer samples to be used
    * in the final FM modulator (baseband). Sample rate is expected to be 48 kS/s. This is the original 48 kS/s
    * 16 bit audio output of the modulator.
    */
    template <size_t N>
    std::array<int16_t, N*10> symbols_to_baseband(std::array<int8_t, N> symbols, bool invert = false)
    {
        std::array<int16_t, N*10> baseband;
        baseband.fill(0);

        for (size_t i = 0; i != symbols.size(); ++i) {
            baseband[i * 10] = symbols[i];
        }

        for (auto& b : baseband) {
            b = rrc(b) * 7168.0 * (invert ? -1.0 : 1.0);
        }

        return baseband;
    }

    std::array<int8_t, 368> make_lsf(lsf_t& lsf, bool streamElsePacket = false)
    {
        lsf.fill(0);

        M17Randomizer randomizer;
        PolynomialInterleaver<45, 92, 368> interleaver;
        CRC16 crc(0x5935, 0xFFFF);

        auto rit = std::copy(dest_.begin(), dest_.end(), lsf.begin());
        std::copy(source_.begin(), source_.end(), rit);

        lsf[12] = can_ >> 1;
        lsf[13] = (streamElsePacket ? 5 : 2) | ((can_ & 1) << 7);

        if (gnss_on_)
        {
            lsf[13] |= (1<<5);
            std::copy(gnss_.begin(), gnss_.end(), &lsf[14]);
        }

        crc.reset();

        for (size_t i = 0; i != 28; ++i) {
            crc(lsf[i]);
        }

        std::array<uint8_t, 2> checksum = crc.get_bytes();
        lsf[28] = checksum[0];
        lsf[29] = checksum[1];

        std::array<uint8_t, 488> encoded;
        size_t index = 0;
        uint32_t memory = 0;

        for (auto b : lsf)
        {
            for (size_t i = 0; i != 8; ++i)
            {
                uint32_t x = (b & 0x80) >> 7;
                b <<= 1;
                memory = modemm17::update_memory<4>(memory, x);
                encoded[index++] = modemm17::convolve_bit(031, memory);
                encoded[index++] = modemm17::convolve_bit(027, memory);
            }
        }

        // Flush the encoder.
        for (size_t i = 0; i != 4; ++i)
        {
            memory = modemm17::update_memory<4>(memory, 0);
            encoded[index++] = modemm17::convolve_bit(031, memory);
            encoded[index++] = modemm17::convolve_bit(027, memory);
        }

        std::array<int8_t, 368> punctured;
        auto size = puncture<488, 368, 61>(encoded, punctured, P1);

        if (size != 368) {
            qWarning() << "modemm17::M17Modulator::make_lsf: incorrect size (not 368)" << size;
        }

        interleaver.interleave(punctured);
        randomizer.randomize(punctured);

        return punctured;
    }

    static lich_segment_t make_lich_segment(std::array<uint8_t, 5> segment, uint8_t segment_number)
    {
        lich_segment_t result;
        uint16_t tmp;
        uint32_t encoded;

        tmp = segment[0];
        tmp <<= 4;
        tmp |= ((segment[1] >> 4) & 0x0F);
        // tmp = segment[0] << 4 | ((segment[1] >> 4) & 0x0F);
        encoded = modemm17::Golay24::encode24(tmp);

        for (size_t i = 0; i != 24; ++i)
        {
            result[i] = (encoded & (1 << 23)) != 0 ? 1 : 0;
            encoded <<= 1;
        }

        tmp = segment[1] & 0x0F;
        tmp <<= 8;
        tmp |= segment[2];
        // tmp = ((segment[1] & 0x0F) << 8) | segment[2];
        encoded = modemm17::Golay24::encode24(tmp);

        for (size_t i = 24; i != 48; ++i)
        {
            result[i] = (encoded & (1 << 23)) != 0 ? 1 : 0;
            encoded <<= 1;
        }

        tmp = segment[3];
        tmp <<= 4;
        tmp |= ((segment[4] >> 4) & 0x0F);
        // tmp = segment[3] << 4 | ((segment[4] >> 4) & 0x0F);
        encoded = modemm17::Golay24::encode24(tmp);

        for (size_t i = 48; i != 72; ++i)
        {
            result[i] = (encoded & (1 << 23)) != 0 ? 1 : 0;
            encoded <<= 1;
        }

        tmp = segment[4] & 0x0F;
        tmp <<= 8;
        tmp |= (segment_number << 5);
        // tmp = ((segment[4] & 0x0F) << 8) | (segment_number << 5);
        encoded = modemm17::Golay24::encode24(tmp);

        for (size_t i = 72; i != 96; ++i)
        {
            result[i] = (encoded & (1 << 23)) != 0 ? 1 : 0;
            encoded <<= 1;
        }

        return result;
    }

    static std::array<int8_t, 272> make_stream_data_frame(uint16_t frame_number, const codec_frame_t& payload)
    {
        std::array<uint8_t, 18> data;   // FN, payload = 2 + 16;
        data[0] = uint8_t((frame_number >> 8) & 0xFF);
        data[1] = uint8_t(frame_number & 0xFF);
        std::copy(payload.begin(), payload.end(), data.begin() + 2);
        std::array<uint8_t, 296> encoded;
        size_t index = 0;
        uint32_t memory = 0;

        for (auto b : data)
        {
            for (size_t i = 0; i != 8; ++i)
            {
                uint32_t x = (b & 0x80) >> 7;
                b <<= 1;
                memory = modemm17::update_memory<4>(memory, x);
                encoded[index++] = modemm17::convolve_bit(031, memory);
                encoded[index++] = modemm17::convolve_bit(027, memory);
            }
        }

        // Flush the encoder.
        for (size_t i = 0; i != 4; ++i)
        {
            memory = modemm17::update_memory<4>(memory, 0);
            encoded[index++] = modemm17::convolve_bit(031, memory);
            encoded[index++] = modemm17::convolve_bit(027, memory);
        }

        std::array<int8_t, 272> punctured;
        auto size = modemm17::puncture<296, 272, 12>(encoded, punctured, modemm17::P2);

        if (size != 272) {
            qWarning() << "modemm17::M17Modulator::make_stream_data_frame: incorrect size (not 272)" << size;
        }

        return punctured;
    }

    std::array<int8_t, 368> make_packet_frame(
        uint8_t packet_number,
        int packet_size,
        bool last_packet,
        const std::array<uint8_t, 25> packet
    )
    {
        M17Randomizer randomizer;
        PolynomialInterleaver<45, 92, 368> interleaver;

        std::array<uint8_t, 26> packet_assembly;
        packet_assembly.fill(0);
        std::copy(packet.begin(), packet.begin() + packet_size, packet_assembly.begin());

        if (packet_number == 0) {
            crc_.reset();
        }

        for (int i = 0; i < packet_size; i++) {
            crc_(packet[i]);
        }

        if (last_packet)
        {
            packet_assembly[25] = 0x80 | ((packet_size+2)<<2); // sent packet size includes CRC
            packet_assembly[packet_size]   = crc_.get_bytes()[0];
            packet_assembly[packet_size+1] = crc_.get_bytes()[1];
        }
        else
        {
            packet_assembly[25] = (packet_number<<2);
        }

        std::array<uint8_t, 420> encoded;
        size_t index = 0;
        uint32_t memory = 0;
        uint8_t b;

        for (int bi = 0; bi < 25; bi++)
        {
            b = packet_assembly[bi];

            for (size_t i = 0; i != 8; ++i)
            {
                uint32_t x = (b & 0x80) >> 7;
                b <<= 1;
                memory = modemm17::update_memory<4>(memory, x);
                encoded[index++] = modemm17::convolve_bit(031, memory);
                encoded[index++] = modemm17::convolve_bit(027, memory);
            }
        }

        b = packet_assembly[25];

        for (size_t i = 0; i != 6; ++i)
        {
            uint32_t x = (b & 0x80) >> 7;
            b <<= 1;
            memory = modemm17::update_memory<4>(memory, x);
            encoded[index++] = modemm17::convolve_bit(031, memory);
            encoded[index++] = modemm17::convolve_bit(027, memory);
        }

        // Flush the encoder.
        for (size_t i = 0; i != 4; ++i)
        {
            memory = modemm17::update_memory<4>(memory, 0);
            encoded[index++] = modemm17::convolve_bit(031, memory);
            encoded[index++] = modemm17::convolve_bit(027, memory);
        }

        std::array<int8_t, 368> punctured;
        auto size = puncture<420, 368, 8>(encoded, punctured, P3);

        if (size != 368) {
            qWarning() << "modemm17::M17Modulator::make_packet_frame: incorrect size (not 368)" << size;
        }

        interleaver.interleave(punctured);
        randomizer.randomize(punctured);

        return punctured;
    }

    static std::array<int8_t, 368> make_bert_frame(PRBS9& prbs)
    {
        std::array<uint8_t, 25> data;   // 24.6125 bytes, 197 bits

        // Generate the data (24*8 = 192 bits).
        for (size_t i = 0; i != data.size() - 1; ++i)
        {
            uint8_t byte = 0;

            for (int i = 0; i != 8; ++i)
            {
                byte <<= 1;
                byte |= prbs.generate();
            }

            data[i] = byte;
        }

        // Generate the data (last 5 bits).
        uint8_t byte = 0;
        for (int i = 0; i != 5; ++i)
        {
            byte <<= 1;
            byte |= prbs.generate();
        }

        byte <<= 3;
        data[24] = byte;

        // Convolutional encode
        std::array<uint8_t, 402> encoded;
        size_t index = 0;
        uint32_t memory = 0;

        // 24*8 = 192 first bits
        for (size_t i = 0; i != data.size() - 1; ++i)
        {
            auto b = data[i];

            for (size_t j = 0; j != 8; ++j)
            {
                uint32_t x = (b & 0x80) >> 7;
                b <<= 1;
                memory = update_memory<4>(memory, x);
                encoded[index++] = convolve_bit(031, memory);
                encoded[index++] = convolve_bit(027, memory);
            }
        }

        // last 5 bits
        auto b = data[24];
        for (size_t j = 0; j != 5; ++j)
        {
            uint32_t x = (b & 0x80) >> 7;
            b <<= 1;
            memory = update_memory<4>(memory, x);
            encoded[index++] = convolve_bit(031, memory);
            encoded[index++] = convolve_bit(027, memory);
        }

        // Flush the encoder.
        for (size_t i = 0; i != 4; ++i)
        {
            memory = update_memory<4>(memory, 0);
            encoded[index++] = convolve_bit(031, memory);
            encoded[index++] = convolve_bit(027, memory);
        }

        std::array<int8_t, 368> punctured;
        auto size = puncture<402, 368, 12>(encoded, punctured, P2);

        if (size != 368) {
            qWarning() << "modemm17::M17Modulator::make_bert_frame: incorrect size (not 368)" << size;
        }

        return punctured;
    }

    static void interleave_and_randomize(std::array<int8_t, 368>& punctured)
    {
        M17Randomizer randomizer;
        PolynomialInterleaver<45, 92, 368> interleaver;

        interleaver.interleave(punctured);
        randomizer.randomize(punctured);
    }

    M17Modulator(const std::string& source, const std::string& dest = "") :
        source_(encode_callsign(source)),
        dest_(encode_callsign(dest)),
        can_(10),
        rrc(makeFirFilter(rrc_taps)),
        crc_(0x5935, 0xFFFF)
    {
        gnss_.fill(0);
        gnss_on_ = false;
    }

    /**
     * Set the source identifier (callsign) for the transmitter.
     */
    void source(const std::string& callsign) {
        source_ = encode_callsign(callsign);
    }

    /**
     * Set the destination identifier for the transmitter.  A blank value is
     * interpreted as the broadcast address.  This is the default.
     */
    void dest(const std::string& callsign) {
        dest_ = encode_callsign(callsign);
    }

    /**
     * Set the Channel Access Number (0..15)
     */
    void can(uint8_t can) {
        can_ = can & 0xF;
    }

    /**
    * Set GNSS data
    */
    void set_gnss(float lat, float lon, float alt)
    {
        gnss_ = LinkSetupFrame::encode_gnss(lat, lon, alt);
        gnss_on_ = true;
    }

    /**
    * Reset GNSS data
    */
    void reset_gnss()
    {
        gnss_.fill(0);
        gnss_on_ = false;
    }

private:
    LinkSetupFrame::encoded_call_t source_;
    LinkSetupFrame::encoded_call_t dest_;
    LinkSetupFrame::gnss_t gnss_;
    bool gnss_on_;
    uint8_t can_;
    BaseFirFilter<150> rrc;
    static const std::array<float, 150> rrc_taps;
    CRC16 crc_;

    static LinkSetupFrame::encoded_call_t encode_callsign(std::string callsign)
    {
        LinkSetupFrame::encoded_call_t encoded_call = {0xff,0xff,0xff,0xff,0xff,0xff};

        if (callsign.empty() || callsign.size() > 9) {
            return encoded_call;
        }

        modemm17::LinkSetupFrame::call_t call;
        call.fill(0);
        std::copy(callsign.begin(), callsign.end(), call.begin());
        encoded_call = LinkSetupFrame::encode_callsign(call);
        return encoded_call;
    }

    template <typename T, size_t N>
    static std::array<T, N * 2 + 1> conv_encode(std::array<T, N> data)
    {
        std::array<T, N * 2 + 1> result;

        uint8_t bit_index = 0;
        uint8_t byte_index = 0;
        uint8_t tmp = 0;

        uint32_t memory = 0;

        for (auto b : data)
        {
            for (size_t i = 0; i != 8; ++i)
            {
                uint32_t x = (b & 0x80) >> 7;
                b <<= 1;
                memory = update_memory<4>(memory, x);
                tmp = (tmp << 1) | convolve_bit(031, memory);
                tmp = (tmp << 1) | convolve_bit(027, memory);
                bit_index += 2;

                if (bit_index == 8)
                {
                    bit_index = 0;
                    result[byte_index++] = tmp;
                    tmp = 0;
                }
            }
        }

        // Flush the encoder.
        for (size_t i = 0; i != 4; ++i)
        {
            memory = update_memory<4>(memory, 0);
            tmp = (tmp << 1) | convolve_bit(031, memory);
            tmp = (tmp << 1) | convolve_bit(027, memory);
            bit_index += 2;

            if (bit_index == 8)
            {
                bit_index = 0;
                result[byte_index++] = tmp;
                tmp = 0;
            }
        }

        // Frame may not end on a byte boundary.
        if (bit_index != 0)
        {
            while (bit_index++ != 8) {
                tmp <<= 1;
            }

            result[byte_index] = tmp;
        }

        return result;
    }
};

} // modemm17
