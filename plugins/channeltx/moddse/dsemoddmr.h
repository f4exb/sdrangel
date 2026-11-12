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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODDMR_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODDMR_H_

#include "dsemodprotocol.h"

/**
 * DMR (ETSI TS 102 361-1) base station (BS) frame builder.
 *
 * Transmits continuously as a BS: both time slots with CACH, the selected slot
 * carrying the voice call (voice LC header, voice superframes with embedded LC,
 * terminator with LC) and the other slot carrying idle bursts.
 *
 * Each nextFrame() call produces one 60 ms TDMA frame (slot 1 then slot 2),
 * i.e. 288 dibits, as 4FSK levels.
 */
class DSEModDMR : public DSEModProtocol
{
public:
    enum DataType
    {
        DataTypePIHeader = 0,
        DataTypeVoiceLCHeader = 1,
        DataTypeTerminatorWithLC = 2,
        DataTypeCSBK = 3,
        DataTypeMBCHeader = 4,
        DataTypeMBCContinuation = 5,
        DataTypeDataHeader = 6,
        DataTypeRate12Data = 7,
        DataTypeRate34Data = 8,
        DataTypeIdle = 9,
        DataTypeRate1Data = 10,
        DataTypeUnifiedSingleBlock = 11
    };

    enum LCSS
    {
        LCSSSingle = 0,
        LCSSFirst = 1,
        LCSSLast = 2,
        LCSSContinuation = 3
    };

    static constexpr int m_slotBits = 288;      //!< CACH + burst
    static constexpr int m_cachBits = 24;
    static constexpr int m_burstBits = 264;
    static constexpr int m_syncBits = 48;
    static constexpr int m_infoBits = 196;
    static constexpr int m_voiceFrameBits = 72;
    static constexpr int m_burstsPerSuperframe = 6;
    static constexpr int m_headerBursts = 2;
    static constexpr int m_terminatorBursts = 2;
    static constexpr int m_startFrames = 2;
    static constexpr int m_hangFrames = 16;
    //! Superframes between GPS Info embedded LCs: about 1.8 s, the rest carrying the voice LC
    static constexpr int m_positionIntervalSuperframes = 5;

    DSEModDMR();
    virtual ~DSEModDMR();

    virtual void start(const DSEModSettings& settings);
    virtual void stop();
    virtual void abort();
    virtual bool isActive() const { return m_state != StateIdle; }
    virtual bool nextFrame(std::vector<int8_t>& levels);

    virtual void setPosition(bool valid, double latitude, double longitude, float altitude,
        int speed = -1, int course = -1, int source = 1);

    virtual DSEModVocoder::Rate getVocoderRate() const { return DSEModVocoder::Rate3600x2450; }
    virtual Shaping getShaping() const { return ShapingRRC4FSK; }
    virtual DSEModSettings::DSEModMode getMode() const { return DSEModSettings::DSEModModeDMR; }
    virtual const char *getName() const { return "DMR"; }
    virtual uint32_t getVoiceFrameCount() const { return m_voiceFrameCount; }
    virtual int getFramesPerBlock() const { return 3; }
    //! The header bursts take no voice, and a BS's idle bursts before them (getPrimeCredit)
    virtual int getPrimeCredit() const { return m_direct ? 4 : 10; }

    //! Build the 9 byte full LC (group or unit to unit voice channel user) from settings
    static void buildLC(const DSEModSettings& settings, uint8_t *lc);
    /**
     * Build the 9 byte GPS Info full LC (ETSI TS 102 361-2 7.1.1.4) for a position.
     * positionError is the standard's 3 bit accuracy class, 0 for better than 2 m to 7 for
     * unknown. Longitude and latitude are clamped to what their fields hold.
     */
    static void buildGPSLC(double latitude, double longitude, int positionError, uint8_t *lc);
    //! Build a complete slot (CACH + general data burst) as 288 bits
    static void buildDataBurst(int colorCode, int dataType, const uint8_t *info196, int tc, int cachLcss, const uint8_t *cachPayload, uint8_t *bits);
    //! Build a complete slot (CACH + voice burst) as 288 bits. embedded is 32 bits or null for a sync burst
    static void buildVoiceBurst(int colorCode, const uint8_t *vf1, const uint8_t *vf2, const uint8_t *vf3,
        const uint8_t *embedded, int embLcss, int tc, int cachLcss, const uint8_t *cachPayload, uint8_t *bits);
    //! Vocoder frame (DVSI order) to 72 on-air bits (MSB of byte 0 first)
    static void vocoderFrameToBits(const uint8_t *frame, uint8_t *bits);
    //! Append 288 bits as 144 dibit levels
    static void appendBits(std::vector<int8_t>& levels, const uint8_t *bits, int nbBits);

    static const uint8_t m_syncVoiceBS[m_syncBits];
    static const uint8_t m_syncDataBS[m_syncBits];
    static const uint8_t m_syncVoiceMS[m_syncBits];
    static const uint8_t m_syncDataMS[m_syncBits];

private:
    enum State
    {
        StateIdle,
        StateStart,
        StateHeader,
        StateVoice,
        StateTerminator,
        StateHang
    };

    State m_state;
    bool m_stopRequested;
    bool m_direct;                //!< MS sourced single slot bursts (direct mode / simplex hotspot) instead of continuous BS
    int m_colorCode;
    int m_voiceSlot;              //!< 0 or 1
    uint8_t m_lc[9];
    uint8_t m_vlcInfo[m_infoBits];
    uint8_t m_tlcInfo[m_infoBits];
    uint8_t m_idleInfo[m_infoBits];
    uint8_t m_embeddedLC[128];
    uint8_t m_embeddedGPS[128];   //!< GPS Info, rebuilt whenever the position moves
    bool m_positionValid;
    bool m_sendPosition;          //!< The setting at key up: a transmission sends it or does not
    double m_latitude;
    double m_longitude;
    int m_positionError;          //!< ETSI accuracy class of the position source
    bool m_gpsPending;            //!< The position moved: rebuild before the next one goes out
    bool m_gpsSuperframe;         //!< This superframe's embedded LC is the position
    int m_superframeCount;        //!< Superframes since the start, for the position interval
    int m_stateFrameCount;
    int m_superframeIndex;        //!< 0..5 burst A..F
    int m_cachLcssIndex;          //!< 0..3 short LC fragment cycle
    uint32_t m_voiceFrameCount;

    void buildVoiceSlot(uint8_t *bits);
    void updateEmbeddedGPS();
    void buildIdleSlot(int tc, uint8_t *bits);
    void applySync(uint8_t *bits, bool voice); //!< Replace the BS sync of a slot by the MS one in direct mode
    int nextCachLcss();
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODDMR_H_
