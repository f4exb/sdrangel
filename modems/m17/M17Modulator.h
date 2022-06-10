#pragma once

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

namespace mobilinkd
{

/**
 * Common routines extracted from the original asynchronous M17 modulator.
 * It is used to produce the various symbol sequences but modulation is handled at
 * upper level.
 */
struct M17Modulator
{
public:
    using symbols_t = std::array<int8_t, 192>;      // One frame of symbols.
    using baseband_t = std::array<int16_t, 1920>;   // One frame of baseband data @ 48ksps
    using bitstream_t = std::array<uint8_t, 48>;    // M17 frame of bits (in bytes).
    using lsf_t = std::array<uint8_t, 30>;          // Link setup frame bytes.
    using lich_segment_t = std::array<uint8_t, 12>; // Golay-encoded LICH.
    using lich_t = std::array<lich_segment_t, 6>;   // All LICH segments.
    using audio_frame_t = std::array<int16_t, 320>;
    using codec_frame_t = std::array<uint8_t, 16>;
    using payload_t = std::array<uint8_t, 34>;      // Bytes in the payload of a data frame.
    using frame_t = std::array<uint8_t, 46>;        // M17 frame (without sync word).

    static constexpr std::array<uint8_t, 2> SYNC_WORD = {0x32, 0x43};
    static constexpr std::array<uint8_t, 2> LSF_SYNC_WORD = {0x55, 0xF7};
    static constexpr std::array<uint8_t, 2> DATA_SYNC_WORD = {0xFF, 0x5D};

    static constexpr int8_t bits_to_symbol(uint8_t bits)
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

    /**
     * Encode each LSF segment into a Golay-encoded LICH segment bitstream.
     */
    static
    lich_segment_t make_lich_segment(std::array<uint8_t, 5> segment, uint8_t segment_number)
    {
        lich_segment_t result;
        uint16_t tmp;
        uint32_t encoded;

        tmp = segment[0] << 4 | ((segment[1] >> 4) & 0x0F);
        encoded = mobilinkd::Golay24::encode24(tmp);

        for (size_t i = 0; i != 24; ++i)
        {
            assign_bit_index(result, i, (encoded & (1 << 23)) != 0);
            encoded <<= 1;
        }

        tmp = ((segment[1] & 0x0F) << 8) | segment[2];
        encoded = mobilinkd::Golay24::encode24(tmp);

        for (size_t i = 24; i != 48; ++i)
        {
            assign_bit_index(result, i, (encoded & (1 << 23)) != 0);
            encoded <<= 1;
        }

        tmp = segment[3] << 4 | ((segment[4] >> 4) & 0x0F);
        encoded = mobilinkd::Golay24::encode24(tmp);

        for (size_t i = 48; i != 72; ++i)
        {
            assign_bit_index(result, i, (encoded & (1 << 23)) != 0);
            encoded <<= 1;
        }

        tmp = ((segment[4] & 0x0F) << 8) | (segment_number << 5);
        encoded = mobilinkd::Golay24::encode24(tmp);

        for (size_t i = 72; i != 96; ++i)
        {
            assign_bit_index(result, i, (encoded & (1 << 23)) != 0);
            encoded <<= 1;
        }

        return result;
    }

    /**
     * Construct the link setup frame and split into LICH segments.
     */
    void make_link_setup(lich_t& lich, mobilinkd::M17Modulator::frame_t lsf)
    {
        using namespace mobilinkd;

        lsf_t lsf;
        lsf.fill(0);

        auto rit = std::copy(source_.begin(), source_.end(), lsf.begin());
        std::copy(dest_.begin(), dest_.end(), rit);
        lsf[12] = 0;
        lsf[13] = 5;

        crc_.reset();

        for (size_t i = 0; i != 28; ++i) {
            crc_(lsf[i]);
        }

        auto checksum = crc_.get_bytes();
        lsf[28] = checksum[0];
        lsf[29] = checksum[1];

        // Build LICH segments
        for (size_t i = 0; i != lich.size(); ++i)
        {
            std::array<uint8_t, 5> segment;
            std::copy(lsf.begin() + i * 5, lsf.begin() + (i + 1) * 5, segment.begin());
            auto lich_segment = make_lich_segment(segment, i);
            std::copy(lich_segment.begin(), lich_segment.end(), lich[i].begin());
        }

        auto encoded = conv_encode(lsf);

        auto size = puncture_bytes(encoded, lsf, P1);
        assert(size == 368);

        interleaver_.interleave(lsf);
        randomizer_(lsf);
    }

    /**
     * Append the LICH and Convolutionally encoded payload, interleave and randomize
     * the frame bits, and produce the frame.
     */
    void make_audio_frame(const lich_segment_t& lich, const payload_t& data, mobilinkd::M17Modulator::frame_t& frame)
    {
        using namespace mobilinkd;

        auto it = std::copy(lich.begin(), lich.end(), frame.begin());
        std::copy(data.begin(), data.end(), it);

        interleaver_.interleave(frame);
        randomizer_(frame);
    }

    /**
     * Assemble the audio frame payload by appending the frame number, encoded audio,
     * and CRC, then convolutionally coding and puncturing the data.
     */
    payload_t make_payload(uint16_t frame_number, const codec_frame_t& payload)
    {
        std::array<uint8_t, 20> data;   // FN, Audio, CRC = 2 + 16 + 2;
        data[0] = uint8_t((frame_number >> 8) & 0xFF);
        data[1] = uint8_t(frame_number & 0xFF);
        std::copy(payload.begin(), payload.end(), data.begin() + 2);
        crc_.reset();

        for (size_t i = 0; i != 18; ++i) {
            crc_(data[i]);
        }

        auto checksum = crc_.get_bytes();
        data[18] = checksum[0];
        data[19] = checksum[1];

        auto encoded = conv_encode(data);

        payload_t punctured;
        auto size = puncture_bytes(encoded, punctured, mobilinkd::P2);
        assert(size == 272);
        return punctured;
    }

    /*
    * Converts a suite of 192 symbols (from the 384 bits of a frame) into 1920 16 bit integer samples to be used
    * in the final FM modulator (baseband). Sample rate is expected to be 48 kS/s. This is the original 48 kS/s
    * 16 bit audio output of the modulator.
    */
    static baseband_t symbols_to_baseband(const symbols_t& symbols)
    {
        // Generated using scikit-commpy
        static const auto rrc_taps = std::array<float, 79>{
            -0.009265784007800534, -0.006136551625729697, -0.001125978562075172, 0.004891777252042491,
            0.01071805138282269, 0.01505751553351295, 0.01679337935001369, 0.015256245142156299,
            0.01042830577908502, 0.003031522725559901,  -0.0055333532968188165, -0.013403099825723372,
            -0.018598682349642525, -0.01944761739590459, -0.015005271935951746, -0.0053887880354343935,
            0.008056525910253532, 0.022816244158307273, 0.035513467692208076, 0.04244131815783876,
            0.04025481153629372, 0.02671818654865632, 0.0013810216516704976, -0.03394615682795165,
            -0.07502635967975885, -0.11540977897637611, -0.14703962203941534, -0.16119995609538576,
            -0.14969512896336504, -0.10610329539459686, -0.026921412469634916, 0.08757875030779196,
            0.23293327870303457, 0.4006012210123992, 0.5786324696325503, 0.7528286479934068,
            0.908262741447522, 1.0309661131633199, 1.1095611856548013, 1.1366197723675815,
            1.1095611856548013, 1.0309661131633199, 0.908262741447522, 0.7528286479934068,
            0.5786324696325503,  0.4006012210123992, 0.23293327870303457, 0.08757875030779196,
            -0.026921412469634916, -0.10610329539459686, -0.14969512896336504, -0.16119995609538576,
            -0.14703962203941534, -0.11540977897637611, -0.07502635967975885, -0.03394615682795165,
            0.0013810216516704976, 0.02671818654865632, 0.04025481153629372,  0.04244131815783876,
            0.035513467692208076, 0.022816244158307273, 0.008056525910253532, -0.0053887880354343935,
            -0.015005271935951746, -0.01944761739590459, -0.018598682349642525, -0.013403099825723372,
            -0.0055333532968188165, 0.003031522725559901, 0.01042830577908502, 0.015256245142156299,
            0.01679337935001369, 0.01505751553351295, 0.01071805138282269, 0.004891777252042491,
            -0.001125978562075172, -0.006136551625729697, -0.009265784007800534
        };
        static BaseFirFilter<std::tuple_size<decltype(rrc_taps)>::value> rrc = makeFirFilter(rrc_taps);

        std::array<int16_t, 1920> baseband;
        baseband.fill(0);

        for (size_t i = 0; i != symbols.size(); ++i) {
            baseband[i * 10] = symbols[i];
        }

        for (auto& b : baseband) {
            b = rrc(b) * 25;
        }

        return baseband;
    }

    M17Modulator(const std::string& source, const std::string& dest = "") :
        source_(encode_callsign(source)),
        dest_(encode_callsign(dest))
    { }

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

private:
    M17ByteRandomizer<46> randomizer_;
    PolynomialInterleaver<45, 92, 368> interleaver_;
    CRC16<0x5935, 0xFFFF> crc_;
    LinkSetupFrame::encoded_call_t source_;
    LinkSetupFrame::encoded_call_t dest_;

    static LinkSetupFrame::encoded_call_t encode_callsign(std::string callsign)
    {
        LinkSetupFrame::encoded_call_t encoded_call = {0xff,0xff,0xff,0xff,0xff,0xff};

        if (callsign.empty() || callsign.size() > 9) {
            return encoded_call;
        }

        mobilinkd::LinkSetupFrame::call_t call;
        call.fill(0);
        std::copy(callsign.begin(), callsign.end(), call.begin());
        encoded_call = LinkSetupFrame::encode_callsign(call);
        return encoded_call;
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

} // mobilinkd
