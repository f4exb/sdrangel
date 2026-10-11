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

#ifndef MODEMDVB_DVBSRECEIVER_H
#define MODEMDVB_DVBSRECEIVER_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "dvbs.h"
#include "dvbsfec.h"
#include "dvbsfrontend.h"

class SampleThread;

namespace dvbs {

//! QPSK carrier recovery: the frequency from the spectrum of the symbols' fourth power (which takes the modulation off,
//! leaving a line at four times the offset, unambiguous to an eighth of the symbol rate), then a second order decision
//! directed phase locked loop. The loop is updated every 8 symbols, each group turned by the phase and frequency as they
//! were at its start, so the symbols of a group are independent (the loop's bandwidth is far below an eighth of the
//! symbol rate). Lock is judged from the fourth power's coherence
class MODEMDVB_API QPSKCarrier
{
public:
    typedef std::complex<float> Cf;

    QPSKCarrier();
    void reset();
    //! Noise bandwidth times the symbol period, once tracking
    void setBandwidth(double bnT);
    //! The symbols, with the carrier taken off, into w, from when tracking (none while acquiring). Returns how many.
    //! restarted: set if it lost lock and acquired again, so what follows doesn't follow what went before, with
    //! restartAt the index in w of the first symbol after (those before are of the lock before)
    int process(const Cf *z, int nbSymbols, Cf *w, bool& restarted, int& restartAt);
    bool tracking() const { return m_tracking; }
    bool locked() const { return m_tracking && (m_lock > m_lockThreshold); }
    double lockMetric() const { return m_lock; }
    double frequency() const { return m_frequency - m_pendingShift; }   //!< Cycles a symbol, with any shift to come
    //! The frequency is moved elsewhere (the front end's NCO): taken off the loop's after the symbols the front end
    //! already holds, which it turned at the old frequency (else they'd be turned a few symbols' worth of it too little:
    //! a step in phase of tens of degrees)
    void shiftFrequency(double cyclesPerSymbol, int afterSymbols);
    double mer() const;                 //!< dB, from the decisions
    //! dB, Es/N0 from the symbols' second and fourth moments (M2M4): unlike the MER, not raised by decision errors at
    //! low SNR, and needing no decisions or phase
    double snr() const;
    uint64_t acquisitions() const { return m_acquisitions; }

    static const int m_acquireSymbols = 4096;

private:
    static constexpr double m_pi = 3.141592653589793;
    static constexpr double m_acquireBandwidth = 0.02;      //!< Of the carrier loop, for its first symbols
    static constexpr double m_trackBandwidth = 0.005;
    static constexpr uint64_t m_acquireTrackSymbols = 2000; //!< At the acquisition bandwidth
    static constexpr uint64_t m_unlockedLimit = 30000;      //!< Symbols judged unlocked before acquiring again

    static inline std::complex<float> rotation(double angle);     //!< e^j(angle)
    void computeGains(double bnT);

    bool m_tracking;
    std::vector<std::complex<double>> m_acquire;    //!< Fourth powers
    Cf m_rotator;               //!< e^-j(phase)
    double m_frequency;         //!< Cycles a symbol
    double m_pendingShift;      //!< To take off the frequency after m_pendingSymbols
    int m_pendingSymbols;
    double m_kp, m_ki;
    double m_bandwidth;
    double m_lock;              //!< Average of the fourth power's coherence: 1 without noise, 0 unlocked
    double m_errorPower;
    bool m_averaged;            //!< The error power and moments have had an update since tracking started
    double m_m2;                //!< Averages of the symbols' power and its square, for snr()
    double m_m4;
    uint64_t m_tracked;         //!< Symbols since tracking started
    uint64_t m_unlocked;        //!< Consecutive symbols judged unlocked
    uint64_t m_acquisitions;
    static constexpr double m_lockThreshold = 0.12;
};

//! DVB-S receiver (EN 300 421): samples at the channel rate in, transport stream packets out. If threaded, the front
//! end and carrier recovery run in a thread of their own, error correction in another, and packets are delivered in
//! the thread calling process(). Otherwise all in the caller's thread
class MODEMDVB_API Receiver
{
public:
    typedef std::function<void(const uint8_t *packet)> PacketCallback;
    typedef std::complex<float> Cf;

    enum class State { Spectrum = 0, Carrier = 1, Code = 2, Sync = 3, Locked = 4 };

    struct Status
    {
        State m_state = State::Spectrum;
        FrontEnd::Status m_frontEnd;
        double m_symbolRate = 0.0;      //!< Set
        double m_frequencyOffset = 0.0; //!< Hz, of the carrier from the channel's centre
        double m_clockOffset = 0.0;     //!< ppm
        bool m_carrierLocked = false;
        double m_carrierLock = 0.0;     //!< Lock metric, 0 to 1
        double m_mer = 0.0;             //!< dB
        double m_snr = 0.0;             //!< dB, Es/N0 from the symbols' moments; 0 until tracking
        //! These two count on through reset() and configure() (as the error correction's searches and locks do), so
        //! a change is seen whenever the status is read
        uint64_t m_carrierAcquisitions = 0;
        uint64_t m_restarts = 0;            //!< Acquisitions started again (the spectrum measured again), without lock
        FECDecoder::Status m_fec;
        uint64_t m_queueWaits = 0;
        double m_seconds[3] = {0.0, 0.0, 0.0};  //!< Front end, carrier recovery, error correction
    };

    explicit Receiver(bool threaded = true);
    ~Receiver();

    //! The channel's sample rate, from 1.5 (1 + roll-off, for the matched filter) to 64 times the symbol rate (twice
    //! or more is best). Acquires again
    void configure(double inputRate, double symbolRate);
    //! -1 to find the code rate, else its index (dvbt::CodeRate). From any thread
    void setCodeRate(int rate) { m_codeRate = rate; }
    //! The carrier loop's noise bandwidth times the symbol period, once tracking: wider follows more phase noise (as
    //! a Ku band LNB's at low symbol rates), narrower slips less at low SNR. 0 (the default) for automatic: from 0.005
    //! at an MER of 5 dB to 0.02 from 11 dB. From any thread
    void setCarrierBandwidth(double bnT) { m_carrierBandwidth = bnT; }
    void setPacketCallback(PacketCallback callback) { m_packetCallback = callback; }
    void reset();
    void process(const Cf *samples, int nbSamples);
    //! Wait for everything queued to be demodulated and decoded, and deliver the packets
    void flush();
    //! The front end shared with the DVB-S2 receiver (by SharedFrontEnd, which runs it and gives this its symbols by
    //! processSymbols()), or this receiver's own again (nullptr). With neither demodulating
    void setSharedFrontEnd(FrontEnd *frontEnd);
    //! The shared front end's symbols, in the thread running it
    void processSymbols(const Cf *symbols, int nbSymbols);
    //! Shared, in its thread: the symbols from here don't follow on from the last (samples were dropped)
    void discontinuity();
    //! Shared, in its thread: the front end's NCO moved by hz for the other receiver, from afterSymbols (those it held
    //! were turned as before), so the carrier loop's frequency is moved to match
    void frontEndShifted(double hz, int afterSymbols);
    //! Shared, in its thread: the front end was reset for the other receiver (the spectrum measured again), so the
    //! carrier's acquired again and the error correction searches again
    void frontEndReset();
    //! Shared: whether this has asked for the spectrum to be measured again (taken), and has a lock it'd lose
    bool takeFrontEndRestart() { return m_frontEndRestart.exchange(false); }
    bool frontEndLocked() const { return m_fecLocked; }
    double frontEndEsN0() const { return m_carrier.tracking() ? m_carrier.snr() : 0.0; }
    //! Its own front end's equaliser on (the default) or off (a shared front end's is set there)
    void setEqualiser(bool enabled) { m_frontEnd.setEqualiser(enabled); }
    //! The packets decoded, to the callback, in the caller's thread (process() does it unshared)
    void deliverPackets();
    Status getStatus() const;
    //! Up to maxSymbols of the demodulated symbols, and the input's spectrum (dB, points from -rate/2 to +rate/2). If
    //! threaded, from a moment ago (none the first time)
    void getDisplayData(std::vector<Cf>& symbols, int maxSymbols, std::vector<float>& spectrumDb, int points) const;

private:
    struct SoftBlock
    {
        bool m_reset = false;
        uint64_t m_epoch = 0;
        std::vector<int8_t> m_soft;     //!< I and Q of each symbol
    };

    static constexpr float m_softScale = 16.0f * 1.41421356f;   //!< As DVB-T's: 16 for each unit of an unnormalised QPSK point
    static constexpr double m_foldBack = 0.02;              //!< Cycles a symbol, of the loop's frequency, moved to the NCO
    static constexpr double m_timingAcquire = 2e-3;
    static constexpr double m_timingTrack = 5e-4;
    static constexpr double m_relockSeconds = 4.0;          //!< Without the FEC locked, before the spectrum's measured again
    static constexpr double m_relockMinSymbols = 600000.0;  //!< (and at least these symbols, for the search at low rates)
    static constexpr int m_nbDisplaySymbols = 4096;

    static double now();
    void demodulate(const Cf *samples, int nbSamples);
    //! The front end's symbols in m_symbols: the carrier, and the error correction. start and frontEndEnd for timing
    void demodulateSymbols(double start, double frontEndEnd);
    bool decodeBlock(SoftBlock& block);
    //! To the error correction: a block of soft symbols (its buffer swapped for a free one), or a reset after a restart
    void deliver(SoftBlock& block);
    void deliverReset();
    void runFEC();
    bool idle() const;

    bool m_threaded;
    double m_inputRate;
    double m_symbolRate;
    std::atomic<int> m_codeRate;
    std::atomic<double> m_carrierBandwidth;
    PacketCallback m_packetCallback;

    // Demodulation, in its own thread if threaded
    FrontEnd m_frontEnd;
    FrontEnd *m_fe;                     //!< m_frontEnd, or a shared one
    std::atomic<bool> m_frontEndRestart;    //!< Shared: asked for
    QPSKCarrier m_carrier;
    std::vector<Cf> m_symbols;
    std::vector<Cf> m_tracked;          //!< With the carrier taken off
    std::atomic<bool> m_fecLocked;
    std::unique_ptr<SampleThread> m_demodThread;
    mutable std::mutex m_demodMutex;    //!< For these
    Status m_demodStatus;
    mutable std::atomic<bool> m_displayRequested;
    mutable int m_displayMaxSymbols;
    mutable int m_displayPoints;
    std::vector<Cf> m_displaySymbols;
    std::vector<float> m_displaySpectrum;
    std::vector<Cf> m_recentSymbols;    //!< The last demodulated, for display

    // Error correction, in its own thread if threaded
    FECDecoder m_fec;
    std::thread m_fecThread;
    mutable std::mutex m_mutex;         //!< For the queue, packets and status below
    std::condition_variable m_available;
    std::condition_variable m_space;
    std::condition_variable m_idle;
    std::deque<SoftBlock> m_queue;
    std::vector<SoftBlock> m_free;
    SoftBlock m_filling;
    bool m_busy;
    bool m_stop;
    uint64_t m_resetEpoch;
    uint64_t m_unlockedSymbols;         //!< Since a packet last passed RS (demodulation thread)
    std::atomic<bool> m_fecProgress;    //!< A packet has passed RS (set by the FEC thread, taken by demodulation)
    uint64_t m_lastGoodPackets;         //!< Packets that passed RS, as last counted (FEC thread)
    uint64_t m_restarts;                //!< Acquisitions started again, without lock (demodulation thread)
    uint64_t m_decodeEpoch;
    int m_fecCodeRate;                  //!< As the decoder has it
    std::vector<uint8_t> m_fecPackets;  //!< From one block
    std::vector<uint8_t> m_packets;     //!< For the caller's thread
    std::vector<uint8_t> m_delivering;
    FECDecoder::Status m_fecStatus;
    uint64_t m_queueWaits;
    std::atomic<double> m_fecSeconds;   //!< Written in the error correction thread

    static const size_t m_maxQueued = 64;
};

} // namespace dvbs

#endif // MODEMDVB_DVBSRECEIVER_H
