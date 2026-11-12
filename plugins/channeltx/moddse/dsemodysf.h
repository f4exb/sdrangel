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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODYSF_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODYSF_H_

#include <string>
#include <vector>

#include <QString>

#include "dsp/dv/dvysf.h"

#include "dsemodprotocol.h"

/**
 * Yaesu System Fusion (YSF) C4FM frame builder, V/D mode type 2 (DN mode).
 *
 * Each frame is 100 ms / 480 dibits at 4800 symbols/s: 20 dibit sync (D471C9634D),
 * 100 dibit FICH and 360 dibit payload.
 *  - Header frame (FI = 0): payload is CSD1 (destination + source callsigns) and
 *    CSD2 (downlink + uplink callsigns), 180 dibits each, interleaved in 36 dibit chunks.
 *  - Communication frames (FI = 1, DT = 2): five 72 dibit blocks each with a 20 dibit
 *    DCH slice and 52 dibits of voice (49 AMBE+2 2450 bps parameter bits, the 27 most
 *    important sent 3 times). The DCH of frame FN carries: 0 destination, 1 source,
 *    2 downlink, 3 uplink, 4 rem1 + rem2, 5 rem3 + rem4, 6 DT1, 7 DT2 (10 bytes each).
 *  - Terminator frame (FI = 2): same payload as the header.
 *
 * Transmission: short preamble, header, communication frames FN 0..7 repeating,
 * terminator when stopped. Each nextFrame() call produces one frame (five 20 ms
 * vocoder frames are consumed per communication frame).
 */
class DSEModYSF : public DSEModProtocol
{
public:
    enum FrameInformation
    {
        FIHeader = 0,
        FICommunication = 1,
        FITerminator = 2,
        FITest = 3
    };

    enum DataType
    {
        DTVoiceData1 = 0,
        DTDataFullRate = 1,
        DTVoiceData2 = 2,
        DTVoiceFullRate = 3
    };

    static constexpr int m_frameDibits = 480;
    static constexpr int m_syncDibits = 20;
    static constexpr int m_fichDibits = 100;
    static constexpr int m_payloadDibits = 360;
    static constexpr int m_dchChunkDibits = 36;      //!< Header/terminator DCH1/DCH2 chunk
    static constexpr int m_vd2BlockDibits = 72;      //!< V/D type 2 block: 20 DCH + 52 voice
    static constexpr int m_vd2DCHSliceDibits = 20;
    static constexpr int m_voiceFramesPerFrame = 5;
    static constexpr int m_framesPerSuperframe = 8;  //!< FN 0..7 (FT = 7)
    static constexpr int m_callsignLength = 10;
    static constexpr int m_preambleDibits = 96;      //!< 20 ms of alternating +3/-3 before the header

    //! dataType: the voice mode, V/D type 2 (DN, the default), V/D type 1 or voice full rate (VW)
    explicit DSEModYSF(DataType dataType = DTVoiceData2);
    virtual ~DSEModYSF();

    virtual void start(const DSEModSettings& settings);
    virtual void stop();
    virtual void abort();
    virtual bool isActive() const { return m_state != StateIdle; }
    //! A text message sent after the voice as data full rate frames (DT 1), in the layout of a
    //! Wires-X text message upload ("Ge_": 45 bytes of radio serial, times and room, 80 characters
    //! of text, the 0x03 end marker and the 8 bit sum). That layout is EA7EE's reverse engineering,
    //! not Yaesu's: a receiver test, not a feature radios are known to take
    virtual bool queueMessage(const std::string& text);
    //! Data full rate: the message bytes of block 0 as frames FN 0 (CSD1, CSD2), FN 1 (CSD3, bytes
    //! 0..19) and FN n (bytes 20 + 40 (n - 2) and 40 + 40 (n - 2)); the frame total they need
    static int dataFRFrames(int nbBytes) { return nbBytes <= 20 ? 1 : 2 + (nbBytes - 21) / 40; }
    virtual bool nextFrame(std::vector<int8_t>& levels);

    virtual DSEModVocoder::Rate getVocoderRate() const { return m_dataType == DTVoiceFullRate ? DSEModVocoder::RateIMBE7200x4400 : DSEModVocoder::Rate3600x2450; }
    virtual Shaping getShaping() const { return ShapingRRC4FSK; }
    virtual DSEModSettings::DSEModMode getMode() const { return DSEModSettings::DSEModModeYSF; }
    virtual const char *getName() const { return "YSF"; }
    virtual uint32_t getVoiceFrameCount() const { return m_voiceFrameCount; }
    virtual int getFramesPerBlock() const { return m_voiceFramesPerFrame; }
    //! The preamble and header (120 ms) take no voice, and the first frame after them only two (getPrimeCredit)
    virtual int getPrimeCredit() const { return 4; }

    // ---- Static helpers (exposed for the tests) ----------------------------------

    //! FICH for a frame of this transmission (CS 2, CM 0 group CQ, BN/BT 0, FT 7, Dev 0, MR 0, VoIP 0, DT 2, SQL 0, SQ 0)
    static DVYSF::FICH makeFICH(FrameInformation fi, int fn);
    //! Pad or truncate a callsign to 10 characters (upper case, space padded)
    static void formatCallsign(const QString& callsign, uint8_t *out);
    //! Build CSD1 (dest + src) and CSD2 (downlink + uplink), 20 bytes each, from the settings
    static void buildCSD(const DSEModSettings& settings, uint8_t *csd1, uint8_t *csd2);
    //! Header / terminator payload: CSD1 and CSD2 coded and interleaved in 36 dibit chunks (360 dibits)
    static void buildHeaderPayload(const uint8_t *csd1, const uint8_t *csd2, uint8_t *dibits);
    //! V/D type 2 payload: 10 byte DCH data and five 49 bit AMBE parameter frames (360 dibits)
    static void buildVD2Payload(const uint8_t *dch, const uint8_t ambe49[5][DVYSF::m_ambeBits], uint8_t *dibits);
    //! Complete frame: sync + FICH + payload (480 dibits)
    static void buildFrame(const DVYSF::FICH& fich, const uint8_t *payload, uint8_t *dibits);
    //! Append dibits as 4FSK levels
    static void appendDibitLevels(std::vector<int8_t>& levels, const uint8_t *dibits, int nbDibits);

    static const uint8_t m_syncBits[2 * m_syncDibits]; //!< D4 71 C9 63 4D MSB first

private:
    enum State
    {
        StateIdle,
        StatePreamble,
        StateHeader,
        StateVoice,
        StateData,
        StateTerminator
    };

    State m_state;
    bool m_stopRequested;
    uint8_t m_csd1[20];
    uint8_t m_csd2[20];
    uint8_t m_csd3[20];       //!< rem1..rem4
    uint8_t m_dt1[10];
    uint8_t m_dt2[10];
    uint8_t m_headerPayload[m_payloadDibits];
    int m_frameNumber;        //!< FN 0..7
    DataType m_dataType;
    bool m_subHeaderSent;     //!< Voice full rate: the first frame after the header (CSD3 and two voice frames) sent
    std::vector<uint8_t> m_message;   //!< The data full rate message, padded to its frames
    uint32_t m_voiceFrameCount;

    const uint8_t *dchDataForFrame(int fn) const;
    void nextVoiceFrame(std::vector<int8_t>& levels);
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODYSF_H_
