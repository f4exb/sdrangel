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

#ifndef INCLUDE_DSP_DV_DVANGEL61FILE_H
#define INCLUDE_DSP_DV_DVANGEL61FILE_H

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "export.h"

#include "dvangel61.h"

/** The offer of a file (ANGEL61.md 3.4, content type 3) */
struct SDRBASE_API DVAngel61FileOffer
{
    int m_transferId = 0;          //!< 0..255
    int m_flags = 0;
    int m_blockSize = 20;          //!< bytes of a data block, the message field of the frames used
    uint32_t m_fileSize = 0;
    uint32_t m_crc = 0;            //!< CRC-32 of the whole file: its identity and the end to end check
    std::string m_name;            //!< UTF-8, sanitised by the receiver before it is used
    std::string m_mediaType;       //!< optional

    static const uint32_t MaxBlocks = 65536;   //!< what a 16 bit block index holds
    //! The largest block: the largest message field of any profile, revision 3's HF-WB tier 3 data
    //! group (912 bytes), rounded up. Revision 2 had 54, its NB 16-QAM data frame's field
    static const int MaxBlockSize = 1024;

    /**
     * The blocks the file needs, in 64 bits. In 32 bits a file size near the top of the range
     * wraps the rounding up and reports nothing, which is how a transfer used to get past the
     * MaxBlocks check and reach a 4 GB allocation; the result is clamped one above the limit so
     * that every `blocks() > MaxBlocks` test still fires.
     */
    uint64_t blockCount() const
    {
        return m_blockSize > 0
            ? (((uint64_t) m_fileSize + (uint64_t) m_blockSize - 1) / (uint64_t) m_blockSize)
            : 0;
    }
    int blocks() const
    {
        uint64_t n = blockCount();
        return (int) (n > (uint64_t) MaxBlocks ? (uint64_t) MaxBlocks + 1 : n);
    }
    std::vector<uint8_t> encode() const;
    static bool decode(const std::vector<uint8_t>& bytes, DVAngel61FileOffer& offer);
};

/** What the receiver sends back (content type 5) */
struct SDRBASE_API DVAngel61FileControl
{
    enum Kind { Accept = 0, Reject = 1, Missing = 2, Complete = 3, Abort = 4 };
    enum Reason {
        ReasonOperator = 0,        //!< refused by the operator
        ReasonStorage = 1,
        ReasonTooLarge = 2,
        ReasonUnsupported = 3,
        ReasonCrc = 4,             //!< every block arrived and the file's CRC-32 still failed
        ReasonBusy = 5,
        ReasonTimeout = 6
    };

    int m_kind = Missing;
    int m_transferId = 0;
    uint32_t m_crc = 0;            //!< of the file, so that the message names it without the transfer id
    int m_reason = 0;
    std::vector<std::pair<int, int>> m_ranges; //!< first block index, block count

    std::vector<uint8_t> encode() const;
    static bool decode(const std::vector<uint8_t>& bytes, DVAngel61FileControl& control);
};

/**
 * An unfinished transfer kept for a resume (ANGEL61.md 3.4): the bytes that arrived, by byte range
 * rather than by block, so that a later offer of the same file - a new transfer id, another block
 * size (a sender that changed tier starts again at its new field), another session - takes them.
 * A file's identity is its source, size and CRC-32 (the name is shown, not compared).
 */
struct SDRBASE_API DVAngel61FilePartial
{
    uint64_t m_source = 0;
    std::string m_name;
    uint32_t m_fileSize = 0;
    uint32_t m_crc = 0;
    int64_t m_savedSec = 0;                              //!< when it was kept, seconds since the epoch
    std::vector<std::pair<uint32_t, uint32_t>> m_ranges; //!< bytes held: first, count; sorted, apart
    std::vector<uint8_t> m_data;                         //!< m_fileSize bytes, those held in place

    //! Whether every byte from first for count is held
    bool covers(uint32_t first, uint32_t count) const;
    uint32_t bytesHeld() const;
    //! The identity as a file name's stem: source, CRC-32 and size in hexadecimal
    std::string key() const { return keyOf(m_source, m_fileSize, m_crc); }
    static std::string keyOf(uint64_t source, uint32_t fileSize, uint32_t crc);
    //! "A61P", version 1, the identity, the time, the name, the ranges, then the held bytes
    std::vector<uint8_t> encode() const;
    static bool decode(const std::vector<uint8_t>& bytes, DVAngel61FilePartial& partial);
};

/**
 * The unfinished transfers a station keeps (at most MaxPartials, for MaxAgeSec): the receivers put
 * theirs in and take them back for an offer of the same file; the owner keeps them on disk, from
 * what changed and what was removed since it last asked.
 */
class SDRBASE_API DVAngel61FilePartials
{
public:
    static const int MaxPartials = 16;
    static const int64_t MaxAgeSec = 86400;   //!< a day (3.4)

    const DVAngel61FilePartial *find(uint64_t source, uint32_t fileSize, uint32_t crc) const;
    //! Kept (replacing the same file's), the oldest going when there are too many
    void put(const DVAngel61FilePartial& partial);
    //! As put, from the owner's store: not reported as changed
    void load(const DVAngel61FilePartial& partial);
    void remove(uint64_t source, uint32_t fileSize, uint32_t crc);
    //! Those older than MaxAgeSec go
    void expire(int64_t nowSec);
    int size() const { return (int) m_partials.size(); }
    const DVAngel61FilePartial *get(const std::string& key) const;
    std::vector<std::string> takeChanged();
    std::vector<std::string> takeRemoved();

private:
    std::map<std::string, DVAngel61FilePartial> m_partials;
    std::set<std::string> m_changed;
    std::set<std::string> m_removed;

    void insert(const DVAngel61FilePartial& partial, bool changed);
};

/**
 * The transmitting side of a file transfer (ANGEL61.md 3.4): the offer repeated as the blocks
 * go out, then only the blocks the receiver says are missing, until it says complete or either
 * side gives up. It produces message fields; the protocol builder puts them in data frames.
 */
class SDRBASE_API DVAngel61FileSender
{
public:
    enum State { Idle, Sending, Waiting, Done, Failed };
    static const int OfferIntervalFrames = 33;   //!< about 2 s of VHF's 80 ms frames: the default interval
    static const int DefaultMaxRounds = 5;

    DVAngel61FileSender();

    //! blockSize 0: the message field of the frame type the transfer will use
    bool start(const std::string& name, const std::vector<uint8_t>& data, int transferId, int blockSize,
        const std::string& mediaType = std::string());
    void stop();
    //! The next message field, of fieldBytes payload bytes (DVAngel61::messageFieldBytes of the
    //! frame's type), false when there is nothing to send now
    bool nextFieldBytes(int fieldBytes, DVAngel61::MessageFragment& fragment);
    //! What the receiver answered
    void handleControl(const DVAngel61FileControl& control);
    /**
     * Waiting, and the receiver's answer never came (the channel's timer: its offer was lost, or its
     * answer): the offer goes again alone, a round of the limit, so that the receiver owes an answer
     * (3.4) - `missing` names the blocks it has not got, those it kept before the offer included.
     * False when there is nothing to do or the rounds have run out (Failed)
     */
    bool handleNoAnswer();
    /**
     * The offer is repeated every `fields` message fields of the transfer (OfferIntervalFrames by
     * default, about 2 s on VHF). HF's field is a whole data group of four 240 ms frames, so the
     * default would repeat it every 30 s, after most transmissions have ended
     */
    void setOfferInterval(int fields) { m_offerInterval = std::max(1, fields); }
    int getOfferInterval() const { return m_offerInterval; }
    /**
     * The transfer goes to a group - CQ or a talkgroup (3.4, 2.6): nobody answers, so once every
     * block has been sent it is done rather than waiting. Set per transmission, from its routing
     */
    void setGroup(bool group) { m_group = group; }
    bool isGroup() const { return m_group; }

    State getState() const { return m_state; }
    bool isActive() const { return (m_state == Sending) || (m_state == Waiting); }
    //! Nothing left to send in this transmission: the transmitter may key down and wait for the answer
    bool isDrained() const { return m_state == Waiting; }
    const DVAngel61FileOffer& getOffer() const { return m_offer; }
    int getRounds() const { return m_rounds; }
    int getBlocksSent() const { return m_blocksSent; }
    int getBlocksPending() const { return (int) m_pending.size(); }
    //! The file being sent, so that a transfer can start again at another block size
    const std::vector<uint8_t>& getData() const { return m_data; }
    int getRejectReason() const { return m_rejectReason; }
    void setMaxRounds(int rounds) { m_maxRounds = rounds; }

private:
    State m_state;
    DVAngel61FileOffer m_offer;
    std::vector<uint8_t> m_data;
    std::deque<int> m_pending;               //!< block indices still to send
    std::vector<DVAngel61::MessageFragment> m_offerFragments;
    int m_offerFragment;                     //!< next offer fragment to send, m_offerFragments.size() when done
    int m_offerFieldBytes;                   //!< the field size the offer fragments were made for
    int m_framesSinceOffer;
    int m_offerInterval;
    int m_rounds;
    int m_blocksSent;
    int m_rejectReason;
    int m_maxRounds;
    bool m_group = false;

    bool buildOffer(int fieldBytes);
};

/**
 * The receiving side: the offer and the control messages arrive as ordinary messages (the
 * session's reassembler puts them together), the data blocks one frame each. The class keeps
 * the blocks, says what is missing and produces the control message to send back.
 */
class SDRBASE_API DVAngel61FileReceiver
{
public:
    enum State { Idle, Receiving, Complete, Rejected };
    static const int MaxRangesPerControl = 32;

    DVAngel61FileReceiver();

    void reset();
    //! The largest file this station takes, 0 for no limit
    void setMaxFileSize(uint32_t bytes) { m_maxFileSize = bytes; }
    /**
     * A completed content type 3 message. Returns true when a transfer started (or restarted).
     * group: the transmission is addressed to CQ or a talkgroup (3.4, 2.6): the file is received
     * as any other, but no answer is sent - no missing blocks, no complete, no reject
     */
    bool setOffer(uint64_t source, const std::vector<uint8_t>& bytes, bool group = false);
    bool isGroup() const { return m_group; }
    /**
     * A content type 3 fragment was heard for this transfer. The offer itself is only put
     * together once (the reassembler treats its repetitions as duplicates), but its repetition
     * is how the receiver knows the transmission it is listening to still carries the transfer
     * and that it owes an answer at the end of it, even when every data block was destroyed.
     */
    void noteOfferFragment(int transferId);
    //! One content type 4 field. Returns true when the block was kept
    bool addBlock(uint64_t source, int transferId, int blockIndex, const uint8_t *data, int nbBytes);
    //! The transmission ended: the control message to send back, false when there is nothing to say
    bool nextControl(DVAngel61FileControl& control);
    //! Refuse the transfer (the operator, or a limit): the next control message says so
    void reject(int reason);

    /**
     * The operator confirms every offer before its file is kept (3.4). Until accept() the blocks are
     * held in memory only: no answer goes back (the sender, hearing none, offers again), nothing is
     * kept for a resume and the file is not delivered; reject() (ReasonOperator) refuses it
     */
    void setConfirm(bool confirm) { m_confirmRequired = confirm; }
    bool isAwaitingConfirmation() const { return m_hasOffer && !m_confirmed && ((m_state == Receiving) || (m_state == Complete)); }
    bool isConfirmed() const { return m_confirmed; }
    //! The operator takes the file: the answer held back is due now (missing blocks, or complete)
    void accept();

    //! Unfinished transfers for a resume: kept there, and taken back for an offer of the same file
    void setPartials(DVAngel61FilePartials *partials) { m_partials = partials; }
    //! The transmission ended, or another transfer replaces this one: what arrived of an unfinished
    //! file goes to the partials (a confirmed transfer's only)
    void keepPartial();
    //! Blocks this transfer took from a kept partial when its offer arrived
    int getBlocksResumed() const { return m_blocksResumed; }

    State getState() const { return m_state; }
    bool hasOffer() const { return m_hasOffer; }
    const DVAngel61FileOffer& getOffer() const { return m_offer; }
    uint64_t getSource() const { return m_source; }
    const std::vector<uint8_t>& getData() const { return m_data; }
    int getBlocksReceived() const { return m_blocksReceived; }
    int getBlocksExpected() const { return m_hasOffer ? m_offer.blocks() : 0; }
    float getProgress() const;
    int getRejectReason() const { return m_rejectReason; }
    //! The blocks that have not arrived, as ranges of block index and count
    std::vector<std::pair<int, int>> missingRanges(int maxRanges) const;
    //! The name with any path, control character or leading dot taken out of it
    static std::string sanitiseName(const std::string& name);

private:
    State m_state;
    bool m_hasOffer;
    uint64_t m_source;
    DVAngel61FileOffer m_offer;
    std::vector<uint8_t> m_data;
    std::vector<bool> m_received;
    int m_blocksReceived;
    uint32_t m_maxFileSize;
    int m_rejectReason;
    bool m_controlPending;
    bool m_group;
    //! Blocks that arrived before the offer did, kept by transfer id
    int m_earlyTransferId;
    std::vector<std::pair<int, std::vector<uint8_t>>> m_early;
    bool m_confirmRequired = false;
    bool m_confirmed = true;
    DVAngel61FilePartials *m_partials = nullptr;
    int m_blocksResumed = 0;

    //! Every block the partial holds, placed; complete when they are all
    void resume(const DVAngel61FilePartial& partial);
    //! Every block is in: the CRC-32 decides
    void finish();
};

#endif // INCLUDE_DSP_DV_DVANGEL61FILE_H
