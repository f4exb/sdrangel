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

#ifndef INCLUDE_DSDPROTOCOLM17_H
#define INCLUDE_DSDPROTOCOLM17_H

#include <cstdint>
#include <string>
#include <vector>

#include "dsp/dv/dvm17.h"

#include "dsdprotocol.h"

/**
 * M17 decoder (https://spec.m17project.org): 4FSK at 4800 symbols/s, 40 ms frames of 192 symbols
 * whose first 8 are one of four sync words.
 *
 * A stream frame carries two Codec2 3200 packets and a sixth of the link setup frame in its
 * LICH, so a receiver that joined late has the callsigns within 240 ms; a packet transmission
 * carries 25 bytes a frame, which this decoder assembles and checks, delivering SMS text and
 * APRS packets. The end of transmission marker closes the transmission cleanly.
 */
class DSDProtocolM17 : public DSDProtocol
{
public:
    struct TextMessage
    {
        bool m_valid = false;
        std::string m_source;
        std::string m_destination;
        std::string m_text;
        uint64_t m_revision = 0;
    };

    struct Packet
    {
        bool m_valid = false;
        int m_protocol = -1;
        std::vector<uint8_t> m_content;
        uint64_t m_revision = 0;
    };

    DSDProtocolM17(const DSDSymbolHistory& history);

    virtual const char *getName() const { return "M17"; }
    virtual int getSymbolRate() const { return DVM17::SymbolRate; }
    virtual bool isFourLevel() const { return true; }
    /**
     * M17's sync words are 8 symbols where the other protocols' are 20 to 24, so chance finds
     * one in noise every few seconds rather than every few hours, and a stream frame carries
     * nothing that can contradict it: its codec payload is unprotected and its LICH is four
     * Golay words, which decode to *something* about half the time each. The lock is therefore
     * only shown, and its audio only played, once something has ruled that out: a CRC-16 (the
     * link setup frame, the link setup reassembled from six LICH segments, or a packet) or a
     * sync word arriving where the last one said the next would, ProvenSyncRun times over.
     * The cadence is what carries a weak signal, where the CRCs fail: a chance sync in noise is
     * on its own, and one that is not has to keep being right every 40 ms.
     */
    virtual bool isProven() const { return m_proven; }
    //! Bytes of the packet being collected, for the tests that check it stays bounded
    size_t getPacketBytesHeld() const { return m_packetBytes.size(); }
    virtual std::vector<DVSync::Pattern> getSearchPatterns() const;
    virtual float getNominalOuterLevel() const { return (float) DVM17::PeakDeviationHz; }

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
    virtual bool endedNormally() const { return m_normalEnd; }
    virtual std::string getStatusText() const;

    bool hasLSF() const { return m_lsfValid; }
    const DVM17::LSF& getLSF() const { return m_lsf; }
    std::string getSource() const { return m_lsfValid ? DVM17::decodeCallsign(m_lsf.m_source) : std::string(); }
    std::string getDestination() const { return m_lsfValid ? DVM17::decodeCallsign(m_lsf.m_destination) : std::string(); }
    const TextMessage& getTextMessage() const { return m_textMessage; }
    const Packet& getPacket() const { return m_packet; }
    //! An APRS packet (protocol 2) received in a packet transmission, in TNC2 form, taken once
    bool takeAPRSPacket(std::string& packet);

private:
    static constexpr int m_payloadSymbols = 184;   //!< After the 8 sync symbols

    static constexpr int ProvenSyncRun = 4;  //!< Frames that have to arrive on time before a lock with no CRC behind it is shown (2 and 3 still let noise through: 50 and 4 locks an hour)
    bool m_active;
    int64_t m_syncIndex;          //!< Last symbol of the sync of the frame being received
    int64_t m_pendingSync;
    DVSync::Pattern m_pattern; //!< Of the frame being received
    DVSync::Pattern m_pendingPattern;
    int64_t m_burstFirst;
    int64_t m_burstLast;
    int64_t m_burstSample;
    int m_misses;
    bool m_burstValid;
    bool m_normalEnd;
    bool m_lsfValid;
    bool m_proven;                      //!< The lock is not a sync word found in noise
    int m_syncRun;                      //!< Frames in a row that arrived where the last one said they would
    DVM17::LSF m_lsf;
    uint8_t m_lsfBytes[DVM17::LSFBytes];
    uint8_t m_lichSegments;    //!< One bit per segment collected from the LICH
    int m_frameCount;
    uint16_t m_lastFrameNumber;
    std::vector<uint8_t> m_packetBytes;
    int m_packetFrames;
    TextMessage m_textMessage;
    Packet m_packet;
    uint64_t m_revision;
    std::vector<std::string> m_aprsPackets;

    void decodeFrame(int64_t syncIndex, DVSync::Pattern pattern);
    //! A frame arrived where the last one said it would: the evidence a weak signal has
    void noteOnTime();
    void decodeLSF(const float *payload);
    void decodeStream(const float *payload, int64_t syncIndex);
    void decodePacket(const float *payload);
    void finishPacket();
};

#endif // INCLUDE_DSDPROTOCOLM17_H
