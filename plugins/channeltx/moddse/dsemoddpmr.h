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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODDPMR_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODDPMR_H_

#include <deque>
#include <string>
#include <utility>

#include "dsemodprotocol.h"

/**
 * dPMR (ETSI TS 102 490 / TS 102 658 Mode 1 and 2) voice call frame builder.
 *
 * 4FSK, 2400 symbols/s (4800 bit/s), +/- 1050 Hz and +/- 350 Hz deviation.
 * A transmission is: preamble, header frame (FS1 HI0 CC HI1), an integral number
 * of superframes (4 payload frames of 80 ms: FS2/CC + CCH + 4 x TCH), then an end
 * frame (FS3 + END). Each nextFrame() call produces one 80 ms frame (192 symbols)
 * except the end frame which is shorter.
 *
 * All bit arrays hold one bit per byte, MSB first. The coding chain of the
 * header (HI), control channel (CCH) and END fields is CRC + Hamming(12,8) +
 * block interleave + scrambling exactly as ETSI TS 102 490 clause 7 and the
 * dsdcc decoder (DSDdPMR) expect.
 */
class DSEModDPMR : public DSEModProtocol
{
public:
    //! Header type (TS 102 490 table 5.11)
    enum HeaderType
    {
        HeaderTypeCommStart = 0,   //!< Communication start header (a superframe follows)
        HeaderTypeConnReq = 1,
        HeaderTypeUnconnReq = 2,
        HeaderTypeAck = 3,
        HeaderTypeSysReq = 4,
        HeaderTypeAckReply = 5,
        HeaderTypeSysDeliv = 6,
        HeaderTypeStatResp = 7,
        HeaderTypeStatReq = 8
    };

    //! Communication mode (TS 102 490 table 5.7)
    enum CommMode
    {
        CommModeVoice = 0,         //!< Voice, no user data in SLD field
        CommModeVoiceSLD = 1,
        CommModeData1 = 2,
        CommModeData2 = 3,
        CommModeData3 = 4,
        CommModeVoiceData = 5
    };

    //! Communication format (TS 102 490 table 5.8, TS 102 658 table 5.50 for Mode 1)
    enum CommFormat
    {
        CommFormatAllCall = 0,     //!< Call ALL (broadcast)
        CommFormatP2P = 1          //!< Peer to peer
    };

    static constexpr int m_frameBits = 384;          //!< Payload or header frame (80 ms)
    static constexpr int m_frameSymbols = 192;
    static constexpr int m_preambleBits = 72;        //!< Minimum preamble in the header frame
    static constexpr int m_fs1Bits = 48;
    static constexpr int m_fs2Bits = 24;
    static constexpr int m_fs3Bits = 24;
    static constexpr int m_ccBits = 24;              //!< Colour code field
    static constexpr int m_hiInfoBits = 72;          //!< Header information before CRC8
    static constexpr int m_hiCodedBits = 120;
    static constexpr int m_cchInfoBits = 41;         //!< CCH information before CRC7
    static constexpr int m_cchCodedBits = 72;
    static constexpr int m_endInfoBits = 17;         //!< END information before CRC7
    static constexpr int m_endCodedBits = 72;
    static constexpr int m_tchBits = 72;             //!< One AMBE+2 vocoder frame
    static constexpr int m_tchPerFrame = 4;
    static constexpr int m_framesPerSuperframe = 4;
    static constexpr int m_endFrameBits = 96;        //!< FS3 + END
    static constexpr int m_tailBits = 72;            //!< Preamble pattern sent after the end frame
    static constexpr int m_preambleFrames = 1;       //!< Extra 80 ms frames of preamble before the header frame
    static constexpr uint32_t m_allCallId = 0xFFFFFF;
    static constexpr uint8_t m_preambleByte = 0x5F;

    DSEModDPMR();
    virtual ~DSEModDPMR();

    virtual void start(const DSEModSettings& settings);
    virtual void stop();
    virtual void abort();
    virtual bool isActive() const { return m_state != StateIdle; }
    virtual bool nextFrame(std::vector<int8_t>& levels);

    virtual DSEModVocoder::Rate getVocoderRate() const { return DSEModVocoder::Rate3600x2450; }
    virtual Shaping getShaping() const { return ShapingRRC4FSK; }
    virtual DSEModSettings::DSEModMode getMode() const { return DSEModSettings::DSEModModeDPMR; }
    virtual const char *getName() const { return "dPMR"; }
    virtual uint32_t getVoiceFrameCount() const { return m_voiceFrameCount; }
    virtual int getFramesPerBlock() const { return m_tchPerFrame; }
    //! The preamble and header frames (160 ms) take no voice (getPrimeCredit)
    virtual int getPrimeCredit() const { return 7; }
    //! A text message in the slow data of the voice frames (TS 102 490 5.9.1: communication mode
    //! 001, two bytes a frame, each with a flag marking the message's last byte), while the call runs
    virtual bool queueMessage(const std::string& text);

    static constexpr int m_maxSlowDataBytes = 256;

    // ---- Coding primitives (TS 102 490 clause 7, see DVDPMR) ---------------------

    static void crc7Append(uint8_t *bits, int nbBits);
    static void crc8Append(uint8_t *bits, int nbBits);
    //! Shortened Hamming(12,8) (table 7.1): 8 data bits followed by 4 parity bits
    static void hamming128Encode(const uint8_t *data8, uint8_t *code12);
    //! 12 x nbBlocks interleave (tables 7.2 and 7.3): out[i] = in[12 * (i % nbBlocks) + i / nbBlocks]
    static void interleave(const uint8_t *in, int nbBlocks, uint8_t *out);
    //! Scrambling sequence: X^9 + X^5 + 1, initial state all ones (clause 7.4)
    static void scramblerSequence(uint8_t *seq, int nbBits);

    // ---- Field builders ---------------------------------------------------------

    //! Write value as nbBits bits, MSB first
    static void valueToBits(uint32_t value, int nbBits, uint8_t *bits);
    //! 72 bit header information: HT(4) called ID(24) own ID(24) M(3) V(2) F(2) RES(2) CI(11)
    static void buildHeaderInfo(int headerType, uint32_t calledId, uint32_t ownId, int mode, int format, uint8_t *hi72);
    //! CRC8 + Hamming + 12 x 10 interleave + scrambling: 72 -> 120 bits (clause 7.7)
    static void encodeHeaderInfo(const uint8_t *hi72, uint8_t *coded120);
    //! 41 bit CCH: FN(2) ID(12) M(3) V(2) F(2) RES(2) SLD(18). The ID is the upper or lower 12 bits
    //! of the called ID (FN 0, 1) or own ID (FN 2, 3) (clause 9.1). sld18 may be null for all zeros
    static void buildCCH(int frameNumber, uint32_t calledId, uint32_t ownId, int mode, int format, const uint8_t *sld18, uint8_t *cch41);
    //! CRC7 + Hamming + 12 x 6 interleave + scrambling: 41 -> 72 bits (clause 7.6)
    static void encodeCCH(const uint8_t *cch41, uint8_t *coded72);
    //! 17 bit END information: ET(2) ARQ(2) WAIT(4) STAT(5) RES(4)
    static void buildEndInfo(int endType, int ackRequest, int txWait, int status, uint8_t *end17);
    //! CRC7 + Hamming + scrambling of 36 bits then repeated: 17 -> 72 bits (clause 7.8)
    static void encodeEnd(const uint8_t *end17, uint8_t *coded72);
    //! 24 bit di-bit encoded colour code field of colour code 0..63 (table 6.1)
    static uint32_t colourCodeSequence(int colourCode);
    static void colourCodeBits(int colourCode, uint8_t *bits24);
    //! 12 bit colour code value as dsdcc reports it (one bit per symbol: negative symbol = 1)
    static int colourCodeValue(int colourCode);
    //! Vocoder frame (DVSI order = DMR on-air order) to 72 bits, MSB of byte 0 first
    static void vocoderFrameToBits(const uint8_t *frame, uint8_t *bits);

    // ---- Frame builders ---------------------------------------------------------

    //! Header frame: preamble(72) FS1(48) HI0(120) CC(24) HI1(120) = 384 bits
    static void buildHeaderFrame(const uint8_t *hiCoded120, int colourCode, uint8_t *bits384);
    //! Payload frame: FS2 (even frame number) or CC (odd) (24) + CCH (72) + 4 x TCH (72) = 384 bits
    static void buildPayloadFrame(int frameNumber, int colourCode, const uint8_t *cchCoded72, const uint8_t *tch4x72, uint8_t *bits384);
    //! End frame: FS3(24) END(72) = 96 bits
    static void buildEndFrame(const uint8_t *endCoded72, uint8_t *bits96);

    static const uint8_t m_fs1[m_fs1Bits];       //!< 57 FF 5F 75 D5 77
    static const uint8_t m_fs2[m_fs2Bits];       //!< 5F F7 7D
    static const uint8_t m_fs3[m_fs3Bits];       //!< 7D DF F5

private:
    enum State
    {
        StateIdle,
        StatePreamble,
        StateHeader,
        StateVoice,
        StateEnd
    };

    State m_state;
    bool m_stopRequested;
    int m_colourCode;
    uint32_t m_calledId;
    uint32_t m_ownId;
    int m_format;
    uint8_t m_hiCoded[m_hiCodedBits];
    uint8_t m_cchCoded[m_framesPerSuperframe][m_cchCodedBits];
    uint8_t m_endCoded[m_endCodedBits];
    int m_stateFrameCount;
    int m_frameNumber;            //!< 0..3 in the superframe
    uint32_t m_voiceFrameCount;
    std::deque<std::pair<uint8_t, bool>> m_slowData;   //!< Bytes of the queued messages, with their last byte flags
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODDPMR_H_
