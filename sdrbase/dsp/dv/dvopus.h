///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_DSP_DV_DVOPUS_H
#define INCLUDE_DSP_DV_DVOPUS_H

#include <cstdint>
#include <string>

#include "export.h"

struct OpusEncoder;
struct OpusDecoder;

/**
 * Angel61 codec family O1 (ANGEL61.md 3.1.2), WB's tier 3: Opus at 18.4 kbit/s, mono, 48 kHz, one
 * 80 ms packet of exactly 184 bytes a frame (two or more Opus frames in the packet). The settings are
 * normative: OPUS_APPLICATION_AUDIO (audio, not speech - that is the point of the family), constant
 * bit rate, in band FEC off, the bandwidth left to the encoder. Here so that both plugins use one
 * encoder set up one way, and neither links libopus itself.
 */
class SDRBASE_API DVOpus
{
public:
    static constexpr int SampleRate = 48000;
    static constexpr int FrameSamples = 3840;   //!< 80 ms
    static constexpr int PacketBytes = 184;     //!< 1472 bits, WB tier 3's (ANGEL61.md 2.3)
    static constexpr int BitRate = 18400;
    /**
     * The decoder's default complexity: 5, Deep PLC, the neural concealment of lost packets of
     * libopus 1.5 and later built with it (OPUS_OSCE or OPUS_DRED). Measured on revision 2's
     * 60 ms O1 frames: PESQ-WB +0.25 with 5% of the frames lost, +0.19 with 20%, the
     * same when none is; 0.2% of real time in all. LACE and NoLACE (6, 7) add nothing at 18.4
     * kbit/s; the bandwidth extension is not measured by PESQ and stays off until it is listened to.
     */
    static constexpr int DefaultComplexity = 5;

    //! The library's version ("libopus 1.6.1")
    static std::string getVersion();
    //! What a packet's TOC says: the mode (SILK, hybrid, CELT), the audio bandwidth and the frames
    static std::string describePacket(const uint8_t *packet, int nbBytes);

    class SDRBASE_API Encoder
    {
    public:
        Encoder();
        ~Encoder();
        //! Packets of frameSamples and exactly packetBytes: 18.4 kbit/s at the defaults
        bool open(int frameSamples = FrameSamples, int packetBytes = PacketBytes);
        void close();
        bool isOpen() const { return m_encoder != nullptr; }
        //! frameSamples in, one packet of packetBytes out. False if the encoder is not open or failed
        bool encode(const int16_t *pcm, uint8_t *packet);
        int getFrameSamples() const { return m_frameSamples; }
        int getPacketBytes() const { return m_packetBytes; }

    private:
        OpusEncoder *m_encoder;
        int m_frameSamples;
        int m_packetBytes;
    };

    class SDRBASE_API Decoder
    {
    public:
        Decoder();
        ~Decoder();
        //! Packets of frameSamples and packetBytes
        bool open(int frameSamples = FrameSamples, int packetBytes = PacketBytes);
        void close();
        bool isOpen() const { return m_decoder != nullptr; }
        //! One packet in, frameSamples out. False if the packet did not decode
        bool decode(const uint8_t *packet, int16_t *pcm);
        //! A lost packet: a frame of the decoder's concealment, from its own state (3.1.1's rule)
        bool decodeLost(int16_t *pcm);
        int getFrameSamples() const { return m_frameSamples; }
        int getPacketBytes() const { return m_packetBytes; }
        //! The decoder's state cleared (a new transmission)
        void reset();
        /**
         * libopus 1.5 and later, built with OSCE: the decoder complexity (5 and above: Deep PLC,
         * the neural concealment of lost packets; 6: LACE; 7: NoLACE, speech enhancement of SILK
         * frames) and 1.6's bandwidth extension of wideband SILK speech to fullband (complexity 4
         * and above, a library built with ENABLE_OSCE_BWE). Ignored by a library without them.
         * Applied now and at every open; DefaultComplexity and no extension until called.
         */
        void setEnhancements(int complexity, bool bandwidthExtension);

    private:
        OpusDecoder *m_decoder;
        int m_frameSamples;
        int m_packetBytes;
        int m_complexity;
        bool m_bandwidthExtension;

        void applyEnhancements();
    };
};

#endif // INCLUDE_DSP_DV_DVOPUS_H
