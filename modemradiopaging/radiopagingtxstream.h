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

#ifndef MODEMRADIOPAGING_RADIOPAGINGTXSTREAM_H
#define MODEMRADIOPAGING_RADIOPAGINGTXSTREAM_H

#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "export.h"
#include "radiopagingflex.h"
#include "radiopagingflexscheduler.h"
#include "radiopaginggsc.h"
#include "radiopagingpocsag.h"
#include "radiopagingwav.h"
#include "radiopagingvoiceprocessor.h"

// Symbols of a transmitter, a block at a time, for the modulator (RadioPagingFskMod).
//
// A transmission is a sequence of blocks; it ends when next() returns false. One-shot
// streams make a transmission of what is queued and end it; network streams never end a
// transmission: they fill with idle signal when nothing is queued. Messages carry an id of
// the caller; a block lists the ids of the messages it completes.
struct RadioPagingTxBlock
{
    double m_baud = 0.0;                //!< Symbols per second; 750 and the fill are not whole divisors of 38400
    std::vector<float> m_levels;        //!< -1 to +1, +1 = highest frequency
    double m_shaping = 0.0;             //!< Gaussian cutoff of the block, Hz
    double m_deviation = 0.0;           //!< Deviation of the outer symbol, Hz; 0: the modulator's
    std::vector<uint64_t> m_ids;        //!< Messages completed by the block
    std::vector<uint64_t> m_repeatIds;  //!< Copies completed by the block of messages sent before (retransmissions)
    //!< How each message of m_ids and of m_repeatIds went: FLEX its message number, fragments, R, cycle, frame and phase;
    //!< POCSAG its batch, frame and codewords; GSC its preamble, batch mode and blocks. Empty when not known
    std::vector<RadioPagingFlexEncoder::SentStatus> m_status;
    std::vector<RadioPagingFlexEncoder::SentStatus> m_repeatStatus;
    std::vector<uint64_t> m_failedIds;  //!< Messages dropped before they were sent (a group message that missed its frame)
    std::vector<uint64_t> m_startIds;   //!< Messages that go on air with the block and complete in a later one (voice pages)
    bool m_ers = false;                 //!< FLEX emergency re-synchronization

    //!< Ticks per second of the time base of mixed streams: a whole number for every rate
    //!< (512, 750, 1200, 1600, 2400, 3200, 6400)
    static const int m_tickRate = 192000;
    //!< Length of the block in ticks
    int64_t ticks() const { return (int64_t) std::llround((double) m_levels.size() * m_tickRate / m_baud); }
};

class MODEMRADIOPAGING_API RadioPagingTxStream
{
public:
    virtual ~RadioPagingTxStream() {}

    //!< A transmission can start (network: always)
    virtual bool hasWork() const = 0;
    //!< A transmission starts: epochMs is the time of its first symbol, milliseconds since the
    //!< Unix epoch (UTC). FLEX places its frames on the time grid from it
    virtual void begin(int64_t epochMs) = 0;
    //!< Next block of the transmission. False: the transmission ends
    virtual bool next(RadioPagingTxBlock& block) = 0;
    //!< Messages queued and not in a block yet
    virtual size_t pending() const = 0;
    //!< Drop the messages queued: their ids
    virtual std::vector<uint64_t> clear() = 0;
    //!< The transmission is cut: drop the blocks prepared and not returned yet, the ids of the
    //!< messages they carried. Messages still queued stay
    virtual std::vector<uint64_t> cut() { return std::vector<uint64_t>(); }
    virtual bool continuous() const = 0;
};

// POCSAG (ITU-R M.584). Messages are grouped by speed and polarity.
//   One-shot: each message is a transmission: preamble and batches.
//   Network: the preamble once, then batches without end: the messages of the current
//   group at the next batch, idle batches when there are none. After two idle batches the
//   next group with messages in the carousel order (1200, 512, 2400 normal, then inverted)
//   takes over with a new preamble (ITU-R M.584: a transmission starts with its preamble); so does it after
//   m_maxBlocksPerTurn message blocks in a row, so that a busy group does not hold the
//   others. With no message anywhere the network goes back to the default group.
// Normal polarity: a binary 1 is a negative frequency deviation.
class MODEMRADIOPAGING_API RadioPagingPocsagTxStream : public RadioPagingTxStream
{
public:
    static const int m_idleBatchesBeforeSwitch = 2;
    //!< Message blocks of a group in a row before another group with messages takes over
    static const int m_maxBlocksPerTurn = 4;

    RadioPagingPocsagTxStream(bool network, int baud, bool inverted);

    //! When a message is sent (network only): the first transmission waits m_delayMs from the moment it is queued,
    //! then it is sent m_repeat times more, m_intervalMs apart (from the batch it went in)
    struct Schedule
    {
        int m_repeat = 0;               //!< 0-10
        int64_t m_delayMs = 0;
        int64_t m_intervalMs = 10000;
    };

    static const int m_maxRepeat = 10;
    static const int64_t m_maxScheduleMs = 3600000;    //!< Longest delay and interval: an hour

    //!< Queue a message at a speed and polarity. False and an error if it can not be encoded, or a schedule with
    //!< repeats or a delay is given to a one-shot stream (a transmission ends with its last message)
    bool add(const RadioPagingPocsag::TxMessage& message, int baud, bool inverted, uint64_t id, std::string& error,
        const Schedule& schedule);
    bool add(const RadioPagingPocsag::TxMessage& message, int baud, bool inverted, uint64_t id, std::string& error);
    //!< Time of the next block, ms since the Unix epoch: kept by next() from begin(); a mixed stream sets it before
    //!< nextMixed()
    void setTime(int64_t nowMs) { m_nowMs = nowMs; }
    //!< Group of the idle batches when the network starts (network only)
    void setDefaultGroup(int baud, bool inverted);
    //!< Group being sent
    int groupBaud() const { return m_group.first; }
    bool groupInverted() const { return m_group.second; }

    // Mixing with FLEX (RadioPagingMixTxStream): POCSAG gets the time between FLEX frames, in
    // runs. No idle batches: when there is nothing to send FLEX frames fill the time.
    //!< A run starts: the next block begins with a preamble
    void startRun();
    //!< The next block of the run, at most maxTicks long: the preamble (when due) and the
    //!< batches of as many of the queued messages of the group as fit. A group with no more
    //!< messages, or whose turn is over (m_maxBlocksPerTurn) while another one waits, gives
    //!< way to the next one, with a new preamble. False if nothing fits
    //!< overhead: ticks the caller sends before the block for the speed of the group (the 750
    //!< bps pattern), counted in maxTicks
    bool nextMixed(RadioPagingTxBlock& block, int64_t maxTicks, const std::function<int64_t(int)>& overhead = nullptr);

    bool hasWork() const override;
    void begin(int64_t epochMs) override;
    bool next(RadioPagingTxBlock& block) override;
    size_t pending() const override;
    std::vector<uint64_t> clear() override;
    bool continuous() const override { return m_network; }

    //!< Preamble or batch bits as levels of the polarity
    static std::vector<float> levels(const std::vector<uint8_t>& bits, bool inverted);
    static std::vector<uint8_t> preambleBits();
    static std::vector<uint8_t> idleBatchBits();

private:
    typedef std::pair<int, bool> Group;     //!< Baud, inverted

    struct Item
    {
        RadioPagingPocsag::TxMessage m_message;
        uint64_t m_id;
        int64_t m_delayMs = 0;
        int64_t m_notBefore = -1;       //!< ms since the epoch it may be sent from, -1 until set (with m_delayMs)
        int m_repeatsLeft = 0;
        int64_t m_intervalMs = 0;
        bool m_repeat = false;          //!< A copy sent again: its id was reported with the first transmission
    };

    bool m_network;
    Group m_defaultGroup;                   //!< Network: group of the idle batches
    Group m_group;                          //!< Network: group being sent
    bool m_preambleDue;
    int m_idleBatches;
    int m_turnBlocks;                       //!< Message blocks sent in the turn of the group
    std::map<Group, std::deque<Item>> m_queues;
    std::deque<std::pair<Group, Item>> m_oneShot;   //!< One-shot: in arrival order
    bool m_inTransmission;
    int64_t m_nowMs = 0;
    int m_batchesSincePreamble = 0;         //!< Batches sent after the last preamble: the batch numbers of the messages

    bool nextGroup(Group& group) const;
    //!< The next block of the network: a preamble, messages or idle batches (there is always one)
    void nextNetwork(RadioPagingTxBlock& block);
    //!< The delay of the messages queued since the last block starts now
    void startDelays();
    //!< Items of the queue that may be sent now (their delay or interval is over); the delay of new ones starts now
    std::vector<size_t> dueItems(std::deque<Item>& queue);
    bool hasDue(const std::deque<Item>& queue) const;
    //!< Take the items sent from the queue (indexes ascending), queue their repeats; the ids of first transmissions,
    //!< and of the copies in repeatIds, in the block with where each went (the placements of the encoding, in the order
    //!< of sent, their batches counted from firstBatch)
    void takeSent(std::deque<Item>& queue, const std::vector<size_t>& sent, const RadioPagingPocsag::EncodeResult& result,
        int firstBatch, RadioPagingTxBlock& block);
};

// FLEX (ARIB RCR STD-43A): one network, in one mode and polarity (2.1.3 defines one mapping
// of the symbols). A pager is set for one polarity and decodes only the frames of it, so the
// other polarity is a second logical channel on the same carrier, shared frame by frame.
// One encoder per polarity holds its queue, placement and message numbers. A transmission starts with the emergency
// re-synchronization (3.2.1), at least a battery saving cycle of the collapse long and
// ending on a frame boundary of the time grid, then frames one after the other, each with
// the cycle and frame number of its time. A frame is in the network polarity; it goes to
// the other polarity when that one has a message due in the frame and the network has no
// mandatory content due in it. The re-synchronization is its own inverse (the cycle
// shifted by half), so it serves both polarities. Each frame carries the messages due in
// it (their capcode listens to it): priority addresses first, then the others, tone only
// last, oldest first; a message that does not fit waits for its next frame.
// The collapse sets the length of the re-synchronization: pagers with that collapse wake up
// once in 2^collapse frames and must see it.
//   One-shot: the transmission ends after the last queued message has been sent. The frames
//   are scheduled with collapse 0 (the pagers have just been re-synchronized and listen to
//   every frame), so a message goes in the first frame.
//   Network: frames without end, idle frames when nothing is queued. The collapse is also
//   the scheduling parameter of the network: a message waits for a frame its capcode
//   listens to.
class MODEMRADIOPAGING_API RadioPagingFlexTxStream : public RadioPagingTxStream
{
public:
    static const int m_shaping = 3200;      //!< STD-43A 2.1.9 baseband filter, every rate

    RadioPagingFlexTxStream(bool network, RadioPagingFlex::Mode mode, bool inverted, int collapse);

    //!< Queue a message in a polarity. False and an error if it is not valid
    bool add(const RadioPagingFlexEncoder::TxMessage& message, bool inverted, uint64_t id, std::string& error);
    //!< Mode of the frames from the next one (each frame announces its mode in sync 1)
    void setMode(RadioPagingFlex::Mode mode);
    RadioPagingFlex::Mode mode() const { return m_encoders[0].mode(); }
    //!< Roaming by simulcast system ID in every frame sent (see RadioPagingFlexEncoder::Roaming)
    void setRoaming(const RadioPagingFlexEncoder::Roaming& roaming);
    //!< Polarity of the network from the next transmission (begin())
    void setInverted(bool inverted) { m_nextInverted = inverted; }
    bool inverted() const { return m_inverted; }
    //!< The network has content that must go in the frame (fragments after the first of a message, the time
    //!< of frame 0 of cycle 0): the frame can not go to the other polarity
    bool mandatory(uint64_t frameIndex) const;
    //!< Date and time of the frames sent (see RadioPagingFlexEncoder::TimeService). False and an error if a
    //!< field is out of range
    bool setTimeService(const RadioPagingFlexEncoder::TimeService& time, std::string& error);
    //!< Message numbering of the network (see RadioPagingFlexEncoder::Numbering). False and an error if out of range
    bool setNumbering(const RadioPagingFlexEncoder::Numbering& numbering, std::string& error);
    //!< System Event Notification in each frame for frames (see RadioPagingFlexEncoder::notifySystemEvent), in both
    //!< polarities. Network only: false and an error in one-shot or out of range
    bool notifySystemEvent(unsigned events, int frames, std::string& error);
    void cancelSystemEvent();
    //!< Frames of the notification still to send in the polarity of the network, 0 when none; its event flags
    int systemEventFrames() const { return m_encoders[m_inverted ? 1 : 0].systemEventFrames(); }
    unsigned systemEvents() const { return m_encoders[m_inverted ? 1 : 0].systemEvents(); }
    //!< False and an error if out of range. Applies to the next re-synchronization
    bool setCollapse(int collapse, std::string& error);
    //!< Collapse of the frames (block information word, placement): network only, 0 for one-shot
    int scheduleCollapse() const { return m_collapse; }
    //!< The collapse of the frames apart from that of the re-synchronization (network only):
    //!< the system collapse of a mixed channel that sends some frames a minute
    bool setScheduleCollapse(int collapse, std::string& error);
    //!< Frames not transmitted (128 entries, non zero = reserved; frames 0-3 can not be): the
    //!< messages of capcodes that listen to none of the others are refused (STD-43A 7.1.2)
    bool setReservedFrames(const std::vector<uint8_t>& table, std::string& error);

    // For RadioPagingMixTxStream, which decides frame by frame
    //!< The next frame to build is this one (frames between were not sent). Not inside a frame
    void setFrame(uint64_t frame) { if (m_blocks.empty()) { m_frame = frame; } }
    //!< Blocks of a frame are still to be returned by next()
    bool frameInProgress() const { return !m_blocks.empty(); }
    //!< A message of either polarity is due in the frame
    bool due(uint64_t frameIndex) const;
    //!< Collapse the re-synchronization is long enough for
    int ersCollapse() const { return m_ersCollapse; }

    bool hasWork() const override;
    void begin(int64_t epochMs) override;
    bool next(RadioPagingTxBlock& block) override;
    size_t pending() const override;
    std::vector<uint64_t> clear() override;
    std::vector<uint64_t> cut() override;
    bool continuous() const override { return m_network; }

    //!< Frame index (cycle and frame from it, RadioPagingFlexScheduler) of a time: frames of
    //!< 1.875 s from the Unix epoch, which is on a cycle boundary
    static uint64_t frameIndex(int64_t epochMs) { return (uint64_t) (epochMs / RadioPagingFlex::m_frameMs); }
    //!< First frame index and re-synchronization bits of a transmission starting at a time
    static void plan(int64_t epochMs, int collapse, uint64_t& firstFrame, int& ersBits);
    static std::vector<float> levels(const RadioPagingFlex::SymbolBlock& block);
    uint64_t currentFrame() const { return m_frame; }

private:
    bool m_network;
    RadioPagingFlexEncoder m_encoders[2];       //!< Normal, inverted
    std::map<uint32_t, uint64_t> m_ids[2];      //!< Encoder id to caller id, until the last retransmission
    std::set<uint32_t> m_sent[2];               //!< Encoder ids sent once, waiting for retransmissions
    bool m_inverted;                            //!< Polarity of the network
    bool m_nextInverted;                        //!< Polarity of the next transmission
    int m_collapse;                             //!< Of the frames: network collapse, 0 for one-shot
    int m_ersCollapse;                          //!< Of the re-synchronization length
    uint64_t m_frame;                           //!< Index of the next frame
    int m_ersBits;                              //!< Re-synchronization still to send
    std::deque<RadioPagingTxBlock> m_blocks;    //!< Blocks of the frame being sent

    void buildFrame();
};

// GSC (Golay Sequential Code, Rep. ITU-R M.900-2), one-shot only: GSC has no network mode.
// The queued messages make transmissions of one polarity (the same for a whole transmission):
//   Auto               messages of the same preamble value in a batch (up to 32), a message
//                      alone after its own preamble
//   Single             one message per transmission
//   Batch              as Auto, a batch even for one message
//   NonBatterySaver    the queued messages of the polarity after one 1100 pattern, whatever
//                      their preamble values
// At most 32 messages (RadioPagingGsc::m_maxBatchMessages) go in a transmission in any mode;
// the others stay queued, in order, for the next one.
// A voice page is a transmission of its own, in any mode (Motorola GSC manual, Figure 6-1):
//   the page (preamble, start code, address, activation code), the alert time of 2 s (the
//   carrier unmodulated), the voice message (the samples of a sound file or of the microphone,
//   processed: RadioPagingVoiceProcessor, FM with the full scale at the deviation, no emphasis)
//   and the turn-off word (a comma and the activation code again). The voice is a block at the
//   rate of the samples (RadioPagingWav); the modulator smooths the held samples with a
//   Gaussian filter of a cutoff at half the sample rate.
// Normal polarity: a binary 1 is a positive deviation; the voice is inverted with it.
class MODEMRADIOPAGING_API RadioPagingGscTxStream : public RadioPagingTxStream
{
public:
    typedef std::shared_ptr<const RadioPagingWav::Audio> Voice;

    static const int m_alertMs = 2000;          //!< Alert time of a voice page, the carrier unmodulated

    explicit RadioPagingGscTxStream(RadioPagingGsc::Mode mode);

    //!< Queue a message in a polarity. False and an error if it can not be encoded. A voice
    //!< page needs the voice (the other types none)
    bool add(const RadioPagingGsc::TxMessage& message, bool inverted, uint64_t id, std::string& error,
        const Voice& voice = Voice());
    //!< Queue a live voice page: the page and the alert time, then the voice as it is pushed with
    //!< pushLive (the carrier unmodulated when none is waiting), until endLive or maxSeconds of
    //!< voice, then the turn-off word. The voice message of the live page is sent once it has ended
    bool addLive(const RadioPagingGsc::TxMessage& message, bool inverted, uint64_t id, std::string& error, int maxSeconds);
    //!< The alert time is over: the voice is taken. False for a page not in this phase
    bool liveOpen() const { return m_live == LiveVoice; }
    //!< A live page is being sent (any phase), and its id
    bool liveActive() const { return m_live != LiveNone; }
    uint64_t liveId() const { return m_liveId; }
    //!< Voice samples of the live page, at m_sampleRate, -1 to +1 (taken when the voice is open)
    void pushLive(const float *samples, size_t count);
    //!< The voice is over: the turn-off word follows what is pushed
    void endLive();
    void setMode(RadioPagingGsc::Mode mode) { m_mode = mode; }
    RadioPagingGsc::Mode mode() const { return m_mode; }

    bool hasWork() const override { return !m_queue.empty(); }
    void begin(int64_t epochMs) override;
    bool next(RadioPagingTxBlock& block) override;
    size_t pending() const override { return m_queue.size(); }
    std::vector<uint64_t> clear() override;
    std::vector<uint64_t> cut() override;
    bool continuous() const override { return false; }

    static const int m_baud = 600;      //!< Rep. ITU-R M.900-2: 600 bit/s

private:
    struct Item
    {
        RadioPagingGsc::TxMessage m_message;
        bool m_inverted;
        int m_preamble;                 //!< Preamble value index of the address
        uint64_t m_id;
        Voice m_voice;                  //!< Voice page only
        bool m_live = false;            //!< Live voice page: no voice yet
        int m_liveSeconds = 0;          //!< Longest voice of a live page
    };

    enum LivePhase {
        LiveNone,
        LiveAlert,                      //!< The page and the alert time are being sent
        LiveVoice,                      //!< The voice is taken as it is pushed
    };

    LivePhase m_live = LiveNone;
    uint64_t m_liveId = 0;
    bool m_liveInverted = false;
    bool m_liveEnding = false;          //!< endLive was called
    size_t m_liveLimit = 0;             //!< Samples of the longest voice
    size_t m_liveSent = 0;              //!< Voice samples sent
    std::deque<float> m_liveSamples;    //!< Voice waiting to be sent
    RadioPagingVoiceProcessor m_liveProcessor{RadioPagingWav::m_sampleRate};    //!< Voice of the live page
    RadioPagingTxBlock m_liveEnd;       //!< The turn-off word
    bool nextLive(RadioPagingTxBlock& block);

    RadioPagingGsc::Mode m_mode;
    std::deque<Item> m_queue;
    std::deque<RadioPagingTxBlock> m_blocks;    //!< Blocks of the transmission being sent
    std::vector<uint64_t> m_failed;     //!< Ids of messages taken from the queue that did not encode
    bool m_inTransmission;
};

// FLEX mixed with POCSAG (STD-43A Chapter 7), network only. FLEX frames stay on their time
// grid and POCSAG gets the time between them:
//   - a frame is FLEX when it is required: frames 0-3 of the cycle (roaming: never mixed,
//     Motorola 3.9.1), frames a FLEX message of either polarity is due in, and the minimum
//     activity (7.1.1: the first 2^collapse frames of each minute, of each 4 minutes on a
//     shared channel; 7.1.2: the first N frames of each minute, the system collapse 5, the
//     capcodes of the other frames refused);
//   - otherwise POCSAG, when it has messages: from a frame boundary, blocks of the
//     POCSAG network stream (RadioPagingPocsagTxStream) that end before the next required
//     frame. After FLEX the run starts with the 750 bps 1,0 pattern (7.2.1, 7.2.2), then the
//     preamble and the batches. When the next block does not fit, 1,0 at 1600 bps fills to
//     the frame boundary (7.1);
//   - with nothing to send, FLEX idle frames fill the time (Motorola 4.3.3.2); a frame not
//     transmitted (7.1.2) is filled with 1,0.
// Time is counted in ticks (RadioPagingTxBlock::m_tickRate), so that every rate is exact.
class MODEMRADIOPAGING_API RadioPagingMixTxStream : public RadioPagingTxStream
{
public:
    struct Config
    {
        RadioPagingFlex::Mode m_flexMode = RadioPagingFlex::Mode1600_2;
        bool m_flexInverted = false;        //!< Polarity of the FLEX network
        int m_collapse = 0;                 //!< 0-7: re-synchronization; 7.1.1: the schedule
        int m_framesPerMinute = 0;          //!< 0: plan of 7.1.1; N (1-32): plan of 7.1.2
        bool m_shared = false;              //!< Minute is 4 minutes (128 frames)
        double m_flexDeviation = 4800.0;
        double m_pocsagDeviation = 4500.0;
        int m_pocsagBaud = 1200;            //!< Default group
        bool m_pocsagInverted = false;
        int m_switchMs[3] = {400, 200, 100};    //!< 750 bps pattern before POCSAG: 512, 1200, 2400
        RadioPagingFlexEncoder::Roaming m_roaming;  //!< Frames 0-3 are FLEX whatever the plan; with roaming they carry the system ID
        RadioPagingFlexEncoder::TimeService m_time; //!< Date and time of the FLEX frames
        RadioPagingFlexEncoder::Numbering m_numbering;  //!< Message numbering of the FLEX network
    };

    static const int m_minuteFrames = 32;   //!< Frames of a minute
    static const int m_switchBaud = 750;    //!< STD-43A 7.2: the pattern that activates POCSAG
    static const int m_fillBaud = 1600;     //!< STD-43A 7.1: the filling pattern

    RadioPagingMixTxStream(const Config& config);

    //!< Apply a configuration; the plan and the polarity take effect from the next frames
    //!< and transmission. False and an error if the schedule is not valid
    bool setConfig(const Config& config, std::string& error);
    const Config& config() const { return m_config; }

    bool addFlex(const RadioPagingFlexEncoder::TxMessage& message, bool inverted, uint64_t id, std::string& error);
    bool addPocsag(const RadioPagingPocsag::TxMessage& message, int baud, bool inverted, uint64_t id, std::string& error,
        const RadioPagingPocsagTxStream::Schedule& schedule = RadioPagingPocsagTxStream::Schedule());

    bool hasWork() const override { return true; }
    void begin(int64_t epochMs) override;
    bool next(RadioPagingTxBlock& block) override;
    size_t pending() const override;
    std::vector<uint64_t> clear() override;
    std::vector<uint64_t> cut() override;
    bool continuous() const override { return true; }

    //!< Frame the next block belongs to or follows, and the time to its start
    uint64_t currentFrame() const { return m_frameIdx; }
    int64_t ticksToBoundary() const { return m_toBoundary; }
    //!< The frame is FLEX whatever POCSAG has to send
    bool required(uint64_t frameIndex) const;
    RadioPagingFlexTxStream& flex() { return m_flex; }
    const RadioPagingFlexTxStream& flex() const { return m_flex; }
    RadioPagingPocsagTxStream& pocsag() { return m_pocsag; }

    static const int64_t m_frameTicks = (int64_t) RadioPagingFlex::m_frameMs * RadioPagingTxBlock::m_tickRate / 1000;

private:
    Config m_config;
    RadioPagingFlexTxStream m_flex;
    RadioPagingPocsagTxStream m_pocsag;
    std::vector<uint8_t> m_reserved;            //!< Frames not transmitted (7.1.2)
    bool m_ersPending;
    bool m_flexFrame;                           //!< Blocks of a FLEX frame are being returned
    bool m_lastFlex;                            //!< The last block was FLEX or fill: POCSAG starts a run
    uint64_t m_frameIdx;                        //!< Frame starting at the next boundary
    int64_t m_toBoundary;                       //!< Ticks to that boundary, 0: now
    RadioPagingTxBlock m_pendingBlock;
    bool m_hasPending;

    void advance(int64_t ticks);
    int64_t availableTicks() const;
    int64_t switchTicks(int baud) const;
    RadioPagingTxBlock switchBlock(int baud, bool inverted) const;
    RadioPagingTxBlock fillBlock(int64_t ticks) const;
    bool reserved(uint64_t frameIndex) const;
    bool minimumActivity(uint32_t frame) const;
    int64_t longestGapTicks() const;
    bool nextPocsag(RadioPagingTxBlock& block);
    void nextFlex(RadioPagingTxBlock& block);
};

#endif // MODEMRADIOPAGING_RADIOPAGINGTXSTREAM_H
