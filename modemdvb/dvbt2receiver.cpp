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

#include "util/profiler.h"

#include "dvbt2receiver.h"
#include "samplethread.h"

namespace dvbt2 {

double Receiver::now()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

Receiver::Receiver(int bandwidthHz, FFTFactory *fftFactory, bool threaded, int decoderThreads) :
    m_ofdm(bandwidthHz, fftFactory),
    m_outputRate(sampleRate(bandwidthHz)),
    m_inputRate(m_outputRate),
    m_resampling(false),
    m_clockFrames(0),
    m_clockMeasured(false),
    m_selectedPLP(-1),
    m_threaded(threaded),
    m_frameValid(false),
    m_address(0),
    m_maxIterations(m_defaultMaxIterations),
    m_analysis(false),
    m_outlierLimiting(true),
    m_busy(0),
    m_nextSequence(0),
    m_finishSequence(0),
    m_stop(false)
{
    m_canceller.configure(m_inputRate, 0.85 * m_outputRate); // The occupied bandwidth, 7.77 MHz in 8 (extended)

    // An interleaving frame has a few batches of blocks for the LDPC decoder, so two frames are decoded at once, to
    // keep the decoder threads busy while one is de-interleaved and delivered. With the SIMD decoder, a few threads is
    // plenty, and each decoder holds megabytes of messages: more than 4 overflow the cache (and on hybrid CPUs reach
    // the efficiency cores), so each batch is slower. PSB3 on an i9-14900K: 4 threads decoded at 5.2 times real time
    // with 18 s of LDPC for 30 s; 8 at 4.7 times with 38 s
    const int cores = std::max(1, (int) std::thread::hardware_concurrency());
    const int threads = decoderThreads > 0 ? decoderThreads : std::max(1, std::min(4, cores - 2));
    m_workers = !threaded ? 1 : threads >= 4 ? 2 : 1;

    for (int w = 0; w < m_workers; w++) {
        m_fecs.emplace_back(new FEC());
    }

    m_ofdm.setCallback([this](const OFDMDemod::Symbol& symbol) {
        onSymbol(symbol);
    });
    // Only one worker at a time decodes BBFRAMEs, so their packets are collected without a lock
    m_bbDecoder.setPacketCallback([this](const uint8_t *packet) {
        m_bbPackets.insert(m_bbPackets.end(), packet, packet + 188);
    });

    m_ofdmStatus = m_ofdm.getStatus();

    // The workers decode too
    m_pool.reset(new LDPCPool(std::max(0, threads - m_workers)));

    if (m_threaded)
    {
        for (int w = 0; w < m_workers; w++) {
            m_threads.emplace_back(&Receiver::worker, this, w);
        }

        m_demodThread.reset(new SampleThread([this](const Cf *samples, int nbSamples) {
            demodulate(samples, nbSamples);
        }));
    }
}

Receiver::~Receiver()
{
    m_demodThread.reset(); // Before the workers, which it feeds

    if (m_threaded)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }

        m_frameAvailable.notify_all();
        m_idle.notify_all();

        for (std::thread& thread : m_threads) {
            thread.join();
        }
    }

    // Once the workers have finished, so there are no jobs
    m_pool.reset();
}

void Receiver::setLogCallback(OFDMDemod::LogCallback callback)
{
    m_log = callback;
    m_ofdm.setLogCallback(callback);
}

void Receiver::reset()
{
    if (m_demodThread) {
        m_demodThread->discard();
    }

    // The frames queued are the signal's before: dropped, not decoded (that blocked the caller while the LDPC decoded
    // them, and sent their packets after a retune). Those being decoded finish, their packets dropped too, and the
    // dropped frames' turns are passed over
    if (m_threaded)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        while (!m_queue.empty())
        {
            if (m_freeFrames.size() < 2) {
                m_freeFrames.push_back(std::move(m_queue.front()));
            }

            m_queue.pop_front();
        }

        m_idle.notify_all();
        m_idle.wait(lock, [this]() { return m_busy == 0; });
        m_finishSequence = m_nextSequence;
        m_packets.clear();
    }
    else
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_packets.clear();
    }

    m_bbPackets.clear();
    resetDemodulator();
    m_clockFrames = 0; // The OFDM frame count it's compared with starts again
    m_clockMeasured = false;
    m_frameValid = false;
    m_bbDecoder.reset();

    {
        std::lock_guard<std::mutex> lock(m_displayMutex);
        m_displayCells.clear();
    }

    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_status = Status();
}

// With the demodulator thread, if any, idle
void Receiver::resetDemodulator()
{
    m_ofdm.reset();
    m_ofdm.setProfile(m_profile);
    // Its acquisitions and frames are counted from 0 again, so a frame (or part of an interleaving frame) received
    // from now on could be taken as following the last
    m_frameValid = false;
    m_currentParts = -1;
    m_lastAcquisition = ~0ULL;
    std::lock_guard<std::mutex> lock(m_ofdmMutex);
    m_ofdmStatus = m_ofdm.getStatus();
    m_clockAdjustments.clear();
    m_clockLandedFrames = 0;
    // The notches the samples from now on are notched with (the canceller keeps them)
    m_notchChanges.clear();
    m_notchesQueued = m_notchFrequencies;
    m_ofdm.setNotches(m_notchesQueued, m_inputRate);
    m_samplesPushed = 0;
    m_samplesDemodulated = 0;
}

void Receiver::setProfile(Profile profile)
{
    if (profile == m_profile) {
        return;
    }

    m_profile = profile;
    reset();
}

void Receiver::setInputSampleRate(double sampleRate)
{
    if (m_demodThread) {
        m_demodThread->discard();
    }

    flush();
    m_bbDecoder.discontinuity(); // A packet it was assembling can't be completed by what's received at the new rate
    m_inputRate = sampleRate;
    m_canceller.configure(sampleRate, 0.85 * m_outputRate); // The occupied bandwidth, 7.77 MHz in 8 (extended)
    m_resampling = std::abs(sampleRate / m_outputRate - 1.0) > 1e-6;
    m_resampler.setRatioCorrection(1.0);
    m_ratioCorrection = 1.0;

    if (m_resampling) {
        m_resampler.configure(m_inputRate, m_outputRate, 0.43 * m_outputRate); // Extended carrier mode is 7.77 MHz wide
    }

    resetDemodulator();
    m_clockFrames = 0;
    m_clockMeasured = false;
}

void Receiver::process(const Cf *samples, int nbSamples)
{
    PROFILER_START()
    double start = now();
    // Strong narrowband interferers notched out first
    samples = m_canceller.process(samples, nbSamples, m_notched);
    const Cf *input = samples;
    int nbInput = nbSamples;

    if (m_resampling)
    {
        // Into a buffer about the size it was (the demodulator thread's are reused), so it needn't be filled first
        const int maxOut = m_resampler.maxOutputs(nbSamples);
        m_resampled.resize(maxOut);
        m_resampled.resize(m_resampler.process(samples, nbSamples, m_resampled.data(), maxOut));
        input = m_resampled.data();
        nbInput = (int) m_resampled.size();
    }
    else
    {
        int keep = std::min(nbSamples, 64);
        m_lastInput.assign(samples + nbSamples - keep, samples + nbSamples);
    }

    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.m_seconds[0] += now() - start;
        m_status.m_notches.clear();

        for (const auto& notch : m_canceller.getNotches()) {
            m_status.m_notches.push_back(notch.m_frequency * m_inputRate);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_ofdmMutex);
        m_notchFrequencies.clear();

        for (const auto& notch : m_canceller.getNotches()) {
            m_notchFrequencies.push_back(notch.m_frequency * m_inputRate);
        }

        // The canceller notches all of this call's samples with them, so they apply from its first (the demodulator
        // thread may be well behind)
        if (m_notchFrequencies != m_notchesQueued)
        {
            m_notchesQueued = m_notchFrequencies;
            m_notchChanges.push_back({m_samplesPushed, m_notchFrequencies});
        }
    }

    PROFILER_STOP("DVB-T2 resampling")

    if (!m_demodThread) {
        demodulate(input, nbInput);
    } else if (m_resampling) {
        m_demodThread->push(m_resampled); // Swapped for another buffer, rather than copied
    } else {
        m_demodThread->push(input, nbInput);
    }

    m_samplesPushed += nbInput;
    correctClock();
    PROFILER_RESTART()
    deliverPackets();
    PROFILER_STOP("DVB-T2 packet delivery")
}

// OFDM demodulation, the frames' assembly and L1 decoding, and what other threads need of them
void Receiver::demodulate(const Cf *samples, int nbSamples)
{
    PROFILER_START()
    double start = now();

    // Clock corrections from these samples on
    {
        std::lock_guard<std::mutex> lock(m_ofdmMutex);

        while (!m_clockAdjustments.empty() && (m_clockAdjustments.front().m_sample <= m_samplesDemodulated))
        {
            m_ofdm.adjustClockOffset(m_clockAdjustments.front().m_ppm, m_clockAdjustments.front().m_sample);
            m_clockAdjustments.pop_front();
            m_clockLandedFrames = m_ofdm.getStatus().m_frames;
        }

        while (!m_notchChanges.empty() && (m_notchChanges.front().m_sample <= m_samplesDemodulated))
        {
            m_ofdm.setNotches(m_notchChanges.front().m_frequencies, m_inputRate);
            m_notchChanges.pop_front();
        }
    }

    m_samplesDemodulated += nbSamples;
    m_ofdm.process(samples, nbSamples);
    OFDMDemod::Status status = m_ofdm.getStatus();

    {
        std::lock_guard<std::mutex> lock(m_ofdmMutex);
        m_ofdmStatus = status;
    }

    {
        std::lock_guard<std::mutex> lock(m_displayMutex);

        if (m_displayRequested)
        {
            m_ofdm.getDisplayData(m_ofdmCells, m_displayMaxCells, m_ofdmResponse, m_displayPoints);
            m_ofdm.getImpulseResponse(m_ofdmImpulse, m_displayPoints);
            m_displayRequested = false;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.m_seconds[0] += now() - start;
    }

    PROFILER_STOP("DVB-T2 demodulation")
}

// As demodulate() last left it, threaded or not, as the demodulator may be running in another thread
OFDMDemod::Status Receiver::ofdmStatus() const
{
    std::lock_guard<std::mutex> lock(m_ofdmMutex);
    return m_ofdmStatus;
}

// Correct the sample clock by the offset measured from the P1 positions, resampling when it's significant
void Receiver::correctClock()
{
    OFDMDemod::Status status;
    uint64_t landed;

    // Not until the last correction has reached the demodulator (which, with a backlog, as when a file is read ahead,
    // may be frames behind), and then from its frames after it: before, its status is of the offset before the
    // correction, which would be corrected again
    {
        std::lock_guard<std::mutex> lock(m_ofdmMutex);

        if (!m_clockAdjustments.empty()) {
            return;
        }

        status = m_ofdmStatus;
        landed = m_clockLandedFrames;
    }

    // The first correction after a frame, as a large offset spoils reception until it's corrected (in 32K, from
    // about 15 ppm, by interference between the carriers), then every few frames
    // Or first from the spacing of P1s, when the frame can't be decoded to measure it (from 40 ppm in 32K)
    // Whether or not the input was already being resampled from another rate
    const bool first = !m_clockMeasured;
    const bool fromP1s = first && status.m_clockFromP1s;

    if (!fromP1s && ((status.m_state != OFDMDemod::Symbols)
        || (status.m_frames < std::max(m_clockFrames, landed) + (first ? 1 : m_clockCorrectionFrames)))) {
        return;
    }

    m_clockFrames = status.m_frames;
    double ppm = status.m_clockOffset;

    if (!std::isfinite(ppm) || (std::abs(ppm) > 1000.0)) {
        return; // Not a real clock offset: a measurement gone wrong
    }

    if (!m_resampling)
    {
        if (std::abs(ppm) < m_clockCorrectionThreshold) {
            return;
        }

        m_resampling = true;
        m_resampler.setRatioCorrection(1.0);
        m_ratioCorrection = 1.0;
        m_resampler.configure(m_inputRate, m_outputRate, 0.43 * m_outputRate);
        m_resampler.prime(m_lastInput);
        setRatioCorrection(1.0 + ppm * 1e-6);
        m_clockMeasured = true;
        return;
    }

    // Frames longer than they should be (positive offset) mean more input samples are needed per output sample
    // The first measurement is accurate, so all of it (a HackRF's 15 ppm would otherwise take 3 s to get under 1),
    // then most of each, to follow drift without chasing the measurements' noise
    if (std::abs(ppm) > 0.05) {
        setRatioCorrection(m_resampler.getRatioCorrection() * (1.0 + (first ? 1.0 : 0.7) * ppm * 1e-6));
    }

    m_clockMeasured = true;
}

// And the demodulator's clock offset changes with it, from the samples resampled with it
void Receiver::setRatioCorrection(double ratio)
{
    const double change = -(ratio - m_resampler.getRatioCorrection()) * 1e6;
    m_resampler.setRatioCorrection(ratio);
    m_ratioCorrection = m_resampler.getRatioCorrection();
    std::lock_guard<std::mutex> lock(m_ofdmMutex);
    m_clockAdjustments.push_back({m_samplesPushed, change});
}

void Receiver::deliverPackets()
{
    // The buffers are swapped, so they're reused
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_delivered.swap(m_packets);
    }

    if (m_packetCallback)
    {
        for (size_t i = 0; i + 188 <= m_delivered.size(); i += 188) {
            m_packetCallback(&m_delivered[i]);
        }
    }

    m_delivered.clear();
}

void Receiver::flush()
{
    if (m_demodThread) {
        m_demodThread->drain();
    }

    if (m_threaded)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_idle.wait(lock, [this]() { return m_queue.empty() && (m_busy == 0); });
    }

    // The last packet, held for the next's CRC-8 (no worker is decoding now)
    m_bbDecoder.flush();

    if (!m_bbPackets.empty())
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_packets.insert(m_packets.end(), m_bbPackets.begin(), m_bbPackets.end());
        m_bbPackets.clear();
    }

    deliverPackets();
}

Receiver::Status Receiver::getStatus()
{
    Status status;

    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        status = m_status;
    }

    status.m_ofdm = ofdmStatus();
    status.m_ofdm.m_clockOffset += (m_ratioCorrection - 1.0) * 1e6; // Include the correction applied
    return status;
}

// The cells of the OFDM symbols include L1 signalling, and a rotated constellation's are spread over two cells, so
// the PLP's cells after de-interleaving show the constellation better
// The demodulator's are copied by demodulate() when asked for, threaded or not, as it may be running in another thread
void Receiver::getDisplayData(std::vector<Cf>& cells, int maxCells, std::vector<float>& responseDb, int points) const
{
    std::lock_guard<std::mutex> lock(m_displayMutex);
    cells = m_ofdmCells;
    responseDb = m_ofdmResponse;
    m_displayRequested = true;
    m_displayMaxCells = maxCells;
    m_displayPoints = points;

    if (!m_displayCells.empty()) {
        cells.assign(m_displayCells.begin(), m_displayCells.begin() + std::min((size_t) maxCells, m_displayCells.size()));
    }
}

void Receiver::getImpulseResponse(ImpulseResponse& response) const
{
    std::lock_guard<std::mutex> lock(m_displayMutex);
    response = m_ofdmImpulse;
}

void Receiver::onSymbol(const OFDMDemod::Symbol& symbol)
{
    const OFDMDemod::Status ofdm = m_ofdm.getStatus();
    const int np2 = numP2(ofdm.m_fftSize);

    if (symbol.m_l == 0)
    {
        m_symbolFrame = symbol.m_frame;
        // Empty, keeping their buffers
        m_p2Cells.resize(np2);
        m_p2Scales.resize(np2);

        for (int l = 0; l < np2; l++)
        {
            m_p2Cells[l].clear();
            m_p2Scales[l].clear();
        }

        m_frameValid = false;
        m_address = 0;
        m_ranges.clear();
        m_rangeCursor = 0;
    }

    if (symbol.m_p2)
    {
        m_p2Cells[symbol.m_l].assign(symbol.m_cells, symbol.m_cells + symbol.m_nbCells);
        m_p2Scales[symbol.m_l].assign(symbol.m_llrScales, symbol.m_llrScales + symbol.m_nbCells);

        if (symbol.m_l == np2 - 1)
        {
            PROFILER_START()
            decodeL1();
            PROFILER_STOP("DVB-T2 L1 decoding")
        }

        return;
    }

    if (!m_frameValid) {
        return;
    }

    int nbCells = symbol.m_nbCells;

    if (symbol.m_frameClosing)
    {
        // Only the first CFC cells of the frame closing symbol are used
        const Tables& t = Tables::get(ofdm.m_fftSize, m_pre.m_bwtExt, m_pre.m_pilotPattern, m_pre.toneReservation());
        nbCells = std::min(nbCells, t.m_cfc);
    }

    PROFILER_START()
    addCells(symbol.m_cells, symbol.m_llrScales, nbCells);
    PROFILER_STOP("DVB-T2 frame building")

    if (symbol.m_l == np2 + m_pre.m_numDataSymbols - 1) {
        frameComplete();
    }
}

void Receiver::decodeL1()
{
    const int np2 = (int) m_p2Cells.size();
    const int cp2 = (int) m_p2Cells[0].size();
    std::vector<Cf> cells(l1PreCells);
    std::vector<float> scales(l1PreCells);

    // Zig-zag across the P2 symbols (8.3.5)
    for (int i = 0; i < l1PreCells; i++)
    {
        cells[i] = m_p2Cells[i % np2][i / np2];
        scales[i] = m_p2Scales[i % np2][i / np2];
    }

    L1Pre pre;

    if (!m_l1Codec.decodePre(cells.data(), scales.data(), pre) || (pre.fftSize() != m_ofdm.getStatus().m_fftSize))
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.m_l1Errors++;
        m_status.m_l1Valid = false;
        return;
    }

    // The frame's length: within the frame level PN sequence, and at most 250 ms (8.3.1)
    const int fftSize = pre.fftSize();
    const int maxSymbols = (int) ((0.25 * m_outputRate - 2048) / (fftSize + guardSamples(fftSize, pre.m_guard)));

    if (np2 + pre.m_numDataSymbols > std::min(framePNLength, maxSymbols))
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.m_l1Errors++;
        m_status.m_l1Valid = false;
        return;
    }

    // The OFDM can continue with the data symbols, even if L1-post fails
    m_ofdm.setFrameParameters(pre);
    m_pre = pre;
    const int postCells = pre.m_l1PostSize;
    const int preRows = l1PreCells / np2;
    const int postRows = postCells / np2;

    if ((postCells % np2) || (preRows + postRows > cp2))
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.m_l1Errors++;
        m_status.m_l1Valid = false;
        return;
    }

    cells.resize(postCells);
    scales.resize(postCells);

    for (int i = 0; i < postCells; i++)
    {
        cells[i] = m_p2Cells[i % np2][preRows + i / np2];
        scales[i] = m_p2Scales[i % np2][preRows + i / np2];
    }

    L1Post post;

    if (!m_l1Codec.decodePost(cells.data(), scales.data(), pre, post) || post.m_plps.empty())
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.m_l1Errors++;
        m_status.m_l1Valid = false;
        return;
    }

    m_post = post;
    m_ofdm.setFEFLength(pre.m_s2 & 1 ? ((int64_t) post.m_fefLengthMsb << 22) | post.m_fefLength : 0);

    // The PLP: the one selected, or if it isn't there, the first data PLP (type 1 or 2) that can be decoded
    const int selected = m_selectedPLP;
    const PLP *plp = nullptr;
    const PLP *firstData = nullptr;

    for (const PLP& p : post.m_plps)
    {
        if ((selected >= 0) && (p.m_id == selected))
        {
            plp = &p;
            break;
        }

        if (!firstData && ((p.m_type == 1) || (p.m_type == 2)) && p.m_valid) {
            firstData = &p;
        }
    }

    if (!plp) {
        plp = firstData;
    }

    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.m_l1Valid = true;
        m_status.m_pre = pre;
        m_status.m_post = post;
        m_status.m_plpId = plp ? plp->m_id : -1;
    }

    // Not if its code rate, modulation or FEC type is reserved
    if (!plp || !plp->m_valid) {
        return;
    }

    m_plp = *plp;

    // Its cells in this frame (8.2, 8.3.6.3): it's in every IJUMP-th frame from FIRST_FRAME_IDX, and an Interleaving Frame
    // is spread over PI of them, part (frame / IJUMP) mod PI of it in each, as there's a whole number in a super-frame
    const int frameInterval = std::max(1, m_plp.m_frameInterval);
    const int frameOffset = post.m_frameIdx - m_plp.m_firstFrameIdx;
    const bool present = (frameOffset >= 0) && ((frameOffset % frameInterval) == 0);
    const int pi = m_plp.interleavingFrames();
    const int ncells = cellsPerFECBlock(m_plp.m_fecType, m_plp.m_constellation);

    if (!present || (m_plp.m_numBlocks == 0) || (m_plp.m_numBlocks > m_plp.m_numBlocksMax) || (pi < 1) || (ncells % pi)) {
        return;
    }

    const int part = (frameOffset / frameInterval) % pi;
    const int total = m_plp.m_numBlocks * ncells / pi;
    const int nsub = m_plp.m_type == 2 ? std::max(1, post.m_subSlicesPerFrame) : 1;
    const int64_t interval = m_plp.m_type == 2 ? post.m_subSliceInterval : 0;

    // Its cells must be within the frame's data cells (8.3.6.3): the P2 symbols' after L1, then the data symbols'
    const Tables& t = Tables::get(fftSize, pre.m_bwtExt, pre.m_pilotPattern, pre.toneReservation());
    size_t cdata = 0;

    for (int phase = 0; phase < t.m_dy; phase++) {
        cdata = std::max(cdata, t.m_data[phase].size());
    }

    const bool miso = (pre.m_s1 == (int) PreambleType::T2MISO) || (pre.m_s1 == (int) PreambleType::T2LiteMISO);
    const bool closing = frameClosingSymbol(fftSize, pre.m_guard, pre.m_pilotPattern, miso) && (pre.m_numDataSymbols > 0);
    const int64_t capacity = (int64_t) np2 * (cp2 - preRows - postRows)
        + (int64_t) (pre.m_numDataSymbols - (closing ? 1 : 0)) * (int64_t) cdata + (closing ? t.m_cfc : 0);

    if (m_plp.m_start + (nsub - 1) * interval + total / nsub > capacity) {
        return;
    }

    if (part == 0)
    {
        m_current.m_plp = m_plp;
        m_current.m_firstFrame = m_symbolFrame;
        m_current.m_acquisition = m_ofdm.getStatus().m_acquisitions;
        m_current.m_cells.clear();
        m_current.m_scales.clear();
        // Not more than a T2-frame's, or MTI, until the cells are there
        const size_t reserve = std::min((size_t) total * pi, std::max((size_t) total, m_maxTICells));
        m_current.m_cells.reserve(reserve);
        m_current.m_scales.reserve(reserve);
        m_currentParts = 0;
    }
    else if ((m_currentParts != part) || (m_symbolFrame != m_currentFrame + frameInterval)
        || (m_current.m_plp.m_id != m_plp.m_id) || (m_current.m_plp.m_numBlocks != m_plp.m_numBlocks)
        || ((int) m_current.m_cells.size() != part * total))
    {
        m_currentParts = -1; // A part is missing: wait for the next Interleaving Frame
        return;
    }

    for (int j = 0; j < nsub; j++)
    {
        int start = (int) (m_plp.m_start + j * interval);
        m_ranges.push_back({start, start + total / nsub});
    }

    m_currentFrame = m_symbolFrame;
    m_frameValid = true;

    // The rest of the P2 symbols (none, if L1 fills them: not indexed, as one past the end)
    for (int l = 0; l < np2; l++) {
        addCells(m_p2Cells[l].data() + preRows + postRows, m_p2Scales[l].data() + preRows + postRows, cp2 - preRows - postRows);
    }
}

// The ranges are in order of their starts (sub-slices), and the cells come in address order, so the ranges before
// the cursor are passed, and those from one starting after these cells on are still to come
void Receiver::addCells(const Cf *cells, const float *scales, int nbCells)
{
    const int first = m_address;
    const int last = m_address + nbCells;

    while ((m_rangeCursor < m_ranges.size()) && (m_ranges[m_rangeCursor].second <= first)) {
        m_rangeCursor++;
    }

    for (size_t r = m_rangeCursor; (r < m_ranges.size()) && (m_ranges[r].first < last); r++)
    {
        const std::pair<int, int>& range = m_ranges[r];
        int start = std::max(first, range.first);
        int end = std::min(last, range.second);

        if (start < end)
        {
            m_current.m_cells.insert(m_current.m_cells.end(), cells + (start - first), cells + (end - first));
            m_current.m_scales.insert(m_current.m_scales.end(), scales + (start - first), scales + (end - first));
        }
    }

    m_address = last;
}

void Receiver::frameComplete()
{
    m_frameValid = false;
    const int ncells = cellsPerFECBlock(m_current.m_plp.m_fecType, m_current.m_plp.m_constellation);
    const int pi = m_current.m_plp.interleavingFrames();

    if ((int) m_current.m_cells.size() != (m_currentParts + 1) * (m_current.m_plp.m_numBlocks * ncells / pi))
    {
        m_currentParts = -1;
        return;
    }

    if (++m_currentParts < pi) {
        return; // More of it to come
    }

    // Following the last decoded? (PI x IJUMP T2-frames later, since the same acquisition)
    const int step = pi * std::max(1, m_current.m_plp.m_frameInterval);
    // Or of another PLP, whose packets can't continue the last's
    m_current.m_gap = (m_current.m_acquisition != m_lastAcquisition) || (m_current.m_firstFrame != m_lastFirstFrame + step)
        || (m_current.m_plp.m_id != m_lastPLPId);
    m_lastAcquisition = m_current.m_acquisition;
    m_lastFirstFrame = m_current.m_firstFrame;
    m_lastPLPId = m_current.m_plp.m_id;
    m_current.m_sequence = m_nextSequence++;

    if (!m_threaded)
    {
        decodeInterleavingFrame(m_current, *m_fecs[0]);
        return;
    }

    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_queue.size() >= m_maxQueuedFrames)
    {
        std::lock_guard<std::mutex> statusLock(m_statusMutex);
        m_status.m_queueWaits++;
    }

    PROFILER_START()
    m_idle.wait(lock, [this]() { return m_queue.size() < m_maxQueuedFrames; });
    PROFILER_STOP("DVB-T2 demodulation waiting for FEC")
    m_queue.push_back(std::move(m_current));

    if (m_freeFrames.empty())
    {
        m_current = InterleavingFrame();
    }
    else
    {
        m_current = std::move(m_freeFrames.back());
        m_freeFrames.pop_back();
    }

    lock.unlock();
    m_frameAvailable.notify_one();
}

void Receiver::worker(int index)
{
    while (true)
    {
        InterleavingFrame frame;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_frameAvailable.wait(lock, [this]() { return m_stop || !m_queue.empty(); });

            if (m_stop) {
                return;
            }

            frame = std::move(m_queue.front());
            m_queue.pop_front();
            m_busy++;
        }

        m_idle.notify_all();
        decodeInterleavingFrame(frame, *m_fecs[index]);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_busy--;

            if (m_freeFrames.size() < 2) {
                m_freeFrames.push_back(std::move(frame));
            }
        }

        m_idle.notify_all();
    }
}

void Receiver::Turn::take()
{
    if (m_taken) {
        return;
    }

    if (m_receiver.m_threaded)
    {
        std::unique_lock<std::mutex> lock(m_receiver.m_mutex);
        m_receiver.m_turn.wait(lock, [this]() { return m_receiver.m_finishSequence == m_sequence; });
    }

    m_taken = true;
}

// Taken first if it wasn't, so the frame still finishes in order
Receiver::Turn::~Turn()
{
    take();

    if (m_receiver.m_threaded)
    {
        {
            std::lock_guard<std::mutex> lock(m_receiver.m_mutex);
            m_receiver.m_finishSequence++;
        }

        m_receiver.m_turn.notify_all();
    }
    else
    {
        m_receiver.m_finishSequence++;
    }
}

void Receiver::decodeInterleavingFrame(InterleavingFrame& frame, FEC& fec)
{
    Turn turn(*this, frame.m_sequence);

    // Out of memory (as for the buffers of a larger frame than before), the frame is lost, rather than the process (the
    // exception would reach the worker thread's end). The FEC is set up again for the next, as it may be part way, and
    // the BBFRAME decoder, in turn, told of the gap, as the next frame would seem to follow this
    try
    {
        decodeFrame(frame, fec, turn);
    }
    catch (const std::exception&)
    {
        fec.m_ldpc = nullptr;
        turn.take();
        m_bbDecoder.discontinuity();
    }
}

// Time, cell and bit de-interleaving and demapping (6.5, 6.4, 6.3, 6.2, 6.1.3), then the FEC
void Receiver::decodeFrame(InterleavingFrame& frame, FEC& fec, Turn& turn)
{
    const PLP& plp = frame.m_plp;
    double start = now();
    // The settings, as they are now, for the whole interleaving frame
    fec.m_maxIterations = m_maxIterations;
    fec.m_outlierLimiting = m_outlierLimiting;
    fec.m_analysis = m_analysis;

    if (!fec.m_ldpc || (fec.m_type != plp.m_fecType) || (fec.m_rate != plp.m_codeRate) || (fec.m_constellation != plp.m_constellation))
    {
        fec.m_type = plp.m_fecType;
        fec.m_rate = plp.m_codeRate;
        fec.m_constellation = plp.m_constellation;
        fec.m_llrs.clear(); // They may be for the other block length
        fec.m_ldpc = &fec.m_context.decoder(CodeFamily::T2, plp.m_fecType, plp.m_codeRate);
        fec.m_bch.reset(new BCH(plp.m_fecType, bchT(plp.m_fecType, plp.m_codeRate)));
        fec.m_cellInterleaver.reset(new CellInterleaver(cellsPerFECBlock(plp.m_fecType, plp.m_constellation)));
        cellBitMap(plp.m_fecType, plp.m_codeRate, plp.m_constellation, fec.m_bitMap);
    }

    const int ncells = cellsPerFECBlock(plp.m_fecType, plp.m_constellation);
    const int nblocks = plp.m_numBlocks;
    const bool ti = plp.m_timeIlLength > 0;
    const int nti = ti ? plp.tiBlocks() : nblocks; // Without time interleaving, each FEC block is on its own

    if ((int) fec.m_llrs.size() < nblocks) {
        fec.m_llrs.resize(nblocks, std::vector<int8_t>(nldpc(plp.m_fecType)));
    }

    // Cells much less reliable than average (deep fades, or no channel estimate yet) are left out of the MER
    double meanScale = 0.0;

    for (size_t i = 0; i < frame.m_scales.size(); i += 64) {
        meanScale += frame.m_scales[i];
    }

    const float merThreshold = (float) (0.1 * meanScale / std::max<size_t>(1, (frame.m_scales.size() + 63) / 64));
    // Cells to display, spread over the frame
    const int displayStep = std::max(1, (nblocks * ncells) / 2000);

    // Time de-interleaving, into the cells of each FEC block in turn. Each block can then be demapped on its own
    fec.m_cells.resize((size_t) nblocks * ncells);
    fec.m_scales.resize((size_t) nblocks * ncells);
    fec.m_blockR.resize(nblocks);
    size_t offset = 0;
    int block = 0;
    int maxNfec = 1;

    for (int s = 0; s < nti; s++)
    {
        const int nfec = ti ? nblocks / nti + (s >= nti - (nblocks % nti) ? 1 : 0) : 1;
        const int size = nfec * ncells;
        Cf *d = &fec.m_cells[(size_t) block * ncells];
        float *ds = &fec.m_scales[(size_t) block * ncells];
        maxNfec = std::max(maxNfec, nfec);

        if (ti)
        {
            // As timeInterleaver(): cells written into the columns of NFEC blocks of 5 columns of Nr rows (6.5.3), and
            // read out a row at a time, so cell i of the TI-block is (row i / (5 NFEC), column i mod (5 NFEC)), which
            // is block column / 5's cell (column mod 5) Nr + row
            const int nr = ncells / 5;
            const Cf *source = &frame.m_cells[offset];
            const float *sourceScales = &frame.m_scales[offset];

            for (int row = 0; row < nr; row++)
            {
                for (int r = 0; r < nfec; r++)
                {
                    Cf *dr = d + (size_t) r * ncells + row;
                    float *dsr = ds + (size_t) r * ncells + row;

                    for (int c = 0; c < 5; c++)
                    {
                        dr[c * nr] = *source++;
                        dsr[c * nr] = *sourceScales++;
                    }
                }
            }
        }
        else
        {
            std::copy(frame.m_cells.begin() + offset, frame.m_cells.begin() + offset + size, d);
            std::copy(frame.m_scales.begin() + offset, frame.m_scales.begin() + offset + size, ds);
        }

        offset += size;

        for (int r = 0; r < nfec; r++) {
            fec.m_blockR[block++] = ti ? r : 0;
        }
    }

    // The cell interleaver works out each block's shift when first asked for it, so ask now, before the threads
    fec.m_cellInterleaver->shift(maxNfec - 1);
    fec.m_blockDisplay.resize(nblocks);

    if (fec.m_analysis)
    {
        fec.m_analysisRotation = plp.m_rotation;
        fec.m_analysisCells.resize(std::max<size_t>(fec.m_analysisCells.size(), nblocks));
        fec.m_analysisScales.resize(std::max<size_t>(fec.m_analysisScales.size(), nblocks));
    }

    double deinterleaveEnd = now();

    // Each batch of as many blocks as the LDPC decoder's SIMD width is demapped, LDPC decoded, then BCH decoded, as
    // each block's is independent, by the worker or a decoder thread. Only the BBFRAMEs need doing in order
    const int lanes = fec.m_ldpc->lanes();
    const int batches = (nblocks + lanes - 1) / lanes;
    fec.m_iterations.assign(batches, 0);
    fec.m_unconverged.assign(batches, 0);

    const size_t codewordBytes = kldpc(plp.m_fecType, plp.m_codeRate) / 8;
    fec.m_packed.resize(nblocks);
    fec.m_corrected.assign(nblocks, 0);

    for (std::vector<uint8_t>& packed : fec.m_packed) {
        packed.resize(codewordBytes);
    }

    // Each batch's own, so the sums don't depend on which thread decoded which
    std::vector<double> errorPowers(batches, 0.0);
    std::vector<int> errorCounts(batches, 0);
    LDPCPool::Job job;
    job.m_family = CodeFamily::T2;
    job.m_type = plp.m_fecType;
    job.m_rate = plp.m_codeRate;
    job.m_batches = batches;
    job.m_decode = [&](LDPC& ldpc, LDPCPool::Context& context, int b) {
        const int count = std::min(lanes, nblocks - b * lanes);

        if (!context.m_scratch) {
            context.m_scratch.reset(new DemapScratch());
        }

        DemapScratch& scratch = static_cast<DemapScratch&>(*context.m_scratch);

        PROFILER_START()

        for (int i = 0; i < count; i++) {
            demapBlock(fec, plp, b * lanes + i, merThreshold, displayStep, scratch, errorPowers[b], errorCounts[b]);
        }

        PROFILER_STOP("DVB-T2 demapping (a batch)")
        PROFILER_RESTART()
        fec.m_iterations[b] = decodeLDPC(fec, ldpc, b, count, scratch);
        PROFILER_STOP("DVB-T2 LDPC (a batch)")
        PROFILER_RESTART()
        decodeBCH(fec, b * lanes, count);
        PROFILER_STOP("DVB-T2 BCH (a batch)")
    };
    // An exception in the pool (out of memory) fails the frame's blocks, rather than ending the process (it would
    // reach the worker thread's end), and the frame still takes its turn
    try
    {
        m_pool->run(job, fec.m_context);
    }
    catch (const std::exception&)
    {
        for (int b = 0; b < nblocks; b++) {
            fec.m_corrected[b] = -1;
        }

        for (int b = 0; b < batches; b++)
        {
            fec.m_iterations[b] = 0;
            errorPowers[b] = 0.0;
            errorCounts[b] = 0;
        }
    }

    double errorPower = 0.0;
    int errorCount = 0;

    for (int b = 0; b < batches; b++)
    {
        errorPower += errorPowers[b];
        errorCount += errorCounts[b];
    }

    fec.m_displayCells.clear();

    for (int b = 0; (b < nblocks) && (fec.m_displayCells.size() < 2000); b++) {
        fec.m_displayCells.insert(fec.m_displayCells.end(), fec.m_blockDisplay[b].begin(), fec.m_blockDisplay[b].end());
    }

    if (fec.m_displayCells.size() > 2000) {
        fec.m_displayCells.resize(2000);
    }

    double ldpcEnd = now();

    // The BBFRAMEs in the order the frames were received
    turn.take();
    double bbStart = now();

    if (frame.m_gap) {
        m_bbDecoder.discontinuity();
    }

    decodeBlocks(fec, nblocks);
    double end = now();

    // The statistics and display too in frame order, before the next frame's turn, so the averages don't depend on
    // which worker finished first
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.m_seconds[1] += deinterleaveEnd - start;
        m_status.m_seconds[2] += ldpcEnd - deinterleaveEnd;
        m_status.m_seconds[3] += end - bbStart;

        {
            std::lock_guard<std::mutex> displayLock(m_displayMutex);
            m_displayCells.swap(fec.m_displayCells);
        }

        if (errorCount > 0)
        {
            double mer = 10.0 * std::log10(errorCount / std::max(errorPower, 1e-12));
            m_status.m_mer = m_status.m_mer == 0.0 ? mer : 0.8 * m_status.m_mer + 0.2 * mer;
        }

        for (int b = 0; b < batches; b++)
        {
            const int it = fec.m_iterations[b];
            m_status.m_ldpcIterations = m_status.m_ldpcIterations == 0.0 ? it : 0.95 * m_status.m_ldpcIterations + 0.05 * it;
            m_status.m_ldpcFailures += fec.m_unconverged[b];
        }
    }

    // The turn passes to the next frame when the caller's Turn ends
    PROFILER_ADD("DVB-T2 time de-interleaving", (qint64) ((deinterleaveEnd - start) * 1e9))
    PROFILER_ADD("DVB-T2 demapping, LDPC and BCH", (qint64) ((ldpcEnd - deinterleaveEnd) * 1e9))
    PROFILER_ADD("DVB-T2 BBFRAMEs", (qint64) ((end - bbStart) * 1e9))
}

// Decode a batch of blocks, from m_llrs[batch * lanes], with the thread's decoder
int Receiver::decodeLDPC(FEC& fec, LDPC& ldpc, int batch, int nbBlocks, DemapScratch& scratch)
{
    std::vector<int8_t *>& blocks = scratch.m_blocks;
    blocks.resize(nbBlocks);

    for (int i = 0; i < nbBlocks; i++) {
        blocks[i] = fec.m_llrs[batch * ldpc.lanes() + i].data();
    }

    return ldpc.decode(blocks.data(), nbBlocks, fec.m_maxIterations, &fec.m_unconverged[batch]);
}

// Re-encode a decoded block, and compare the cells that would have been sent with those received
void Receiver::analyseBlock(FEC& fec, int block, const uint8_t *packed)
{
    if (block >= (int) fec.m_analysisCells.size()) {
        return;
    }

    LDPC& ldpc = *fec.m_ldpc;
    const int k = ldpc.k();
    const int eta = bitsPerCell(fec.m_constellation);
    const int ncells = ldpc.n() / eta;
    std::vector<uint8_t> codeword(ldpc.n());

    for (int i = 0; i < k; i++) {
        codeword[i] = (packed[i / 8] >> (7 - (i % 8))) & 1;
    }

    ldpc.encode(codeword.data(), codeword.data() + k);
    std::vector<Cf> f(ncells);
    uint8_t bits[8];

    for (int q = 0; q < ncells; q++)
    {
        for (int e = 0; e < eta; e++) {
            bits[e] = codeword[fec.m_bitMap[q * eta + e]];
        }

        f[q] = mapCell(bits, eta);
    }

    // Rotated as sent: the real part of cell q from f[q], the imaginary part from f[q - 1]
    const bool rotated = fec.m_analysisRotation;
    Cf r = std::polar(1.0f, rotationAngle(fec.m_constellation));
    const std::vector<Cf>& received = fec.m_analysisCells[block];
    const std::vector<float>& scales = fec.m_analysisScales[block];
    std::vector<float> sorted(scales);
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const float median = sorted[sorted.size() / 2];
    double e0 = 0.0, e1 = 0.0;
    uint64_t outliers[3] = {0, 0, 0};
    double byScale[8] = {0.0};
    uint64_t countByScale[8] = {0};

    for (int q = 0; q < ncells; q++)
    {
        Cf sent = rotated ? Cf((r * f[q]).real(), (r * f[(q + ncells - 1) % ncells]).imag()) : f[q];
        Cf error = received[q] - sent;
        double variance = scales[q] > 0.0f ? 4.0 / scales[q] : 1e9; // Per dimension, as the LLRs assume
        double a = error.real() * error.real() / variance;
        double b = error.imag() * error.imag() / variance;
        e0 += a;
        e1 += b;
        outliers[0] += (a > 9.0) + (b > 9.0);
        outliers[1] += (a > 16.0) + (b > 16.0);
        outliers[2] += (a > 25.0) + (b > 25.0);
        int octave = scales[q] > 0.0f ? (int) std::floor(std::log2(scales[q] / median)) + 4 : 0;
        octave = std::max(0, std::min(7, octave));
        byScale[octave] += a + b;
        countByScale[octave] += 2;
    }

    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_status.m_analysedCells += ncells;
    m_status.m_normalisedError[0] += e0;
    m_status.m_normalisedError[1] += e1;

    for (int i = 0; i < 3; i++) {
        m_status.m_outliers[i] += outliers[i];
    }

    for (int i = 0; i < 8; i++)
    {
        m_status.m_errorByScale[i] += byScale[i];
        m_status.m_cellsByScale[i] += countByScale[i];
    }
}

// BCH, descrambling and BBFRAMEs, in order
// Cell de-interleaving (6.4), the cells of rotated constellations recombined (6.3), outlier limiting and demapping
void Receiver::demapBlock(FEC& fec, const PLP& plp, int block, float merThreshold, int displayStep, DemapScratch& s,
    double& errorPower, int& errorCount)
{
    const int ncells = cellsPerFECBlock(plp.m_fecType, plp.m_constellation);
    const int eta = bitsPerCell(plp.m_constellation);
    const Cf *d = &fec.m_cells[(size_t) block * ncells];
    const float *ds = &fec.m_scales[(size_t) block * ncells];
    const int r = fec.m_blockR[block];
    s.m_g.resize(ncells);
    s.m_gs.resize(ncells);
    s.m_re.resize(ncells);
    s.m_im.resize(ncells);
    s.m_scales.resize(ncells);
    s.m_scalesQ.resize(ncells);
    s.m_errors.resize(ncells);
    // The cell interleaver's addresses, (l0[q] + shift) mod ncells, each term less than ncells
    const int *l0 = fec.m_cellInterleaver->l0();
    const int shift = fec.m_cellInterleaver->shift(r);

    for (int q = 0; q < ncells; q++)
    {
        int a = l0[q] + shift;
        a = a >= ncells ? a - ncells : a;
        s.m_g[q] = d[a];
        s.m_gs[q] = ds[a];
    }

    if (fec.m_analysis)
    {
        fec.m_analysisCells[block] = s.m_g;
        fec.m_analysisScales[block] = s.m_gs;
    }

    if (plp.m_rotation)
    {
        // The imaginary part of a rotated cell is carried by the next one. Both axes are demapped with one reliability,
        // the harmonic mean of the two cells'. Each axis's own (its noise variance, relative to a cell of scale 1, is
        // cos^2 a / si + sin^2 a / sq for the real part, sin^2 a / si + cos^2 a / sq for the imaginary) decoded strong
        // echoes far better in loopback, but off air (PSB3 at 26 dB MER, 20 s from a B210) it left 4 times as many
        // blocks that BCH couldn't correct, as one cell's noise estimate is then trusted alone
        const Cf rotation = std::polar(1.0f, -rotationAngle(plp.m_constellation));

        for (int q = 0; q < ncells; q++)
        {
            int next = q + 1 < ncells ? q + 1 : 0;
            Cf cell = Cf(s.m_g[q].real(), s.m_g[next].imag()) * rotation;
            float si = s.m_gs[q], sq = s.m_gs[next];
            s.m_re[q] = cell.real();
            s.m_im[q] = cell.imag();
            s.m_scales[q] = s.m_scalesQ[q] = si + sq > 0.0f ? 2.0f * si * sq / (si + sq) : 0.0f;
        }
    }
    else
    {
        for (int q = 0; q < ncells; q++)
        {
            s.m_re[q] = s.m_g[q].real();
            s.m_im[q] = s.m_g[q].imag();
            s.m_scales[q] = s.m_gs[q];
        }
    }

    // A cell further from the nearest constellation point than noise is likely to put it (Gaussian noise does so
    // about once in a million cells) has probably been hit by interference or impulsive noise, so is less
    // reliable than it would appear. Its LLRs are scaled inversely to its error power, to limit their influence
    const float threshold = fec.m_outlierLimiting ? m_outlierThreshold : 0.0f;

    // Each 16th cell's reliability for the MER, before outlier limiting scales it down: the MER's threshold leaves out
    // faded cells, not those hit by impulses (which would read it high). A rotated cell's is the harmonic mean of its
    // axes' (the same as that of the two cells it came from)
    s.m_merScales.resize((ncells + 15) / 16);

    for (int q = 0; q < ncells; q += 16)
    {
        const float si = s.m_scales[q], sq = plp.m_rotation ? s.m_scalesQ[q] : 0.0f;
        s.m_merScales[q / 16] = !plp.m_rotation ? si : si + sq > 0.0f ? 2.0f * si * sq / (si + sq) : 0.0f;
    }

    if (plp.m_rotation)
    {
        demapCellsAxes(s.m_re.data(), s.m_im.data(), s.m_scales.data(), s.m_scalesQ.data(), ncells, eta, threshold,
            fec.m_bitMap.data(), fec.m_llrs[block].data(), s.m_errors.data());
    }
    else
    {
        demapCells(s.m_re.data(), s.m_im.data(), s.m_scales.data(), ncells, eta, threshold, fec.m_bitMap.data(),
            fec.m_llrs[block].data(), s.m_errors.data());
    }

    // Error to the nearest constellation point, for the MER
    for (int q = 0; q < ncells; q += 16)
    {
        if (s.m_merScales[q / 16] > merThreshold)
        {
            errorPower += s.m_errors[q];
            errorCount++;
        }
    }

    std::vector<Cf>& display = fec.m_blockDisplay[block];
    display.clear();

    for (int q = (displayStep - ((block * ncells) % displayStep)) % displayStep; q < ncells; q += displayStep) {
        display.push_back(Cf(s.m_re[q], s.m_im[q]));
    }
}

// Hard decisions on the LDPC decoded blocks' BCH codewords, corrected
void Receiver::decodeBCH(FEC& fec, int first, int nbBlocks)
{
    for (int b = first; b < first + nbBlocks; b++)
    {
        const int8_t *llrs = fec.m_llrs[b].data();
        std::vector<uint8_t>& packed = fec.m_packed[b];

        // 8 LLRs at a time: their sign bits, one per byte, gathered by a multiply into the top byte, the first LLR's
        // the MSB (bit 8j + 9i of the product is LLR j's from term i, so bit 56 + i is LLR 7 - i's). The bytes of
        // the 64-bit word are in memory order on little endian processors
        for (size_t i = 0; i < packed.size(); i++)
        {
            uint64_t v;
            std::memcpy(&v, llrs + 8 * i, 8);
            v = (v >> 7) & 0x0101010101010101ULL;
            packed[i] = (uint8_t) ((v * 0x8040201008040201ULL) >> 56);
        }

        fec.m_corrected[b] = fec.m_bch->decode(packed.data(), (int) packed.size());
    }
}

void Receiver::decodeBlocks(FEC& fec, int nbBlocks)
{
    const int kbchBits = kbch(fec.m_type, fec.m_rate);
    int bchFailures = 0, bchCorrected = 0, bchCorrectedBlocks = 0;

    for (int b = 0; b < nbBlocks; b++)
    {
        std::vector<uint8_t>& packed = fec.m_packed[b];
        int corrected = fec.m_corrected[b];
        bool bad = corrected < 0;

        if (fec.m_analysis && !bad) {
            analyseBlock(fec, b, packed.data());
        }

        bchFailures += bad;
        bchCorrected += std::max(0, corrected);
        bchCorrectedBlocks += corrected > 0;
        scrambleBytes(packed.data(), kbchBits / 8);
        m_bbDecoder.process(packed.data(), kbchBits, bad);
    }

    // The frame's packets for the caller's thread, with one lock
    if (!m_bbPackets.empty())
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_packets.empty()) {
                m_packets.swap(m_bbPackets);
            } else {
                m_packets.insert(m_packets.end(), m_bbPackets.begin(), m_bbPackets.end());
            }
        }

        m_bbPackets.clear();
    }

    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_status.m_fecBlocks += nbBlocks;
    m_status.m_bchFailures += bchFailures;
    m_status.m_bchCorrected += bchCorrected;
    m_status.m_bchCorrectedBlocks += bchCorrectedBlocks;
    m_status.m_bb = m_bbDecoder.stats();
}

} // namespace dvbt2
