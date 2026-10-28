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
#include <iterator>

#include "util/profiler.h"

#include "dvbtreceiver.h"
#include "samplethread.h"

namespace dvbt {

const double Receiver::m_clockCorrectionThreshold = 3.0;
const double Receiver::m_viterbiAddLoad = 0.85;
const double Receiver::m_viterbiRemoveLoad = 0.6;

double Receiver::now()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

Receiver::Receiver(int bandwidthHz, FFTFactory *fftFactory, bool threaded) :
    m_outputRate(sampleRate(bandwidthHz)),
    m_inputRate(sampleRate(bandwidthHz)),
    m_resampling(false),
    m_samplesPushed(0),
    m_samplesDemodulated(0),
    m_clockUpdateSymbols(0),
    m_ofdm(bandwidthHz, fftFactory),
    m_ofdmSeconds(0.0),
    m_displayRequested(false),
    m_displayMaxCells(0),
    m_displayPoints(0),
    m_threaded(threaded),
    m_innerBusy(false),
    m_busy(false),
    m_stop(false),
    m_queueWaits(0)
{
    m_ofdm.setSymbolCallback([this](const OFDMDemod::Symbol& symbol) {
        onSymbol(symbol);
    });
    m_fec.setPacketCallback([this](const uint8_t *packet) {
        onPacket(packet);
    });
    m_ofdmStatus = m_ofdm.getStatus();
    m_canceller.configure(m_inputRate, 0.83 * m_outputRate); // The occupied bandwidth, 7.61 MHz in 8

    if (m_threaded)
    {
        m_thread = std::thread(&Receiver::runFEC, this);
        m_innerThread = std::thread(&Receiver::runInner, this);
        m_demodThread.reset(new SampleThread([this](const Cf *samples, int nbSamples) {
            demodulate(samples, nbSamples);
        }));
    }
}

Receiver::~Receiver()
{
    m_demodThread.reset(); // Before error correction, which it feeds

    if (m_threaded)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }

        m_symbolAvailable.notify_all();
        m_spaceAvailable.notify_all();
        m_demappedAvailable.notify_all();
        m_demappedSpace.notify_all();
        m_innerThread.join();
        m_thread.join();
    }
}

void Receiver::reset()
{
    if (m_demodThread) {
        m_demodThread->discard();
    }

    m_ofdm.reset();
    m_clockUpdateSymbols = 0;

    std::vector<double> notches;
    double inputRate;
    {
        std::lock_guard<std::mutex> lock(m_ofdmMutex);
        m_ofdmStatus = m_ofdm.getStatus();
        m_displayCells.clear();
        m_displayResponse.clear();
        m_clockAdjustments.clear();
        m_samplesPushed = 0;
        m_samplesDemodulated = 0;
        // The notches the samples from now on are notched with (the canceller keeps them)
        m_notchChanges.clear();
        m_notchFrequencies.clear();

        for (const auto& notch : m_canceller.getNotches()) {
            m_notchFrequencies.push_back(notch.m_frequency * m_inputRate);
        }

        m_notchesQueued = m_notchFrequencies;
        notches = m_notchesQueued;
        inputRate = m_inputRate;
    }

    m_ofdm.setNotches(notches, inputRate); // The demodulator thread isn't running

    if (m_threaded)
    {
        // Discard what is queued, and have the worker reset error correction before anything else
        std::lock_guard<std::mutex> lock(m_mutex);

        while (!m_queue.empty())
        {
            m_free.push_back(std::move(m_queue.front()));
            m_queue.pop_front();
        }

        while (!m_demappedQueue.empty())
        {
            m_demappedFree.push_back(std::move(m_demappedQueue.front()));
            m_demappedQueue.pop_front();
        }

        m_demappedSpace.notify_all();

        // Packets from symbols already being decoded are dropped until this reset has passed through decoding
        QueuedSymbol resetSymbol;
        resetSymbol.m_reset = true;
        resetSymbol.m_index = 0;
        resetSymbol.m_epoch = ++m_resetEpoch;
        m_queue.push_back(std::move(resetSymbol));
        m_packets.clear();
        m_symbolAvailable.notify_one();
    }
    else
    {
        m_fec.reset();
    }
}

void Receiver::setInputSampleRate(double sampleRate)
{
    {
        std::lock_guard<std::mutex> lock(m_ofdmMutex); // The demodulator thread reads it
        m_inputRate = sampleRate;
    }

    m_canceller.configure(sampleRate, 0.83 * m_outputRate);
    m_resampling = std::abs(sampleRate / m_outputRate - 1.0) > 1e-6;
    m_resampler.setRatioCorrection(1.0);

    if (m_resampling) {
        m_resampler.configure(m_inputRate, m_outputRate, 0.4165 * m_outputRate); // Half the occupied bandwidth
    }

    reset();
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
        m_resampled.clear();
        m_resampler.process(samples, nbSamples, m_resampled);
        input = m_resampled.data();
        nbInput = (int) m_resampled.size();
    }
    else
    {
        int keep = std::min(nbSamples, 64);
        m_lastInput.assign(samples + nbSamples - keep, samples + nbSamples);
    }

    {
        std::lock_guard<std::mutex> lock(m_ofdmMutex);
        m_ofdmSeconds += now() - start;

        m_notchFrequencies.clear();

        for (const auto& notch : m_canceller.getNotches()) {
            m_notchFrequencies.push_back(notch.m_frequency * m_inputRate);
        }

        // The canceller notches all of this call's samples with them, so they apply from its first
        if (m_notchFrequencies != m_notchesQueued)
        {
            m_notchesQueued = m_notchFrequencies;
            m_notchChanges.push_back({m_samplesPushed, m_notchFrequencies});
        }
    }

    PROFILER_STOP("DVB-T resampling")

    if (m_demodThread) {
        m_demodThread->push(input, nbInput);
    } else {
        demodulate(input, nbInput);
    }

    m_samplesPushed += nbInput;

    correctClock();
    PROFILER_RESTART()
    deliverPackets();
    PROFILER_STOP("DVB-T packet delivery")
}

// OFDM demodulation, and what other threads need of it
void Receiver::demodulate(const Cf *samples, int nbSamples)
{
    PROFILER_START()
    double start = now();

    // Clock corrections and notches from these samples on
    {
        std::lock_guard<std::mutex> lock(m_ofdmMutex);

        while (!m_clockAdjustments.empty() && (m_clockAdjustments.front().m_sample <= m_samplesDemodulated))
        {
            m_ofdm.adjustClockOffset(m_clockAdjustments.front().m_ppm);
            m_clockAdjustments.pop_front();
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
        m_ofdmSeconds += now() - start;

        if (m_displayRequested)
        {
            m_ofdm.getDisplayData(m_displayCells, m_displayMaxCells, m_displayResponse, m_displayPoints);
            m_ofdm.getImpulseResponse(m_displayImpulse, m_displayPoints);
            m_displayRequested = false;
        }
    }

    PROFILER_STOP("DVB-T demodulation")
}

OFDMDemod::Status Receiver::ofdmStatus() const
{
    std::lock_guard<std::mutex> lock(m_ofdmMutex);
    return m_demodThread ? m_ofdmStatus : m_ofdm.getStatus();
}

void Receiver::getDisplayData(std::vector<Cf>& cells, int maxCells, std::vector<float>& responseDb, int points) const
{
    if (!m_demodThread)
    {
        m_displayPoints = points; // For getImpulseResponse()
        m_ofdm.getDisplayData(cells, maxCells, responseDb, points);
        return;
    }

    std::lock_guard<std::mutex> lock(m_ofdmMutex);
    cells = m_displayCells;
    responseDb = m_displayResponse;
    m_displayRequested = true;
    m_displayMaxCells = maxCells;
    m_displayPoints = points;
}

void Receiver::getImpulseResponse(ImpulseResponse& response) const
{
    if (!m_demodThread)
    {
        m_ofdm.getImpulseResponse(response, std::max(1, m_displayPoints));
        return;
    }

    std::lock_guard<std::mutex> lock(m_ofdmMutex);
    response = m_displayImpulse;
}

// Trim the resampling ratio by the measured clock offset, starting resampling if needed
void Receiver::correctClock()
{
    OFDMDemod::Status status = ofdmStatus();

    // First once the offset has been averaged over a few symbols, as large offsets spoil reception until corrected
    // (from 40 ppm in 8K, by interference between the carriers). Then every so often
    if ((status.m_state < OFDMDemod::TPSSync) || (status.m_clockMeasurements < m_clockFirstMeasurements)
        || ((m_clockUpdateSymbols > 0) && (status.m_symbols < m_clockUpdateSymbols + m_clockCorrectionPeriod))) {
        return;
    }

    const bool first = m_clockUpdateSymbols == 0;
    m_clockUpdateSymbols = status.m_symbols;
    double ppm = status.m_clockOffset;

    if (!m_resampling)
    {
        if (std::abs(ppm) < m_clockCorrectionThreshold) {
            return;
        }

        m_resampling = true;
        m_resampler.setRatioCorrection(1.0);
        double inputRate;
        {
            std::lock_guard<std::mutex> lock(m_ofdmMutex);
            inputRate = m_inputRate;
        }
        m_resampler.configure(inputRate, m_outputRate, 0.4165 * m_outputRate);
        m_resampler.prime(m_lastInput);
        setRatioCorrection(1.0 - ppm * 1e-6); // The measurement is accurate, so start with all of it
        return;
    }

    // A positive offset means each sample spans more time than it should, so take fewer input samples per output
    // All of the first measurement, as when resampling starts, also when the input was resampled from another rate
    setRatioCorrection(m_resampler.getRatioCorrection() * (1.0 - (first ? 1.0 : 0.5) * ppm * 1e-6));
}

// And the demodulator's clock offset changes with it, from the samples resampled with it
void Receiver::setRatioCorrection(double ratio)
{
    const double change = (ratio - m_resampler.getRatioCorrection()) * 1e6;
    m_resampler.setRatioCorrection(ratio);
    std::lock_guard<std::mutex> lock(m_ofdmMutex);
    m_clockAdjustments.push_back({m_samplesPushed, change});
}

void Receiver::onSymbol(const OFDMDemod::Symbol& symbol)
{
    if (m_symbolObserver) {
        m_symbolObserver(symbol);
    }

    if (!m_threaded)
    {
        m_fec.decode(symbol.m_index, *symbol.m_tps, symbol.m_cells, symbol.m_csi, symbol.m_nbCells);
        return;
    }

    // Copied (about 72 kB in 8K) before taking the lock, which the other threads take too, into a buffer they've
    // finished with
    m_filling.m_reset = false;
    m_filling.m_index = symbol.m_index;
    m_filling.m_tps = *symbol.m_tps;
    m_filling.m_cells.assign(symbol.m_cells, symbol.m_cells + symbol.m_nbCells);
    m_filling.m_csi.assign(symbol.m_csi, symbol.m_csi + symbol.m_nbCells);

    std::unique_lock<std::mutex> lock(m_mutex);

    // Wait rather than drop symbols, so decoding a recording is lossless. In real time, error correction normally
    // keeps up easily, so this only happens on overload
    if (m_queue.size() >= m_maxQueuedSymbols)
    {
        m_queueWaits++;
        PROFILER_START()
        m_spaceAvailable.wait(lock, [this] { return m_stop || (m_queue.size() < m_maxQueuedSymbols / 2); });
        PROFILER_STOP("DVB-T demodulation waiting for FEC")
    }

    m_queue.push_back(std::move(m_filling));

    if (!m_free.empty())
    {
        m_filling = std::move(m_free.back());
        m_free.pop_back();
    }
    else
    {
        m_filling = QueuedSymbol();
    }

    lock.unlock();
    m_symbolAvailable.notify_one();
}

// In the worker thread, if threaded
void Receiver::onPacket(const uint8_t *packet)
{
    if (!m_threaded)
    {
        if (m_packetCallback) {
            m_packetCallback(packet);
        }

        return;
    }

    // Gathered without the lock, and added to m_packets after the call that decoded them
    m_fecPackets.insert(m_fecPackets.end(), packet, packet + tsPacketSize);
}

// Inner de-interleaving and de-mapping, in its own thread, as the first stage of error correction
void Receiver::runInner()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    while (true)
    {
        m_symbolAvailable.wait(lock, [this] { return m_stop || !m_queue.empty(); });

        if (m_stop) {
            break;
        }

        QueuedSymbol symbol = std::move(m_queue.front());
        m_queue.pop_front();
        QueuedDemapped demapped;

        if (!m_demappedFree.empty())
        {
            demapped = std::move(m_demappedFree.back());
            m_demappedFree.pop_back();
        }

        m_innerBusy = true;
        lock.unlock();
        m_spaceAvailable.notify_one();

        PROFILER_START()
        demapped.m_reset = symbol.m_reset;
        demapped.m_epoch = symbol.m_epoch;

        if (symbol.m_reset)
        {
            m_fec.resetDemap();
        }
        else
        {
            // An exception (out of memory) loses the symbol, not the process
            try
            {
                m_fec.demap(symbol.m_index, symbol.m_tps, symbol.m_cells.data(), symbol.m_csi.data(),
                    (int) symbol.m_cells.size(), demapped.m_demapped);
            }
            catch (const std::exception&)
            {
                // Decoding starts again after the gap
                demapped.m_demapped.m_llrs.clear();
                demapped.m_demapped.m_reconfigured = true;
            }
        }

        PROFILER_STOP("DVB-T inner")
        lock.lock();
        m_free.push_back(std::move(symbol));

        // The buffers an overload queued up (up to 37 MB) aren't all kept once it's over
        if (m_queue.empty() && (m_free.size() > m_maxFreeSymbols))
        {
            std::vector<QueuedSymbol> excess(std::make_move_iterator(m_free.begin() + m_maxFreeSymbols),
                std::make_move_iterator(m_free.end()));
            m_free.resize(m_maxFreeSymbols);
            lock.unlock();
            excess.clear(); // Freed without the lock
            lock.lock();
        }

        if (m_demappedQueue.size() >= m_maxQueuedDemapped)
        {
            m_queueWaits++;
            PROFILER_START()
            m_demappedSpace.wait(lock, [this] { return m_stop || (m_demappedQueue.size() < m_maxQueuedDemapped / 2); });
            PROFILER_STOP("DVB-T inner waiting for FEC")

            if (m_stop) {
                break;
            }
        }

        m_demappedQueue.push_back(std::move(demapped));
        m_innerBusy = false;
        m_demappedAvailable.notify_one();
    }
}

void Receiver::setViterbiThreads(int threads)
{
    std::lock_guard<std::mutex> lock(m_mutex); // Not to cross with runFEC() changing them
    m_autoViterbi = false;
    m_fec.setViterbiThreads(threads);
}

// Viterbi and outer decoding, in its own thread, after de-mapping
void Receiver::runFEC()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_fecWindowStart = std::chrono::steady_clock::now();
    // The OFDM, inner and this thread each take a core
    const int maxViterbiThreads = std::min(3, std::max(0, (int) std::thread::hardware_concurrency() - 3));

    while (true)
    {
        m_demappedAvailable.wait(lock, [this] { return m_stop || !m_demappedQueue.empty(); });

        if (m_stop) {
            break;
        }

        QueuedDemapped demapped = std::move(m_demappedQueue.front());
        m_demappedQueue.pop_front();
        m_busy = true;
        lock.unlock();
        m_demappedSpace.notify_one();

        PROFILER_START()
        const auto start = std::chrono::steady_clock::now();

        if (demapped.m_reset)
        {
            m_fec.resetDecode();
            lock.lock();
            m_decodeEpoch = demapped.m_epoch; // Packets are wanted again, if there's been no reset since
            lock.unlock();
        }
        else
        {
            try {
                m_fec.decodeDemapped(demapped.m_demapped);
            } catch (const std::exception&) {
                // Lost, not the process
            }
        }

        PROFILER_STOP("DVB-T FEC")

        const auto end = std::chrono::steady_clock::now();
        m_fecBusy += std::chrono::duration<double>(end - start).count();
        FECDecoder::Status status = m_fec.getStatus();
        lock.lock();

        // The packets from this call, unless there's been a reset since
        if (!m_fecPackets.empty())
        {
            if (m_decodeEpoch == m_resetEpoch) {
                m_packets.insert(m_packets.end(), m_fecPackets.begin(), m_fecPackets.end());
            }

            m_fecPackets.clear();
        }

        // On a slow CPU, this thread may not keep up: busy nearly all the time, with symbols waiting for it. Then
        // Viterbi decoding is shared with more threads, one at a time. When it would be comfortably under that with
        // one fewer, for a while (as after a start-up hiccup), one goes. Either way the output is the same
        const double window = std::chrono::duration<double>(end - m_fecWindowStart).count();

        if (window >= 2.0)
        {
            const double viterbi = std::max(0.0, status.m_seconds[1] - m_fecViterbiSeconds);
            m_fecViterbiSeconds = status.m_seconds[1];

            if (m_autoViterbi)
            {
                if ((m_fecBusy > m_viterbiAddLoad * window) && (m_demappedQueue.size() >= m_maxQueuedDemapped / 2)
                    && (m_viterbiThreads < maxViterbiThreads))
                {
                    m_fec.setViterbiThreads(++m_viterbiThreads);
                    m_fecQuietWindows = 0;
                }
                else if (m_viterbiThreads > 0)
                {
                    // With n threads besides this one, each decodes about 1 / (n + 1) of the steps, so with one fewer
                    // the Viterbi time here would be about (n + 1) / n times as long
                    const double fewer = m_fecBusy + viterbi / m_viterbiThreads;

                    if (fewer >= m_viterbiRemoveLoad * window) {
                        m_fecQuietWindows = 0;
                    } else if (++m_fecQuietWindows >= m_viterbiRemoveWindows) {
                        m_fec.setViterbiThreads(--m_viterbiThreads);
                        m_fecQuietWindows = 0;
                    }
                }
            }

            m_fecBusy = 0.0;
            m_fecWindowStart = end;
        }

        m_fecStatus = status;
        m_demappedFree.push_back(std::move(demapped));
        m_busy = false;

        if (idle()) {
            m_idle.notify_all();
        }
    }
}

// With m_mutex locked
bool Receiver::idle() const
{
    return m_queue.empty() && !m_innerBusy && m_demappedQueue.empty() && !m_busy;
}

void Receiver::deliverPackets()
{
    if (!m_threaded) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_delivering.swap(m_packets);
    }

    if (m_packetCallback)
    {
        for (size_t i = 0; i + tsPacketSize <= m_delivering.size(); i += tsPacketSize) {
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
        m_idle.wait(lock, [this] { return idle(); });
    }

    deliverPackets();
}

Receiver::Status Receiver::getStatus() const
{
    Status status;
    status.m_ofdm = ofdmStatus();
    status.m_ofdm.m_clockOffset += (1.0 - m_resampler.getRatioCorrection()) * 1e6; // Include the correction applied
    FECDecoder::Status fec;

    if (m_threaded)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Until a reset's passed through decoding, the FEC's status is the signal's before (in Auto, its TS sync would
        // be taken for this one's)
        if (m_decodeEpoch == m_resetEpoch) {
            fec = m_fecStatus;
        }

        status.m_queueWaits = m_queueWaits;
    }
    else
    {
        fec = m_fec.getStatus();
        status.m_queueWaits = 0;
    }

    status.m_outer = fec.m_outer;
    status.m_tsSync = fec.m_tsSync;
    status.m_supported = fec.m_supported;
    status.m_lowPriority = fec.m_lowPriority;
    status.m_mer = fec.m_mer;
    status.m_preViterbiBER = fec.m_preViterbiBER;
    status.m_postViterbiBER = fec.m_postViterbiBER;
    status.m_viterbiBits = fec.m_viterbiBits;
    {
        std::lock_guard<std::mutex> lock(m_ofdmMutex);
        status.m_seconds[0] = m_ofdmSeconds;
        status.m_notches = m_notchFrequencies;
    }

    std::copy(fec.m_seconds, fec.m_seconds + 4, status.m_seconds + 1);

    return status;
}

} // namespace dvbt
