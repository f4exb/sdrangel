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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODP25P2_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODP25P2_H_

#include <vector>

#include "dsemodprotocol.h"

/**
 * APCO P25 Phase 2 (TIA-102.BBAC two slot TDMA) outbound frame builder: 4FSK at
 * 6000 symbols/s standing in for H-CPM (+/- 2250 Hz and +/- 750 Hz, the symbol
 * average frequency of the +/- 3pi/4 and +/- pi/4 phase steps), AMBE+2 3600x2450,
 * a clear group voice call on channel 0 (slot 1) with channel 1 idle.
 *
 * Each nextFrame() call produces one 30 ms burst of 180 dibits: the ISCH (the
 * sync word in timeslots 2, 3, 6, 7, 10, 11, the timeslot number otherwise) then
 * 160 payload dibits with the DUID at dibits 20, 57, 142, 179. Channel 0 sends a
 * FACCH carrying MAC_PTT, then repeats four 4V bursts (four voice codewords, a
 * quarter of the encryption sync ESS-B), a 2V burst (two codewords, ESS-A) and a
 * SACCH carrying MAC_ACTIVE; on stop it sends a FACCH with MAC_END_PTT and two
 * SACCH with MAC_IDLE. Channel 1 sends unscrambled SACCH bursts with MAC_IDLE.
 * The channel 0 payload is scrambled with the sequence derived from the WACN,
 * system ID and NAC.
 */
class DSEModP25P2 : public DSEModProtocol
{
public:
    static const int m_burstDibits = 180;

    DSEModP25P2();
    virtual ~DSEModP25P2();

    virtual void start(const DSEModSettings& settings);
    virtual void stop();
    virtual void abort();
    virtual bool isActive() const { return m_state != StateIdle; }
    virtual bool nextFrame(std::vector<int8_t>& levels);

    virtual DSEModVocoder::Rate getVocoderRate() const { return DSEModVocoder::Rate3600x2450; }
    virtual Shaping getShaping() const { return ShapingRRC4FSK; }
    virtual DSEModSettings::DSEModMode getMode() const { return DSEModSettings::DSEModModeP25P2; }
    virtual const char *getName() const { return "P25P2"; }
    virtual uint32_t getVoiceFrameCount() const { return m_voiceFrameCount; }
    virtual int getFramesPerBlock() const { return 4; }

    // ---- Static builders (exposed for the unit tests) ----------------------------------

    //! MAC_PTT PDU (18 bytes, FACCH): MI, ALGID, KID, source, group
    static void buildMacPTT(const DSEModSettings& settings, const uint8_t *mi9, uint8_t *pdu18);
    //! MAC_END_PTT PDU (18 bytes, FACCH)
    static void buildMacEndPTT(const DSEModSettings& settings, uint8_t *pdu18);
    //! MAC_ACTIVE (21 bytes, SACCH), null information
    static void buildMacActive(uint8_t *pdu21);
    //! MAC_IDLE (21 bytes, SACCH)
    static void buildMacIdle(uint8_t *pdu21);
    //! MAC_IDLE carrying a Network Status Broadcast (abbreviated, MCO 0x7B: WACN, system ID and
    //! colour code), as a voice channel's clear SACCH may: what a receiver needs to descramble
    static void buildMacIdleNetworkStatus(uint32_t wacn, uint16_t sysid, uint16_t nac, uint8_t *pdu21);
    //! Payload dibits of a SACCH or FACCH burst (dibits 20-179 of the burst, DUID included)
    static void buildACCHBurst(bool fast, bool scrambled, const uint8_t *pdu, uint8_t *dibits160);
    //! Payload dibits of a 4V (four 9 byte codewords, ESS-B fragment index 0-3) or 2V (two
    //! codewords, ESS-A) burst. frames holds the codewords one after the other, 9 bytes each.
    static void buildVoiceBurst(bool twoV, const uint8_t *frames, const uint8_t *essB16, const uint8_t *essA28, int essBIndex, uint8_t *dibits160);
    //! ISCH of a timeslot (0-11): 20 dibits
    static void buildISCH(int timeslot, int superframe, uint8_t *dibits20);

private:
    enum State
    {
        StateIdle,
        StatePTT,       //!< Next channel 0 burst is the FACCH MAC_PTT
        StateVoice,
        StateEndPTT,    //!< Next channel 0 burst is the FACCH MAC_END_PTT
        StateIdleBursts //!< Trailing SACCH MAC_IDLE bursts
    };

    State m_state;
    bool m_stopRequested;
    int m_timeslot;             //!< Timeslot of the next burst
    int m_superframe;
    int m_voiceBurst;           //!< Voice burst within the 4V 4V 4V 4V 2V SACCH cycle (0-5)
    int m_idleBursts;
    uint32_t m_wacn;
    uint16_t m_sysid;
    uint16_t m_nac;
    DSEModSettings m_settings;
    uint8_t m_mi[9];
    uint8_t m_essB[16];
    uint8_t m_essA[28];
    std::vector<uint8_t> m_mask;    //!< Scrambling sequence (4320 bits)
    uint32_t m_voiceFrameCount;

    void scramble(int timeslot, uint8_t *dibits160) const;
    void appendBurst(std::vector<int8_t>& levels, int timeslot, const uint8_t *dibits160) const;
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODP25P2_H_
