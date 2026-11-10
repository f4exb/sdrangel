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

#ifndef INCLUDE_DSDPROTOCOLP25_H
#define INCLUDE_DSDPROTOCOLP25_H

#include <cstdint>
#include <string>
#include <vector>

#include "dsdprotocol.h"
#include "dsdp25trunking.h"

/**
 * APCO P25 Phase 1 (C4FM 4800 symbols/s) decoder.
 *
 * Every data unit starts with the 24 dibit frame sync and the 32 dibit
 * network identifier (NID: NAC and data unit ID). A status dibit follows
 * every 35 data dibits, counted from the start of the unit. The voice
 * units LDU1 and LDU2 (864 dibits on air) carry nine 144 bit IMBE frames
 * each, with the link control (LDU1) or the encryption sync (LDU2) and
 * the low speed data between them. The header (HDU) carries the
 * talkgroup and the encryption parameters; the terminators (TDU, TDULC)
 * end the call.
 *
 * Outputs: the IMBE frames with their LLRs, the NAC, the talkgroup and
 * source from the link control, the encryption algorithm.
 */
class DSDProtocolP25 : public DSDProtocol
{
public:
    enum DUID { DUIDHDU = 0x0, DUIDTDU = 0x3, DUIDLDU1 = 0x5, DUIDTSDU = 0x7, DUIDLDU2 = 0xA, DUIDPDU = 0xC, DUIDTDULC = 0xF };

    static const int m_syncDibits = 24;
    static const int m_nidDibits = 32;
    static const int m_hduDibits = 396;
    static const int m_lduDibits = 864;
    static const int m_tduDibits = 72;
    static const int m_tdulcDibits = 216;
    static const int m_tsduDataDibits = 56;   //!< Sync and NID; a TSDU or PDU carries 1 to 3 (TSDU) 98 dibit blocks after them
    static const int m_blockDibits = 98;

    DSDProtocolP25(const DSDSymbolHistory& history);

    virtual const char *getName() const { return "P25"; }
    virtual int getSymbolRate() const { return 4800; }
    virtual bool isFourLevel() const { return true; }
    virtual std::vector<DVSync::Pattern> getSearchPatterns() const;
    virtual float getNominalOuterLevel() const { return 1800.0f; }

    virtual void start(DVSync::Pattern pattern, int64_t syncIndex);
    virtual bool pushSymbol(int64_t index, const DSDSoftSymbol& symbol, int64_t sampleIndex);
    virtual void getBurstSpan(int64_t& first, int64_t& last) const;
    virtual void processBurst();
    virtual bool wasBurstValid() const { return m_burstValid; }
    virtual bool getNextSync(int64_t& syncIndex, std::vector<DVSync::Pattern>& patterns) const;
    virtual void resync(DVSync::Pattern pattern, int64_t syncIndex);
    /**
     * The 24 symbol sync is hard to match by chance, but not impossible, and nothing after it
     * refuses the match: when the NID cannot be read the decoder assumes the usual LDU1/LDU2
     * sequence and carries on, which is right for a real transmission fading and wrong for
     * noise, whose NID never reads. The lock is therefore only shown, and its audio only
     * played, once one NID has decoded - BCH(63,16), which about one random word in a thousand
     * passes, against the network identifier it then has to keep (below).
     */
    virtual bool isProven() const { return m_proven; }
    virtual bool acceptUnexpectedSync(DVSync::Pattern pattern, int64_t syncIndex) const;
    virtual void getConfirmSpan(int64_t syncIndex, int64_t& first, int64_t& last) const;
    virtual bool confirmWithoutSync(int64_t syncIndex, bool present);
    virtual void end();
    virtual std::string getStatusText() const;

    int getNAC() const { return m_nac; }
    int getDUID() const { return m_duid; }
    bool getLinkControl(bool& group, uint32_t& source, uint32_t& destination) const;
    //! Trunking: the network status broadcast gives the WACN and system ID (valid when true)
    bool getNetworkStatus(uint32_t& wacn, uint16_t& sysid) const;
    int getTsbkCount() const { return m_tsbkCount; }

    /** A text message received as packet data (TIA-102.BAAA packet data unit, IPv4/UDP inside) */
    struct TextMessage
    {
        bool m_valid = false;
        uint32_t m_source = 0;      //!< Logical link ID of the sender (the extended address, or the header's when outbound)
        uint32_t m_destination = 0; //!< Logical link ID of the destination (the header's when inbound)
        bool m_confirmed = false;   //!< Confirmed (rate 3/4 blocks) rather than unconfirmed delivery
        std::string m_kind;         //!< "TMS" or "UTF-16BE"
        std::string m_text;
        uint64_t m_revision = 0;
    };

    const TextMessage& getTextMessage() const { return m_textMessage; }
    int getPacketCount() const { return m_packetCount; }
    int getPacketFailed() const { return m_packetFailed; }
    bool isEncrypted() const { return m_algid != 0x80; }
    int getAlgorithm() const { return m_algid; }

private:
    bool m_active;
    int64_t m_unitStart;           //!< On air dibit index of the first sync dibit of the unit being received
    int m_unitLength;           //!< On air dibits, 0 until the NID is read
    int m_blocks;               //!< Blocks of a TSDU or PDU (1 to 3): read from the last block flag, or grown while no sync turns up
    bool m_lastBlock;           //!< The messages of the unit have ended (last block flag): no more blocks to parse
    int m_blocksDecoded;        //!< Blocks of the unit parsed so far

    /** The packet data unit being received: its header, then its data blocks assembled */
    struct Packet
    {
        bool m_headerValid = false;
        bool m_confirmed = false;   //!< Rate 3/4 blocks of 16 bytes rather than rate 1/2 of 12
        bool m_outbound = false;    //!< I/O bit: the address is the destination
        int m_format = 0;
        int m_sap = 0;
        int m_mfid = 0;
        uint32_t m_address = 0;     //!< Logical link ID of the header
        int m_blocks = 0;           //!< Data blocks announced by the header
        int m_padOctets = 0;
        int m_offset = 0;           //!< Data header offset (blocks of the secondary header)
        int m_blocksReceived = 0;
        bool m_failed = false;      //!< A data block did not decode: the packet is lost
        std::vector<uint8_t> m_data;
    };

    Packet m_packet;
    TextMessage m_textMessage;
    int m_packetCount;          //!< Packets whose CRC-32 checked
    int m_packetFailed;
    uint64_t m_metadataRevision;
    int m_tsbkCount;
    int m_tsbkFailed;
    std::string m_tsbkText;     //!< Last trunking block decoded
    std::string m_grantText;    //!< Last channel grant
    int m_grantAt;              //!< Block count when it was decoded

    DSDP25Trunking m_trunking;  //!< Site, network and channel plan from the trunking blocks
    int m_duid;
    int m_nac;
    bool m_nidValid;
    int64_t m_pendingStart;
    int m_pendingLength;
    int m_pendingDuid;
    bool m_pendingValid;        //!< NID of the pending unit decoded
    bool m_burstValid;
    int m_lastDuid;
    bool m_proven;                 //!< A NID has decoded: the lock is not a sync word found in noise
    int m_misses;
    int m_silent;
    int m_absent;
    bool m_lcValid;
    bool m_group;
    uint32_t m_source;
    uint32_t m_destination;
    uint8_t m_mfid;
    int m_algid;
    uint16_t m_kid;
    std::string m_typeText;

    //! On air dibit index of data dibit k of the unit starting at unitStart
    static int64_t onAir(int64_t unitStart, int k) { return unitStart + k + k / 35; }
    //! On air length of a unit of nbData data dibits (status symbols included)
    static int onAirLength(int nbData) { return nbData + nbData / 35; }
    //! A trunking or data unit's length on air: filled out to the end of its last status symbol
    //! period (35 data dibits and the status symbol), where the next unit starts
    static int paddedLength(int nbData) { return ((nbData + 34) / 35) * 36; }
    static bool isVariable(int duid) { return (duid == DUIDTSDU) || (duid == DUIDPDU); }
    //! Blocks a trunking or data unit may run to: three for a TSDU, what its header says for a PDU
    int maxBlocks() const { return ((m_duid == DUIDPDU) && m_packet.m_headerValid) ? m_packet.m_blocks + 1 : 3; }
    void decodePacketBlock(int k);
    void finishPacket();
    void readNID();
    int unitLength(int duid) const;
    void gatherLlrs(int64_t unitStart, int firstDataDibit, int nbDibits, float *llr, uint8_t *bits) const;
    void processHDU(int64_t unitStart);
    void processLDU(int64_t unitStart, int duid);
    void processTDULC(int64_t unitStart);
    //! Decode block k (0 based) of the trunking or data unit at unitStart. Returns the last block flag or -1.
    int decodeBlock(int64_t unitStart, int k, uint8_t *bytes12) const;
    void parseTSBK(const uint8_t *block);
    //! Channel field (identifier and number) as text: the frequency when the identifier is known
    void parseLC(const uint8_t *lc9);
};

#endif // INCLUDE_DSDPROTOCOLP25_H
