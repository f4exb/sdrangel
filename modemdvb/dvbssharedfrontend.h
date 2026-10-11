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

#ifndef MODEMDVB_DVBSSHAREDFRONTEND_H
#define MODEMDVB_DVBSSHAREDFRONTEND_H

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "export.h"
#include "dvbs.h"
#include "dvbsfrontend.h"

class SampleThread;

namespace dvbs2 {
class Receiver;
}

namespace dvbs {

class Receiver;

//! One front end (the spectrum's carrier estimate, the matched filter and timing) for a DVB-S and a DVB-S2 receiver,
//! in its own thread, its symbols to whichever are fed, rather than each running its own on the same samples (as
//! while Auto searches for either: half the front end's CPU, a fifth to a third of a core at broadcast rates). Each
//! DVB-S receiver's carrier runs in this thread too, so it moves the NCO in step with the symbols, as it would its own.
//! The DVB-S2 receiver's physical layer runs in a thread of its own beside it, its symbols queued (together the front
//! end and it took more than a core at 27.5 MS/s on a laptop): what it asks of the front end (the matched filter's
//! roll-off, large frequency corrections) is made by this thread between blocks. Its restarts (the spectrum measured
//! again) are those a receiver asks for, unless the other's locked; its timing loop tracks if either is locked
class MODEMDVB_API SharedFrontEnd
{
public:
    //! The receivers use this front end until it's destroyed
    SharedFrontEnd(Receiver *receiverS, dvbs2::Receiver *receiverS2, bool threaded = true);
    ~SharedFrontEnd();

    //! The receivers' too, with nothing being demodulated
    void configure(double inputRate, double symbolRate);
    //! The front end and both receivers, the samples queued dropped (as on a retune)
    void reset();
    //! Waits for the block being demodulated, the samples queued dropped: the receivers can then be changed (as their
    //! scrambling code) from the caller's thread, until the next process()
    void discard();
    //! Which receivers the symbols go to
    void setFeeding(bool s, bool s2);
    //! A receiver reset in this thread, before the next block (as when Auto finds the other, its lock not to be
    //! interrupted by dropping samples)
    void resetReceiverS() { m_resetS = true; }
    void resetReceiverS2() { m_resetS2 = true; }
    void process(const Cf *samples, int nbSamples);
    //! Waits for everything queued to be demodulated (the receivers' error correction may still be decoding)
    void flush();
    //! Samples were dropped before the next: those queued are too, and the receivers start again from them (DVB-S2's
    //! search, DVB-S's error correction)
    void discontinuity();
    FrontEnd::Status getStatus() const;
    //! The equaliser on (the default) or off
    void setEqualiser(bool enabled) { m_frontEnd.setEqualiser(enabled); }

private:
    struct S2Block;
    class S2Thread;

    static constexpr double m_timingAcquire = 2e-3;         //!< As the receivers'
    static constexpr double m_timingTrack = 5e-4;
    static constexpr size_t m_maxQueuedS2 = 1 << 20;        //!< Symbols queued for DVB-S2, before the front end's thread waits

    void demodulate(const Cf *samples, int nbSamples);
    //! In DVB-S2's thread (or this one, unthreaded)
    void demodulateS2(S2Block& block);
    //! What the DVB-S2 receiver's asked of the front end, made between blocks
    void takeRequestsS2();
    //! Its frequency corrections asked for so far taken as made (the front end's starting again, or configured)
    void forgetRequestsS2();

    Receiver *m_receiverS;
    dvbs2::Receiver *m_receiverS2;
    FrontEnd m_frontEnd;
    std::vector<Cf> m_symbols;
    double m_otherLoad = 0.0;           //!< Cores the front end and DVB-S receiver use, averaged
    std::atomic<bool> m_discontinuity{false};
    std::unique_ptr<SampleThread> m_thread;
    std::unique_ptr<S2Thread> m_threadS2;
    std::vector<Cf> m_symbolsS2;        //!< A buffer to queue DVB-S2's symbols in
    uint64_t m_symbolIndex = 0;         //!< Symbols made, of all blocks
    uint64_t m_requestsS2 = 0;          //!< DVB-S2's frequency corrections made
    double m_requestedS2 = 0.0;         //!< Their sum, Hz
    uint64_t m_boundaryS2 = 0;          //!< The first symbol after the last
    std::atomic<bool> m_feedS;
    std::atomic<bool> m_feedS2;
    std::atomic<bool> m_resetS;
    std::atomic<bool> m_resetS2;
    mutable std::mutex m_statusMutex;
    FrontEnd::Status m_status;
};

} // namespace dvbs

#endif // MODEMDVB_DVBSSHAREDFRONTEND_H
