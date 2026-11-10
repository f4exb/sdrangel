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

#ifndef INCLUDE_DSDPROTOCOLP25P2_H
#define INCLUDE_DSDPROTOCOLP25P2_H

#include <cstdint>
#include <vector>

#include "dsp/dv/dvp25p2.h"

#include "dsdprotocol.h"
#include "dsdp25trunking.h"

/**
 * APCO P25 Phase 2 (two slot TDMA, H-DQPSK or H-CPM at 6000 symbols/s,
 * received as four levels through the discriminator) decoder.
 *
 * Every 180 dibit burst starts with a 20 dibit ISCH: the sync word in
 * timeslots 2, 3, 6, 7, 10, 11 of the 12 burst superframe, the timeslot
 * number in the others, so the burst after a sync is confirmed by decoding
 * its ISCH. The four DUID dibits say what the burst carries: 4V and 2V
 * voice bursts (four or two AMBE+2 codewords, in the DMR order, and the
 * encryption sync spread over the five bursts of a channel), a SACCH or a
 * FACCH with a MAC PDU (PTT, end of PTT, idle, active, hang time: the
 * talkgroup, the source and the encryption parameters), or the LCCH of a
 * TDMA control channel. Timeslots alternate between the two channels,
 * reported as slots 1 and 2.
 *
 * The payload is scrambled with a sequence derived from the WACN, the
 * system ID and the NAC of the system; the voice and the scrambled
 * signalling can only be decoded when those are set (setSystem()), the
 * unscrambled SACCH and FACCH always.
 */
class DSDProtocolP25P2 : public DSDProtocol
{
public:
    static const int m_burstDibits = DVP25P2::m_burstDibits;
    static const int m_ischDibits = DVP25P2::m_ischDibits;

    DSDProtocolP25P2(const DSDSymbolHistory& history);

    virtual const char *getName() const { return "P25P2"; }
    virtual int getSymbolRate() const { return 6000; }
    virtual bool isFourLevel() const { return true; }
    virtual std::vector<DVSync::Pattern> getSearchPatterns() const;
    virtual float getNominalOuterLevel() const { return 2250.0f; }

    virtual void start(DVSync::Pattern pattern, int64_t syncIndex);
    virtual bool pushSymbol(int64_t index, const DSDSoftSymbol& symbol, int64_t sampleIndex);
    virtual void getBurstSpan(int64_t& first, int64_t& last) const;
    virtual void processBurst();
    virtual bool wasBurstValid() const { return m_burstValid; }
    virtual bool getNextSync(int64_t& syncIndex, std::vector<DVSync::Pattern>& patterns) const;
    virtual void resync(DVSync::Pattern pattern, int64_t syncIndex);
    virtual void getConfirmSpan(int64_t syncIndex, int64_t& first, int64_t& last) const;
    virtual bool confirmWithoutSync(int64_t syncIndex, bool present);
    virtual void end();
    virtual std::string getStatusText() const;

    //! The system the scrambling sequence derives from
    void setSystem(uint32_t wacn, uint16_t sysid, uint16_t nac);
    bool hasSystem() const { return m_hasSystem; }
    int getTimeslot() const { return m_timeslot; }
    //! Call parameters of a channel (0 or 1) from the last PTT: talkgroup and source
    bool getCall(int channel, uint32_t& source, uint16_t& group) const;
    int getAlgorithm(int channel) const { return m_channels[channel & 1].m_algid; }
    bool isEncrypted(int channel) const { return m_channels[channel & 1].m_algid != 0x80; }
    const char *getBurstTypeText(int channel) const { return m_channels[channel & 1].m_typeText; }
    const char *getMacText(int channel) const { return m_channels[channel & 1].m_macText; }
    //! The last MAC message (MCO) decoded from a SACCH, FACCH or LCCH, and the last channel grant
    const std::string& getSignallingText() const { return m_signallingText; }
    const std::string& getGrantText() const { return m_grantText; }
    const DSDP25Trunking& getTrunking() const { return m_trunking; }
    //! The system (WACN, system ID, NAC) was learned from a Network Status Broadcast heard in the
    //! clear, none having been set
    bool isSystemLearned() const { return m_systemLearned; }

private:
    struct Channel
    {
        int m_voiceBurst = 0;       //!< Voice bursts since the last 2V burst (0-4)
        uint8_t m_essB[16] = { 0 };
        int m_essBursts = 0;        //!< 4V bursts whose ESS-B fragment is in m_essB (bit mask)
        int m_algid = 0x80;
        uint16_t m_keyid = 0;
        uint8_t m_mi[9] = { 0 };
        bool m_callValid = false;
        uint32_t m_source = 0;
        uint16_t m_group = 0;
        const char *m_typeText = "---";
        const char *m_macText = "";
    };

    bool m_active;
    int64_t m_burstStart;          //!< Symbol index of the first ISCH dibit of the burst being received
    bool m_ischPending;         //!< The ISCH of that burst is still to come (the previous burst just completed)
    int64_t m_pendingStart;
    bool m_burstValid;
    int m_timeslot;             //!< Timeslot of the burst starting at m_timeslotStart, -1 when not known yet
    int64_t m_timeslotStart;
    int m_misses;
    int m_silent;
    int m_absent;
    bool m_hasSystem;
    uint32_t m_wacn;
    uint16_t m_sysid;
    uint16_t m_nac;
    std::vector<uint8_t> m_mask;    //!< Scrambling sequence of a superframe
    struct Deferred
    {
        int64_t m_burstStart;
        int m_duid;
    };

    Channel m_channels[2];
    DSDP25Trunking m_trunking;
    std::string m_signallingText;
    std::string m_grantText;
    bool m_systemLearned;
    std::vector<Deferred> m_deferred;   //!< Bursts received before the timeslot was known
    int m_lastDuid;

    //! Timeslot of the burst starting at burstStart, or -1
    int timeslotAt(int64_t burstStart) const;
    //! ISCH of the burst whose first dibit is at burstStart. Returns the type decoded within
    //! the distance limit, updating the timeslot tracking.
    DVP25P2::ISCHType readISCH(int64_t burstStart, int& distance);
    void gather(int64_t burstStart, int firstDibit, int nbDibits, float *llr, uint8_t *bits) const;
    void descramble(int timeslot, int firstDibit, int nbDibits, float *llr, uint8_t *bits) const;
    void processPayload(int64_t burstStart, int duid, int channel);
    void processVoice(int64_t burstStart, int duid, int channel);
    void processACCH(int64_t burstStart, int duid, int channel);
    void parseMAC(const uint8_t *pdu, int nbBytes, Channel& channel);
    void processLCCH(int64_t burstStart, int channel);
    //! The MAC messages of an MCO area (SACCH octets 1-20, FACCH 1-17, LCCH 1-18)
    void parseMCOs(const uint8_t *area, int nbBytes);
    //! Take the system from the network status broadcasts when none was set
    void learnSystem(int colourCode);
};

#endif // INCLUDE_DSDPROTOCOLP25P2_H
