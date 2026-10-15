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

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <thread>

#include "util/profiler.h"
#include "dvbssharedfrontend.h"
#include "dvbsreceiver.h"
#include "dvbs2receiver.h"
#include "samplethread.h"

namespace dvbs {

// A block of the front end's symbols for DVB-S2, with what it needs of the front end then
struct SharedFrontEnd::S2Block
{
    std::vector<Cf> m_symbols;
    uint64_t m_first = 0;           //!< Index of the first
    FrontEnd::Status m_frontEnd;
    uint64_t m_corrections = 0;     //!< DVB-S2's frequency corrections made
    uint64_t m_boundary = 0;        //!< The first symbol after the last
    double m_otherLoad = 0.0;       //!< Cores the rest of DVB-S2's thread uses
    bool m_feed = false;            //!< The symbols to DVB-S2 (else only the reset or discontinuity)
    bool m_reset = false;
    bool m_discontinuity = false;
};

// Runs DVB-S2 on the blocks, in order, in a thread of its own. The caller waits if too many symbols are queued
class SharedFrontEnd::S2Thread
{
public:
    explicit S2Thread(std::function<void(S2Block&)> process) :
        m_process(process)
    {
        m_thread = std::thread(&S2Thread::run, this);
    }

    ~S2Thread()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }

        m_available.notify_all();
        m_space.notify_all();
        m_thread.join();
    }

    //! The block's taken, its symbols given a buffer to reuse
    void push(S2Block& block)
    {
        S2Block queued;
        queued.m_symbols.swap(block.m_symbols);
        queued.m_first = block.m_first;
        queued.m_frontEnd = block.m_frontEnd;
        queued.m_corrections = block.m_corrections;
        queued.m_boundary = block.m_boundary;
        queued.m_otherLoad = block.m_otherLoad;
        queued.m_feed = block.m_feed;
        queued.m_reset = block.m_reset;
        queued.m_discontinuity = block.m_discontinuity;
        const size_t n = queued.m_symbols.size();
        std::unique_lock<std::mutex> lock(m_mutex);

        if (!m_free.empty())
        {
            block.m_symbols.swap(m_free.back());
            m_free.pop_back();
        }

        if (m_queued >= m_maxQueuedS2)
        {
            PROFILER_START()
            m_space.wait(lock, [this]() { return m_stop || (m_queued < m_maxQueuedS2); });
            PROFILER_STOP("DVB-S/S2 front end thread waiting for DVB-S2's thread")
        }

        m_queued += n;
        m_queue.push_back(std::move(queued));
        lock.unlock();
        m_available.notify_one();
    }

    void drain()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_idle.wait(lock, [this]() { return m_queue.empty() && !m_busy; });
    }

    //! What's queued dropped, and waits for the block being processed
    void discard()
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        while (!m_queue.empty())
        {
            recycle(m_queue.front());
            m_queue.pop_front();
        }

        m_queued = 0;
        m_space.notify_all();
        m_idle.wait(lock, [this]() { return !m_busy; });
    }

private:
    void run()
    {
        while (true)
        {
            S2Block block;

            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_available.wait(lock, [this]() { return m_stop || !m_queue.empty(); });

                if (m_stop) {
                    return;
                }

                block = std::move(m_queue.front());
                m_queue.pop_front();
                m_queued -= block.m_symbols.size();
                m_busy = true;
            }

            m_space.notify_one();

            try {
                m_process(block);
            } catch (const std::exception&) {
            }

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_busy = false;
                recycle(block);
            }

            m_idle.notify_all();
        }
    }

    // With m_mutex
    void recycle(S2Block& block)
    {
        if ((m_free.size() < 16) && (block.m_symbols.capacity() > 0)) {
            m_free.push_back(std::move(block.m_symbols));
        }
    }

    std::function<void(S2Block&)> m_process;
    size_t m_queued = 0;
    bool m_busy = false;
    bool m_stop = false;
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_available;
    std::condition_variable m_space;
    std::condition_variable m_idle;
    std::deque<S2Block> m_queue;
    std::vector<std::vector<Cf>> m_free;
};

SharedFrontEnd::SharedFrontEnd(Receiver *receiverS, dvbs2::Receiver *receiverS2, bool threaded) :
    m_receiverS(receiverS),
    m_receiverS2(receiverS2),
    m_feedS(true),
    m_feedS2(true),
    m_resetS(false),
    m_resetS2(false)
{
    m_frontEnd.setTimingBandwidth(m_timingAcquire);
    m_receiverS->setSharedFrontEnd(&m_frontEnd);
    m_receiverS2->setSharedFrontEnd(&m_frontEnd);

    if (threaded)
    {
        m_threadS2.reset(new S2Thread([this](S2Block& block) {
            demodulateS2(block);
        }));
        m_thread.reset(new SampleThread([this](const Cf *samples, int nbSamples) {
            demodulate(samples, nbSamples);
        }));
    }
}

SharedFrontEnd::~SharedFrontEnd()
{
    m_thread.reset();
    m_threadS2.reset();
    m_receiverS->setSharedFrontEnd(nullptr);
    m_receiverS2->setSharedFrontEnd(nullptr);
}

void SharedFrontEnd::configure(double inputRate, double symbolRate)
{
    discard();
    m_frontEnd.configure(inputRate, symbolRate, 0.35);
    m_frontEnd.setTimingBandwidth(m_timingAcquire);
    m_receiverS->configure(inputRate, symbolRate);
    m_receiverS2->configure(inputRate, symbolRate);
    m_resetS = false;
    m_resetS2 = false;
    forgetRequestsS2();
}

void SharedFrontEnd::reset()
{
    discard();
    m_frontEnd.reset();
    m_frontEnd.setTimingBandwidth(m_timingAcquire);
    m_receiverS->reset();
    m_receiverS2->reset();
    m_resetS = false;
    m_resetS2 = false;
    forgetRequestsS2();
}

// The front end's thread first: it queues for DVB-S2's
void SharedFrontEnd::discard()
{
    if (m_thread) {
        m_thread->discard();
    }

    if (m_threadS2) {
        m_threadS2->discard();
    }
}

void SharedFrontEnd::setFeeding(bool s, bool s2)
{
    m_feedS = s;
    m_feedS2 = s2;
}

void SharedFrontEnd::process(const Cf *samples, int nbSamples)
{
    if (m_thread) {
        m_thread->push(samples, nbSamples);
    } else {
        demodulate(samples, nbSamples);
    }
}

void SharedFrontEnd::discontinuity()
{
    discard();
    m_discontinuity = true;
}

void SharedFrontEnd::flush()
{
    if (m_thread) {
        m_thread->drain();
    }

    if (m_threadS2) {
        m_threadS2->drain();
    }
}

void SharedFrontEnd::forgetRequestsS2()
{
    m_requestsS2 = m_receiverS2->frequencyRequests();
    m_requestedS2 = m_receiverS2->frequencyRequested();
    m_boundaryS2 = m_symbolIndex;
}

void SharedFrontEnd::takeRequestsS2()
{
    const double rollOff = m_receiverS2->frontEndRollOff();

    if (rollOff > 0.0) {
        m_frontEnd.setRollOff(rollOff);
    }

    // The count first: the sum's then at least those
    const uint64_t requests = m_receiverS2->frequencyRequests();

    if (requests != m_requestsS2)
    {
        const double requested = m_receiverS2->frequencyRequested();
        const int held = m_frontEnd.heldSymbols();
        m_frontEnd.adjustFrequency(requested - m_requestedS2);
        // The symbols of the samples it holds were turned by the NCO as it was. DVB-S's carrier loop, fed too, tracks
        // what's left after the NCO: moved to match
        m_boundaryS2 = m_symbolIndex + held;

        if (m_feedS) {
            m_receiverS->frontEndShifted(requested - m_requestedS2, held);
        }

        m_requestsS2 = requests;
        m_requestedS2 = requested;
    }
}

void SharedFrontEnd::demodulateS2(S2Block& block)
{
    if (block.m_reset) {
        m_receiverS2->reset();
    }

    if (block.m_discontinuity) {
        m_receiverS2->discontinuity();
    }

    if (block.m_feed)
    {
        PROFILER_START()
        m_receiverS2->processSymbols(block.m_symbols.data(), (int) block.m_symbols.size(), block.m_first, block.m_frontEnd,
            block.m_corrections, block.m_boundary, block.m_otherLoad);
        PROFILER_STOP(m_threadS2 ? "DVB-S2 thread: physical layer" : "DVB-S/S2 shared thread: DVB-S2")
    }
}

FrontEnd::Status SharedFrontEnd::getStatus() const
{
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_status;
}

void SharedFrontEnd::demodulate(const Cf *samples, int nbSamples)
{
    if (m_resetS.exchange(false)) {
        m_receiverS->reset();
    }

    // DVB-S2's in its thread, before the block's symbols
    const bool resetS2 = m_resetS2.exchange(false);
    const bool discontinuity = m_discontinuity.exchange(false);

    if (discontinuity) {
        m_receiverS->discontinuity();
    }

    const bool feedS = m_feedS, feedS2 = m_feedS2;

    // DVB-S2's roll-off while it's fed (DVB-S can be received through a lower one's filter, if not as well), else
    // DVB-S's
    if (feedS2) {
        takeRequestsS2();
    } else if (feedS) {
        m_frontEnd.setRollOff(rollOffValue(RollOff::R0_35));
    }

    const auto start = std::chrono::steady_clock::now();
    PROFILER_START()
    m_symbols.clear();
    m_frontEnd.process(samples, nbSamples, m_symbols);
    PROFILER_STOP(m_threadS2 ? "DVB-S/S2 front end thread: front end" : "DVB-S/S2 shared thread: front end")
    const uint64_t first = m_symbolIndex;
    m_symbolIndex += m_symbols.size();

    // Each fed even without symbols (while the spectrum's measured), so its status and restarts go on
    if (feedS)
    {
        PROFILER_RESTART()
        m_receiverS->processSymbols(m_symbols.data(), (int) m_symbols.size());
        PROFILER_STOP(m_threadS2 ? "DVB-S/S2 front end thread: DVB-S" : "DVB-S/S2 shared thread: DVB-S")
    }

    // The front end's and DVB-S's load (cores), so the DVB-S2 search, if in this thread too, has what's left
    const double inputRate = m_frontEnd.inputRate();

    if ((nbSamples > 0) && (inputRate > 0.0))
    {
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        m_otherLoad += 0.05 * (seconds * inputRate / nbSamples - m_otherLoad);
    }

    if (feedS2 || resetS2 || discontinuity)
    {
        S2Block block;
        block.m_first = first;
        block.m_frontEnd = m_frontEnd.getStatus();
        block.m_corrections = m_requestsS2;
        block.m_boundary = m_boundaryS2;
        block.m_otherLoad = m_threadS2 ? 0.0 : m_otherLoad;
        block.m_feed = feedS2;
        block.m_reset = resetS2;
        block.m_discontinuity = discontinuity;

        if (m_threadS2)
        {
            if (feedS2)
            {
                m_symbolsS2.assign(m_symbols.begin(), m_symbols.end());
                block.m_symbols.swap(m_symbolsS2);
            }

            m_threadS2->push(block);
            m_symbolsS2.swap(block.m_symbols);
        }
        else
        {
            block.m_symbols.swap(m_symbols);
            demodulateS2(block);
            m_symbols.swap(block.m_symbols);
        }
    }

    // The spectrum measured again if a receiver's asked (it's restarted its own part), unless the other has a lock
    // that would lose. The timing loop tracking if either's locked
    const bool restartS = feedS && m_receiverS->takeFrontEndRestart();
    const bool restartS2 = feedS2 && m_receiverS2->takeFrontEndRestart();
    const bool lockedS = feedS && m_receiverS->frontEndLocked();
    const bool lockedS2 = feedS2 && m_receiverS2->frontEndLocked();

    if ((restartS && !lockedS2) || (restartS2 && !lockedS))
    {
        m_frontEnd.reset();
        forgetRequestsS2();

        // DVB-S's carrier was of the front end before (unless it restarted itself, asking for this)
        if (!restartS) {
            m_receiverS->frontEndReset();
        }
    }

    if (feedS2)
    {
        const double ppm = m_receiverS2->takeClockRequest();

        if (ppm != 0.0) {
            m_frontEnd.adjustClock(ppm);
        }
    }

    // DVB-S2's lock, at its SNR (VL-SNR's can be far below DVB-S's)
    m_frontEnd.setTimingBandwidth(lockedS2 ? FrontEnd::trackingBandwidth(m_timingTrack, m_receiverS2->frontEndEsN0())
        : lockedS ? m_timingTrack : feedS2 ? dvbs2::Receiver::searchTimingBandwidth(m_frontEnd.getStatus().m_spectralEsN0) : m_timingAcquire);
    m_frontEnd.setEqualiserReference(lockedS || lockedS2, lockedS2 ? m_receiverS2->frontEndEsN0() : m_receiverS->frontEndEsN0());
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_status = m_frontEnd.getStatus();
}

} // namespace dvbs
