///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef MODEMDVB_SAMPLETHREAD_H
#define MODEMDVB_SAMPLETHREAD_H

#include <cassert>
#include <complex>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

// Internal to modemdvb: runs a function on blocks of samples, in the order given, in a thread of its own, so the
// caller can get on with something else. The caller waits if too many samples are queued. Destroying it waits for the
// block being processed, but drops those queued: drain() first to have them processed
class SampleThread
{
public:
    typedef std::complex<float> Cf;
    typedef std::function<void(const Cf *samples, int nbSamples)> Process;

    explicit SampleThread(Process process, size_t maxQueued = 1 << 20) :
        m_process(process),
        m_maxQueued(maxQueued),
        m_queued(0),
        m_busy(false),
        m_stop(false)
    {
        m_thread = std::thread(&SampleThread::run, this);
    }

    ~SampleThread()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }

        m_available.notify_all();
        m_space.notify_all();
        m_thread.join();
    }

    void push(const Cf *samples, int nbSamples)
    {
        if (nbSamples <= 0) {
            return;
        }

        std::vector<Cf> block;
        std::unique_lock<std::mutex> lock(m_mutex);

        if (!m_free.empty())
        {
            block = std::move(m_free.back());
            m_free.pop_back();
        }

        lock.unlock();
        block.assign(samples, samples + nbSamples);
        lock.lock();
        m_space.wait(lock, [this]() { return m_stop || (m_queued < m_maxQueued); });
        m_queued += nbSamples;
        m_queue.push_back(std::move(block));
        lock.unlock();
        m_available.notify_one();
    }

    //! The same, without copying: block's samples are taken, and it's given a buffer to reuse, of any size (not
    //! cleared, so resizing it to about the size it was doesn't have to fill it)
    void push(std::vector<Cf>& block)
    {
        if (block.empty()) {
            return;
        }

        std::vector<Cf> queued;
        queued.swap(block);
        const size_t nbSamples = queued.size();
        std::unique_lock<std::mutex> lock(m_mutex);

        if (!m_free.empty())
        {
            block.swap(m_free.back());
            m_free.pop_back();
        }

        m_space.wait(lock, [this]() { return m_stop || (m_queued < m_maxQueued); });
        m_queued += nbSamples;
        m_queue.push_back(std::move(queued));
        lock.unlock();
        m_available.notify_one();
    }

    //! Wait until everything queued has been processed. Not from the function (it would wait for itself)
    void drain()
    {
        const bool own = std::this_thread::get_id() == m_thread.get_id();
        assert(!own);

        if (own) {
            return;
        }

        std::unique_lock<std::mutex> lock(m_mutex);
        m_idle.wait(lock, [this]() { return m_queue.empty() && !m_busy; });
    }

    //! Discard what is queued, and wait for what is being processed. The function isn't called again until more
    //! samples are pushed, so the caller can then change what it uses. From the function, only discards (what's being
    //! processed is the caller's)
    void discard()
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        while (!m_queue.empty())
        {
            m_free.push_back(std::move(m_queue.front()));
            m_queue.pop_front();
        }

        m_queued = 0;
        m_space.notify_all();

        if (std::this_thread::get_id() != m_thread.get_id()) {
            m_idle.wait(lock, [this]() { return !m_busy; });
        }
    }

private:
    void run()
    {
        while (true)
        {
            std::vector<Cf> block;

            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_available.wait(lock, [this]() { return m_stop || !m_queue.empty(); });

                if (m_stop) {
                    return;
                }

                block = std::move(m_queue.front());
                m_queue.pop_front();
                m_queued -= block.size();
                m_busy = true;
            }

            m_space.notify_one();

            // An exception (out of memory) loses the block, rather than ending the process at the thread's end
            try {
                m_process(block.data(), (int) block.size());
            } catch (...) {
            }

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_busy = false;

                if (m_free.size() < 16) {
                    m_free.push_back(std::move(block));
                }
            }

            m_idle.notify_all();
        }
    }

    Process m_process;
    size_t m_maxQueued;     //!< Samples
    size_t m_queued;
    bool m_busy;
    bool m_stop;
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_available;
    std::condition_variable m_space;
    std::condition_variable m_idle;
    std::deque<std::vector<Cf>> m_queue;
    std::vector<std::vector<Cf>> m_free;
};

#endif // MODEMDVB_SAMPLETHREAD_H
