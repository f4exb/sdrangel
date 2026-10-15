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
#include <exception>

#include "util/profiler.h"
#include "dvbsreceiver.h"
#include "samplethread.h"

namespace dvbs {

// e^j(angle): for the small angles a loop turns by, sine and cosine to the fifth and fourth power (to 1e-6 within 0.3
// radians, and the rotator's renormalised each group), else exactly
inline std::complex<float> QPSKCarrier::rotation(double angle)
{
    if (std::fabs(angle) > 0.3) {
        return std::complex<float>((float) std::cos(angle), (float) std::sin(angle));
    }

    const float x = (float) angle, x2 = x * x;
    return std::complex<float>(1.0f - x2 * (0.5f - x2 * (1.0f / 24.0f)), x * (1.0f - x2 * ((1.0f / 6.0f) - x2 * (1.0f / 120.0f))));
}

double Receiver::now()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

QPSKCarrier::QPSKCarrier() :
    m_tracking(false),
    m_rotator(1.0f, 0.0f),
    m_frequency(0.0),
    m_kp(0.0),
    m_ki(0.0),
    m_bandwidth(m_trackBandwidth),
    m_lock(0.0),
    m_errorPower(1.0),
    m_averaged(false),
    m_m2(0.0),
    m_m4(0.0),
    m_tracked(0),
    m_unlocked(0),
    m_acquisitions(0)
{
    reset();
}

void QPSKCarrier::reset()
{
    m_tracking = false;
    m_acquire.clear();
    m_acquire.reserve(m_acquireSymbols);
    m_rotator = Cf(1.0f, 0.0f);
    m_frequency = 0.0;
    m_pendingShift = 0.0;
    m_pendingSymbols = 0;
    m_lock = 0.0;
    m_errorPower = 1.0;
    m_averaged = false;
    m_m2 = 0.0;
    m_m4 = 0.0;
    m_tracked = 0;
    m_unlocked = 0;
}

void QPSKCarrier::shiftFrequency(double cyclesPerSymbol, int afterSymbols)
{
    if (afterSymbols <= 0)
    {
        m_frequency -= cyclesPerSymbol;
    }
    else
    {
        m_pendingShift += cyclesPerSymbol;
        m_pendingSymbols = afterSymbols;
    }
}

void QPSKCarrier::setBandwidth(double bnT)
{
    if (bnT != m_bandwidth)
    {
        m_bandwidth = bnT;

        if (m_tracking && (m_tracked >= m_acquireTrackSymbols)) {
            computeGains(bnT);
        }
    }
}

void QPSKCarrier::computeGains(double bnT)
{
    // Phase detector Im(w conj(d)) for unit power symbols: sin of the error, so a gain of 1 (cycles: 2 pi)
    const double zeta = 0.707;
    const double theta = bnT / (zeta + 1.0 / (4.0 * zeta));
    const double d = 1.0 + 2.0 * zeta * theta + theta * theta;
    m_kp = 4.0 * zeta * theta / d / (2.0 * m_pi);
    m_ki = 4.0 * theta * theta / d / (2.0 * m_pi);
}

int QPSKCarrier::process(const Cf *z, int nbSymbols, Cf *w, bool& restarted, int& restartAt)
{
    static const float a = 0.70710678f;
    static const int group = 8;
    int i = 0, out = 0;

    while (i < nbSymbols)
    {
        if (!m_tracking)
        {
            for (; (i < nbSymbols) && ((int) m_acquire.size() < m_acquireSymbols); i++)
            {
                const std::complex<double> zd(z[i].real(), z[i].imag());
                const std::complex<double> z2 = zd * zd;
                m_acquire.push_back(z2 * z2);
            }

            if ((int) m_acquire.size() < m_acquireSymbols) {
                break;
            }

            // The fourth power's line, at 4 times the offset
            fft(m_acquire);
            m_frequency = peakFrequency(m_acquire) / 4.0;
            m_pendingShift = 0.0;
            m_pendingSymbols = 0;
            m_acquire.clear();
            m_rotator = Cf(1.0f, 0.0f);
            m_tracking = true;
            m_tracked = 0;
            m_unlocked = 0;
            m_lock = 0.0;
            m_errorPower = 1.0;
            m_averaged = false;
            m_m2 = 0.0;
            m_m4 = 0.0;
            m_acquisitions++;
            computeGains(m_acquireBandwidth);
            continue;
        }

        const int first = i;
        double lockNumerator = 0.0, lockDenominator = 0.0, errorPower = 0.0, powerSum = 0.0;

        for (int n = 0; (i < nbSymbols) && m_tracking; i += n)
        {
            // A group ends where a shift of the frequency is due
            n = std::min(m_pendingSymbols > 0 ? std::min(group, m_pendingSymbols) : group, nbSymbols - i);
            // This group turned at the loop's frequency from the rotator
            const Cf step = rotation(-2.0 * m_pi * m_frequency);
            Cf rot = m_rotator;
            float error = 0.0f, num = 0.0f, den = 0.0f, err = 0.0f, pw = 0.0f;

            float rr = rot.real(), ri = rot.imag();
            const float sr = step.real(), si = step.imag();

            for (int k = 0; k < n; k++)
            {
                const float zr = z[i + k].real(), zi = z[i + k].imag();
                const Cf v(zr * rr - zi * ri, zr * ri + zi * rr);
                const float t = rr * sr - ri * si;
                ri = rr * si + ri * sr;
                rr = t;
                w[out + k] = v;
                const float dr = withSignOf(a, v.real()), di = withSignOf(a, v.imag());
                error += v.imag() * dr - v.real() * di; // Im(v conj(d)): the sine of the error, at unit power
                const float er = v.real() - dr, ei = v.imag() - di;
                err += er * er + ei * ei;
                const float p = v.real() * v.real() + v.imag() * v.imag();
                const float re2 = v.real() * v.real() - v.imag() * v.imag(), im2 = 2.0f * v.real() * v.imag();
                num -= re2 * re2 - im2 * im2;   // -Re(v^4): QPSK's points' fourth power is -1
                den += p * p;
                pw += p;
            }

            out += n;
            lockNumerator += num;
            lockDenominator += den;
            powerSum += pw;
            errorPower += err;
            // The loop, as a symbol by symbol one would have moved over the group
            m_frequency += m_ki * error;
            const double turn = -2.0 * m_pi * m_kp * error;
            rot = Cf(rr, ri);
            m_rotator = rot * rotation(turn);
            // Back to unit magnitude, by a Newton step for 1 / sqrt (it drifts by rounding only)
            m_rotator *= 1.5f - 0.5f * std::norm(m_rotator);
            const uint64_t before = m_tracked;
            m_tracked += n;

            if ((m_pendingSymbols > 0) && ((m_pendingSymbols -= n) == 0))
            {
                m_frequency -= m_pendingShift;
                m_pendingShift = 0.0;
            }

            if ((before < m_acquireTrackSymbols) && (m_tracked >= m_acquireTrackSymbols)) {
                computeGains(m_bandwidth);
            }
        }

        // Lock (the fourth power's coherence) and error power, averaged over a couple of thousand symbols
        const int count = i - first;

        if ((count > 0) && (lockDenominator > 0.0))
        {
            const double weight = std::min(1.0, count / 2048.0);
            m_lock += (lockNumerator / lockDenominator - m_lock) * weight;
            // The first update whole (from 0, the moments' SNR would read noise alone until they'd averaged up)
            const double average = m_averaged ? std::min(1.0, count / 4096.0) : 1.0;
            m_errorPower += (errorPower / count - m_errorPower) * average;
            m_m2 += (powerSum / count - m_m2) * average;
            m_m4 += (lockDenominator / count - m_m4) * average;
            m_averaged = true;
        }

        // Unlocked for a while: acquire again
        if ((m_tracked > (uint64_t) m_acquireSymbols) && (m_lock < m_lockThreshold / 2.0))
        {
            m_unlocked += count;

            if (m_unlocked > m_unlockedLimit)
            {
                m_tracking = false;
                restarted = true;
                restartAt = out;
            }
        }
        else
        {
            m_unlocked = 0;
        }
    }

    return out;
}

double QPSKCarrier::mer() const
{
    return -10.0 * std::log10(std::max(1e-10, m_errorPower));
}

// For a constant envelope (QPSK) in complex Gaussian noise, E|y|^2 = S + N and E|y|^4 = S^2 + 4SN + 2N^2, so
// S = sqrt(2 M2^2 - M4) and N = M2 - S
double QPSKCarrier::snr() const
{
    const double s2 = 2.0 * m_m2 * m_m2 - m_m4;

    if ((m_m2 <= 0.0) || (s2 <= 0.0)) {
        return -10.0; // Noise alone, as far as can be told
    }

    const double s = std::sqrt(s2);
    const double n = m_m2 - s;
    return n > 0.0 ? std::min(50.0, 10.0 * std::log10(s / n)) : 50.0;
}

Receiver::Receiver(bool threaded) :
    m_threaded(threaded),
    m_inputRate(2e6),
    m_symbolRate(1e6),
    m_codeRate(-1),
    m_carrierBandwidth(0.0),
    m_fecLocked(false),
    m_displayRequested(true),
    m_displayMaxSymbols(2000),
    m_displayPoints(512),
    m_busy(false),
    m_stop(false),
    m_resetEpoch(0),
    m_unlockedSymbols(0),
    m_fecProgress(false),
    m_lastGoodPackets(0),
    m_restarts(0),
    m_decodeEpoch(0),
    m_fecCodeRate(-1),
    m_queueWaits(0),
    m_fecSeconds(0.0)
{
    m_fe = &m_frontEnd;
    m_frontEndRestart = false;
    m_fec.setPacketCallback([this](const uint8_t *packet) {
        m_fecPackets.insert(m_fecPackets.end(), packet, packet + dvbt::tsPacketSize);
    });
    m_frontEnd.configure(m_inputRate, m_symbolRate, 0.35);
    m_fec.setSymbolRate(m_symbolRate);
    m_frontEnd.setTimingBandwidth(m_timingAcquire);
    m_recentSymbols.assign(m_nbDisplaySymbols, Cf(0.0f, 0.0f));

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
    m_frontEnd.configure(inputRate, symbolRate, 0.35);
    m_fec.setSymbolRate(symbolRate);
    reset();
}

void Receiver::setSharedFrontEnd(FrontEnd *frontEnd)
{
    m_fe = frontEnd ? frontEnd : &m_frontEnd;
    m_frontEndRestart = false;
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
    m_carrier.reset();
    m_fecLocked = false;
    m_unlockedSymbols = 0;
    m_fecProgress = false;

    {
        std::lock_guard<std::mutex> lock(m_demodMutex);
        m_demodStatus = Status();
        m_demodStatus.m_symbolRate = m_symbolRate;
        m_displaySymbols.clear();
        std::fill(m_recentSymbols.begin(), m_recentSymbols.end(), Cf(0.0f, 0.0f));
    }

    if (m_threaded)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        while (!m_queue.empty())
        {
            if (!m_queue.front().m_reset && (m_free.size() < m_maxQueued + 2))
            {
                m_queue.front().m_soft.clear();
                m_free.push_back(std::move(m_queue.front()));
            }

            m_queue.pop_front();
        }

        SoftBlock block;
        block.m_reset = true;
        block.m_epoch = ++m_resetEpoch;
        m_queue.push_back(std::move(block));
        m_packets.clear();
        // Again with the new epoch: the error correction thread may have published a block's lock since. The status
        // is the last block's until the reset's decoded: not its lock
        m_fecLocked = false;
        m_fecProgress = false;
        m_fecStatus.m_state = FECDecoder::State::Searching;
        m_fecStatus.m_tsSync = false;
        m_space.notify_all();
        m_available.notify_one();
    }
    else
    {
        m_fec.reset();
        m_fecPackets.clear();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_packets.clear();
        m_fecStatus = m_fec.getStatus();
    }
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

void Receiver::demodulate(const Cf *samples, int nbSamples)
{
    const double start = now();
    m_symbols.clear();
    m_frontEnd.process(samples, nbSamples, m_symbols);
    demodulateSymbols(start, now());
}

void Receiver::processSymbols(const Cf *symbols, int nbSymbols)
{
    const double start = now();
    m_symbols.assign(symbols, symbols + nbSymbols);
    demodulateSymbols(start, start);
}

// The error correction's sync (Viterbi, deinterleaver, packets) would be lost in the gap, its packets failing RS until
// it found it was: it searches again from the symbols after (the carrier loop keeps its frequency, and follows the
// phase again). As after a restart, the packets aren't dropped: the transport stream goes on after the gap
void Receiver::discontinuity()
{
    deliverReset();
}

void Receiver::frontEndShifted(double hz, int afterSymbols)
{
    // Acquiring, the frequency's measured afresh
    if (m_carrier.tracking() && (m_symbolRate > 0.0)) {
        m_carrier.shiftFrequency(hz / m_symbolRate, afterSymbols);
    }
}

// As a restart of this receiver's own, without asking for the front end's: the carrier acquired again from the symbols
// after, and the error correction searching again
void Receiver::frontEndReset()
{
    m_carrier.reset();
    m_unlockedSymbols = 0;
    deliverReset();
}

void Receiver::demodulateSymbols(double start, double frontEndEnd)
{
    const double bandwidth = m_carrierBandwidth;

    if (bandwidth > 0.0)
    {
        m_carrier.setBandwidth(bandwidth);
    }
    else if (m_carrier.tracking())
    {
        // Narrow at low SNR, where a wide loop slips, wide at high SNR, to follow phase noise
        const double mer = m_carrier.mer();
        m_carrier.setBandwidth(std::max(0.005, std::min(0.02, 0.005 + (mer - 5.0) * 0.0025)));
    }
    SoftBlock& block = m_filling;
    block.m_reset = false;
    bool restart = false;
    int restartAt = 0;
    m_tracked.resize(m_symbols.size());
    const int n = m_carrier.process(m_symbols.data(), (int) m_symbols.size(), m_tracked.data(), restart, restartAt);
    block.m_soft.resize(2 * n);
    softBits(m_tracked.data(), n, m_softScale, block.m_soft.data());

    // The last symbols, for display
    if (n > 0)
    {
        const int keep = std::min(n, (int) m_recentSymbols.size());
        m_recentSymbols.erase(m_recentSymbols.begin(), m_recentSymbols.begin() + keep);
        m_recentSymbols.insert(m_recentSymbols.end(), m_tracked.begin() + (n - keep), m_tracked.begin() + n);
    }

    // A large frequency offset is moved to the front end's NCO, so the matched filter stays centred on the signal (only
    // when tracking: an unlocked loop's frequency is nothing to move, and would mix two into the next acquisition's).
    // A shared front end's only with the FEC locked: the carrier loop can track a DVB-S2 signal for a while, and move
    // it from under the DVB-S2 receiver searching
    if (m_carrier.tracking() && ((m_fe == &m_frontEnd) || m_fecLocked) && (std::fabs(m_carrier.frequency()) > m_foldBack))
    {
        const double f = m_carrier.frequency();
        const int held = m_fe->heldSymbols();
        m_fe->adjustFrequency(f * m_symbolRate);
        m_carrier.shiftFrequency(f, held);
    }

    // No packet through RS for a while: the carrier may have been found wrong, a quarter of the symbol rate out (as QPSK
    // turned 90 degrees a symbol looks right to the carrier loop, and the code search can even find sync bytes now and
    // then), from a spectrum measured as the signal changed, so it's measured again
    if (m_fecProgress.exchange(false) || m_fe->getStatus().m_estimating)
    {
        m_unlockedSymbols = 0;
    }
    else if ((m_unlockedSymbols += m_symbols.size()) > std::max(m_relockSeconds * m_symbolRate, m_relockMinSymbols))
    {
        // A shared front end's restarted by its owner, if the DVB-S2 receiver hasn't a lock it'd lose
        if (m_fe == &m_frontEnd)
        {
            m_frontEnd.reset();
            m_frontEnd.setTimingBandwidth(m_timingAcquire);
        }
        else
        {
            m_frontEndRestart = true;
        }

        m_carrier.reset();
        m_unlockedSymbols = 0;
        m_restarts++;
        restart = true;
        restartAt = n;  // This block's symbols are the lock's before: before the reset
    }

    // A shared one's owner sets them, from both receivers, after each block
    if (m_fe == &m_frontEnd)
    {
        m_frontEnd.setTimingBandwidth(m_fecLocked ? m_timingTrack : m_timingAcquire);
        m_frontEnd.setEqualiserReference(m_fecLocked, frontEndEsN0());
    }

    const double end = now();

    {
        std::lock_guard<std::mutex> lock(m_demodMutex);
        Status& s = m_demodStatus;
        s.m_frontEnd = m_fe->getStatus();
        s.m_symbolRate = m_symbolRate;
        s.m_carrierLocked = m_carrier.locked();
        s.m_carrierLock = m_carrier.lockMetric();
        s.m_mer = m_carrier.tracking() ? m_carrier.mer() : 0.0;
        s.m_snr = m_carrier.tracking() ? m_carrier.snr() : 0.0;
        s.m_carrierAcquisitions = m_carrier.acquisitions();
        s.m_restarts = m_restarts;
        s.m_frequencyOffset = s.m_frontEnd.m_frequencyOffset + (m_carrier.tracking() ? m_carrier.frequency() * m_symbolRate : 0.0);
        s.m_clockOffset = s.m_frontEnd.m_clockOffset;
        s.m_seconds[0] += frontEndEnd - start;
        s.m_seconds[1] += end - frontEndEnd;

        if (m_displayRequested)
        {
            const int count = std::min(m_displayMaxSymbols, (int) m_recentSymbols.size());
            m_displaySymbols.assign(m_recentSymbols.end() - count, m_recentSymbols.end());

            m_fe->getSpectrum(m_displaySpectrum, m_displayPoints);
            m_displayRequested = false;
        }
    }

    // After a restart, the symbols of the lock before it go to the error correction first, then the reset
    if (restart)
    {
        if (restartAt > 0)
        {
            SoftBlock before;
            before.m_soft.assign(block.m_soft.begin(), block.m_soft.begin() + 2 * restartAt);
            block.m_soft.erase(block.m_soft.begin(), block.m_soft.begin() + 2 * restartAt);
            deliver(before);
        }

        deliverReset();
    }

    deliver(block);
}

void Receiver::deliverReset()
{
    SoftBlock resetBlock;
    resetBlock.m_reset = true;

    if (m_threaded)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        resetBlock.m_epoch = m_resetEpoch; // Not a reset of the receiver: packets are still wanted
        m_queue.push_back(std::move(resetBlock));
        m_available.notify_one();
    }
    else
    {
        resetBlock.m_epoch = m_resetEpoch;
        decodeBlock(resetBlock);
    }
}

void Receiver::deliver(SoftBlock& block)
{
    if (block.m_soft.empty()) {
        return;
    }

    if (!m_threaded)
    {
        decodeBlock(block);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_packets.insert(m_packets.end(), m_fecPackets.begin(), m_fecPackets.end());
        m_fecPackets.clear();
        return;
    }

    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_queue.size() >= m_maxQueued)
    {
        m_queueWaits++;
        PROFILER_START()
        m_space.wait(lock, [this]() { return m_stop || (m_queue.size() < m_maxQueued); });
        PROFILER_STOP("DVB-S waiting for its FEC thread")
    }

    SoftBlock queued;

    if (!m_free.empty())
    {
        queued = std::move(m_free.back());
        m_free.pop_back();
    }

    queued.m_reset = false;
    queued.m_soft.swap(block.m_soft);
    m_queue.push_back(std::move(queued));
    m_available.notify_one();
}

// In the error correction thread (or the caller's, unthreaded). Returns whether packets passed RS
bool Receiver::decodeBlock(SoftBlock& block)
{
    const int rate = m_codeRate;

    if (rate != m_fecCodeRate)
    {
        m_fecCodeRate = rate;
        m_fec.setCodeRate(rate);
    }

    const double start = now();

    if (block.m_reset) {
        m_fec.reset();
    } else {
        m_fec.decode(block.m_soft.data(), (int) block.m_soft.size() / 2);
    }

    m_fecSeconds = m_fecSeconds + (now() - start); // Only this thread writes it
    PROFILER_ADD("DVB-S FEC thread (search and decoding)", (qint64) ((now() - start) * 1e9))
    const FECDecoder::Status& fecStatus = m_fec.getStatus();
    // RS failures include packets dropped before the outer decoder's synchronised, not counted as packets
    const uint64_t good = fecStatus.m_outer.m_packets - std::min(fecStatus.m_outer.m_packets, fecStatus.m_outer.m_rsFailed);
    // Packets through RS with the transport stream in sync: a false lock's garbage may now and then pass RS
    const bool progress = (good > m_lastGoodPackets) && fecStatus.m_tsSync;
    m_lastGoodPackets = good;

    // Threaded, runFEC() publishes them, if the block's not from before a reset
    if (!m_threaded)
    {
        m_fecLocked = fecStatus.m_state == FECDecoder::State::Locked;

        if (progress) {
            m_fecProgress = true;
        }

        std::lock_guard<std::mutex> lock(m_mutex); // getStatus() may be called from another thread
        m_fecStatus = fecStatus;
    }

    return progress;
}

void Receiver::runFEC()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    while (true)
    {
        m_available.wait(lock, [this]() { return m_stop || !m_queue.empty(); });

        if (m_stop) {
            break;
        }

        SoftBlock block = std::move(m_queue.front());
        m_queue.pop_front();
        m_busy = true;
        lock.unlock();
        m_space.notify_one();

        bool progress = false;

        // An exception (out of memory) loses the block, the decoder searching again, rather than ending the process
        try {
            progress = decodeBlock(block);
        } catch (const std::exception&) {
            m_fec.reset();
        }

        const FECDecoder::Status status = m_fec.getStatus();
        lock.lock();

        if (block.m_reset && (block.m_epoch == m_resetEpoch)) {
            m_decodeEpoch = block.m_epoch;
        }

        // A block of the signal before a reset of the receiver: its lock and progress aren't this one's. Published with
        // the epoch checked under the lock, so reset() can't be followed by an old block's lock
        const bool current = m_decodeEpoch == m_resetEpoch;
        m_fecLocked = current && (status.m_state == FECDecoder::State::Locked);

        if (current && progress) {
            m_fecProgress = true;
        }

        // Packets from before a reset aren't wanted
        if (!m_fecPackets.empty())
        {
            if (m_decodeEpoch == m_resetEpoch) {
                m_packets.insert(m_packets.end(), m_fecPackets.begin(), m_fecPackets.end());
            }

            m_fecPackets.clear();
        }

        if (m_decodeEpoch == m_resetEpoch) {
            m_fecStatus = status;
        }

        // Blocks are reused for their buffers: not reset blocks, which have none (one is queued at each restart, many
        // a second without a signal), and no more than can be queued
        if (!block.m_reset && (m_free.size() < m_maxQueued + 2))
        {
            block.m_soft.clear();
            m_free.push_back(std::move(block));
        }

        m_busy = false;

        if (idle()) {
            m_idle.notify_all();
        }
    }
}

// With m_mutex locked
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
        for (size_t i = 0; i + dvbt::tsPacketSize <= m_delivering.size(); i += dvbt::tsPacketSize) {
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

    {
        std::lock_guard<std::mutex> lock(m_demodMutex);
        status = m_demodStatus;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        status.m_fec = m_fecStatus;
        status.m_queueWaits = m_queueWaits;
        status.m_seconds[2] = m_fecSeconds;
    }

    if (status.m_frontEnd.m_estimating) {
        status.m_state = State::Spectrum;
    } else if (!status.m_carrierLocked && (status.m_fec.m_state != FECDecoder::State::Locked)) {
        status.m_state = State::Carrier;
    } else if (status.m_fec.m_state != FECDecoder::State::Locked) {
        status.m_state = State::Code;
    } else if (!status.m_fec.m_tsSync) {
        status.m_state = State::Sync;
    } else {
        status.m_state = State::Locked;
    }

    return status;
}

void Receiver::getDisplayData(std::vector<Cf>& symbols, int maxSymbols, std::vector<float>& spectrumDb, int points) const
{
    std::lock_guard<std::mutex> lock(m_demodMutex);
    symbols = m_displaySymbols;
    spectrumDb = m_displaySpectrum;
    m_displayMaxSymbols = std::min(maxSymbols, m_nbDisplaySymbols);
    m_displayPoints = points;
    m_displayRequested = true;
}

} // namespace dvbs
