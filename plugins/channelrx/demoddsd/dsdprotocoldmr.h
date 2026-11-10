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

#ifndef INCLUDE_DSDPROTOCOLDMR_H
#define INCLUDE_DSDPROTOCOLDMR_H

#include <cstdint>
#include <string>

#include "dsdprotocol.h"

class DSDTestUnits;

/**
 * DMR (ETSI TS 102 361-1) burst decoder.
 *
 * A burst is 144 dibits (30 ms): CACH 0..11 (base station only), payload
 * 12..65, sync or EMB with embedded signalling 66..89, payload 90..143. Base
 * stations send both slots so a burst centre comes every 144 dibits; mobile
 * stations send one slot so it comes every 288. Voice bursts B to F of a
 * superframe carry an EMB instead of a sync and are confirmed by decoding it.
 *
 * Outputs: three AMBE+2 voice frames per voice burst with their LLRs, the
 * link control (source, destination, group) from the voice LC header, the
 * terminator or the embedded signalling, colour code, slot and burst types.
 */
class DSDProtocolDMR : public DSDProtocol
{
public:
    enum StationType { StationUnknown, StationBase, StationMobile, StationDirect };

    struct LinkControl
    {
        bool m_valid = false;
        bool m_group = false;
        bool m_emergency = false;
        bool m_privacy = false;
        bool m_broadcast = false;
        bool m_openVoiceCallMode = false;
        int m_priority = 0;
        int m_featureSetId = 0;
        uint32_t m_source = 0;
        uint32_t m_destination = 0;
    };

    struct TalkerAlias
    {
        bool m_valid = false;
        int m_format = -1;          //!< 0: 7-bit, 1: ISO-8859-1, 2: UTF-8, 3: UTF-16BE
        int m_length = 0;           //!< Character count signalled over the air
        std::string m_text;         //!< UTF-8
        uint64_t m_revision = 0;    //!< Changes only when a complete new alias is decoded
    };

    struct GPSInfo
    {
        bool m_valid = false;
        int m_positionError = 7;    //!< ETSI position-error code, 0 (best) to 7 (unknown)
        double m_latitude = 0.0;
        double m_longitude = 0.0;
        uint64_t m_revision = 0;
    };

    struct TextMessage
    {
        bool m_valid = false;
        bool m_group = false;
        int m_format = -1;          //!< ETSI UDT format, 16 for UTF-16BE over IP/UDP (port 5016), 17 for Motorola TMS (port 4007)
        uint32_t m_source = 0;
        uint32_t m_destination = 0;
        std::string m_text;         //!< UTF-8
        uint64_t m_revision = 0;
    };

    static const int m_burstDibits = 144;
    static const int m_syncEndOffset = 89;   //!< Dibit index of the last sync symbol in a burst

    DSDProtocolDMR(const DSDSymbolHistory& history);

    virtual const char *getName() const { return "DMR"; }
    virtual int getSymbolRate() const { return 4800; }
    virtual bool isFourLevel() const { return true; }
    virtual std::vector<DVSync::Pattern> getSearchPatterns() const;
    virtual float getNominalOuterLevel() const { return 1944.0f; }

    virtual void start(DVSync::Pattern pattern, int64_t syncIndex);
    virtual bool pushSymbol(int64_t index, const DSDSoftSymbol& symbol, int64_t sampleIndex);
    virtual void getBurstSpan(int64_t& first, int64_t& last) const;
    virtual void processBurst();
    virtual bool getNextSync(int64_t& syncIndex, std::vector<DVSync::Pattern>& patterns) const;
    virtual void resync(DVSync::Pattern pattern, int64_t syncIndex);
    virtual void getConfirmSpan(int64_t syncIndex, int64_t& first, int64_t& last) const;
    virtual bool confirmWithoutSync(int64_t syncIndex, bool present);
    virtual bool isProven() const { return m_proven; }
    virtual void end();
    virtual std::string getStatusText() const;

    StationType getStationType() const { return m_station; }
    int getColorCode() const { return m_colorCode; }
    //! Last validated group or unit-to-unit voice link control for a slot
    LinkControl getLinkControl(int slot) const;
    TalkerAlias getTalkerAlias(int slot) const;
    GPSInfo getGPSInfo(int slot) const;
    TextMessage getTextMessage(int slot) const;
    const char *getSlotTypeText(int slot) const;

private:
    friend class DSDTestUnits;

    enum BurstKind { BurstNone, BurstVoiceSync, BurstVoiceEmb, BurstData };

    struct SlotState
    {
        struct TalkerAliasAssembly
        {
            bool m_active = false;
            int m_format = -1;
            int m_length = 0;
            uint8_t m_bits[217] = {};
            unsigned m_receivedMask = 0;
        };

        struct UDTAssembly
        {
            bool m_active = false;
            bool m_group = false;
            int m_format = -1;
            int m_padNibbles = 0;
            int m_blocksExpected = 0;
            int m_blocksReceived = 0;
            uint32_t m_source = 0;
            uint32_t m_destination = 0;
            uint8_t m_data[48] = {};   // up to four appended blocks
        };

        struct PacketAssembly
        {
            bool m_active = false;
            bool m_confirmed = false;
            bool m_group = false;
            int m_sap = -1;
            int m_padOctets = 0;
            int m_blocksExpected = 0;
            int m_blocksReceived = 0;
            int m_dataBytes = 0;
            int m_nextSerial = -1;
            uint32_t m_source = 0;
            uint32_t m_destination = 0;
            uint8_t m_data[1536] = {};
        };

        LinkControl m_linkControl;
        TalkerAlias m_talkerAlias;
        TalkerAliasAssembly m_talkerAliasAssembly;
        GPSInfo m_gpsInfo;
        TextMessage m_textMessage;
        UDTAssembly m_udtAssembly;
        PacketAssembly m_packetAssembly;
        int m_dataType = -1;
        bool m_voice = false;
        int m_voiceSequence = 0;
        uint8_t m_embedded[128];
        int m_embeddedFragments = 0;
        std::string m_typeText = "---";
    };

    StationType m_station;
    int m_period;               //!< Dibits between burst centres
    int64_t m_centre;              //!< Index of the last sync symbol of the burst being received
    int64_t m_nextCentre;
    int64_t m_pendingCentre;       //!< Burst awaiting processBurst()
    BurstKind m_kind;           //!< Kind of the burst being received
    bool m_active;
    int m_colorCode;
    bool m_proven;              //!< A slot type or two EMBs have decoded since the lock: the checks may be lenient
    int m_strictEmbs;           //!< Consecutive near exact EMBs with one colour code while unproven
    int m_currentSlot;          //!< From the CACH (base station) or 0
    bool m_slotKnown;           //!< A base station burst's slot has been read: the others follow by alternation
    int64_t m_lastBurstCentre;     //!< Centre and slot of the last base station burst processed
    int m_lastBurstSlot;
    int m_slotDisagreements;    //!< Consecutive CACHs contradicting the alternation
    SlotState m_slots[2];
    int m_misses;
    uint64_t m_metadataRevision;

    void classify(DVSync::Pattern pattern);
    void decodeBurst(int64_t centre);
    void processData(int64_t start, int slot);
    void processVoice(int64_t start, int slot);
    void emitProtocolBurst(int64_t start, int slot, const std::string& type, int sequence);
    void emitVoiceFrame(int64_t firstDibit, int64_t secondDibit, int nbFirst, int slot, int64_t sample);
    bool checkEmb(int64_t centre, int& colorCode, int& lcss, bool& pi) const;
    void parseLC(const uint8_t *lc, SlotState& slot);
    void parseTalkerAlias(const uint8_t *lc, int flco, SlotState& slot);
    void publishTalkerAlias(SlotState& slot);
    void parseDataHeader(const uint8_t *header, SlotState& slot);
    void parseDataBlock(const uint8_t *block, SlotState& slot);
    void parsePacketBlock(const uint8_t *block, SlotState& slot);
    void publishPacketText(SlotState& slot);
};

#endif // INCLUDE_DSDPROTOCOLDMR_H
