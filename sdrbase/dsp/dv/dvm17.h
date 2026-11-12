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

#ifndef INCLUDE_DSP_DV_DVM17_H
#define INCLUDE_DSP_DV_DVM17_H

#include <cstdint>
#include <string>
#include <vector>

#include "export.h"

/**
 * M17 (https://spec.m17project.org): 4FSK at 4800 symbols/s in a 12.5 kHz channel, 40 ms frames
 * of 192 symbols, Codec2 3200 voice or packets. The pieces of the air interface, as the other
 * protocols of this library have them: the link setup frame and its callsigns, the rate 1/2
 * K=5 convolutional code (the same one YSF and NXDN use, punctured three ways), the quadratic
 * interleaver, the randomiser, the LICH of a stream frame, and the frames themselves as 4FSK
 * levels. No Qt and no state: the transmitter and the receiver of the plugins hold that.
 */
class SDRBASE_API DVM17
{
public:
    static constexpr int SymbolRate = 4800;
    static constexpr int FrameSymbols = 192;       //!< 40 ms
    static constexpr int FrameMs = 40;
    static constexpr int SyncSymbols = 8;
    static constexpr int SyncBits = 16;
    static constexpr int FrameBits = 384;
    static constexpr int PayloadBits = 368;        //!< After the sync: what is interleaved and randomised
    static constexpr int LSFBytes = 30;
    static constexpr int LSFBits = 240;            //!< Destination (48), source (48), type (16), meta (112), CRC (16)
    static constexpr int StreamDataBits = 144;     //!< Frame number (16) and payload (128)
    static constexpr int StreamCodedBits = 272;    //!< After P2
    static constexpr int PacketDataBits = 206;     //!< 25 bytes, an end of packet flag and a 5 bit counter
    static constexpr int PacketBytes = 25;
    static constexpr int LICHBits = 96;            //!< Four Golay(24,12) codewords of a sixth of the LSF
    static constexpr int LICHSegments = 6;
    static constexpr int FlushBits = 4;            //!< Tail bits returning the encoder to state 0
    static constexpr int PeakDeviationHz = 2400;   //!< Outer symbols; the inner ones are a third of it
    static const double RrcAlpha;              //!< 0.5, over 8 symbols

    //! Sync words, most significant bit first (the first symbol is the two most significant bits)
    enum SyncWord
    {
        SyncLSF = 0x55F7,
        SyncStream = 0xFF5D,
        SyncPacket = 0x75FF,
        SyncBERT = 0xDF55,
        SyncEOT = 0x555D
    };

    //! Dibit 01 is +3, 00 is +1, 10 is -1, 11 is -3
    static int8_t dibitToLevel(int dibit);
    static int levelToDibit(int8_t level);
    //! The 8 symbols of a sync word
    static void syncLevels(int syncWord, int8_t *levels);

    static constexpr uint64_t CallsignBroadcast = 0xFFFFFFFFFFFFull;
    //! Base-40 over up to 9 characters (the encoding Angel61 borrowed); "ALL" and "" are the broadcast
    static uint64_t encodeCallsign(const std::string& callsign);
    static std::string decodeCallsign(uint64_t code);

    /** The link setup frame (30 bytes): who is talking to whom and what the transmission carries */
    struct LSF
    {
        uint64_t m_destination = CallsignBroadcast;
        uint64_t m_source = 0;
        uint16_t m_type = 0x0005;      //!< stream, voice: see the type helpers
        uint8_t m_meta[14] = { 0 };

        // The type field (bit 0 is the least significant bit of the 16 bit word)
        bool isStream() const { return (m_type & 0x0001) != 0; }
        int getDataType() const { return (m_type >> 1) & 0x3; }      //!< 1 data, 2 voice, 3 voice and data
        int getEncryption() const { return (m_type >> 3) & 0x3; }
        int getCAN() const { return (m_type >> 7) & 0xF; }           //!< Channel access number
        void setStream(bool stream) { m_type = (uint16_t) ((m_type & ~0x0001) | (stream ? 1 : 0)); }
        void setDataType(int type) { m_type = (uint16_t) ((m_type & ~0x0006) | ((type & 3) << 1)); }
        void setCAN(int can) { m_type = (uint16_t) ((m_type & ~0x0780) | ((can & 0xF) << 7)); }
    };

    static uint16_t crc16(const uint8_t *bytes, int nbBytes);      //!< Polynomial 0x5935, all ones in
    static void packLSF(const LSF& lsf, uint8_t *bytes);           //!< 30 bytes with the CRC
    static bool unpackLSF(const uint8_t *bytes, LSF& lsf);         //!< Checks the CRC

    // ---- Coding -------------------------------------------------------------------------------
    //! The puncture patterns of the three frame kinds (1 keeps a coded bit, 0 drops it)
    static const int8_t m_puncture1[61];    //!< Link setup: 488 coded bits to 368
    static const int8_t m_puncture2[12];    //!< Stream: 296 to 272
    static const int8_t m_puncture3[8];     //!< Packet: 420 to 368

    //! nbBits data bits and four flush bits through the rate 1/2 K=5 code, then the pattern
    static int encodePunctured(const uint8_t *bits, int nbBits, const int8_t *pattern, int patternLength, uint8_t *out);
    //! The reverse: the punctured LLRs to the data bits (zero LLR where a bit was dropped)
    static bool decodePunctured(const float *llr, int nbCoded, int nbBits, const int8_t *pattern, int patternLength,
        uint8_t *bits, int *cost = nullptr);

    //! The quadratic permutation of the 368 payload bits, and its inverse for LLRs
    static void interleave(const uint8_t *in, uint8_t *out);
    static void deinterleave(const float *in, float *out);
    //! The 46 byte randomiser, its own inverse; the LLR form flips the sign of the bits it inverts
    static void randomise(uint8_t *bits, int nbBits);
    static void randomiseLLR(float *llr, int nbBits);

    //! A sixth of the LSF (five bytes) and its segment number as the 96 bit LICH of a stream frame
    static void packLICH(const uint8_t *lsfBytes, int segment, uint8_t *bits);
    //! Returns the segment number, or -1 when a codeword could not be corrected
    static int unpackLICH(const float *llr, uint8_t *lsfChunk);

    // ---- Frames (192 levels each) -------------------------------------------------------------
    static void buildLSFFrame(const LSF& lsf, int8_t *levels);
    /**
     * A stream frame: the LICH segment of the LSF, the frame number (bit 15 marks the last frame)
     * and 16 bytes of payload (two Codec2 3200 frames of a voice transmission).
     */
    static void buildStreamFrame(const uint8_t *lsfBytes, int segment, uint16_t frameNumber, const uint8_t *payload, int8_t *levels);
    /**
     * A packet frame: 25 bytes, or fewer in the last one of a packet, where the counter is the
     * number of bytes it carries.
     */
    static void buildPacketFrame(const uint8_t *payload, int nbBytes, bool last, int counter, int8_t *levels);
    static void buildEOTFrame(int8_t *levels);

    //! The payload bits of a frame from its 384 LLRs: deinterleaved and derandomised, sync skipped
    static void framePayload(const float *llr, float *payload);
    static bool decodeLSFFrame(const float *payload, LSF& lsf, int *cost = nullptr);
    //! frameNumber and 16 bytes of payload; segment and lsfChunk are the LICH when it decoded
    static bool decodeStreamFrame(const float *payload, uint16_t& frameNumber, uint8_t *payloadBytes,
        int& segment, uint8_t *lsfChunk, int *cost = nullptr);
    //! nbBytes bytes of the packet; last and counter say whether the packet ends here
    static bool decodePacketFrame(const float *payload, uint8_t *bytes, bool& last, int& counter, int *cost = nullptr);

    // ---- Packets (the payload of a packet transmission) ---------------------------------------
    enum Protocol
    {
        ProtocolRaw = 0x00,
        ProtocolAX25 = 0x01,
        ProtocolAPRS = 0x02,
        Protocol6LoWPAN = 0x03,
        ProtocolIPv4 = 0x04,
        ProtocolSMS = 0x05,
        ProtocolWinlink = 0x06
    };
    static constexpr int MaxPacketBytes = 823;   //!< 33 frames of 25 bytes, CRC included

    //! protocol byte, content, CRC-16: what a packet transmission's frames carry
    static std::vector<uint8_t> buildPacket(int protocol, const std::vector<uint8_t>& content);
    //! Checks the CRC and returns the protocol, or -1
    static int parsePacket(const std::vector<uint8_t>& packet, std::vector<uint8_t>& content);
};

#endif // INCLUDE_DSP_DV_DVM17_H
