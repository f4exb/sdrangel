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

// Self test of the transmitter streams: block structure, POCSAG carousel, FLEX
// re-synchronization and frame timing. Reception of the streams is tested through the
// modulator and the demodulator in the modulator plugin test.

#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <QtEndian>

#include "radiopagingtxstream.h"

namespace {

int passes = 0;
int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (ok) {
        passes++;
    } else {
        failures++;
    }
}

const int batchBits = 32 * 17;

RadioPagingPocsag::TxMessage pocsag(uint32_t address, const char *text)
{
    RadioPagingPocsag::TxMessage m;
    m.m_address = address;
    m.m_function = 3;
    m.m_type = RadioPagingPocsag::Alphanumeric;
    m.m_text = text;
    return m;
}

RadioPagingFlexEncoder::TxMessage flex(uint64_t capcode, const char *text)
{
    RadioPagingFlexEncoder::TxMessage m;
    m.m_capcode = capcode;
    m.m_type = RadioPagingFlex::Alphanumeric;
    m.m_text = text;
    return m;
}

// Sign of each level: the bits of a POCSAG block in normal polarity (1 = negative)
std::vector<uint8_t> pocsagBits(const RadioPagingTxBlock& block)
{
    std::vector<uint8_t> bits;
    for (float l : block.m_levels) {
        bits.push_back(l < 0.0f ? 1 : 0);
    }
    return bits;
}

uint32_t word(const std::vector<uint8_t>& bits, size_t at)
{
    uint32_t w = 0;
    for (size_t i = 0; i < 32; i++) {
        w = (w << 1) | bits[at + i];
    }
    return w;
}

// Repeat and send delay of the POCSAG network: the time of each block is the time of the one before it plus its length
void testPocsagSchedule()
{
    RadioPagingPocsagTxStream stream(true, 1200, false);
    RadioPagingTxBlock block;
    std::string error;
    const std::vector<float> idle = RadioPagingPocsagTxStream::levels(RadioPagingPocsagTxStream::idleBatchBits(), false);
    const std::vector<float> preamble = RadioPagingPocsagTxStream::levels(RadioPagingPocsagTxStream::preambleBits(), false);

    stream.begin(0);
    RadioPagingPocsagTxStream::Schedule schedule;
    schedule.m_repeat = 2;
    schedule.m_delayMs = 2000;
    schedule.m_intervalMs = 3000;
    bool added = stream.add(pocsag(1234, "repeated"), 1200, false, 7, error, schedule);
    RadioPagingPocsagTxStream::Schedule tooMany;
    tooMany.m_repeat = 11;
    bool refused = !stream.add(pocsag(1234, "x"), 1200, false, 8, error, tooMany);

    // Times (ms) of the message blocks and the ids they report
    std::vector<int64_t> times;
    std::vector<size_t> ids;
    std::vector<std::vector<uint64_t>> repeatIds;
    int64_t now = 0;

    for (int i = 0; (i < 200) && (now < 20000); i++)
    {
        stream.next(block);
        if ((block.m_levels != idle) && (block.m_levels != preamble))
        {
            times.push_back(now);
            ids.push_back(block.m_ids.size());
            repeatIds.push_back(block.m_repeatIds);
        }
        now += (int64_t) std::llround((double) block.m_levels.size() * 1000.0 / block.m_baud);
    }

    bool ok = added && refused && (times.size() == 3) && (times[0] >= 2000) && (times[1] - times[0] >= 3000)
        && (times[2] - times[1] >= 3000) && (times[1] - times[0] < 3000 + 500) && (ids[0] == 1) && (ids[1] == 0) && (ids[2] == 0)
        && repeatIds[0].empty() && (repeatIds[1] == std::vector<uint64_t>{7}) && (repeatIds[2] == std::vector<uint64_t>{7})
        && (stream.pending() == 0);
    std::string why;
    for (size_t i = 0; i < times.size(); i++) {
        why += " " + std::to_string(times[i]) + "/" + std::to_string(ids[i]) + "/" + std::to_string(repeatIds[i].size());
    }
    check(ok, "POCSAG network: first transmission after the send delay, repeats at the interval, the id reported once, "
        "the copies as repeats (" + why + ")");

    RadioPagingPocsagTxStream oneShot(false, 1200, false);
    check(!oneShot.add(pocsag(1234, "x"), 1200, false, 9, error, schedule), "POCSAG one-shot: repeat and send delay refused");
}

void testPocsagNetwork()
{
    RadioPagingPocsagTxStream stream(true, 1200, false);
    RadioPagingTxBlock block;
    std::string error;

    check(stream.hasWork() && stream.continuous(), "POCSAG network: always has work, continuous");
    stream.begin(0);

    // Preamble, then idle batches
    bool ok = stream.next(block);
    check(ok && (block.m_baud == 1200) && (block.m_levels.size() == (size_t) RadioPagingPocsag::m_preambleBits)
        && block.m_ids.empty(), "POCSAG network: starts with the preamble at the default group");

    ok = stream.next(block);
    std::vector<uint8_t> bits = pocsagBits(block);
    check(ok && (bits.size() == (size_t) batchBits) && (word(bits, 0) == RadioPagingPocsag::m_syncWord)
        && (word(bits, 32) == RadioPagingPocsag::m_idleWord) && (word(bits, 32 * 16) == RadioPagingPocsag::m_idleWord),
        "POCSAG network: idle batch (sync and 16 idle codewords) when nothing is queued");

    // A message of the current group goes in the next batches
    check(stream.add(pocsag(1234567, "NETWORK"), 1200, false, 7, error), "POCSAG network: message queued");
    ok = stream.next(block);
    bits = pocsagBits(block);
    check(ok && (block.m_ids.size() == 1) && (block.m_ids[0] == 7) && ((bits.size() % batchBits) == 0)
        && (word(bits, 0) == RadioPagingPocsag::m_syncWord) && (stream.pending() == 0),
        "POCSAG network: the message at the next batch, whole batches, no preamble");

    // A message of another group: two idle batches, then its preamble and its batches
    check(stream.add(pocsag(7654, "OTHER"), 512, true, 8, error), "POCSAG network: message of another group queued");
    RadioPagingTxBlock b1, b2, b3, b4;
    stream.next(b1);
    stream.next(b2);
    stream.next(b3);
    stream.next(b4);
    check((b1.m_baud == 1200) && (b1.m_levels.size() == (size_t) batchBits) && b1.m_ids.empty()
        && (b2.m_baud == 1200) && (b2.m_levels.size() == (size_t) batchBits) && b2.m_ids.empty(),
        "POCSAG network: two idle batches before switching group");
    check((b3.m_baud == 512) && (b3.m_levels.size() == (size_t) RadioPagingPocsag::m_preambleBits)
        && (b3.m_levels[0] == 1.0f), "POCSAG network: preamble of the next group (512 inverted: its first bit, 1, is positive)");
    std::vector<uint8_t> inv = pocsagBits(b4);
    for (auto& b : inv) {
        b ^= 1;     // Inverted polarity
    }
    check((b4.m_baud == 512) && (b4.m_ids.size() == 1) && (b4.m_ids[0] == 8) && (word(inv, 0) == RadioPagingPocsag::m_syncWord),
        "POCSAG network: its batches in its polarity");

    // Clear drops what is queued
    stream.add(pocsag(1, "A"), 2400, false, 9, error);
    std::vector<uint64_t> dropped = stream.clear();
    check((dropped.size() == 1) && (dropped[0] == 9) && (stream.pending() == 0), "POCSAG network: clear returns the dropped ids");
    check(!stream.add(pocsag(2007665, "IDLE"), 1200, false, 10, error) && !error.empty(),
        "POCSAG: address of the idle codeword refused");
}

// A group fed at every batch does not hold the others; with nothing queued the network goes
// back to its default group, a new default too
void testPocsagNetworkTurns()
{
    RadioPagingPocsagTxStream stream(true, 1200, false);
    RadioPagingTxBlock block;
    std::string error;
    uint64_t id = 100;

    stream.begin(0);
    stream.next(block);     // Preamble
    stream.add(pocsag(7654, "WAITING"), 512, false, 1, error);

    int blocks1200 = 0;
    bool otherSent = false;

    for (int i = 0; (i < 20) && !otherSent; i++)
    {
        stream.add(pocsag(1000 + i, "BUSY"), 1200, false, id++, error);     // A message at every batch
        stream.next(block);
        if ((block.m_baud == 1200) && !block.m_ids.empty()) {
            blocks1200++;
        }
        otherSent = (block.m_baud == 512) && !block.m_ids.empty() && (block.m_ids[0] == 1);
    }

    check(otherSent && (blocks1200 == RadioPagingPocsagTxStream::m_maxBlocksPerTurn),
        "POCSAG network: a busy group gives way after its turn of message blocks");

    // Nothing queued: back to the default group after the idle batches; then a new default
    stream.clear();
    bool backTo1200 = false;
    for (int i = 0; (i < 10) && !backTo1200; i++)
    {
        stream.next(block);
        backTo1200 = (block.m_baud == 1200) && (block.m_levels.size() == (size_t) RadioPagingPocsag::m_preambleBits);
    }
    stream.setDefaultGroup(2400, true);
    bool to2400 = false;
    for (int i = 0; (i < 10) && !to2400; i++)
    {
        stream.next(block);
        to2400 = (block.m_baud == 2400) && (block.m_levels.size() == (size_t) RadioPagingPocsag::m_preambleBits);
    }
    check(backTo1200 && to2400, "POCSAG network: idle, it returns to the default group, a changed default is taken while running");
}

void testPocsagOneShot()
{
    RadioPagingPocsagTxStream stream(false, 1200, false);
    RadioPagingTxBlock block;
    std::string error;

    check(!stream.hasWork() && !stream.continuous(), "POCSAG one-shot: no work when nothing is queued");
    stream.add(pocsag(100, "ONE"), 512, false, 1, error);
    stream.add(pocsag(200, "TWO"), 2400, true, 2, error);
    check(stream.hasWork() && (stream.pending() == 2), "POCSAG one-shot: work when messages are queued");

    stream.begin(0);
    bool a = stream.next(block);
    bool first = a && (block.m_baud == 512) && (block.m_ids.size() == 1) && (block.m_ids[0] == 1)
        && (block.m_levels.size() > (size_t) RadioPagingPocsag::m_preambleBits);
    bool end = !stream.next(block);
    stream.begin(0);
    bool b = stream.next(block);
    bool second = b && (block.m_baud == 2400) && (block.m_ids.size() == 1) && (block.m_ids[0] == 2);
    bool end2 = !stream.next(block);
    check(first && end && second && end2 && !stream.hasWork(),
        "POCSAG one-shot: one transmission per message, preamble included, each ends");
}

void testFlexPlan()
{
    uint64_t frame;
    int ers;

    // Collapse 0: at least 1.875 s, to the frame boundary after
    RadioPagingFlexTxStream::plan(1000, 0, frame, ers);
    check((frame == 2) && (ers == 4400), "FLEX plan: start at 1.000 s, collapse 0: first frame 2 (3.750 s), 4400 bits of re-synchronization");

    // On a boundary: exactly the minimum
    RadioPagingFlexTxStream::plan(3750, 0, frame, ers);
    check((frame == 3) && (ers == 3000), "FLEX plan: start on a frame boundary (3.750 s): one frame of re-synchronization, first frame 3");

    // Collapse 3: 8 frames
    RadioPagingFlexTxStream::plan(0, 3, frame, ers);
    check((frame == 8) && (ers == 24000), "FLEX plan: collapse 3: 8 frames (15 s) of re-synchronization");

    // The time grid: frame and cycle of an hour (cycle = minute / 4)
    RadioPagingFlexScheduler::FrameTime t = RadioPagingFlexScheduler::frameTime(RadioPagingFlexTxStream::frameIndex(
        (int64_t) (3600 + 4 * 60 + 3) * 1000));
    check((t.m_cycle == 1) && (t.m_frame == 1), "FLEX time grid: 1 h 04 min 03 s is cycle 1 frame 1");
}

void testFlexErs()
{
    RadioPagingFlex::SymbolBlock ers = RadioPagingFlex::ers(200, false);
    // BS, Ar, inverted BS, inverted Ar (Table 3.2.1-1), transmission order
    const char *cycle = "1010101010101010" "11001011001000000101100100111001" "0101010101010101" "00110100110111111010011011000110";
    bool ok = (ers.m_baud == 1600) && (ers.m_levels == 2) && (ers.m_symbols.size() == 200);

    for (int i = 0; ok && (i < 200); i++) {
        ok = (ers.m_symbols[i] == (cycle[i % 96] - '0'));
    }
    check(ok, "FLEX re-synchronization: BS, Ar, inverted BS, inverted Ar repeated (STD-43A Table 3.2.1-1)");
    check(RadioPagingFlex::ersMinBits(7) == 3000 * 128, "FLEX re-synchronization: collapse 7 is 128 frames");
}

double seconds(const RadioPagingTxBlock& block)
{
    return (double) block.m_levels.size() / block.m_baud;
}

void testFlexOneShot()
{
    RadioPagingFlexTxStream stream(false, RadioPagingFlex::Mode1600_2, false, 0);
    RadioPagingTxBlock block;
    std::string error;

    check(!stream.hasWork(), "FLEX one-shot: no work when nothing is queued");
    check(stream.add(flex(123456, "FLEX ONE SHOT"), false, 5, error), "FLEX one-shot: message queued");
    check(!stream.add(flex(0, "BAD"), false, 6, error) && !error.empty(), "FLEX: capcode 0 refused");

    stream.begin(1000);
    bool ok = stream.next(block);
    check(ok && (block.m_baud == 1600) && (block.m_levels.size() == 4400) && block.m_ids.empty(),
        "FLEX one-shot: starts with the re-synchronization to the frame boundary");

    // Frames until the message is sent, each 1.875 s, then the end
    double frameSeconds = 0.0;
    int frames = 0;
    std::vector<uint64_t> sent;
    bool durationsOk = true;

    while (stream.next(block) && (frames < 10))
    {
        frameSeconds += seconds(block);
        sent.insert(sent.end(), block.m_ids.begin(), block.m_ids.end());

        if (std::fabs(frameSeconds - 1.875) < 1e-9)
        {
            frames++;
            frameSeconds = 0.0;
        }
        else if (frameSeconds > 1.875 + 1e-9)
        {
            durationsOk = false;
            break;
        }
    }

    check(durationsOk && (frames == 1) && (frameSeconds == 0.0), "FLEX one-shot: frames of exactly 1.875 s; collapse 0: one frame");
    check((sent.size() == 1) && (sent[0] == 5) && !stream.hasWork(), "FLEX one-shot: the message sent, then the transmission ends");

    // One-shot collapse 3: the re-synchronization reaches pagers of collapse 3 (8 frames),
    // the frames are scheduled with collapse 0: the message is in the first frame
    RadioPagingFlexTxStream collapsed(false, RadioPagingFlex::Mode1600_2, false, 3);
    uint64_t firstFrame;
    int ersBits;
    RadioPagingFlexTxStream::plan(1000, 3, firstFrame, ersBits);
    collapsed.add(flex(123457, "FLEX ONE SHOT"), false, 7, error);
    collapsed.begin(1000);
    bool ers = collapsed.next(block) && block.m_ers && ((int) block.m_levels.size() == ersBits)
        && (ersBits >= RadioPagingFlex::ersMinBits(3));
    double firstSeconds = 0.0;
    bool inFirst = false;

    while (collapsed.next(block) && (firstSeconds < 1.875 - 1e-9))
    {
        firstSeconds += seconds(block);
        inFirst = inFirst || (!block.m_ers && !block.m_ids.empty() && (block.m_ids[0] == 7));
    }

    check((collapsed.scheduleCollapse() == 0) && (collapsed.ersCollapse() == 3) && ers && inFirst,
        "FLEX one-shot collapse 3: re-synchronization of 8 frames, flagged; message in the first frame (collapse 0)");

    // One-shot at every speed: long messages (248 characters fill a frame, 500 are fragments) are all sent, and the
    // transmission ends (found with a radio round trip: at 3200 and 6400 they stayed queued)
    for (RadioPagingFlex::Mode mode : {RadioPagingFlex::Mode1600_2, RadioPagingFlex::Mode3200_2, RadioPagingFlex::Mode3200_4, RadioPagingFlex::Mode6400_4})
    {
        RadioPagingFlexTxStream shot(false, mode, false, 0);
        const std::string t248(248, 'A');
        const std::string t249(249, 'B');
        const std::string t500(500, 'C');
        RadioPagingFlexEncoder::TxMessage m248 = flex(1010020, t248.c_str());
        RadioPagingFlexEncoder::TxMessage m249 = flex(1010021, t249.c_str());
        RadioPagingFlexEncoder::TxMessage m500 = flex(1010022, t500.c_str());
        bool queued = shot.add(m248, false, 20, error) && shot.add(m249, false, 21, error) && shot.add(m500, false, 22, error)
            && shot.add(flex(1010031, "after them"), false, 31, error);
        std::vector<uint64_t> ids;
        int blocks = 0;

        shot.begin(1000);

        while (shot.next(block) && (blocks < 2000))
        {
            ids.insert(ids.end(), block.m_ids.begin(), block.m_ids.end());
            blocks++;
        }

        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        check(queued && (ids == std::vector<uint64_t>({20, 21, 22, 31})) && !shot.hasWork(),
            (std::string("FLEX one-shot ") + RadioPagingFlex::modeName(mode) + ": long and fragmented messages sent, then the end").c_str());
    }

    // Network collapse 2: both
    RadioPagingFlexTxStream network(true, RadioPagingFlex::Mode1600_2, false, 2);
    check((network.scheduleCollapse() == 2) && (network.ersCollapse() == 2), "FLEX network: the collapse schedules the frames and sets the re-synchronization");
}

void testFlexNetwork()
{
    RadioPagingFlexTxStream stream(true, RadioPagingFlex::Mode3200_4, false, 0);
    RadioPagingTxBlock block;
    std::string error;

    stream.begin(0);
    stream.next(block);     // Re-synchronization
    uint64_t first = stream.currentFrame();

    // Idle frames without end
    double total = 0.0;
    for (int i = 0; i < 6; i++)
    {
        stream.next(block);
        total += seconds(block);
    }
    check((std::fabs(total - 3 * 1.875) < 1e-9) && (stream.currentFrame() == first + 3),
        "FLEX network: idle frames one after the other, numbered by their time");

    // A message of the network: the frame in the mode of the network
    stream.add(flex(123456, "FAST"), false, 11, error);
    RadioPagingTxBlock sync, data;
    stream.next(sync);
    stream.next(data);
    check((sync.m_baud == 1600) && (data.m_baud == 1600) && (data.m_ids.size() == 1) && (data.m_ids[0] == 11)
        && (std::fabs(seconds(sync) + seconds(data) - 1.875) < 1e-9),
        "FLEX network: the frame in the mode of the network (3200/4 is 1600 baud 4 levels), 1.875 s");
    bool fourLevels = false;
    for (float l : data.m_levels) {
        fourLevels = fourLevels || (std::fabs(std::fabs(l) - 1.0f / 3.0f) < 1e-6);
    }
    check(fourLevels, "FLEX network: 4-level data part");

    // Collapse 2: a capcode listens to one frame in 4; the message waits for it
    RadioPagingFlexTxStream c2(true, RadioPagingFlex::Mode1600_2, false, 2);
    c2.begin(0);
    c2.next(block);
    uint64_t capcode = 16 * 3 + 1;      // Frame 3 of each cycle, 3 mod 4
    c2.add(flex(capcode, "WAIT"), false, 12, error);
    uint64_t sentIn = 0;
    for (int i = 0; (i < 20) && (sentIn == 0); i++)
    {
        uint64_t frameIndex = c2.currentFrame();
        RadioPagingTxBlock s, d;
        c2.next(s);
        c2.next(d);
        if (!d.m_ids.empty()) {
            sentIn = frameIndex;
        }
    }
    check((sentIn != 0) && ((sentIn % 128) % 4 == 3), "FLEX network collapse 2: the message in a frame its capcode listens to");

    // The other polarity: the frame its message is due in goes to it (the network has nothing
    // mandatory), the other frames stay in the network polarity
    RadioPagingFlexTxStream mixed(true, RadioPagingFlex::Mode1600_2, false, 2);
    mixed.begin(0);
    mixed.next(block);
    mixed.add(flex(capcode, "INVERTED"), true, 30, error);
    mixed.add(flex(16 * 1 + 1, "NORMAL"), false, 31, error);    // Frame 1 mod 4
    uint64_t invertedIn = 0, normalIn = 0;
    int invertedFrames = 0;
    for (int i = 0; (i < 8) && ((invertedIn == 0) || (normalIn == 0)); i++)
    {
        uint64_t frameIndex = mixed.currentFrame();
        RadioPagingTxBlock s, d;
        mixed.next(s);
        mixed.next(d);
        // Sync 1 of the frame: its bit sync starts 1,0 in normal polarity (a positive level first)
        bool inverted = s.m_levels[0] < 0.0f;
        invertedFrames += inverted ? 1 : 0;
        if (!d.m_ids.empty() && (d.m_ids[0] == 30) && inverted) {
            invertedIn = frameIndex;
        }
        if (!d.m_ids.empty() && (d.m_ids[0] == 31) && !inverted) {
            normalIn = frameIndex;
        }
    }
    check((invertedIn % 4 == 3) && (normalIn % 4 == 1) && (invertedFrames == 1) && !mixed.inverted(),
        "FLEX network: a frame goes to the other polarity for its message, the others stay in the network polarity");

    // Retransmission: the id with the first transmission, the copies as repeats, the interval in frames;
    // the send delay from the first frame built after the message is queued
    RadioPagingFlexTxStream repeating(true, RadioPagingFlex::Mode1600_2, false, 0);
    repeating.begin(0);
    repeating.next(block);
    RadioPagingFlexEncoder::TxMessage copied = flex(123456, "COPIES");
    copied.m_retransmit = 2;
    copied.m_retransmitInterval = 3;
    copied.m_sendDelay = 2;
    bool queued = repeating.add(copied, false, 40, error);
    std::vector<uint64_t> firstFrames, copyFrames;
    for (int i = 0; i < 16; i++)
    {
        uint64_t frameIndex = repeating.currentFrame();
        RadioPagingTxBlock s, d;
        repeating.next(s);
        repeating.next(d);
        if (d.m_ids == std::vector<uint64_t>{40}) {
            firstFrames.push_back(frameIndex);
        }
        if (d.m_repeatIds == std::vector<uint64_t>{40}) {
            copyFrames.push_back(frameIndex);
        }
    }
    uint64_t start = (firstFrames.size() == 1) ? firstFrames[0] : 0;
    check(queued && (firstFrames.size() == 1) && (copyFrames.size() == 2) && (copyFrames[0] == start + 3)
        && (copyFrames[1] == start + 6) && (repeating.pending() == 0),
        "FLEX network retransmission: the id once, two copies 3 frames apart");
}

// A transmission cut in a frame: the ids of the frame are returned, not lost
void testFlexCut()
{
    RadioPagingFlexTxStream stream(false, RadioPagingFlex::Mode1600_2, false, 0);
    RadioPagingTxBlock block;
    std::string error;

    stream.add(flex(123456, "CUT"), false, 20, error);
    stream.begin(0);
    stream.next(block);     // Re-synchronization
    stream.next(block);     // Sync part of the frame: the message is out of the queue
    std::vector<uint64_t> cut = stream.cut();
    check((cut.size() == 1) && (cut[0] == 20) && (stream.pending() == 0) && stream.clear().empty() && !stream.next(block),
        "FLEX cut in a frame: its message id returned once, nothing left");
}

} // namespace

// ---------------------------------------------------------------------------
// FLEX mixed with POCSAG
// ---------------------------------------------------------------------------

struct Step
{
    RadioPagingTxBlock m_block;
    int64_t m_start;        //!< Ticks since the first frame, after the re-synchronization
};

// The blocks after the re-synchronization, with their start times; stops after count blocks
std::vector<Step> mixRun(RadioPagingMixTxStream& stream, int count)
{
    std::vector<Step> steps;
    RadioPagingTxBlock block;
    int64_t t = 0;

    for (int i = 0; i < count; i++)
    {
        stream.next(block);
        Step s;
        s.m_block = block;
        s.m_start = t;
        t += block.ticks();
        steps.push_back(s);
    }

    return steps;
}

RadioPagingMixTxStream::Config mixConfig()
{
    RadioPagingMixTxStream::Config c;
    c.m_flexMode = RadioPagingFlex::Mode1600_2;
    c.m_collapse = 0;
    c.m_pocsagBaud = 1200;
    return c;
}

void testMixed()
{
    const int64_t F = RadioPagingMixTxStream::m_frameTicks;
    std::string error;
    RadioPagingTxBlock block;

    // Nothing queued: FLEX idle frames, one after the other
    {
        RadioPagingMixTxStream stream(mixConfig());
        stream.begin(0);
        stream.next(block);     // Re-synchronization
        check(block.m_ers && (block.m_deviation == 4800.0), "Mixed: starts with the re-synchronization, FLEX deviation");
        std::vector<Step> steps = mixRun(stream, 12);
        int64_t total = 0;
        bool flexOnly = true;
        for (const Step& s : steps) {
            total += s.m_block.ticks();
            flexOnly = flexOnly && (s.m_block.m_deviation == 4800.0) && (s.m_block.m_baud == 1600);
        }
        check(flexOnly && (total == 6 * F), "Mixed: nothing queued, FLEX idle frames fill the time, 1.875 s each");
    }

    // POCSAG in the gap after the mandatory frames 1-3: switch pattern, preamble and batches, then
    // 1,0 up to the frame boundary; every FLEX frame stays on its grid
    {
        RadioPagingMixTxStream stream(mixConfig());
        stream.begin(0);
        stream.next(block);
        uint64_t first = stream.currentFrame();     // Frame 1 of the cycle
        check(stream.addPocsag(pocsag(1234567, "MIXED"), 1200, false, 50, error), "Mixed: POCSAG message queued");
        std::vector<Step> steps = mixRun(stream, 14);

        int sw = -1, data = -1, fill = -1;
        for (size_t i = 0; i < steps.size(); i++)
        {
            const RadioPagingTxBlock& b = steps[i].m_block;
            if ((b.m_baud == 750.0) && (sw < 0)) {
                sw = (int) i;
            } else if (!b.m_ids.empty() && (data < 0)) {
                data = (int) i;
            } else if ((data >= 0) && (fill < 0) && (b.m_levels.size() < 3000) && (b.m_baud > 1500.0) && (b.m_baud < 1700.0)
                       && (b.m_baud != 1600.0)) {
                fill = (int) i;
            }
        }

        bool ok = (sw >= 0) && (data == sw + 1) && (fill == data + 1);
        check(ok, "Mixed: switch pattern, POCSAG block, 1,0 fill");

        if (ok)
        {
            const RadioPagingTxBlock& s = steps[sw].m_block;
            const RadioPagingTxBlock& d = steps[data].m_block;
            const RadioPagingTxBlock& f = steps[fill].m_block;
            // 200 ms at 750 bps for 1200, POCSAG parameters, the POCSAG start frame is after frames 1-3
            check((s.m_levels.size() == 150) && (s.m_deviation == 4500.0) && (d.m_baud == 1200.0) && (d.m_deviation == 4500.0)
                && (d.m_ids[0] == 50) && (d.m_shaping == 1200.0) && (f.m_deviation == 4800.0),
                "Mixed: 200 ms switch pattern for 1200, POCSAG deviation and shaping, FLEX deviation on the fill");
            int64_t run = s.ticks() + d.ticks() + f.ticks();
            int64_t start = steps[sw].m_start;
            check((start % F == 0) && (run == F) && (start / F == 3), "Mixed: the POCSAG run starts at frame 4 and ends on the frame grid");
        }

        // Every FLEX frame starts on the grid: after the POCSAG run the blocks add up to whole frames
        int64_t total = 0;
        for (const Step& st : steps) {
            total += st.m_block.ticks();
        }
        for (int i = 0; (i < 4) && ((total % F) != 0); i++)
        {
            stream.next(block);
            total += block.ticks();
        }
        check(((total % F) == 0) && (stream.currentFrame() == first + (uint64_t) (total / F)) && (stream.ticksToBoundary() == 0),
            "Mixed: all blocks add up to whole frames, the frame counter follows");
    }

    // Two groups in one gap: one switch pattern, the second group has its preamble only
    {
        RadioPagingMixTxStream stream(mixConfig());
        stream.begin(0);
        stream.next(block);
        stream.addPocsag(pocsag(100, "ONE"), 1200, false, 60, error);
        stream.addPocsag(pocsag(200, "TWO"), 512, true, 61, error);
        stream.addPocsag(pocsag(300, "THREE"), 2400, false, 62, error);
        std::vector<Step> steps = mixRun(stream, 30);
        int switches = 0, dataBlocks = 0;
        std::vector<uint64_t> order;
        for (const Step& s : steps)
        {
            switches += (s.m_block.m_baud == 750.0) ? 1 : 0;
            if (!s.m_block.m_ids.empty())
            {
                dataBlocks++;
                order.push_back(s.m_block.m_ids[0]);
            }
        }
        // Carousel: 1200n, 512n, 2400n, 1200i, 512i, 2400i: from 1200n the next with messages is 2400n
        check((switches == 1) && (dataBlocks == 3) && (order == std::vector<uint64_t>{60, 62, 61}),
            "Mixed: three groups (1200, 2400, 512 inverted) in one gap, one switch pattern, carousel order");
    }

    // 2400 and 512 switch patterns: 100 ms and 400 ms
    {
        RadioPagingMixTxStream::Config c = mixConfig();
        c.m_pocsagBaud = 2400;
        RadioPagingMixTxStream s2400(c);
        s2400.begin(0);
        s2400.next(block);
        s2400.addPocsag(pocsag(100, "FAST"), 2400, false, 70, error);
        std::vector<Step> a = mixRun(s2400, 14);
        size_t n2400 = 0;
        for (const Step& s : a) {
            n2400 = (s.m_block.m_baud == 750.0) ? s.m_block.m_levels.size() : n2400;
        }

        c.m_pocsagBaud = 512;
        RadioPagingMixTxStream s512(c);
        s512.begin(0);
        s512.next(block);
        s512.addPocsag(pocsag(100, "SLOW"), 512, false, 71, error);
        std::vector<Step> b = mixRun(s512, 14);
        size_t n512 = 0;
        for (const Step& s : b) {
            n512 = (s.m_block.m_baud == 750.0) ? s.m_block.m_levels.size() : n512;
        }
        check((n2400 == 75) && (n512 == 300), "Mixed: switch pattern of 100 ms for 2400 and 400 ms for 512");
    }

    // A FLEX message due: its frame is FLEX, the POCSAG message waits behind it
    {
        RadioPagingMixTxStream stream(mixConfig());
        stream.begin(0);
        stream.next(block);
        stream.addPocsag(pocsag(100, "LATE"), 1200, false, 80, error);
        stream.addFlex(flex(123456, "FIRST"), false, 81, error);
        std::vector<Step> steps = mixRun(stream, 20);
        int flexAt = -1, pocsagAt = -1;
        for (size_t i = 0; i < steps.size(); i++)
        {
            const RadioPagingTxBlock& b = steps[i].m_block;
            if (!b.m_ids.empty() && (b.m_ids[0] == 81) && (flexAt < 0)) {
                flexAt = (int) i;
            }
            if (!b.m_ids.empty() && (b.m_ids[0] == 80) && (pocsagAt < 0)) {
                pocsagAt = (int) i;
            }
        }
        // Frames 1-3 are FLEX anyway: the FLEX message goes in frame 1
        check((flexAt >= 0) && (pocsagAt > flexAt) && (steps[flexAt].m_start < 2 * F), "Mixed: the FLEX message in the first frame, POCSAG after the mandatory frames");
    }

    // 7.1.2 plan: 2 frames a minute, collapse 5; capcodes of the other frames refused; a frame not
    // transmitted is filled with 1,0, never with the re-synchronization
    {
        RadioPagingMixTxStream::Config c = mixConfig();
        c.m_framesPerMinute = 2;
        RadioPagingMixTxStream stream(c);
        stream.begin(0);
        stream.next(block);
        std::string e1, e2;
        bool refused = !stream.addFlex(flex(16 * 10 + 1, "NO"), false, 90, e1) && !e1.empty();
        bool accepted = stream.addFlex(flex(16 * 1 + 1, "YES"), false, 91, e2);
        check(refused && accepted && (stream.flex().scheduleCollapse() == 5), "Mixed 7.1.2: capcode of an untransmitted frame refused, system collapse 5");

        std::vector<Step> steps = mixRun(stream, 60);
        bool noErs = true;
        int fills = 0;
        for (const Step& s : steps)
        {
            noErs = noErs && !s.m_block.m_ers;
            fills += ((s.m_block.m_baud > 1599.0) && (s.m_block.m_baud < 1601.0) && (s.m_block.m_levels.size() == 3000) && (s.m_block.m_levels[0] > 0.0f)
                && (s.m_block.m_levels[1] < 0.0f)) ? 1 : 0;
        }
        check(noErs && (fills > 0), "Mixed 7.1.2: frames not transmitted are filled with 1,0 at 1600 bps");
    }

    // Time kept at 512 and 2400 bps (no residual dropped): after every POCSAG or fill block the
    // time the blocks took is the time the stream counts
    for (int baud : {512, 2400, 1200})
    {
        RadioPagingMixTxStream::Config c = mixConfig();
        c.m_pocsagBaud = baud;
        c.m_collapse = 2;
        RadioPagingMixTxStream stream(c);
        stream.begin(0);
        stream.next(block);
        uint64_t first = stream.currentFrame();
        int64_t total = 0;
        bool consistent = true;
        int checked = 0;

        for (int i = 0; i < 300; i++)
        {
            if ((i % 3) == 0)
            {
                std::string text(1 + (i * 37) % 200, 'A');
                stream.addPocsag(pocsag(1000 + i, text.c_str()), baud, false, 500 + i, error);
            }

            stream.next(block);
            total += block.ticks();
            bool pocsagBlock = (block.m_deviation == 4500.0);
            // A fill alternates all through (the bit sync of a FLEX frame is only the start of its sync part)
            bool fillBlock = (block.m_deviation == 4800.0) && (block.m_levels.size() >= 2);
            for (size_t k = 0; fillBlock && (k < block.m_levels.size()); k++) {
                fillBlock = ((k & 1) ? (block.m_levels[k] < 0.0f) : (block.m_levels[k] > 0.0f));
            }

            // After a pair (switch pattern and data) the time of the data is counted too
            if ((pocsagBlock && (block.m_baud != 750.0)) || fillBlock)
            {
                int64_t counted = (int64_t) (stream.currentFrame() - first) * F - stream.ticksToBoundary();
                consistent = consistent && (counted == total);
                checked++;
            }
        }

        char what[160];
        std::snprintf(what, sizeof(what), "Mixed %d bps: the time of the blocks is the time counted (%d POCSAG and fill blocks)", baud, checked);
        check(consistent && (checked > 20), what);
    }

    // A message longer than the longest gap, or a plan with no time for POCSAG, is refused
    {
        RadioPagingMixTxStream::Config c = mixConfig();
        c.m_collapse = 4;
        RadioPagingMixTxStream longGap(c);
        std::string text(1900, 'A');
        std::string e1, e2, e3;
        bool tooLong = !longGap.addPocsag(pocsag(100, text.c_str()), 512, false, 1, e1) && !e1.empty();
        bool fits = longGap.addPocsag(pocsag(100, "SHORT"), 512, false, 2, e2);

        c.m_collapse = 5;       // 32 frames a minute: nothing left
        RadioPagingMixTxStream noTime(c);
        bool none = !noTime.addPocsag(pocsag(100, "NO TIME"), 1200, false, 3, e3) && !e3.empty();
        check(tooLong && fits && none, "Mixed: a message longer than the longest gap, or a plan with no time for POCSAG, is refused");
    }

    // A group refilled at every run does not hold the others across the gaps
    {
        RadioPagingMixTxStream stream(mixConfig());
        stream.begin(0);
        stream.next(block);
        stream.addPocsag(pocsag(7654, "WAITING"), 512, false, 600, error);
        bool sent = false;

        for (int i = 0; (i < 120) && !sent; i++)
        {
            stream.addPocsag(pocsag(1000 + i, "BUSY"), 1200, false, 700 + i, error);
            stream.next(block);
            sent = !block.m_ids.empty() && (block.m_ids[0] == 600);
        }

        check(sent, "Mixed: a group refilled at every run gives way to the one waiting");
    }

    // 7.1.2: a capcode of the class 2 or 3 is refused too (frames 2 and 3 are sent, the pager would
    // see one frame in 4 minutes)
    {
        RadioPagingMixTxStream::Config c = mixConfig();
        c.m_framesPerMinute = 2;
        RadioPagingMixTxStream stream(c);
        std::string e1, e2;
        bool class2 = !stream.addFlex(flex(16 * 2 + 1, "NO"), false, 1, e1) && !e1.empty();
        bool class1 = stream.addFlex(flex(16 * 1 + 1, "YES"), false, 2, e2);
        check(class2 && class1, "Mixed 7.1.2: a capcode of the frames 2 or 3 of the minute is refused with 2 frames a minute");
    }

    // Clear behind the switch pattern: the message is returned, its time kept as fill
    {
        RadioPagingMixTxStream stream(mixConfig());
        stream.begin(0);
        stream.next(block);
        stream.addPocsag(pocsag(100, "CLEARED"), 1200, false, 99, error);
        int64_t total = 0;
        bool gotSwitch = false;

        for (int i = 0; (i < 12) && !gotSwitch; i++)
        {
            stream.next(block);
            total += block.ticks();
            gotSwitch = (block.m_baud == 750.0);
        }

        std::vector<uint64_t> cleared = stream.clear();
        stream.next(block);
        total += block.ticks();
        bool filled = block.m_ids.empty() && (block.m_deviation == 4800.0);
        stream.next(block);     // The fill to the boundary, or the FLEX frame
        total += block.ticks();
        check(gotSwitch && (cleared.size() == 1) && (cleared[0] == 99) && filled && (stream.pending() == 0),
            "Mixed: clear behind the switch pattern returns the message, its time is filled");
    }

    // Cut: the POCSAG block behind the switch pattern is returned, not lost
    {
        RadioPagingMixTxStream stream(mixConfig());
        stream.begin(0);
        stream.next(block);
        stream.addPocsag(pocsag(100, "CUT"), 1200, false, 95, error);
        bool gotSwitch = false;
        for (int i = 0; (i < 12) && !gotSwitch; i++)
        {
            stream.next(block);
            gotSwitch = (block.m_baud == 750.0);
        }
        std::vector<uint64_t> cut = stream.cut();
        check(gotSwitch && (cut.size() == 1) && (cut[0] == 95) && (stream.pending() == 0), "Mixed: cut after the switch pattern returns the POCSAG message");
    }
}

// ---------------------------------------------------------------------------
// GSC
// ---------------------------------------------------------------------------

RadioPagingGsc::TxMessage gsc(const std::string& address, int number, const char *text,
    RadioPagingGsc::MessageType type = RadioPagingGsc::Alphanumeric)
{
    RadioPagingGsc::TxMessage m;
    m.m_address = address;
    m.m_addressNumber = number;
    m.m_type = type;
    m.m_text = text;
    return m;
}

int gscPreamble(const std::string& address)
{
    RadioPagingGsc::EncodeOptions options;
    options.m_mode = RadioPagingGsc::Single;
    return RadioPagingGsc::encode({ gsc(address, 1, "") }, options).m_preamble;
}

// An address with the preamble value of the other one, or without it
std::string gscAddressLike(const std::string& other, bool same)
{
    int wanted = gscPreamble(other);

    for (int i = 0; i < 1000; i++)
    {
        char address[8];
        std::snprintf(address, sizeof(address), "%06d", 100000 + i * 7);
        int p = gscPreamble(address);

        if (p >= 0 && ((p == wanted) == same) && (address != other)) {
            return address;
        }
    }

    return std::string();
}

// Number of transmissions and the ids of each
std::vector<std::vector<uint64_t>> gscTransmissions(RadioPagingGscTxStream& stream, std::vector<float> *firstLevels = nullptr)
{
    std::vector<std::vector<uint64_t>> out;
    RadioPagingTxBlock block;

    while (stream.hasWork())
    {
        stream.begin(0);

        if (!stream.next(block)) {
            break;
        }

        if (firstLevels && out.empty()) {
            *firstLevels = block.m_levels;
        }

        out.push_back(block.m_ids);

        if (stream.next(block)) {
            break;      // A transmission is one block
        }
    }

    return out;
}

void testGsc()
{
    std::string error;
    std::string a = "123456";
    std::string same = gscAddressLike(a, true);
    std::string other = gscAddressLike(a, false);

    // Auto: the messages of one preamble value in a batch, the other one after
    {
        RadioPagingGscTxStream stream(RadioPagingGsc::Auto);
        check(stream.add(gsc(a, 1, "ONE"), false, 1, error) && stream.add(gsc(other, 2, "TWO"), false, 2, error)
            && stream.add(gsc(same, 3, "THREE"), false, 3, error), "GSC: messages queued");
        std::vector<std::vector<uint64_t>> t = gscTransmissions(stream);
        check((t.size() == 2) && (t[0] == std::vector<uint64_t>{1, 3}) && (t[1] == std::vector<uint64_t>{2}) && !stream.hasWork(),
            "GSC auto: the messages of the same preamble value in one batch, the other message after it");
    }

    // Single, Batch (one message in a batch), NonBatterySaver
    {
        RadioPagingGscTxStream single(RadioPagingGsc::Single);
        single.add(gsc(a, 1, "ONE"), false, 1, error);
        single.add(gsc(same, 1, "TWO"), false, 2, error);
        check(gscTransmissions(single).size() == 2, "GSC single: one message per transmission");

        RadioPagingGscTxStream nbs(RadioPagingGsc::NonBatterySaver);
        nbs.add(gsc(a, 1, "ONE"), false, 1, error);
        nbs.add(gsc(other, 1, "TWO"), false, 2, error);
        nbs.add(gsc(same, 1, "THREE"), true, 3, error);
        std::vector<std::vector<uint64_t>> t = gscTransmissions(nbs);
        check((t.size() == 2) && (t[0] == std::vector<uint64_t>{1, 2}) && (t[1] == std::vector<uint64_t>{3}),
            "GSC non battery saver: all messages of a polarity in one transmission, whatever their preamble value");

        RadioPagingGscTxStream one(RadioPagingGsc::Auto);
        RadioPagingGscTxStream batch(RadioPagingGsc::Batch);
        one.add(gsc(a, 1, "ONE"), false, 1, error);
        batch.add(gsc(a, 1, "ONE"), false, 1, error);
        std::vector<float> alone, inBatch;
        gscTransmissions(one, &alone);
        gscTransmissions(batch, &inBatch);
        check(alone.size() != inBatch.size(), "GSC batch: a batch even for one message (different from a single message)");
    }

    // Polarity: the levels flip, polarities are not mixed in a transmission
    {
        RadioPagingGscTxStream normal(RadioPagingGsc::Single);
        RadioPagingGscTxStream inverted(RadioPagingGsc::Single);
        normal.add(gsc(a, 1, "POLARITY"), false, 1, error);
        inverted.add(gsc(a, 1, "POLARITY"), true, 1, error);
        std::vector<float> n, i;
        gscTransmissions(normal, &n);
        gscTransmissions(inverted, &i);
        bool flipped = (n.size() == i.size()) && !n.empty();
        for (size_t k = 0; flipped && (k < n.size()); k++) {
            flipped = (n[k] == -i[k]) && (std::fabs(n[k]) == 1.0f);
        }

        RadioPagingGscTxStream mixed(RadioPagingGsc::Auto);
        mixed.add(gsc(a, 1, "ONE"), false, 1, error);
        mixed.add(gsc(same, 1, "TWO"), true, 2, error);
        check(flipped && (gscTransmissions(mixed).size() == 2), "GSC: inverted polarity flips the levels; polarities are not mixed in a transmission");
    }

    // At most 32 messages in a transmission; the others stay queued, in order, for the next
    {
        RadioPagingGscTxStream batch(RadioPagingGsc::Auto);
        RadioPagingGscTxStream nbs(RadioPagingGsc::NonBatterySaver);

        int queued = 0;

        for (int n = 0; (n < 2000) && (queued < 33); n++)
        {
            char address[8];
            std::snprintf(address, sizeof(address), "%06d", 100000 + n * 7);

            if (gscPreamble(address) < 0) {
                continue;       // Not an address
            }

            bool ok = batch.add(gsc(a, 1, "X"), false, 1000 + queued, error);      // One preamble value
            ok = nbs.add(gsc(address, 1, "X"), false, 2000 + queued, error) && ok;     // Any
            queued += ok ? 1 : 0;
        }

        std::vector<std::vector<uint64_t>> tb = gscTransmissions(batch);
        std::vector<std::vector<uint64_t>> tn = gscTransmissions(nbs);
        bool batchOk = (tb.size() == 2) && (tb[0].size() == 32) && (tb[1] == std::vector<uint64_t>{1032}) && (tb[0].front() == 1000) && (tb[0].back() == 1031);
        bool nbsOk = (tn.size() == 2) && (tn[0].size() == 32) && (tn[1] == std::vector<uint64_t>{2032});
        check(batchOk && nbsOk, "GSC: 33 messages go in 32 and 1, in order, none lost");
    }

    // The group is built from the first message queued; the others keep their order
    {
        RadioPagingGscTxStream stream(RadioPagingGsc::Auto);
        std::string otherB = gscAddressLike(other, true);
        stream.add(gsc(a, 1, "A"), false, 1, error);
        stream.add(gsc(other, 1, "B"), false, 2, error);
        stream.add(gsc(same, 1, "C"), false, 3, error);
        stream.add(gsc(otherB, 1, "D"), false, 4, error);
        std::vector<std::vector<uint64_t>> t = gscTransmissions(stream);
        check((t.size() == 2) && (t[0] == std::vector<uint64_t>{1, 3}) && (t[1] == std::vector<uint64_t>{2, 4}),
            "GSC auto: the first message queued sets the group, the others keep their order");
    }

    // Block parameters
    {
        RadioPagingGscTxStream stream(RadioPagingGsc::Single);
        RadioPagingTxBlock block;
        stream.add(gsc(a, 1, "BLOCK"), false, 7, error);
        stream.begin(0);
        bool ok = stream.next(block);
        check(ok && (block.m_baud == 600.0) && (block.m_shaping == 600.0) && (block.m_ids == std::vector<uint64_t>{7}) && !stream.next(block),
            "GSC: 600 baud, shaping at the baud rate, the ids of the transmission, then it ends");
    }

    // Refusals and clear
    {
        RadioPagingGscTxStream stream(RadioPagingGsc::Auto);
        std::string e1, e2, e3, e4;
        bool voice = !stream.add(gsc(a, 1, "", RadioPagingGsc::Voice), false, 1, e1) && !e1.empty()
            && !stream.add(gsc(a, 1, "X", RadioPagingGsc::Data), false, 1, e1);
        bool badAddress = !stream.add(gsc("12345", 1, "X"), false, 2, e2) && !e2.empty();
        bool badNumber = !stream.add(gsc(a, 5, "X"), false, 3, e3) && !e3.empty();
        std::string text(400, 'A');
        bool tooLong = !stream.add(gsc(a, 1, text.c_str()), false, 4, e4) && !e4.empty();
        stream.add(gsc(a, 1, "KEEP"), false, 5, error);
        stream.add(gsc(other, 1, "KEEP"), false, 6, error);
        std::vector<uint64_t> cleared = stream.clear();
        check(voice && badAddress && badNumber && tooLong && (cleared == std::vector<uint64_t>{5, 6}) && (stream.pending() == 0),
            "GSC: voice, bad address, address number and too long a text refused; clear returns the ids");
    }
}

// ---------------------------------------------------------------------------
// GSC voice pages: sound file and transmission
// ---------------------------------------------------------------------------

void put16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(v & 0xff); b.push_back(v >> 8); }
void put32(std::vector<uint8_t>& b, uint32_t v) { put16(b, v & 0xffff); put16(b, v >> 16); }

// A WAV file of a sine wave: format 1 (PCM) or 3 (float), bits, channels, rate, seconds. The right
// channel (if any) has the same sine
std::vector<uint8_t> makeWav(int format, int bits, int channels, int rate, double seconds, double hz = 1000.0, double amplitude = 0.5)
{
    std::vector<uint8_t> b;
    size_t frames = (size_t) (seconds * rate);
    size_t dataSize = frames * channels * (bits / 8);

    b.insert(b.end(), {'R', 'I', 'F', 'F'});
    put32(b, (uint32_t) (36 + dataSize));
    b.insert(b.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    put32(b, 16);
    put16(b, (uint16_t) format);
    put16(b, (uint16_t) channels);
    put32(b, (uint32_t) rate);
    put32(b, (uint32_t) (rate * channels * (bits / 8)));
    put16(b, (uint16_t) (channels * (bits / 8)));
    put16(b, (uint16_t) bits);
    b.insert(b.end(), {'d', 'a', 't', 'a'});
    put32(b, (uint32_t) dataSize);

    for (size_t i = 0; i < frames; i++)
    {
        double v = amplitude * std::sin(2.0 * M_PI * hz * i / rate);

        for (int c = 0; c < channels; c++)
        {
            if (format == 3)
            {
                uint8_t le[4];
                qToLittleEndian<float>((float) v, le);   // IEEE 754 binary32, little-endian (WAVE_FORMAT_IEEE_FLOAT)
                b.insert(b.end(), le, le + 4);
            }
            else if (bits == 8) {
                b.push_back((uint8_t) (128 + (int) std::lround(v * 127.0)));
            } else if (bits == 16) {
                put16(b, (uint16_t) (int16_t) std::lround(v * 32767.0));
            } else if (bits == 24)
            {
                int32_t x = (int32_t) std::lround(v * 8388607.0);
                b.push_back(x & 0xff);
                b.push_back((x >> 8) & 0xff);
                b.push_back((x >> 16) & 0xff);
            }
            else {
                put32(b, (uint32_t) (int32_t) std::lround(v * 2147483647.0));
            }
        }
    }

    return b;
}

double peak(const std::vector<float>& v, size_t from = 0)
{
    double p = 0.0;

    for (size_t i = from; i < v.size(); i++) {
        p = std::max(p, (double) std::fabs(v[i]));
    }

    return p;
}

int zeroCrossings(const std::vector<float>& v)
{
    // With hysteresis: the filter start-up and tail leave samples near zero
    int n = 0;
    int state = 0;

    for (size_t i = 0; i < v.size(); i++)
    {
        int s = (v[i] > 0.1f) ? 1 : ((v[i] < -0.1f) ? -1 : 0);

        if (s != 0)
        {
            n += ((state != 0) && (s != state)) ? 1 : 0;
            state = s;
        }
    }

    return n;
}

void testGscVoice()
{
    std::string error;

    // The rate of the modulator: the samples as they are, no gain, no filter
    {
        std::vector<uint8_t> file = makeWav(1, 16, 1, 38400, 0.5);
        RadioPagingWav::Audio audio;
        bool ok = RadioPagingWav::decode(file, audio, error, 50.0, false);
        bool exact = ok && (audio.m_samples.size() == 19200) && (audio.m_rate == 38400.0);

        for (size_t i = 0; exact && (i < audio.m_samples.size()); i++)
        {
            int16_t raw = (int16_t) (file[44 + 2 * i] | (file[45 + 2 * i] << 8));
            exact = (audio.m_samples[i] == (float) (raw / 32768.0));
        }

        check(exact, "WAV 38400 Hz 16 bit mono: the samples as they are");
    }

    // Resampled to 38400 Hz with the Interpolator: the length, the level and the pitch stay
    for (int rate : {8000, 22050, 44100, 48000})
    {
        std::vector<uint8_t> file = makeWav(1, 16, (rate == 48000) ? 2 : 1, rate, 1.0);
        RadioPagingWav::Audio audio;
        bool ok = RadioPagingWav::decode(file, audio, error, 50.0, false);
        double level = ok ? peak(audio.m_samples, 2000) : 0.0;
        int crossings = ok ? zeroCrossings(audio.m_samples) : 0;
        char what[160];

        std::snprintf(what, sizeof(what), "WAV %d Hz: 1 s, 1 kHz, 0.5 to 38400 Hz (%zu samples, peak %.3f, %d crossings)",
            rate, audio.m_samples.size(), level, crossings);
        check(ok && (std::abs((int) audio.m_samples.size() - 38400) <= 40) && (std::fabs(level - 0.5) < 0.03)
            && (std::abs(crossings - 2000) <= 40), what);
    }

    // Other formats; refusals
    {
        RadioPagingWav::Audio audio;
        bool formats = true;

        for (const auto& f : { makeWav(1, 8, 1, 16000, 0.2), makeWav(1, 24, 1, 16000, 0.2), makeWav(1, 32, 1, 16000, 0.2),
                makeWav(3, 32, 1, 16000, 0.2) })
        {
            formats = formats && RadioPagingWav::decode(f, audio, error, 50.0, false) && (std::fabs(peak(audio.m_samples, 500) - 0.5) < 0.03);
        }

        std::string e1, e2, e3;
        bool badFile = !RadioPagingWav::decode(std::vector<uint8_t>(100, 'X'), audio, e1, 50.0, false) && !e1.empty();
        bool fast = !RadioPagingWav::decode(makeWav(1, 16, 1, 96000, 0.1), audio, e2, 50.0, false) && !e2.empty();
        bool missing = !RadioPagingWav::load("/nonexistent/voice.wav", audio, e3, 50.0, false) && !e3.empty();
        check(formats && badFile && fast && missing, "WAV: 8, 24, 32 bit and float read; a file that is not WAV, above 76.8 kHz or missing refused");
    }

    // The voice length of the transmitter: refused, or cut at the limit (just cut)
    {
        std::vector<uint8_t> file = makeWav(1, 16, 1, 38400, 3.0);
        RadioPagingWav::Audio refused, cut;
        std::string e1;
        bool no = !RadioPagingWav::decode(file, refused, e1, 2.0, false) && !e1.empty();
        bool yes = RadioPagingWav::decode(file, cut, error, 2.0, true) && cut.m_trimmed && (cut.m_samples.size() == 76800)
            && (std::fabs(cut.m_fileSeconds - 3.0) < 1e-9);
        RadioPagingWav::Audio whole;
        bool fits = RadioPagingWav::decode(file, whole, error, 5.0, true) && !whole.m_trimmed && (whole.m_samples.size() == 115200);
        // Just cut: the first samples are the samples of the file
        bool same = yes;
        for (size_t i = 0; same && (i < cut.m_samples.size()); i++) {
            same = (cut.m_samples[i] == whole.m_samples[i]);
        }
        check(no && yes && fits && same, "WAV longer than the voice length: refused, or cut at the limit without changing the samples");
    }

    // A voice page: the page, the alert time and the voice, the turn-off word
    {
        std::vector<uint8_t> file = makeWav(1, 16, 1, 38400, 0.5);
        RadioPagingWav::Audio audio;
        RadioPagingWav::decode(file, audio, error, 50.0, false);
        RadioPagingGscTxStream::Voice voice = std::make_shared<const RadioPagingWav::Audio>(audio);

        for (bool inverted : {false, true})
        {
            RadioPagingGscTxStream stream(RadioPagingGsc::Auto);
            RadioPagingTxBlock page, sound, end, more;
            std::string e1;
            bool refused = !stream.add(gsc("123456", 1, "", RadioPagingGsc::Voice), inverted, 1, e1) && !e1.empty();
            bool added = stream.add(gsc("123456", 2, "", RadioPagingGsc::Voice), inverted, 5, error, voice);
            stream.begin(0);
            bool ok = stream.next(page) && stream.next(sound) && stream.next(end) && !stream.next(more);

            // The voice is processed (RadioPagingVoiceProcessor, the samples still in it at the end too) and
            // inverted with the polarity of the transmission
            RadioPagingVoiceProcessor processor(38400);
            std::vector<float> expected;
            for (size_t i = 0; i < audio.m_samples.size() + (size_t) processor.delay(); i++)
            {
                float s = processor.process((i < audio.m_samples.size()) ? audio.m_samples[i] : 0.0f);
                if (i >= (size_t) processor.delay()) {
                    expected.push_back(s);
                }
            }
            bool voiceOk = ok && (sound.m_baud == 38400.0) && (sound.m_levels.size() == 76800 + expected.size()) && sound.m_ids.empty();
            for (size_t i = 0; voiceOk && (i < 76800); i++) {
                voiceOk = (sound.m_levels[i] == 0.0f);
            }
            for (size_t i = 0; voiceOk && (i < expected.size()); i++) {
                voiceOk = (sound.m_levels[76800 + i] == (inverted ? -expected[i] : expected[i]));
            }

            // The page ends with the activation code; the turn-off word is the activation code after a comma
            std::vector<uint8_t> expectedEnd;
            RadioPagingGsc::appendActivationCode(expectedEnd);
            bool endOk = ok && (end.m_baud == 600.0) && (end.m_levels.size() == expectedEnd.size()) && (end.m_ids == std::vector<uint64_t>{5})
                && page.m_ids.empty();
            for (size_t i = 0; endOk && (i < expectedEnd.size()); i++) {
                endOk = (end.m_levels[i] > 0.0f) == ((expectedEnd[i] != 0) != inverted);
            }

            RadioPagingGsc::EncodeOptions options;
            options.m_mode = RadioPagingGsc::Single;
            RadioPagingGsc::EncodeResult expectedPage = RadioPagingGsc::encode({ gsc("123456", 2, "", RadioPagingGsc::Voice) }, options);
            bool pageOk = ok && (page.m_baud == 600.0) && (page.m_levels.size() == expectedPage.m_bits.size());
            for (size_t i = 0; pageOk && (i < expectedPage.m_bits.size()); i++) {
                pageOk = (page.m_levels[i] > 0.0f) == ((expectedPage.m_bits[i] != 0) != inverted);
            }

            char what[160];
            std::snprintf(what, sizeof(what), "GSC voice page %s: page, 2 s alert and the voice, turn-off word with the id; no voice file refused",
                inverted ? "inverted" : "normal");
            check(refused && added && voiceOk && endOk && pageOk, what);
        }
    }

    // A voice page is a transmission of its own, in any mode
    {
        std::vector<float> samples(3840, 0.25f);
        RadioPagingWav::Audio audio;
        audio.m_samples = samples;
        audio.m_rate = 38400.0;
        RadioPagingGscTxStream::Voice voice = std::make_shared<const RadioPagingWav::Audio>(audio);
        std::string error2;

        for (RadioPagingGsc::Mode mode : {RadioPagingGsc::Auto, RadioPagingGsc::Batch, RadioPagingGsc::NonBatterySaver})
        {
            RadioPagingGscTxStream stream(mode);
            stream.add(gsc("123456", 1, "BEFORE"), false, 1, error2);
            stream.add(gsc("123456", 1, "", RadioPagingGsc::Voice), false, 2, error2, voice);
            stream.add(gsc("123456", 2, "AFTER"), false, 3, error2);
            std::vector<std::vector<uint64_t>> t;
            RadioPagingTxBlock block;

            while (stream.hasWork())
            {
                stream.begin(0);
                std::vector<uint64_t> ids;

                while (stream.next(block)) {
                    ids.insert(ids.end(), block.m_ids.begin(), block.m_ids.end());
                }

                t.push_back(ids);
            }

            check((t.size() == 2) && (t[0] == std::vector<uint64_t>{1, 3}) && (t[1] == std::vector<uint64_t>{2}),
                "GSC: a voice page is a transmission of its own, the other pages are grouped around it");
        }
    }

    // A live voice page: the page, the alert time, then the voice pushed (silence when none waits), the turn-off word
    {
        RadioPagingGscTxStream stream(RadioPagingGsc::Auto);
        std::string error2;
        RadioPagingTxBlock block;
        bool added = stream.addLive(gsc("123456", 1, "", RadioPagingGsc::Voice), false, 77, error2, 2);
        stream.begin(0);
        bool page = stream.next(block) && (block.m_baud == 600) && block.m_ids.empty();
        bool alert = stream.next(block) && (block.m_levels.size() == 76800) && (block.m_levels[100] == 0.0f) && !stream.liveOpen();
        bool silent = stream.next(block) && stream.liveOpen() && (block.m_levels.size() == 384) && (block.m_levels[0] == 0.0f);
        std::vector<float> voice(2000, 0.5f);
        stream.pushLive(voice.data(), voice.size());
        size_t got = 0;
        bool voiced = true;
        RadioPagingVoiceProcessor processor(38400);

        // The voice pushed, processed
        while (got < voice.size())
        {
            voiced = stream.next(block) && voiced && (block.m_baud == 38400);
            for (size_t i = 0; voiced && (i < block.m_levels.size()); i++) {
                voiced = (block.m_levels[i] == processor.process(0.5f));
            }
            got += block.m_levels.size();
        }

        stream.endLive();
        bool end = stream.next(block) && (block.m_baud == 600) && (block.m_ids == std::vector<uint64_t>{77});
        bool over = !stream.next(block) && !stream.liveActive();
        check(added && page && alert && silent && voiced && (got == 2000) && end && over,
            "GSC live voice page: page, alert, silence then the voice pushed, turn-off word with the id");

        // The longest voice: the page ends by itself
        RadioPagingGscTxStream limited(RadioPagingGsc::Auto);
        limited.addLive(gsc("123456", 1, "", RadioPagingGsc::Voice), true, 78, error2, 1);
        limited.begin(0);
        limited.next(block);
        limited.next(block);
        size_t total = 0;
        bool ended = false;

        for (int i = 0; (i < 1000) && !ended; i++)
        {
            limited.next(block);
            ended = !block.m_ids.empty();
            total += ended ? 0 : block.m_levels.size();
        }

        check(ended && (total >= 38400) && (total < 38400 + 1000), "GSC live voice page: ends at the longest voice length");

        // Cut while the voice is open: the id is returned once
        RadioPagingGscTxStream cutting(RadioPagingGsc::Auto);
        cutting.addLive(gsc("123456", 1, "", RadioPagingGsc::Voice), false, 79, error2, 5);
        cutting.begin(0);
        cutting.next(block);
        cutting.next(block);
        cutting.next(block);
        check((cutting.cut() == std::vector<uint64_t>{79}) && !cutting.liveActive() && cutting.cut().empty(),
            "GSC live voice page: a cut returns its id once");
    }

    // Cut in a voice page: the id of the turn-off block is returned; clear leaves the transmission going
    {
        std::vector<float> samples(3840, 0.25f);
        RadioPagingWav::Audio audio;
        audio.m_samples = samples;
        audio.m_rate = 38400.0;
        RadioPagingGscTxStream::Voice voice = std::make_shared<const RadioPagingWav::Audio>(audio);
        RadioPagingGscTxStream stream(RadioPagingGsc::Auto);
        RadioPagingTxBlock block;
        std::string error2;

        stream.add(gsc("123456", 1, "", RadioPagingGsc::Voice), false, 7, error2, voice);
        stream.add(gsc("123456", 1, "WAITING"), false, 8, error2);
        stream.begin(0);
        stream.next(block);        // The page
        std::vector<uint64_t> cleared = stream.clear();
        bool goesOn = stream.next(block) && stream.next(block) && (block.m_ids == std::vector<uint64_t>{7});

        stream.begin(0);
        stream.next(block);        // A new transmission? the waiting page was cleared: none
        check((cleared == std::vector<uint64_t>{8}) && goesOn, "GSC voice page: clear drops the waiting pages, the voice page goes on to its turn-off word");

        RadioPagingGscTxStream cutStream(RadioPagingGsc::Auto);
        cutStream.add(gsc("123456", 1, "", RadioPagingGsc::Voice), false, 9, error2, voice);
        cutStream.begin(0);
        cutStream.next(block);
        std::vector<uint64_t> cut = cutStream.cut();
        check((cut == std::vector<uint64_t>{9}) && !cutStream.next(block), "GSC voice page: a cut transmission returns the id of the turn-off word not sent");
    }
}

// Where each message went, reported with the block that completes it (the facts of the record of the message)
void testSentFacts()
{
    std::string error;

    // POCSAG network: after the preamble and an idle batch, the address of frame 7 in batch 1, the next address (frame
    // 0) in the batch after
    {
        RadioPagingPocsagTxStream stream(true, 1200, false);
        RadioPagingTxBlock block;
        stream.begin(0);
        stream.next(block);     // Preamble
        stream.next(block);     // Idle batch
        stream.add(pocsag(1234567, "NETWORK"), 1200, false, 1, error);
        stream.add(pocsag(1000, "SECOND"), 1200, false, 2, error);
        stream.next(block);
        const bool both = (block.m_ids.size() == 2) && (block.m_status.size() == 2);
        // Frame 7 is the last 2 codewords of batch 1: its 3 message codewords run on into batch 2
        check(both && (block.m_status[0].m_batch == 1) && (block.m_status[0].m_frame == 7) && (block.m_status[0].m_codewords == 4)
            && (block.m_status[0].m_lastBatch == 2),
            "Sent facts: POCSAG address in frame 7 of batch 1 after the preamble, 3 message codewords to batch 2");
        // After an idle codeword frame 0 of batch 2 is gone: batch 3
        check(both && (block.m_status[1].m_batch == 3) && (block.m_status[1].m_frame == 0) && (block.m_status[1].m_lastBatch == 3),
            "Sent facts: POCSAG the next address, frame 0, in the batch after");
    }

    // FLEX one-shot: the cycle, frame and phase; a message in fragments the frame of its first fragment too
    {
        RadioPagingFlexTxStream stream(false, RadioPagingFlex::Mode1600_2, false, 0);
        RadioPagingTxBlock block;
        std::map<uint64_t, RadioPagingFlexEncoder::SentStatus> status;
        stream.add(flex(123456, "WHOLE"), false, 1, error);
        stream.add(flex(123460, std::string(600, 'X').c_str()), false, 2, error);
        stream.begin(0);

        while (stream.next(block))
        {
            for (size_t i = 0; (i < block.m_ids.size()) && (i < block.m_status.size()); i++) {
                status[block.m_ids[i]] = block.m_status[i];
            }
        }

        const RadioPagingFlexEncoder::SentStatus& whole = status[1];
        const RadioPagingFlexEncoder::SentStatus& parts = status[2];
        check((status.size() == 2) && (whole.m_cycle >= 0) && (whole.m_frame >= 0) && (whole.m_phase == 0)
            && (whole.m_fragments == 1) && (whole.m_firstFrame == -1),
            "Sent facts: FLEX cycle, frame and phase A (1600), no first fragment for a whole message");
        check((parts.m_fragments > 1) && (parts.m_firstFrame >= 0) && (parts.m_firstCycle >= 0)
            && ((parts.m_firstCycle * 128 + parts.m_firstFrame) < (parts.m_cycle * 128 + parts.m_frame)),
            "Sent facts: FLEX in fragments, its first fragment in an earlier frame than its last");
    }

    // GSC batch: the preamble, the batch mode, each address in its slot
    {
        const std::string a = "123456";
        const std::string same = gscAddressLike(a, true);
        RadioPagingGscTxStream stream(RadioPagingGsc::Batch);
        RadioPagingTxBlock block;
        std::vector<RadioPagingFlexEncoder::SentStatus> status;
        stream.add(gsc(a, 1, "ONE"), false, 1, error);
        stream.add(gsc(same, 1, "TWO"), false, 2, error);
        stream.begin(0);

        while (stream.next(block)) {
            status.insert(status.end(), block.m_status.begin(), block.m_status.end());
        }

        check((status.size() == 2) && (status[0].m_preamble == gscPreamble(a)) && (status[0].m_batch == 1)
            && (status[0].m_slot == 0) && (status[0].m_blocks > 1) && (status[1].m_slot == status[0].m_blocks),
            "Sent facts: GSC preamble, batch mode, the first address in slot 0, the second after the blocks of the first");
    }
}

int main()
{
    testSentFacts();
    testGscVoice();
    testGsc();
    testMixed();
    testPocsagNetwork();
    testPocsagSchedule();
    testPocsagNetworkTurns();
    testPocsagOneShot();
    testFlexPlan();
    testFlexErs();
    testFlexOneShot();
    testFlexNetwork();
    testFlexCut();

    std::printf("%d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
