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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <QDebug>

#include "util/profiler.h"
#include "dvbs2receiver.h"
#include "samplethread.h"

namespace dvbs2 {

double Receiver::now()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

double Receiver::rollOffValue(int ro)
{
    // 3 (RO 11) isn't asked for: it marks the low roll-offs, which alternate with it as 4 to 6
    static const double values[7] = {0.35, 0.25, 0.20, 0.35, 0.15, 0.10, 0.05};
    return (ro >= 0) && (ro < 7) ? values[ro] : 0.35;
}

Receiver::Receiver(bool threaded, int decoderThreads) :
    m_threaded(threaded),
    m_inputRate(2e6),
    m_symbolRate(1e6),
    m_rollOffSetting(0.0),
    m_isiSetting(-1),
    m_maxIterations(m_defaultMaxIterations),
    m_matypeRollOff(-1),
    m_lowRollOffs(false),
    m_sinceLowRollOffs(0),
    m_unlockedSymbols(0),
    m_failedFrames(0),
    m_failingSymbols(0),
    m_restarts(0),
    m_displayRequested(true),
    m_displayMaxCells(2000),
    m_displayPoints(512),
    m_busy(false),
    m_stop(false),
    m_draining(false),
    m_resetEpoch(0),
    m_decodeEpoch(0),
    m_isi(-1),
    m_isisSeen(256, false),
    m_isisNullOnly(256, false),
    m_streamPackets(0),
    m_isiPackets(0),
    m_isiNulls(0),
    m_isiMissing(0),
    m_queueWaits(0)
{
    const int cores = std::max(1, (int) std::thread::hardware_concurrency());
    const int threads = decoderThreads > 0 ? decoderThreads : std::max(1, std::min(4, cores - 2));
    m_pool.reset(new dvbt2::LDPCPool(threads - 1));
    // Whole rounds of the decoders (the pool's threads and the FEC thread's), a few batches at least
    m_takeBatches = (size_t) threads * std::max<size_t>(1, m_minTakeBatches / (size_t) threads);
    m_fe = &m_frontEnd;
    m_frontEndRestart = false;
    m_plLocked = false;
    m_frontEnd.configure(m_inputRate, m_symbolRate, 0.35);
    m_frontEnd.setTimingBandwidth(m_timingAcquire);
    m_bbDecoder.setHighEfficiencyAllowed(false);
    m_bbDecoder.setPacketCallback([this](const uint8_t *packet) {
        m_bbPackets.insert(m_bbPackets.end(), packet, packet + 188);
        m_streamPackets++;
        m_isiPackets++;
        m_isiNulls += (((packet[1] & 0x1f) << 8) | packet[2]) == 0x1fff;
    });
    m_pl.setFrameCallback([this](XFECFrame& frame) { onFrame(frame); });

    // As many as can be in use, so none's made as frames come (with the mutex the FEC thread wants): queued, taken to
    // decode, and resets. Their buffers grow as they're used
    const size_t nbSlots = m_maxQueued + m_takeBatches * m_maxLanes + 8;
    m_slots.reserve(2 * nbSlots);
    m_free.reserve(2 * nbSlots);

    for (size_t i = 0; i < nbSlots; i++)
    {
        m_slots.emplace_back(new FrameSlot());
        m_free.push_back(m_slots.back().get());
    }

    if (m_threaded)
    {
        m_fecThread = std::thread(&Receiver::runFEC, this);
        m_demodThread.reset(new SampleThread([this](const Cf *samples, int nbSamples) {
            demodulate(samples, nbSamples);
        }));
    }
}

Receiver::~Receiver()
{
    m_demodThread.reset();

    if (m_threaded)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }

        m_available.notify_all();
        m_space.notify_all();
        m_fecThread.join();
    }
}

void Receiver::configure(double inputRate, double symbolRate)
{
    if (m_demodThread) {
        m_demodThread->discard();
    }

    m_inputRate = inputRate;
    m_symbolRate = symbolRate;
    m_frontEnd.configure(inputRate, symbolRate, m_rollOffSetting > 0.0 ? (double) m_rollOffSetting : 0.35);
    reset();
}

void Receiver::setScramblingCode(int n)
{
    if (m_demodThread) {
        m_demodThread->discard();
    }

    m_pl.setScramblingCode(n);
    reset();
}

void Receiver::reset()
{
    if (m_demodThread) {
        m_demodThread->discard();
    }

    // A shared front end's reset by its owner
    if (m_fe == &m_frontEnd)
    {
        m_frontEnd.reset();
        m_frontEnd.setTimingBandwidth(m_timingAcquire);
    }

    m_frontEndRestart = false;
    m_plLocked = false;
    m_awaitCorrection = 0;
    m_awaitClock = 0;
    m_pendingCorrection = 0.0;
    m_pl.reset();
    m_unlockedSymbols = 0;
    m_failedFrames = 0;
    m_failingSymbols = 0;
    m_searchSeconds = 0.0;
    m_searchSymbols = 0;
    m_skipSymbols = 0;
    // Of the signal before (as after a retune): the matched filter's back to 0.35 until a BBHEADER. The low roll-offs'
    // tracking is the FEC thread's, reset where it reaches the reset (or below, unthreaded)
    m_matypeRollOff = -1;

    {
        std::lock_guard<std::mutex> lock(m_demodMutex);
        m_demodStatus = Status();
        m_demodStatus.m_symbolRate = m_symbolRate;
        m_displayCells.clear();
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    while (!m_queue.empty())
    {
        m_free.push_back(m_queue.front());
        m_queue.pop_front();
    }

    m_packets.clear();
    FrameSlot *slot = freeSlot();
    slot->m_reset = true;
    slot->m_epoch = ++m_resetEpoch;

    if (m_threaded)
    {
        m_queue.push_back(slot);
        m_space.notify_all();
        m_available.notify_one();
    }
    else
    {
        m_free.push_back(slot);
        m_bbDecoder.reset();
        clearStreams();
        m_lowRollOffs = false;
        m_sinceLowRollOffs = 0;
        m_fecStatus = Status();
        m_decodeEpoch = m_resetEpoch;
    }
}

void Receiver::discontinuity()
{
    // The search's budget skip goes on: cancelled at each drop (when behind, searching), the search ran at full
    // cost, so fell behind again, dropped again, and never kept to its budget
    m_pl.discontinuity();
}

// The spectrum measured again: a shared front end's by its owner, if the DVB-S receiver hasn't a lock it'd lose
void Receiver::restartFrontEnd()
{
    if (m_fe == &m_frontEnd)
    {
        m_frontEnd.reset();
        m_frontEnd.setTimingBandwidth(m_timingAcquire);
    }
    else
    {
        m_frontEndRestart = true;
    }
}

// With m_mutex
Receiver::FrameSlot *Receiver::freeSlot()
{
    if (m_free.empty())
    {
        m_slots.emplace_back(new FrameSlot());
        return m_slots.back().get();
    }

    FrameSlot *slot = m_free.back();
    m_free.pop_back();
    slot->m_reset = false;
    slot->m_skip = false;
    return slot;
}

void Receiver::process(const Cf *samples, int nbSamples)
{
    if (m_demodThread) {
        m_demodThread->push(samples, nbSamples);
    } else {
        demodulate(samples, nbSamples);
    }

    deliverPackets();
}

void Receiver::setSharedFrontEnd(dvbs::FrontEnd *frontEnd)
{
    m_fe = frontEnd ? frontEnd : &m_frontEnd;
    m_frontEndRestart = false;
}

void Receiver::demodulate(const Cf *samples, int nbSamples)
{
    const double start = now();
    m_symbols.clear();
    m_frontEnd.process(samples, nbSamples, m_symbols);
    const double mid = now();

    // Its own front end's load, for the search's budget
    if ((nbSamples > 0) && (m_inputRate > 0.0)) {
        m_otherLoad += 0.05 * ((mid - start) * m_inputRate / nbSamples - m_otherLoad);
    }

    demodulateSymbols(start, mid);
}

void Receiver::processSymbols(const Cf *symbols, int nbSymbols, uint64_t first, const dvbs::FrontEnd::Status& frontEnd,
    uint64_t corrections, uint64_t boundary, double otherLoad)
{
    const double start = now();
    m_feStatus = frontEnd;
    m_otherLoad = otherLoad;

    // A clock correction asked for, made before this block's symbols
    if ((m_awaitClock != 0) && (frontEnd.m_clockAdjustments >= m_awaitClock))
    {
        m_awaitClock = 0;
        m_pl.clockCorrected();
    }

    if (m_awaitCorrection == 0)
    {
        m_symbols.assign(symbols, symbols + nbSymbols);
        demodulateSymbols(start, start);
        return;
    }

    // A frequency correction asked of the front end: the symbols it made before making it (more than when it was in
    // this thread: those queued for this one), and those it held then, are at the frequency before, so they're turned
    // here as it now turns those after. Skipped, as many packets were lost as symbols were queued
    const int before = corrections >= m_awaitCorrection
        ? (int) std::min<uint64_t>((uint64_t) nbSymbols, boundary > first ? boundary - first : 0) : nbSymbols;

    if (before > 0)
    {
        m_symbols.assign(symbols, symbols + before);
        const double step = -2.0 * 3.141592653589793 * m_pendingCorrection;
        const Cf stepRot((float) std::cos(step), (float) std::sin(step));
        Cf rot((float) std::cos(m_pendingPhase), (float) std::sin(m_pendingPhase));

        for (Cf& s : m_symbols)
        {
            s *= rot;
            rot *= stepRot;
        }

        m_pendingPhase = std::remainder(m_pendingPhase + before * step, 2.0 * 3.141592653589793);
        demodulateSymbols(start, start);
    }

    // The front end's turning from here: its phase isn't this one's, so the symbols don't follow on
    if (before < nbSymbols)
    {
        m_awaitCorrection = 0;
        m_pendingCorrection = 0.0;
        m_pendingPhase = 0.0;
        m_pl.discontinuity();
        const double resumed = now();
        m_symbols.assign(symbols + before, symbols + nbSymbols);
        demodulateSymbols(resumed, resumed);
    }
}

void Receiver::demodulateSymbols(double start, double mid)
{
    // The matched filter's roll-off, for the samples from now on (shared, DVB-S's too: it's 0.35 but for a BBHEADER's)
    const double rollOff = m_rollOffSetting > 0.0 ? (double) m_rollOffSetting : (m_matypeRollOff >= 0 ? rollOffValue(m_matypeRollOff) : 0.35);

    if (shared()) {
        m_frontEndRollOff = rollOff;    // Set by its owner, in its thread
    } else {
        m_fe->setRollOff(rollOff);
    }

    const bool searching = m_pl.getStatus().m_state != PLDecoder::State::Locked;
    int first = 0;

    // Searching, at a high symbol rate, could take more than a core (a correlation at every symbol, and headers to
    // decode where noise peaks): then windows of symbols are searched, with those between skipped
    if (searching && (m_skipSymbols > 0))
    {
        first = (int) std::min<int64_t>(m_skipSymbols, (int64_t) m_symbols.size());
        m_skipSymbols -= first;

        if (m_skipSymbols == 0) {
            m_pl.discontinuity();
        }
    }
    else if (!searching)
    {
        // The search's time and symbols kept through brief locks: with gaps in the samples (the sample FIFO
        // overflowing), broken frames lock and lose lock again and again, and reset each time, the budget never
        // applied, so searching ran at full cost, slowing the receiver, so more was dropped
        m_skipSymbols = 0;
        m_lockedSymbols += (int64_t) m_symbols.size();

        if (m_lockedSymbols > m_steadyLockSeconds * m_symbolRate)
        {
            m_searchSeconds = 0.0;
            m_searchSymbols = 0;
        }
    }

    if (searching) {
        m_lockedSymbols = 0;
    }

    m_pl.process(m_symbols.data() + first, (int) m_symbols.size() - first);
    const uint64_t searched = searching ? m_symbols.size() - first : 0;

    if (searching && (first < (int) m_symbols.size()))
    {
        m_searchSeconds += now() - mid;
        m_searchSymbols += (int64_t) m_symbols.size() - first;

        if (m_searchSymbols >= m_searchWindow)
        {
            const double budget = std::max(m_minSearchBudget, std::min(m_searchBudget, m_threadBudget - m_otherLoad));
            const int64_t wanted = (int64_t) (m_searchSeconds * m_symbolRate / budget);
            m_skipSymbols = std::max<int64_t>(0, std::min<int64_t>(wanted - m_searchSymbols, (int64_t) (m_maxSkipSeconds * m_symbolRate)));
            m_searchSeconds = 0.0;
            m_searchSymbols = 0;
        }
    }

    // The symbol clock's error from VL-SNR frames' slips, to the front end's timing loop
    const double clock = m_pl.takeClockCorrection();

    if ((clock != 0.0) && !shared())
    {
        m_fe->adjustClock(clock);
        m_pl.clockCorrected();
    }
    else if (clock != 0.0)
    {
        // Made by the shared front end's owner between its blocks: in effect from the block whose status counts it
        m_clockRequested = m_clockRequested + clock;
        m_awaitClock = m_feStatus.m_clockAdjustments + 1;
    }

    // A large frequency offset goes to the front end's NCO
    const double correction = m_pl.takeFrequencyCorrection();

    if ((correction != 0.0) && !shared())
    {
        m_fe->adjustFrequency(correction * m_symbolRate);
    }
    else if (correction != 0.0)
    {
        // Asked of the shared front end's owner, made between its blocks: the symbols until then are skipped
        m_frequencyRequested = m_frequencyRequested + correction * m_symbolRate;
        m_awaitCorrection = ++m_frequencyRequests;
        m_pendingCorrection += correction;
    }

    const bool locked = m_pl.getStatus().m_state == PLDecoder::State::Locked;
    m_plLocked = locked;
    m_esn0 = m_pl.getStatus().m_esn0;

    // A shared one's owner decides these, from both receivers
    if (!shared())
    {
        m_fe->setTimingBandwidth(locked ? dvbs::FrontEnd::trackingBandwidth(m_timingTrack, m_esn0)
            : searchTimingBandwidth(m_fe->getStatus().m_spectralEsN0));
        m_fe->setEqualiserReference(locked, m_pl.getStatus().m_esn0);
    }

    // Without frames for a while, the spectrum's measured again, in case the carrier was found wrong (or there was no
    // signal then): not sooner than the search could lock, nor while a header VL-SNR's search found waits for its next
    // (a frame on: at low symbol rates, seconds)
    if (locked || frontEndStatus().m_estimating)
    {
        m_unlockedSymbols = 0;
    }
    else if (((m_unlockedSymbols += m_symbols.size()) > relockSeconds(m_symbolRate) * m_symbolRate)
        && !m_pl.vlsnrCandidatePending())
    {
        restartFrontEnd();
        m_pl.reset();
        m_unlockedSymbols = 0;
        m_restarts++;
    }

    // Locked, but no frame decoded for a while: a false lock (on data repeating from frame to frame, that looks like a
    // header), or a signal too weak, so nothing's lost by starting again, with the spectrum measured again
    if (locked && (m_failedFrames >= m_maxFailedFrames))
    {
        if ((m_failingSymbols += m_symbols.size()) > m_failingSeconds * m_symbolRate)
        {
            restartFrontEnd();
            m_pl.reset();
            m_failedFrames = 0;
            m_failingSymbols = 0;
            m_restarts++;
        }
    }
    else
    {
        m_failingSymbols = 0;
    }

    const double end = now();

    {
        std::lock_guard<std::mutex> lock(m_demodMutex);
        Status& s = m_demodStatus;
        s.m_frontEnd = frontEndStatus();
        s.m_pl = m_pl.getStatus();
        s.m_symbolRate = m_symbolRate;
        s.m_rollOff = rollOff;
        s.m_frequencyOffset = s.m_frontEnd.m_frequencyOffset + (s.m_pl.m_inverted ? -1.0 : 1.0) * s.m_pl.m_frequency * m_symbolRate;
        s.m_clockOffset = s.m_frontEnd.m_clockOffset;
        s.m_restarts = m_restarts;
        s.m_searchedSymbols += searched;
        s.m_skippedSymbols += first;
        s.m_seconds[0] += mid - start;
        s.m_seconds[1] += end - mid;

        if (m_displayRequested)
        {
            m_pl.getConstellation(m_displayCells, m_displayMaxCells);
            m_fe->getSpectrum(m_displaySpectrum, m_displayPoints);
            m_displayRequested = false;
        }
    }

    if (!m_threaded)
    {
        decodeFramesCaught(m_pending);

        std::lock_guard<std::mutex> lock(m_mutex);

        for (FrameSlot *slot : m_pending) {
            m_free.push_back(slot);
        }

        m_pending.clear();
    }
}

// Below DVB-S2's MODCODs' thresholds (by the spectrum's estimate, with a margin), only VL-SNR frames can be received
double Receiver::searchTimingBandwidth(double spectralEsN0)
{
    return spectralEsN0 >= -3.0 ? m_timingAcquire : dvbs::FrontEnd::trackingBandwidth(m_timingTrack, spectralEsN0);
}

// VL-SNR's search waits two of its frames, then needs a header and the next: at -6 dB, 3 s was too short below about
// 33 kS/s, and it never locked
double Receiver::relockSeconds(double symbolRate)
{
    return symbolRate > 0.0 ? std::max(m_minRelockSeconds, PLDecoder::acquisitionSymbols() / symbolRate) : m_minRelockSeconds;
}

// From the physical layer decoder, in the demodulator thread
void Receiver::onFrame(XFECFrame& frame)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_threaded && (m_queue.size() >= m_maxQueued))
    {
        m_queueWaits++;
        PROFILER_START()
        m_space.wait(lock, [this]() { return m_stop || (m_queue.size() < m_maxQueued); });
        PROFILER_STOP("DVB-S2 waiting for its FEC thread")
    }

    if (m_free.empty())
    {
        // More in use than expected (unthreaded, a call's many frames): another, made without the mutex
        lock.unlock();
        std::unique_ptr<FrameSlot> spare(new FrameSlot());
        lock.lock();
        m_slots.push_back(std::move(spare));
        m_free.push_back(m_slots.back().get());
    }

    FrameSlot *slot = freeSlot();
    std::swap(slot->m_frame, frame); // The decoder gets the slot's old buffers to fill next
    slot->m_epoch = m_resetEpoch;

    if (m_threaded)
    {
        m_queue.push_back(slot);
        m_available.notify_one();
    }
    else
    {
        m_pending.push_back(slot);
    }
}

const Receiver::Code& Receiver::code(const XFECFrame& frame)
{
    // VL-SNR's after all the MODCODs'
    const FECType type = frame.m_fecType;
    const int key = frame.m_vlsnr >= 0 ? 2 * nbModcods + frame.m_vlsnr : frame.m_modcod * 2 + (type == FECType::Short ? 1 : 0);
    auto it = m_codes.find(key);

    if (it != m_codes.end()) {
        return *it->second;
    }

    const Modulation modulation = frame.m_modulation;
    const CodeRate rate = frame.m_rate;
    std::unique_ptr<Code> c(new Code());
    c->m_vlsnr = frame.m_vlsnr;
    c->m_eta = bitsPerSymbol(modulation);

    // pi/2 BPSK (VL-SNR's) has neither: its cells are its bits'. VL-SNR's QPSK maps the bits sent, in order
    if (frame.m_vlsnr >= 0)
    {
        if (modulation == Modulation::QPSK)
        {
            constellation(modulation, rate, c->m_points, type);
            c->m_map.resize(2 * (size_t) vlsnrDataSymbols(vlsnrModcod(frame.m_vlsnr).m_set));

            for (size_t i = 0; i < c->m_map.size(); i++) {
                c->m_map[i] = (int) i;
            }
        }
    }
    else
    {
        bitMap(type, modulation, rate, c->m_map);
        constellation(modulation, rate, c->m_points, type);
    }

    if ((int) c->m_points.size() >= DemapTable::m_minPoints) {
        c->m_demapTable.build(c->m_points, c->m_eta);
    }

    c->m_kldpc = dvbt2::kldpc(type, rate, CodeFamily::S2);
    c->m_kbch = frame.m_vlsnr >= 0 ? vlsnrKbch(frame.m_vlsnr) : dvbt2::kbch(type, rate, CodeFamily::S2);
    c->m_bch.reset(new dvbt2::BCH(type, dvbt2::bchT(type, rate, CodeFamily::S2)));
    const Code& result = *c;
    m_codes[key] = std::move(c);
    return result;
}

// A batch of frames of the same code, by one thread: demapped, LDPC decoded together, each BCH decoded
void Receiver::decodeBatch(dvbt2::LDPC& ldpc, dvbt2::LDPCPool::Context& context, std::vector<FrameSlot*>& frames,
    size_t first, size_t count, const Code& code)
{
    if (!context.m_scratch) {
        context.m_scratch.reset(new Scratch());
    }

    Scratch& scratch = static_cast<Scratch&>(*context.m_scratch);
    std::vector<int8_t*> blocks(count);
    const int n = ldpc.n();

    for (size_t i = 0; i < count; i++)
    {
        FrameSlot& slot = *frames[first + i];
        const XFECFrame& frame = slot.m_frame;
        const int cells = (int) frame.m_cells.size();
        slot.m_llrs.resize(n);

        if (code.m_vlsnr >= 0)
        {
            vlsnrLLRs(frame, code, slot.m_llrs.data(), n, scratch);
        }
        else
        {
            const float scale = 4.0f / std::max(1e-6f, frame.m_noise);
            demap(frame.m_cells.data(), &scale, cells, code.m_points, code.m_eta, m_outlierThreshold, code.m_map.data(),
                slot.m_llrs.data(), nullptr, &code.m_demapTable, true);
        }

        blocks[i] = slot.m_llrs.data();
    }

    // Frames probably cut by lost samples (the input overflowing, when the CPU can't keep up) aren't let hold up the
    // others: each held its batch to the iteration limit, about 10 times as long as the rest, taking the cores from
    // the front end and physical layer, so more samples were lost. They're given a few times the iterations the rest
    // took (all they can have at threshold, where a header's missed now and then, and the rest take many)
    uint64_t quick = 0;

    for (size_t i = 0; i < count; i++)
    {
        if (frames[first + i]->m_frame.m_suspect) {
            quick |= 1ULL << i;
        }
    }

    int unconverged = 0;
    // VL-SNR's low rate codes, as EN 302 307-2 tables 20b and 20c's figures are for pi/2 BPSK: 75 iterations (their
    // frames carry little, so the time's small)
    const int maxIterations = code.m_vlsnr >= 0 ? std::max<int>(m_maxIterations, m_vlsnrIterations) : (int) m_maxIterations;
    const int iterations = ldpc.decode(blocks.data(), (int) count, maxIterations, &unconverged, quick);

    // BCH: the codeword is the LDPC's information bits (after a VL-SNR frame's shortened ones: a whole number of bytes),
    // from their signs, 8 at a time
    const int shortened = code.m_vlsnr >= 0 ? vlsnrModcod(code.m_vlsnr).m_xs : 0;

    for (size_t i = 0; i < count; i++)
    {
        FrameSlot& slot = *frames[first + i];
        const int8_t *llrs = slot.m_llrs.data() + shortened;
        slot.m_packed.resize((code.m_kldpc - shortened) / 8);

        for (size_t b = 0; b < slot.m_packed.size(); b++)
        {
            uint64_t v;
            std::memcpy(&v, llrs + 8 * b, 8);
            v = (v >> 7) & 0x0101010101010101ULL;
            slot.m_packed[b] = (uint8_t) ((v * 0x8040201008040201ULL) >> 56);
        }

        slot.m_corrected = code.m_bch->decode(slot.m_packed.data(), (int) slot.m_packed.size());
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_fecStatus.m_ldpcFailures += unconverged;
    m_fecStatus.m_ldpcIterations = m_fecStatus.m_fecFrames == 0 ? iterations : 0.95 * m_fecStatus.m_ldpcIterations + 0.05 * iterations;
}

void Receiver::vlsnrLLRs(const XFECFrame& frame, const Code& code, int8_t *llrs, int n, Scratch& scratch)
{
    const VLSNRModcod& m = vlsnrModcod(code.m_vlsnr);
    const int k = code.m_kldpc;
    const float limit = dvbt2::llrLimit;
    const Cf *cells = frame.m_cells.data();
    const int nbCells = (int) frame.m_cells.size();
    int cell = 0;

    // QPSK's from the demapper, as the bits sent; pi/2 BPSK's (on the real axis), 4 x / noise (twice the LLR, as the
    // demapper's), summed over a spread bit's cells
    std::vector<int8_t>& qpsk = scratch.m_cellLLRs;
    const float scale = 4.0f / std::max(1e-6f, frame.m_noise);
    qpsk.clear();

    if ((m.m_modulation == Modulation::QPSK) && (2 * (size_t) nbCells == code.m_map.size()))
    {
        qpsk.resize((size_t) 2 * nbCells);
        demap(cells, &scale, nbCells, code.m_points, 2, m_outlierThreshold, code.m_map.data(), qpsk.data(), nullptr,
            nullptr, true);
    }

    auto next = [&]() {
        if (!qpsk.empty()) {
            return cell < (int) qpsk.size() ? (float) qpsk[cell++] : 0.0f;
        }

        float sum = 0.0f;

        for (int s = 0; s < m.m_spreading; s++, cell++) {
            sum += cell < nbCells ? cells[cell].real() * scale : 0.0f;
        }

        return sum;
    };

    for (int i = 0; i < n; i++)
    {
        float v;

        if (i < m.m_xs) {
            v = limit;      // Shortened: a zero
        } else if ((i >= k) && vlsnrPunctured(m, i - k)) {
            v = 0.0f;       // Punctured: not sent
        } else {
            v = next();
        }

        llrs[i] = (int8_t) std::lrint(std::max(-limit, std::min(limit, v)));
    }
}

// The frames, each code's in batches across the decoder threads, then their BBFRAMEs in order
void Receiver::decodeFrames(std::vector<FrameSlot*>& frames)
{
    const double start = now();

    // The frames of each code together, wherever they are in the batch (VCM and ACM interleave MODCODs frame by
    // frame): so the LDPC decoder's lanes are filled, and its decoders for each code not made again and again
    std::vector<std::vector<FrameSlot*>> groups;

    for (FrameSlot *slot : frames)
    {
        if (slot->m_reset) {
            continue;
        }

        // A frame size and rate that don't go together (an undetected error in the PLS code) can't be decoded
        const XFECFrame& f = slot->m_frame;

        if (dvbt2::kldpc(f.m_fecType, f.m_rate, CodeFamily::S2) == 0)
        {
            slot->m_skip = true;
            continue;
        }

        auto group = std::find_if(groups.begin(), groups.end(), [&f](const std::vector<FrameSlot*>& g) {
            return (g.front()->m_frame.m_modcod == f.m_modcod) && (g.front()->m_frame.m_fecType == f.m_fecType)
                && (g.front()->m_frame.m_vlsnr == f.m_vlsnr);
        });

        if (group == groups.end()) {
            groups.push_back({slot});
        } else {
            group->push_back(slot);
        }
    }

    for (std::vector<FrameSlot*>& group : groups)
    {
        const XFECFrame& f = group.front()->m_frame;
        const Code& c = code(f);
        const int lanes = m_context.decoder(CodeFamily::S2, f.m_fecType, f.m_rate).lanes();
        const size_t count = group.size();
        dvbt2::LDPCPool::Job job;
        job.m_family = CodeFamily::S2;
        job.m_type = f.m_fecType;
        job.m_rate = f.m_rate;
        job.m_batches = (int) ((count + lanes - 1) / lanes);
        job.m_decode = [&, count, lanes](dvbt2::LDPC& ldpc, dvbt2::LDPCPool::Context& context, int batch) {
            const size_t first = (size_t) batch * lanes;
            decodeBatch(ldpc, context, group, first, std::min((size_t) lanes, count - first), c);
        };
        m_pool->run(job, m_context);
    }

    const double mid = now();

    // Then in order, with any reset where it came: the packets of frames before it aren't wanted
    for (FrameSlot *slot : frames)
    {
        if (slot->m_reset) {
            resetDecoding(*slot);
        } else if (!slot->m_skip) {
            deframe(*slot);
        }
    }

    // The packets for the caller's thread, unless there's been a reset since
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_bbPackets.empty())
    {
        if (m_decodeEpoch == m_resetEpoch) {
            m_packets.insert(m_packets.end(), m_bbPackets.begin(), m_bbPackets.end());
        }

        m_bbPackets.clear();
    }

    m_fecStatus.m_seconds[2] += mid - start;
    m_fecStatus.m_seconds[3] += now() - mid;
    m_fecStatus.m_bb = m_bbDecoder.stats();
    m_fecStatus.m_streamPackets = m_streamPackets;
}

void Receiver::resetDecoding(const FrameSlot& slot)
{
    m_bbDecoder.reset();
    m_bbPackets.clear();
    clearStreams();
    m_matypeRollOff = -1; // Of the signal before
    m_lowRollOffs = false;
    m_sinceLowRollOffs = 0;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_decodeEpoch = slot.m_epoch;
    m_fecStatus = Status();
}

// Uncaught, an exception in the FEC thread ended the process (and unthreaded, the slots weren't returned)
void Receiver::decodeFramesCaught(std::vector<FrameSlot*>& frames)
{
    try
    {
        decodeFrames(frames);
        return;
    }
    catch (...)
    {
    }

    try
    {
        // A reset among them: its epoch reached, so the counts and packets after it are reported (if decodeFrames had
        // made it, made again: what it counted since is lost with the rest)
        auto reset = std::find_if(frames.rbegin(), frames.rend(), [](const FrameSlot *slot) { return slot->m_reset; });

        if (reset != frames.rend()) {
            resetDecoding(**reset);
        }

        m_bbDecoder.discontinuity();
    }
    catch (...)
    {
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_fecStatus.m_fecErrors++;
}

void Receiver::clearStreams()
{
    m_isi = -1;
    m_streamPackets = 0;
    std::fill(m_isisSeen.begin(), m_isisSeen.end(), false);
    std::fill(m_isisNullOnly.begin(), m_isisNullOnly.end(), false);
    m_isiPackets = 0;
    m_isiNulls = 0;
    m_isiMissing = 0;
}

void Receiver::deframe(FrameSlot& slot)
{
    const XFECFrame& frame = slot.m_frame;
    const int kbch = frame.m_vlsnr >= 0 ? vlsnrKbch(frame.m_vlsnr) : dvbt2::kbch(frame.m_fecType, frame.m_rate, CodeFamily::S2);
    if (kbch <= 0) {
        qWarning() << "Receiver::deframe: Invalid BCH information length" << kbch;
        return;
    }
    const bool bad = slot.m_corrected < 0;
    dvbt2::scrambleBytes(slot.m_packed.data(), kbch / 8);
    dvbt2::BBFrameDecoder::Header header;
    const bool headerOK = dvbt2::BBFrameDecoder::parseHeader(slot.m_packed.data(), header) && (header.m_mode == 0);

    if (!bad && headerOK) {
        m_failedFrames = 0;
    } else {
        m_failedFrames++;
    }

    // Only a frame BCH corrected is trusted to choose the input stream or the roll-off: a BBHEADER of garbage passes its
    // CRC-8 one time in 256, and would select a stream that doesn't exist (every real frame then skipped) or switch
    // the matched filter
    const bool trusted = headerOK && !bad;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Status& s = m_fecStatus;
        s.m_fecFrames++;
        s.m_bchFailures += bad;
        s.m_bchCorrected += std::max(0, slot.m_corrected);

        if (trusted)
        {
            s.m_matypeRollOff = m_matypeRollOff;
            s.m_multipleStreams = !header.singleStream() || frame.m_timeSlicing;
            s.m_acm = (header.m_matype1 & 0x10) == 0;
        }
    }

    // Frames were lost before this one (perhaps of the stream decoded, whichever this one's of)
    if (frame.m_gap) {
        m_bbDecoder.discontinuity();
    }

    if (trusted)
    {
        // S2X's low roll-offs: RO alternates between 11 and their code (00 0.15, 01 0.10, 10 0.05), so without an 11
        // for a few frames (some lost, in a multiple input stream another's), they've ended: the codes are S2's again
        if (header.rollOff() == 3)
        {
            m_lowRollOffs = true;
            m_sinceLowRollOffs = 0;
        }
        else
        {
            m_lowRollOffs = m_lowRollOffs && (++m_sinceLowRollOffs < m_lowRollOffFrames);

            // Not from a frame of before a reset (still being decoded when it came): the front end's set for the
            // signal since
            if (slot.m_epoch == m_resetEpoch) {
                m_matypeRollOff = header.rollOff() + (m_lowRollOffs ? 4 : 0);
            }
        }

        // In a multiple input stream, the one selected (or the first seen). Time slicing's (EN 302 307-1 Annex M)
        // slices are streams too, by their TSN (which may be the ISI, or not)
        if (!header.singleStream() || frame.m_timeSlicing)
        {
            const int isi = frame.m_timeSlicing ? frame.m_tsn : header.m_matype2;

            if (!m_isisSeen[isi])
            {
                m_isisSeen[isi] = true;
                std::lock_guard<std::mutex> lock(m_mutex);
                m_fecStatus.m_isiCount++;
                m_fecStatus.m_isis.insert(std::lower_bound(m_fecStatus.m_isis.begin(), m_fecStatus.m_isis.end(), isi), isi);
            }

            const int wanted = m_isiSetting;

            if (wanted >= 0)
            {
                if (m_isi != wanted)
                {
                    m_isi = wanted;
                    m_bbDecoder.discontinuity();
                    m_streamPackets = 0;    // After: the discontinuity can give the last stream's packet held
                }
            }
            else if ((m_isi < 0) || (m_isiMissing >= m_missingStreamFrames))
            {
                if (m_isi >= 0) {
                    m_bbDecoder.discontinuity();
                }

                m_streamPackets = isi == m_isi ? m_streamPackets : 0;   // After the discontinuity, as above
                m_isi = isi;
                m_isiPackets = 0;
                m_isiNulls = 0;
                m_isiMissing = 0;
            }
            else if ((m_isiPackets >= m_nullOnlyPackets) && (m_isiNulls == m_isiPackets))
            {
                // Chosen automatically, but carrying only null packets (as a stream of padding does): the next stream
                // seen that hasn't been found so, if there's one
                m_isisNullOnly[m_isi] = true;
                int next = -1;

                for (int k = 1; (k < 256) && (next < 0); k++)
                {
                    const int candidate = (m_isi + k) & 255;

                    if (m_isisSeen[candidate] && !m_isisNullOnly[candidate]) {
                        next = candidate;
                    }
                }

                if (next >= 0)
                {
                    m_isi = next;
                    m_bbDecoder.discontinuity();
                    m_streamPackets = 0;
                }

                m_isiPackets = 0;
                m_isiNulls = 0;
            }

            m_isiMissing = isi == m_isi ? 0 : m_isiMissing + 1;

            if (isi != m_isi)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_fecStatus.m_otherStreams++;
                return;
            }
        }
        else if (m_isi != -1)
        {
            // From a multiple input stream to a single one: the packet the stream before was in the middle of isn't
            // this one's (its SYNCD could happen to fit), so dropped, as on switching streams
            m_isi = -1;
            m_bbDecoder.discontinuity();
            m_streamPackets = 0;    // After: the discontinuity can give the last stream's packet held
        }
    }
    else if (headerOK)
    {
        // An untrusted frame isn't decoded unless its header says it's of the stream being decoded: in a multiple input
        // stream, not before one's chosen, and not one claiming a single stream (an ISI setting doesn't matter then)
        const int wanted = m_isiSetting;
        const int selected = wanted >= 0 ? wanted : m_isi.load();

        const bool single = header.singleStream() && !frame.m_timeSlicing;

        if (single ? (m_isi >= 0) : ((frame.m_timeSlicing ? frame.m_tsn : header.m_matype2) != selected)) {
            return;
        }
    }

    m_bbDecoder.process(slot.m_packed.data(), kbch, bad);
}

void Receiver::runFEC()
{
    std::vector<FrameSlot*> frames;
    std::unique_lock<std::mutex> lock(m_mutex);

    while (true)
    {
        m_available.wait(lock, [this]() { return m_stop || !m_queue.empty(); });

        if (m_stop) {
            break;
        }

        // The LDPC decoder's lanes filled: a batch costs about the same however few of them there are. When the frames
        // are coming slower than they're decoded, they wait (a bounded time, so the delay is at low rates) for a full
        // batch, rather than each being decoded nearly alone as it comes. A reset (no frame) or an undecodable frame
        // decides nothing
        const FrameSlot *front = m_queue.front();
        const XFECFrame& f = front->m_frame;
        const bool decodable = !front->m_reset && (dvbt2::kldpc(f.m_fecType, f.m_rate, CodeFamily::S2) != 0);
        size_t lanes = 1;

        if (decodable)
        {
            // Outside the lock: a decoder for a code not seen yet is made here (its graph and tables), and the shared
            // thread queueing frames would wait for it. The queue may be emptied meanwhile (a reset)
            const FECType type = f.m_fecType;
            const CodeRate rate = f.m_rate;
            lock.unlock();
            lanes = (size_t) std::max(1, m_context.decoder(CodeFamily::S2, type, rate).lanes());
            lock.lock();

            if (m_stop) {
                break;
            }

            if (m_queue.empty()) {
                continue;
            }
        }

        if (m_queue.size() < lanes)
        {
            m_available.wait_for(lock, std::chrono::milliseconds(m_batchWaitMs), [this, lanes]() {
                return m_stop || m_draining || (m_queue.size() >= lanes);
            });

            if (m_stop) {
                break;
            }
        }

        // Whole batches when there are more (so they fill when the frames come faster than they're decoded), the rest
        // left for the next with those that come meanwhile; everything when it's waited or is flushing. At most whole
        // rounds of the decoders: more, and the last round left decoders idle while those queued meanwhile waited (and
        // the slots in use, about 330 KB each, were up to twice the queue)
        size_t take = std::min(m_queue.size(), m_takeBatches * lanes);

        if (!m_draining && (take > lanes)) {
            take -= take % lanes;
        }

        frames.assign(m_queue.begin(), m_queue.begin() + take);
        m_queue.erase(m_queue.begin(), m_queue.begin() + take);
        m_busy = true;
        lock.unlock();
        m_space.notify_all();

        decodeFramesCaught(frames);

        lock.lock();

        for (FrameSlot *slot : frames) {
            m_free.push_back(slot);
        }

        frames.clear();
        m_busy = false;

        if (idle()) {
            m_idle.notify_all();
        }
    }
}

// With m_mutex
bool Receiver::idle() const
{
    return m_queue.empty() && !m_busy;
}

void Receiver::deliverPackets()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_delivering.swap(m_packets);
    }

    if (m_packetCallback)
    {
        for (size_t i = 0; i + 188 <= m_delivering.size(); i += 188) {
            m_packetCallback(&m_delivering[i]);
        }
    }

    m_delivering.clear();
}

void Receiver::flush()
{
    if (m_demodThread) {
        m_demodThread->drain();
    }

    if (m_threaded)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_draining = true;
        m_available.notify_one();
        m_idle.wait(lock, [this] { return idle(); });
        m_draining = false;
    }

    m_bbDecoder.flush();

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_bbPackets.empty() && (m_decodeEpoch == m_resetEpoch)) {
            m_packets.insert(m_packets.end(), m_bbPackets.begin(), m_bbPackets.end());
        }

        m_bbPackets.clear();
    }

    deliverPackets();
}

Receiver::Status Receiver::getStatus() const
{
    Status status;

    {
        std::lock_guard<std::mutex> lock(m_demodMutex);
        status = m_demodStatus;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Until the FEC thread's reached a reset, its counts are still the signal's before (a batch being decoded
        // counts as it goes): none yet, rather than those
        static const Status none;
        const Status& f = m_decodeEpoch == m_resetEpoch ? m_fecStatus : none;
        status.m_matypeRollOff = f.m_matypeRollOff;
        status.m_multipleStreams = f.m_multipleStreams;
        status.m_acm = f.m_acm;
        status.m_isi = m_isi;
        status.m_streamPackets = f.m_streamPackets;
        status.m_isiCount = f.m_isiCount;
        status.m_isis = f.m_isis;
        status.m_fecFrames = f.m_fecFrames;
        status.m_ldpcFailures = f.m_ldpcFailures;
        status.m_fecErrors = f.m_fecErrors;
        status.m_bchFailures = f.m_bchFailures;
        status.m_bchCorrected = f.m_bchCorrected;
        status.m_ldpcIterations = f.m_ldpcIterations;
        status.m_otherStreams = f.m_otherStreams;
        status.m_bb = f.m_bb;
        status.m_queueWaits = m_queueWaits;
        status.m_seconds[2] = f.m_seconds[2];
        status.m_seconds[3] = f.m_seconds[3];
    }

    if (status.m_frontEnd.m_estimating) {
        status.m_state = State::Spectrum;
    } else if (status.m_pl.m_state != PLDecoder::State::Locked) {
        status.m_state = State::Searching;
    } else if (status.m_streamPackets == 0) {
        // Not until packets of the stream chosen: one selected that isn't sent decodes none, whatever came before
        status.m_state = State::Frames;
    } else {
        status.m_state = State::Locked;
    }

    return status;
}

void Receiver::getDisplayData(std::vector<Cf>& cells, int maxCells, std::vector<float>& spectrumDb, int points) const
{
    std::lock_guard<std::mutex> lock(m_demodMutex);
    cells = m_displayCells;
    spectrumDb = m_displaySpectrum;
    m_displayMaxCells = maxCells;
    m_displayPoints = points;
    m_displayRequested = true;
}

} // namespace dvbs2
