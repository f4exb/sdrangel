///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef MODEMRADIOPAGING_RADIOPAGINGFLEXSCHEDULER_H
#define MODEMRADIOPAGING_RADIOPAGINGFLEXSCHEDULER_H

#include <cstdint>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "export.h"
#include "radiopagingflex.h"

// FLEX time grid and placement of messages in frames.
//
// Time is a frame index supplied by the caller (no clock is read). An hour is 15 cycles
// of 128 frames, so cycle = (index / 128) mod 15 and frame = index mod 128. A pager
// listens to the frame (capcode / 16) mod 128 and the phase (capcode / 4) mod 4 of its
// capcode. With a collapse value m it listens to one frame in 2^m only: the frames whose
// number is congruent to its frame modulo 2^m.
class MODEMRADIOPAGING_API RadioPagingFlexScheduler
{
public:
    static const int m_framesPerCycle = 128;
    static const int m_cyclesPerHour = 15;
    static const int m_framesPerHour = 1920;
    static const int m_protectedFrames = 4;         //!< Frames 0-3 can not be reserved
    static const size_t m_numberCounterMax = 25000; //!< Capcodes with a message number counter

    struct CapcodeSlot
    {
        uint32_t m_frame;   //!< (capcode / 16) mod 128
        uint32_t m_phase;   //!< (capcode / 4) mod 4, before the reduction to the phases of a mode
    };

    struct FrameTime
    {
        uint32_t m_cycle;       //!< 0-14
        uint32_t m_frame;       //!< 0-127
        uint32_t m_absolute;    //!< cycle * 128 + frame
    };

    //! Message number sequence of each capcode (STD-43A 3.10.1.3 N, 8.4): the first number of a capcode is 0,
    //! the next ones ascend by one up to the largest number of the network, then 0 again. The least recently
    //! used capcode is dropped when the table is full. Exported on its own: the export of the class does not
    //! cover its nested classes
    class MODEMRADIOPAGING_API NumberCounter
    {
    public:
        explicit NumberCounter(size_t maxEntries = m_numberCounterMax) : m_maxEntries(maxEntries) {}
        //!< The next number of the capcode, after maxNumber (0-63) comes 0
        uint8_t next(uint64_t capcode, int maxNumber = 63);
        //!< The number next() would give, without taking it
        uint8_t upcoming(uint64_t capcode, int maxNumber = 63) const;
        int peek(uint64_t capcode) const;     //!< Last number given, -1 if the capcode has no counter
        void set(uint64_t capcode, uint8_t value);
        size_t size() const { return m_order.size(); }

    private:
        typedef std::list<std::pair<uint64_t, uint8_t>> List;
        void touch(List::iterator it);
        void insert(uint64_t capcode, uint8_t value);

        size_t m_maxEntries;
        List m_order;                                               //!< Most recently used first
        std::unordered_map<uint64_t, List::iterator> m_index;
    };

    static CapcodeSlot capcodeSlot(uint64_t capcode);
    static FrameTime frameTime(uint64_t frameIndex);
    //!< First frame at or after current whose number is congruent to assigned modulo 2^collapse
    static uint32_t nextFrame(uint32_t current, uint32_t assigned, int collapse);
    //!< True if a pager of the capcode listens to the frame
    static bool frameEligible(uint64_t capcode, uint32_t frame, int collapse);
};

// Queue, placement and frame builder of a FLEX transmitter with one mode and polarity.
//
// Messages wait in a queue. For every frame index the encoder returns the frame: the
// messages whose capcode is eligible in the frame are placed in the phase of their
// capcode (priority addresses first, tone only addresses last, a phase that is full
// passes messages to another phase, preferring phases of the most significant bit), a
// message is sent once and removed. A frame without messages is an idle frame
// (block information word and idle words), sent continuously. A frame reserved for
// another transmitter has no output.
class MODEMRADIOPAGING_API RadioPagingFlexEncoder
{
public:
    struct TxMessage
    {
        uint64_t m_capcode = 0;
        RadioPagingFlex::MessageType m_type = RadioPagingFlex::Alphanumeric;
        std::string m_text;         //!< Ignored for tone; short message: the digits of the numeric type
        bool m_priority = false;
        RadioPagingFlex::ShortType m_shortType = RadioPagingFlex::ShortNumeric;    //!< Short message only
        int m_source = 0;           //!< Short message source and numbered types: source code 0-7
        RadioPagingFlex::BinaryParams m_binary;     //!< HEX/binary and secure messages only
        RadioPagingFlex::InstructionParams m_instruction;   //!< Short instruction only
        //!< Numeric format (STD-43A 3.10.1.1): standard, special, numbered (a network with message numbering only) or both
        RadioPagingFlex::NumericFormat m_numericFormat = RadioPagingFlex::NumericStandard;
        //!< Frame 0-127 the message goes in, -1: a frame of the capcode (its collapse). Messages to a temporary address
        //!< go in the frame their short instructions told
        int m_frame = -1;
        //!< A group message (STD-43A Chapter 5.2): the capcodes of the pagers, 2 to m_maxGroupMembers. The message (not
        //!< fragmented, not numbered: numeric, alphanumeric, tone or short message) goes to a temporary address that
        //!< a short instruction tells them, in the frame of the group message; m_capcode is not used. Not with
        //!< retransmissions
        std::vector<uint64_t> m_members;
        int m_slot = -1;            //!< Group: temporary address 0-15, -1 the first one free
        //!< Retransmissions after the first transmission, 0-15 (STD-43A 4.3.7.6: a message sent again from the
        //!< retrieval storage has its number and R = 0). Numbered messages of a network with message numbering only
        int m_retransmit = 0;
        int m_retransmitInterval = 128;     //!< Frames from a transmission to the next one, 1-1920
        int m_sendDelay = 0;                //!< Frames the first transmission waits, 0-1920
    };

    static const int m_maxRetransmit = 15;
    static const int m_maxScheduleFrames = 1920;    //!< Longest retransmission interval and send delay: an hour
    //!< Pagers of a group: the short instructions one phase can carry before the group message, which is at most 127
    //!< frames after the first of them (its frame is a frame number of the cycle, STD-43A 3.9.6): 127 frames of 43
    //!< address and vector word pairs (88 words less the block information word, STD-43A 3.4.1)
    static const int m_maxGroupMembers = 127 * 43;
    //!< Short instructions of a phase in a frame, at most
    static const int m_instructionsPerFrame = 43;
    static const int m_maxGroupCollapse = 6;        //!< A group message follows its instructions within 128 frames (5.2)

    struct EnqueueResult
    {
        bool m_ok = false;
        std::string m_error;
        uint32_t m_id = 0;
    };

    //!< A message as it went: its message number (-1: none, a group message), its fragments and its R flag
    struct SentStatus
    {
        int m_number = -1;
        int m_fragments = 1;
        bool m_retrieval = false;
        int m_cycle = -1;           //!< Where it went: the cycle, frame and phase (0 to 3: A to D) of its last fragment
        int m_frame = -1;
        int m_phase = -1;
        int m_firstCycle = -1;      //!< A message in fragments: the cycle and frame of its first fragment
        int m_firstFrame = -1;
        // The streams of the other protocols report where their messages went in the same structure
        int m_batch = -1;           //!< POCSAG: batch of the address after the preamble, from 0; GSC: batch mode (0, 1)
        int m_lastBatch = -1;       //!< POCSAG: batch of the last codeword
        int m_codewords = -1;       //!< POCSAG: address and message codewords
        int m_preamble = -1;        //!< GSC: preamble value index
        int m_slot = -1;            //!< GSC: slot of the address in its batch, from 0
        int m_blocks = -1;          //!< GSC: 121 bit blocks of the message, the address included
    };

    struct Frame
    {
        bool m_reserved = false;    //!< Frame reserved for another transmitter: no output
        bool m_idle = false;        //!< No message in the frame (a fragment that is not the last is a message)
        uint32_t m_cycle = 0;
        uint32_t m_frame = 0;
        RadioPagingFlex::Mode m_mode = RadioPagingFlex::Mode1600_2;
        bool m_inverted = false;
        unsigned m_traffic = 0;     //!< Low traffic flags of the frame information word
        std::vector<uint32_t> m_sentIds;                    //!< Messages sent: in one piece, or their last fragment (first transmission)
        std::vector<uint32_t> m_sentNumbers;                //!< Message number of each sent id
        std::vector<SentStatus> m_sentStatus;               //!< How each sent id went
        std::vector<uint32_t> m_retransmittedIds;           //!< Messages sent again (retransmissions, R = 0)
        std::vector<SentStatus> m_retransmittedStatus;      //!< How each retransmitted id went
        std::vector<uint32_t> m_doneIds;                    //!< Messages with nothing more queued: sent and retransmitted
        std::vector<uint32_t> m_failedIds;                  //!< Group messages dropped: a short instruction did not go before the group message
        bool m_systemEvent = false;                         //!< The System Event Notification is in every phase of the frame
        RadioPagingFlex::FrameBytes m_bytes;
        std::vector<RadioPagingFlex::SymbolBlock> m_blocks; //!< Sync part then data part; empty if reserved
    };

    RadioPagingFlexEncoder();

    void setMode(RadioPagingFlex::Mode mode) { m_mode = mode; }
    RadioPagingFlex::Mode mode() const { return m_mode; }
    void setInverted(bool inverted) { m_inverted = inverted; }
    bool inverted() const { return m_inverted; }

    //!< Multi-area/roaming by simulcast system ID (STD-43A Chapter 6). The roaming bit of the frame
    //!< information word is set; the first system ID word is in every frame and the second in frames 0 to 3,
    //!< in the phase of 6.1.1.3. The channel has to send frames 0 to 3 in full (the callers do not reserve them).
    //!< The traffic management flags tell which of the 4 traffic groups of the system this channel serves:
    //!< they are announced, the channel does not filter the addresses
    struct Roaming
    {
        bool m_enabled = false;
        unsigned m_localId = 0;             //!< 0-511
        unsigned m_coverageZone = 0;        //!< 0-31
        unsigned m_countryCode = 0;         //!< 0-1023 (ITU-T E.212)
        unsigned m_trafficFlags = 0xf;      //!< 4 flags, T3 to T0
    };

    void setRoaming(const Roaming& roaming) { m_roaming = roaming; }
    const Roaming& roaming() const { return m_roaming; }

    //! Date and time in block information words 001 (date), 010 (time) and 101 A = 0101 (time zone, daylight
    //! saving time, extended seconds), STD-43A 3.7.2. The time is the local time: the frame grid time (UTC, frame
    //! index times 1.875 s from the Unix epoch), plus the offset, the zone and one hour of daylight saving time.
    //! The words fill the block information words 2 to 4 left free (at most 3, after the system ID words of
    //! roaming) in every phase of every frame, so frame 0 of cycle 0 has a time word in every phase. When they
    //! do not all fit they take turns: the frame index modulo 3 chooses the first. 3 is prime to 2^collapse, so
    //! a pager listening to one frame in 2^collapse receives all three within 3 of its frames
    struct TimeService
    {
        enum Reference {
            FrameZero,      //!< The time of frame 0 of the cycle (the start of its bit sync 1): STD-43A 3.7.2
            CurrentFrame    //!< The time of the frame that carries the words
        };

        bool m_enabled = false;
        Reference m_reference = FrameZero;
        int m_zoneMinutes = 0;          //!< Offset of the standard time of the zone from UTC (Table 3.7.2-3)
        bool m_sendZone = true;         //!< Send the time zone word (101)
        bool m_daylightSaving = false;  //!< The time is daylight saving time: one hour ahead, L0 = 0
        int64_t m_offsetSeconds = 0;    //!< Added to the frame grid time: sets any date and time
        //!< A year outside 1994-2025 (the year field) is sent as the latest year in it with the same calendar
        //!< (leap year and weekday of January 1): the pager shows the right weekday
        bool m_equivalentYear = false;
        //!< Extended second S5-S3 of the time zone word (3.7.2, 0-7 steps of 1/64 minute, 0.9375 s): the correction of
        //!< the time a pager shows when the frames are not aligned to the second
        unsigned m_extendedSeconds = 0;
    };

    //!< False and an error if the zone is not in Table 3.7.2-3
    bool setTimeService(const TimeService& time, std::string& error);
    const TimeService& timeService() const { return m_time; }
    //!< Local date and time of a frame as sent (reference, offset, zone and daylight saving applied): seconds
    //!< from the epoch and the 1/64 minute steps of the minute
    int64_t localTimeMs(uint64_t frameIndex) const;
    //!< Collapse 0-7, a pager listens to one frame in 2^collapse. False and error if out of range
    bool setCollapse(int collapse, std::string& error);
    int collapse() const { return m_collapse; }

    //! Message numbering of the network (STD-43A 8.4, 3.10.1.1-3.10.1.4 N and R; numbering is optional).
    //! On: the numbered types (alphanumeric, HEX/binary, numbered short message) of an address take one sequence
    //! of numbers, from 0 up to the largest number of the network (63 at most, the pagers are set to it) then 0
    //! again, with R = 1. A HEX/binary header message (H = 1) and the message after it to the address share the
    //! number. Off: R = 0, a numbered short message is refused. A message the network does not number (numbering
    //! off, and a secure message, which is never numbered) takes the lowest number no message of the address in
    //! progress has (with numbering on, not the next number of the sequence either), 0 when the address has none:
    //! the fragments of a message are told apart (3.10.1.2-3.10.1.4 N). Tone, numeric, and short numeric and
    //! source messages have no number and leave the sequence alone
    struct Numbering
    {
        bool m_enabled = true;
        int m_maxNumber = 63;       //!< 0-63
    };

    //!< False and an error if the largest number is out of range
    bool setNumbering(const Numbering& numbering, std::string& error);
    const Numbering& numbering() const { return m_numbering; }

    //! System Event Notification (STD-43A 3.8.2.4, 3.8.5, Table 3.9.6-1): the pagers are told that the events of the
    //! flags e10-e0 (e0 traffic split by SSID TMF, e1 traffic split by NID TMF, e2 change of the Channel Set Up
    //! Instruction, e3 a new frequency of the NID, e4 a new frequency of the SSID coverage zone, e5-e10 reserved,
    //! 0) take place within the next 4 cycles. The short instruction (type 001, the flags in d0-d10) to the System
    //! Event Notification address goes in each frame for at least one full cycle: in every phase of the frames built
    //! (a pager reads its own phase), ahead of the messages, for the given number of frames. Frames reserved for
    //! another transmitter do not count
    static const int m_minEventFrames = 128;        //!< One full cycle
    static const int m_maxEventFrames = 512;        //!< The 4 cycles of the notice
    static const unsigned m_maxEvents = 0x7ff;
    //!< Start the notification of the events (1-0x7ff) for frames (128-512) from the next frame built. False and an
    //!< error if out of range
    bool notifySystemEvent(unsigned events, int frames, std::string& error);
    void cancelSystemEvent() { m_eventFrames = 0; }
    //!< Frames of the notification still to send, 0 when none; its event flags
    int systemEventFrames() const { return m_eventFrames; }
    unsigned systemEvents() const { return m_events; }
    //!< One entry per frame (128), non zero = reserved. Frames 0-3 can not be reserved: the table
    //!< is not applied and false is returned. An empty table clears all reservations
    bool setReservedFrames(const std::vector<uint8_t>& table, std::string& error);
    const std::vector<uint8_t>& reservedFrames() const { return m_reserved; }

    //!< Queue a message. Rejected if the capcode or text is invalid, the message does not fit
    //!< in a frame, or every frame the capcode is eligible in is reserved. An alphanumeric, HEX/binary or
    //!< secure message longer than a frame is split into fragments (RadioPagingFlex::fragmentMessage) under
    //!< the one id: the first in a frame of the capcode, the others in the frames after it (STD-43A 4.2.1).
    //!< The first transmission waits m_sendDelay frames from the first frame built after the message is queued.
    //!< A message to retransmit (m_retransmit, refused unless it is numbered and the network numbers messages)
    //!< is queued again with its number and R = 0, m_retransmitInterval frames after the frame of its last
    //!< fragment, and goes in a frame of the pager again
    EnqueueResult enqueue(const TxMessage& message);
    //!< Queued messages and fragments still to send, retransmissions waiting included
    size_t pending() const { return m_queue.size(); }
    //!< Ids of the queued messages that no frame can carry with the current collapse and reservations
    std::vector<uint32_t> unschedulable() const;
    //!< Remove a queued message and its retransmissions. False if the id is not queued
    bool remove(uint32_t id);
    //!< Queued messages a frame of the frame index would carry if it had room (their capcode
    //!< listens to the frame, the frame is not reserved)
    size_t eligible(uint64_t frameIndex) const;
    //!< The frame of the frame index has content that must go in it: a fragment after the first of a message
    //!< (the pager waits for it), or the time words of frame 0 of cycle 0
    bool mandatory(uint64_t frameIndex) const;
    //!< Block information words 2 to 4 of a phase of the frame (system ID of roaming, date and time), encoded
    std::vector<uint32_t> extraBlockInfo(uint64_t frameIndex, int phase) const;

    //!< The frame of a frame index. Messages placed in it are removed from the queue
    Frame buildFrame(uint64_t frameIndex);

private:
    struct Item
    {
        uint32_t m_id;      //!< The same for all the fragments of a message
        TxMessage m_message;    //!< The text of the fragment
        int m_number;       //!< -1 until the first placement attempt; the fragments take the number of the first
        bool m_retrieval = false;   //!< R of the message, set with the number
        int m_fragment = 0;     //!< Fragment index (alphanumeric, HEX/binary and secure), in queue order
        int m_fragments = 1;
        int m_signature = -1;   //!< Alphanumeric and HEX/binary first fragment: signature of the whole message
        int m_phase = -1;       //!< Fragments after the first: the phase of the first fragment, -1 until it is sent
        int m_firstCycle = -1;  //!< Fragments after the first: the cycle and frame of the first fragment, -1 until it is sent
        int m_firstFrame = -1;
        int m_delay = 0;            //!< Frames to wait from the first frame built after it was queued
        int64_t m_notBefore = -1;   //!< First frame index it may go in, -1 until set (with m_delay)
        int m_retransmitsLeft = 0;  //!< Retransmissions still to come after this transmission
        bool m_retransmission = false;  //!< A copy sent again (R = 0)
        int m_group = -1;           //!< Group message: its index in m_groups; the short instructions and the group messages
        bool m_delivery = false;    //!< The group message itself (to the temporary address), not a short instruction
    };

    // A group message in progress (STD-43A 5.2): the short instructions go to the members, then the group message to
    // the temporary address in the frame they tell
    struct Group
    {
        uint32_t m_id = 0;              //!< Id of the message
        std::map<int, int> m_slots;     //!< Temporary address of each phase of the pagers (3.8.5 (1): an address goes in a frame twice at most)
        std::vector<uint64_t> m_members;
        TxMessage m_message;            //!< The group message, to the temporary address
        bool m_targetSet = false;
        uint64_t m_targetIndex = 0;     //!< Frame index of the group message; its frame number goes in the instructions
        int m_deadline = 0;
    };

    //!< Queue the group message of a message with members
    EnqueueResult enqueueGroup(const TxMessage& message);
    //!< The frame of the group message once the first short instruction may go, the frame number in the instructions
    void setGroupTargets(uint64_t frameIndex);
    //!< The group messages whose short instructions did not all go before their frame: removed (see Frame::m_failedIds)
    void failLateGroups(uint64_t frameIndex, Frame& frame);
    bool groupReady(const Item& item) const;
    void endGroup(int index);

    //!< The item may go in the frame of the frame index (its send delay or retransmission interval is over)
    static bool due(const Item& item, uint64_t frameIndex);

    bool schedulable(uint64_t capcode) const;
    //!< A fragment goes in a frame after the one of the fragment before it (no earlier fragment is queued);
    //!< the first fragment of a fragmented message waits for a fragmented message to the same address
    //!< queued before it (STD-43A 3.8.5 (2))
    bool fragmentReady(const Item& item) const;
    //!< The item can go in the frame: not reserved, and a frame of the pager (its collapse) for a first
    //!< fragment or an unfragmented message, any frame for the fragments after the first (STD-43A 4.2.1)
    bool frameFor(const Item& item, uint32_t frame) const;
    static RadioPagingFlex::PhaseMessage phaseMessage(const Item& item);
    //!< The rest of an alphanumeric, HEX/binary or secure message from this fragment on, cut again: its first piece
    //!< in messageWords (the room of its phase in the frame being built: STD-43A 4.1, a fragment is what the frame
    //!< has room for), the others in whole frames. False if there is no room for a piece
    bool refragment(std::list<Item>::iterator item, int messageWords);
    //!< A tone only message as a short message with the tone only payload
    static void toneOnly(RadioPagingFlex::PhaseMessage& pm);
    //!< Message number and R of the item and of its other fragments (see Numbering)
    void assignNumber(Item& item);
    //!< The lowest number no message of the capcode in progress (numbered, still queued) has, apart from avoid
    int freeNumber(uint64_t capcode, int avoid) const;

    RadioPagingFlex::Mode m_mode;
    bool m_inverted;
    int m_collapse;
    Numbering m_numbering;
    Roaming m_roaming;
    TimeService m_time;
    std::vector<uint8_t> m_reserved;
    std::list<Item> m_queue;
    uint32_t m_nextId;
    RadioPagingFlexScheduler::NumberCounter m_numbers;
    std::unordered_map<uint64_t, int> m_headerNumbers;  //!< Number of a header message, for the next message of the capcode
    std::unordered_map<uint32_t, std::vector<Item>> m_templates;    //!< Fragments of the messages to retransmit, by id
    std::map<int, Group> m_groups;      //!< Group messages in progress, by the index of Item::m_group
    int m_nextGroup = 0;
    bool m_slotBusy[RadioPagingFlex::m_temporaryCount] = {};    //!< Temporary addresses in use
    unsigned m_events = 0;              //!< System Event Notification: event flags
    int m_eventFrames = 0;              //!< Frames of the notification still to send
};

#endif // MODEMRADIOPAGING_RADIOPAGINGFLEXSCHEDULER_H
