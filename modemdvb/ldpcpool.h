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

#ifndef MODEMDVB_LDPCPOOL_H
#define MODEMDVB_LDPCPOOL_H

#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "dvbt2fec.h"

namespace dvbt2 {

//! Threads decoding batches of LDPC codewords (with whatever goes with them: demapping before, BCH after), for the
//! DVB-T2 and DVB-S2 receivers. A job's batches are taken, oldest job first, a batch at a time as each thread is free,
//! so a batch that takes longer (more iterations) doesn't hold up the rest. The thread that runs a job decodes its
//! batches too. Each thread keeps decoders for the codes it's been given (an LDPC decoder holds megabytes of messages,
//! so a few threads are plenty: more overflow the cache)
class MODEMDVB_API LDPCPool
{
public:
    //! Whatever a thread needs besides its decoder, made by the job when it's null, kept between jobs
    struct Scratch
    {
        virtual ~Scratch() {}
    };

    //! A thread's decoders, by code, and its scratch. The caller of run() has its own
    class Context
    {
    public:
        LDPC& decoder(CodeFamily family, FECType type, CodeRate rate);
        std::unique_ptr<Scratch> m_scratch;

    private:
        struct Entry
        {
            CodeFamily m_family;
            FECType m_type;
            CodeRate m_rate;
            std::unique_ptr<LDPC> m_ldpc;
        };

        std::deque<Entry> m_decoders;   //!< Most recently used first
        //! Bytes of decoders kept, about: their graphs (shared with the other threads' decoders of the code; the
        //! messages are each thread's, for any code), a dozen normal frames' (more short ones), so VCM and ACM's
        //! MODCODs don't each make their decoder again and again (with two kept, three MODCODs in turn did, the
        //! demapping and LDPC taking five times as long), while threads that have seen many codes don't hold them all
        static const size_t m_maxBytes = 16u << 20;
    };

    struct Job
    {
        CodeFamily m_family = CodeFamily::T2;
        FECType m_type = FECType::Normal;
        CodeRate m_rate = CodeRate::R1_2;
        int m_batches = 0;
        //! Decode a batch, with the thread's decoder for the job's code, and its context (for its scratch)
        std::function<void(LDPC& ldpc, Context& context, int batch)> m_decode;
        // For the pool, with its mutex
        int m_next = 0;
        int m_done = 0;
        std::exception_ptr m_error;     //!< The first a batch threw: run() throws it, once the batches running end
    };

    //! threads: besides those calling run()
    explicit LDPCPool(int threads);
    ~LDPCPool();
    LDPCPool(const LDPCPool&) = delete;
    LDPCPool& operator=(const LDPCPool&) = delete;
    int threads() const { return (int) m_threads.size(); }
    //! Decode the job's batches, by this thread (with its context) and the pool's as they're free. Returns when all are
    //! done; if one threw (as on running out of memory), the rest aren't started, and it's thrown when those running
    //! have ended (so none is left using the job)
    void run(Job& job, Context& context);

private:
    void thread();
    //! With the mutex: no more of the job's batches are started, those not started counted as done
    void cancel(Job& job);

    std::vector<std::thread> m_threads;
    std::mutex m_mutex;
    std::condition_variable m_available;
    std::condition_variable m_done;
    std::deque<Job*> m_jobs;    //!< With batches not yet started
    bool m_stop;
};

} // namespace dvbt2

#endif // MODEMDVB_LDPCPOOL_H
