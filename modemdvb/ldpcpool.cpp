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

#include "ldpcpool.h"

namespace dvbt2 {

LDPC& LDPCPool::Context::decoder(CodeFamily family, FECType type, CodeRate rate)
{
    for (auto it = m_decoders.begin(); it != m_decoders.end(); ++it)
    {
        if ((it->m_family == family) && (it->m_type == type) && (it->m_rate == rate))
        {
            if (it != m_decoders.begin())
            {
                Entry entry = std::move(*it);
                m_decoders.erase(it);
                m_decoders.push_front(std::move(entry));
            }

            return *m_decoders.front().m_ldpc;
        }
    }

    Entry entry;
    entry.m_family = family;
    entry.m_type = type;
    entry.m_rate = rate;
    entry.m_ldpc.reset(new LDPC(type, rate, family));
    m_decoders.push_front(std::move(entry));

    // The least recently used dropped, over the budget (the new one's always kept)
    size_t bytes = 0;

    for (const Entry& e : m_decoders) {
        bytes += e.m_ldpc->memoryBytes();
    }

    while ((m_decoders.size() > 1) && (bytes > m_maxBytes))
    {
        bytes -= m_decoders.back().m_ldpc->memoryBytes();
        m_decoders.pop_back();
    }

    return *m_decoders.front().m_ldpc;
}

LDPCPool::LDPCPool(int threads) :
    m_stop(false)
{
    for (int t = 0; t < threads; t++) {
        m_threads.emplace_back(&LDPCPool::thread, this);
    }
}

LDPCPool::~LDPCPool()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }

    m_available.notify_all();

    for (std::thread& thread : m_threads) {
        thread.join();
    }
}

void LDPCPool::run(Job& job, Context& context)
{
    if (job.m_batches <= 0) {
        return;
    }

    if (!m_threads.empty() && (job.m_batches > 1))
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_jobs.push_back(&job);
        }

        // Only as many threads as there are batches besides this thread's first: the rest would find none. Those
        // woken take more as they finish, and this thread takes any left, so none is missed
        const int wake = std::min((int) m_threads.size(), job.m_batches - 1);

        for (int i = 0; i < wake; i++) {
            m_available.notify_one();
        }
    }

    while (true)
    {
        int batch;

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (job.m_next >= job.m_batches) {
                break;
            }

            batch = job.m_next++;

            if (job.m_next == job.m_batches)
            {
                auto it = std::find(m_jobs.begin(), m_jobs.end(), &job);

                if (it != m_jobs.end()) {
                    m_jobs.erase(it);
                }
            }
        }

        try
        {
            job.m_decode(context.decoder(job.m_family, job.m_type, job.m_rate), context, batch);
        }
        catch (...)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (!job.m_error) {
                job.m_error = std::current_exception();
            }

            cancel(job);
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        job.m_done++;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    m_done.wait(lock, [&job]() { return job.m_done == job.m_batches; });

    if (job.m_error) {
        std::rethrow_exception(job.m_error);
    }
}

// Caller must hold m_mutex.
void LDPCPool::cancel(Job& job)
{
    job.m_done += job.m_batches - job.m_next;
    job.m_next = job.m_batches;
    auto it = std::find(m_jobs.begin(), m_jobs.end(), &job);

    if (it != m_jobs.end()) {
        m_jobs.erase(it);
    }
}

void LDPCPool::thread()
{
    Context context;

    while (true)
    {
        Job *job;
        int batch;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_available.wait(lock, [this]() { return m_stop || !m_jobs.empty(); });

            if (m_stop) {
                return;
            }

            job = m_jobs.front();
            batch = job->m_next++;

            if (job->m_next == job->m_batches) {
                m_jobs.pop_front();
            }
        }

        bool finished;

        try
        {
            job->m_decode(context.decoder(job->m_family, job->m_type, job->m_rate), context, batch);
        }
        catch (...)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (!job->m_error) {
                job->m_error = std::current_exception();
            }

            cancel(*job);
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            finished = ++job->m_done == job->m_batches;
        }

        // The job may be gone once its runner sees this
        if (finished) {
            m_done.notify_all();
        }
    }
}

} // namespace dvbt2
