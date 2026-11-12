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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODNXDN_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODNXDN_H_

#include "dsemodprotocol.h"

/**
 * NXDN (NXDN TS 1-A) 4800 bit/s conventional voice frame builder: RDCH voice
 * transmission of a mobile station (direct or through a repeater).
 *
 * Each nextFrame() call produces one frame of 192 symbols (384 bits): FSW
 * (20 bits) + LICH (16) + SACCH (60) + 4 x VCH (72 each). The layer 3 VCALL
 * message (72 bits) is split into 4 SACCH fragments over a 4 frame superframe.
 * The transmission is released with a superframe of frames whose voice slots
 * are stolen by 2 x FACCH1 carrying TX_REL. Everything after the FSW is
 * scrambled with the PN(9,5) sequence as dsdcc DSDNXDN::unscrambleDibit
 * expects.
 *
 * At 2400 symbols/s (NXDN48) a frame is 80 ms and every frame carries four
 * 20 ms EHR voice frames. At 4800 symbols/s (NXDN96) a frame is 40 ms and, as
 * the radios do (a 9600 bit/s EHR recording), a frame of four voice frames
 * alternates with a frame whose voice slots carry the VCALL message in two
 * FACCH1: four voice frames per 80 ms, real time.
 */
class DSEModNXDN : public DSEModProtocol
{
public:
    enum MessageType
    {
        MessageTypeVCALL = 0x01,
        MessageTypeTXREL = 0x08,
        MessageTypeSDCALLHeader = 0x38,   //!< SDCALL_REQ_HEADER
        MessageTypeSDCALLData = 0x39      //!< SDCALL_REQ_USERDATA
    };

    enum CallType
    {
        CallTypeBroadcast = 0,
        CallTypeGroup = 1,
        CallTypeIndividual = 4
    };

    enum RFChannelType
    {
        RFChannelRCCH = 0,
        RFChannelRTCH = 1,
        RFChannelRDCH = 2,
        RFChannelRTCHC = 3
    };

    enum FunctionalChannelType //!< for RTCH/RDCH
    {
        FunctionalSACCHSingle = 0,
        FunctionalUDCH = 1,
        FunctionalSACCHSuperframe = 2,
        FunctionalSACCHIdle = 3
    };

    enum Option //!< for SACCH frames: voice channel stealing
    {
        OptionStealBoth = 0, //!< 2 x FACCH1
        OptionSteal1 = 1,    //!< FACCH1 + 2 x VCH
        OptionSteal2 = 2,    //!< 2 x VCH + FACCH1
        OptionStealNone = 3  //!< 4 x VCH
    };

    static constexpr int m_frameSymbols = 192;
    static constexpr int m_fswBits = 20;
    static constexpr int m_lichBits = 16;
    static constexpr int m_sacchBits = 60;
    static constexpr int m_vchBits = 72;
    static constexpr int m_facch1Bits = 144;
    static constexpr int m_payloadBits = 288;      //!< 4 x VCH or 2 x FACCH1
    static constexpr int m_scrambledSymbols = 182; //!< LICH + SACCH + payload
    static constexpr int m_frameBits = 384;
    static constexpr int m_messageBits = 72;       //!< layer 3 message in a SACCH superframe
    static constexpr int m_sacchFragmentBits = 18;
    static constexpr int m_framesPerSuperframe = 4;
    static constexpr int m_preambleSymbols = 48;
    static constexpr int m_releaseFrames = 4;
    static constexpr int m_lichInfoBits = 8;
    static constexpr unsigned int m_pnSeed = 0xe4;  //!< dsdcc DSDNXDN: TS 1A v0103 section 4.6

    //! fast: 4800 symbols/s (NXDN96, 12.5 kHz) instead of 2400 (NXDN48, 6.25 kHz); voice frames alternate with FACCH1 frames
    DSEModNXDN(bool fast = false);
    virtual ~DSEModNXDN();

    virtual void start(const DSEModSettings& settings);
    virtual void stop();
    virtual void abort();
    virtual bool isActive() const { return m_state != StateIdle; }
    virtual bool nextFrame(std::vector<int8_t>& levels);

    virtual DSEModVocoder::Rate getVocoderRate() const { return DSEModVocoder::Rate3600x2450; }
    virtual Shaping getShaping() const { return ShapingRRC4FSK; }
    virtual DSEModSettings::DSEModMode getMode() const { return m_fast ? DSEModSettings::DSEModModeNXDN96 : DSEModSettings::DSEModModeNXDN; }
    virtual const char *getName() const { return "NXDN"; }
    virtual uint32_t getVoiceFrameCount() const { return m_voiceFrameCount; }
    //! A short data call (TS 1-A SDCALL) with the text as its user data, sent when the transmission ends
    virtual bool queueMessage(const std::string& text);
    virtual int getFramesPerBlock() const { return 4; }
    //! NXDN96's first frame is a VCALL (40 ms), which takes no voice (getPrimeCredit)
    virtual int getPrimeCredit() const { return m_fast ? 2 : 0; }

    //! Layer 3 VCALL / TX_REL message (9 bytes = 72 bits) from settings
    static void buildMessage(int messageType, const DSEModSettings& settings, uint8_t *message);
    //! Generic layer 3 call message: type (6 bits), call type (3 bits), 16 bit source and destination
    static void buildMessage(int messageType, int callType, int sourceId, int destinationId, uint8_t *message);
    //! LICH: RF channel type, functional channel type, option, direction -> 16 on-air bits (before scrambling)
    static void lichEncode(int rfChannelType, int functionalType, int option, int direction, uint8_t *bits);
    //! SACCH: structure (countdown 3..0), RAN and the 18 bit message fragment -> 60 on-air bits (before scrambling)
    static void sacchEncode(int structure, int ran, const uint8_t *fragment, uint8_t *bits);
    //! Fragment index (0..3) of a 72 bit message (9 bytes) -> 18 bits
    static void messageFragment(const uint8_t *message, int index, uint8_t *fragment);
    //! FACCH1: 10 byte layer 3 message -> 144 on-air bits (before scrambling)
    static void facch1Encode(const uint8_t *message, uint8_t *bits);
    //! Vocoder frame (DVSI order, DMR on-air order) to 72 on-air bits (MSB of byte 0 first)
    static void vocoderFrameToBits(const uint8_t *frame, uint8_t *bits);
    //! Scramble the 364 bits following the FSW in place: the MSB of symbol i is inverted when PN bit i is set
    static void scramble(uint8_t *bits);
    //! Assemble a complete frame: FSW + scrambled (LICH + SACCH + payload) as 384 bits
    static void buildFrame(const uint8_t *lich, const uint8_t *sacch, const uint8_t *payload, uint8_t *bits);

    static const uint8_t m_fsw[m_fswBits];               //!< Frame sync word bits
    static const int8_t m_preamble[m_preambleSymbols];   //!< Preamble symbol levels

private:
    enum State
    {
        StateIdle,
        StatePreamble,
        StateVoice,
        StateData,      //!< The short data call: a header frame then the user data frames
        StateRelease
    };

    bool m_fast;
    State m_state;
    bool m_stopRequested;
    int m_ran;
    uint8_t m_vcall[10];
    uint8_t m_txrel[10];
    uint8_t m_facch1TxRel[m_facch1Bits];
    uint8_t m_facch1Vcall[m_facch1Bits];
    int m_superframeIndex; //!< 0..3
    bool m_settingsGroup;
    int m_settingsSource;
    int m_settingsDestination;
    int m_dataPadding;              //!< Bytes of the last user data block that are padding
    int m_releaseCount;
    uint32_t m_voiceFrameCount;
    std::string m_message;          //!< Text queued for a short data call
    std::vector<uint8_t> m_userData; //!< The data call's user data with its CRC-32 and padding
    int m_dataBlocks;               //!< Blocks of 20 bytes it takes
    int m_dataFrame;                //!< Frames of the data call sent (0: the header)

    void buildVoiceFrame(uint8_t *bits);
    void buildDataHeaderFrame(uint8_t *bits);
    void buildDataFrame(int block, uint8_t *bits);
    void buildFrameUDCH(const uint8_t *lich, const uint8_t *udch, uint8_t *bits);
    void buildReleaseFrame(uint8_t *bits);
    void buildVcallFrame(uint8_t *bits);    //!< NXDN96: the frame between two voice frames, VCALL in two FACCH1
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODNXDN_H_
