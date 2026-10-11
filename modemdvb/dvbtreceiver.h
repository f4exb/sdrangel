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

#ifndef MODEMDVB_DVBTRECEIVER_H
#define MODEMDVB_DVBTRECEIVER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "dvbt.h"
#include "dvbtofdm.h"
#include "dvbtfec.h"
#include "cwcanceller.h"
#include "resampler.h"

class SampleThread;

namespace dvbt {

// Complete DVB-T receiver: baseband samples in, MPEG transport stream packets out.
// If threaded, resampling runs in the caller's thread, OFDM demodulation in a thread of its own, inner
// de-interleaving and de-mapping in another, and the rest of error correction (Viterbi and outer decoding) in a third,
// with packets handed back to the caller's thread at the end of each call to process(). Otherwise all in the caller's
// thread
class MODEMDVB_API Receiver
{
public:
    typedef std::function<void(const uint8_t *packet)> PacketCallback; //!< 188-byte TS packet

    struct Status
    {
        OFDMDemod::Status m_ofdm;
        OuterDecoder::Stats m_outer;
        bool m_tsSync = false;
        bool m_supported = true;        //!< False for an invalid mode
        bool m_lowPriority = false;     //!< Decoding a hierarchical mode's low priority stream
        double m_mer = 0.0;             //!< dB
        double m_preViterbiBER = 0.0;   //!< Recent average, from re-encoding the decoded bits
        double m_postViterbiBER = 0.0;  //!< Recent average, from the bits Reed-Solomon corrects. QEF needs < 2e-4
        uint64_t m_viterbiBits = 0;
        uint64_t m_queueWaits = 0;      //!< Times demodulation or de-mapping waited for the next stage to catch up
        std::vector<double> m_notches;  //!< Frequencies of narrowband interferers notched out, Hz from the centre
        double m_seconds[5] = {0.0, 0.0, 0.0, 0.0, 0.0}; //!< Processing time: OFDM (and resampling), inner, Viterbi, BER, outer
    };

    //! FFT engines come from fftFactory if not null
    explicit Receiver(int bandwidthHz = 8000000, FFTFactory *fftFactory = nullptr, bool threaded = true);
    ~Receiver();

    void setPacketCallback(PacketCallback callback) { m_packetCallback = callback; }
    void setLogCallback(OFDMDemod::LogCallback callback) { m_ofdm.setLogCallback(callback); }
    //! Also called with each demodulated symbol, from the thread that calls process(). For analysis
    void setSymbolObserver(OFDMDemod::SymbolCallback callback) { m_symbolObserver = callback; }
    void reset();
    //! Input sample rate, if not sampleRate(bandwidth). Between about 0.8 and 2.2 times that
    void setInputSampleRate(double sampleRate);
    //! Notching out strong narrowband interferers, on by default. From the thread that calls process()
    void setNotchEnabled(bool enabled) { m_canceller.setEnabled(enabled); }
    //! In hierarchical modes, decode the low priority stream rather than the high priority one
    void setLowPriority(bool lowPriority) { m_fec.setLowPriority(lowPriority); }
    //! Threads sharing Viterbi decoding with the error correction thread, for slow CPUs. By default, they're added
    //! while that thread is busy over 85 % of the time (up to 3, and the cores the other threads leave). From
    //! any thread, which stops them being added
    void setViterbiThreads(int threads);
    void process(const Cf *samples, int nbSamples);
    void flush(); //!< Wait for demodulation and error correction to finish what is queued, and output the packets
    Status getStatus() const;
    //! Only to be used after flush()
    const OFDMDemod& getOFDMDemod() const { return m_ofdm; }
    //! As OFDMDemod::getDisplayData(). If threaded, from the demodulator a moment ago (none the first time)
    void getDisplayData(std::vector<Cf>& cells, int maxCells, std::vector<float>& responseDb, int points) const;
    //! As OFDMDemod::getImpulseResponse(), from when getDisplayData() was last satisfied
    void getImpulseResponse(ImpulseResponse& response) const;

private:
    struct QueuedSymbol
    {
        bool m_reset;           //!< Reset error correction, rather than decode
        uint64_t m_epoch = 0;   //!< Of the reset
        int m_index;
        TPS m_tps;
        std::vector<Cf> m_cells;
        std::vector<float> m_csi;
    };

    struct QueuedDemapped
    {
        bool m_reset;           //!< Reset decoding, rather than decode
        uint64_t m_epoch = 0;   //!< Of the reset
        FECDecoder::Demapped m_demapped;
    };

    void demodulate(const Cf *samples, int nbSamples);
    OFDMDemod::Status ofdmStatus() const;
    void onSymbol(const OFDMDemod::Symbol& symbol);
    void onPacket(const uint8_t *packet);
    void runInner();
    void runFEC();
    bool idle() const;
    void deliverPackets();
    void correctClock();

    static constexpr size_t m_maxQueuedSymbols = 512;
    static constexpr size_t m_maxFreeSymbols = 16;     //!< Kept for reuse once the queue empties (about 72 kB each)
    static constexpr size_t m_maxQueuedDemapped = 32;  //!< About 48 kB each
    // Clock offsets beyond this are corrected by resampling, rather than just by moving the FFT window,
    // as they reduce the MER (by a few dB at 20 ppm in 8K mode)
    static const double m_clockCorrectionThreshold;         //!< ppm
    static constexpr uint64_t m_clockCorrectionPeriod = 256;    //!< Symbols
    static constexpr int m_clockFirstMeasurements = 20;         //!< Symbols the clock offset is averaged over, to correct it first
    // Viterbi threads are added while the error correction thread is busy over this fraction of the time, with symbols
    // waiting for it, and removed when it would have been under the lower fraction with one fewer, for a few windows
    static const double m_viterbiAddLoad;
    static const double m_viterbiRemoveLoad;
    static constexpr int m_viterbiRemoveWindows = 3;            //!< Of 2 s

    static double now();

    PacketCallback m_packetCallback;
    OFDMDemod::SymbolCallback m_symbolObserver;
    double m_outputRate;            //!< sampleRate(bandwidth)
    double m_inputRate;
    bool m_resampling;
    Resampler m_resampler;
    CWCanceller m_canceller;
    std::vector<Cf> m_notched;
    std::vector<double> m_notchFrequencies; //!< With m_ofdmMutex
    // Notches reach the demodulator with the samples they apply to, as clock corrections do, as the canceller's
    // output is notched with them from the start of the call to process() that changed them
    struct NotchChange
    {
        uint64_t m_sample;          //!< From this sample pushed to the demodulator
        std::vector<double> m_frequencies;
    };
    std::deque<NotchChange> m_notchChanges; //!< With m_ofdmMutex
    std::vector<double> m_notchesQueued;    //!< The last in m_notchChanges, or given the demodulator
    std::vector<Cf> m_resampled;
    std::vector<Cf> m_lastInput;    //!< Recent input, to start resampling seamlessly
    // Clock corrections reach the demodulator with the samples they apply to, so its estimate follows them
    struct ClockAdjustment
    {
        uint64_t m_sample;          //!< From this sample pushed to the demodulator
        double m_ppm;
    };
    uint64_t m_samplesPushed;
    uint64_t m_samplesDemodulated;
    std::deque<ClockAdjustment> m_clockAdjustments;   //!< With m_ofdmMutex
    void setRatioCorrection(double ratio);
    uint64_t m_clockUpdateSymbols;  //!< When the clock correction was last updated
    OFDMDemod m_ofdm;
    double m_ofdmSeconds;

    // Demodulation, in its own thread if threaded, with what's needed of it from other threads
    std::unique_ptr<SampleThread> m_demodThread;
    mutable std::mutex m_ofdmMutex;             //!< For these
    OFDMDemod::Status m_ofdmStatus;
    mutable bool m_displayRequested;
    mutable int m_displayMaxCells;
    mutable int m_displayPoints;
    std::vector<Cf> m_displayCells;
    std::vector<float> m_displayResponse;
    ImpulseResponse m_displayImpulse;

    // Error correction, in two worker threads if threaded: de-mapping, then decoding
    bool m_threaded;
    FECDecoder m_fec;
    std::thread m_innerThread;
    std::thread m_thread;
    mutable std::mutex m_mutex;     //!< For the queues and status below
    std::condition_variable m_symbolAvailable;
    std::condition_variable m_idle;
    std::condition_variable m_spaceAvailable;
    std::condition_variable m_demappedAvailable;
    std::condition_variable m_demappedSpace;
    std::deque<QueuedSymbol> m_queue;
    std::vector<QueuedSymbol> m_free;
    QueuedSymbol m_filling;         //!< The demodulator thread's next symbol for the queue, filled without m_mutex
    std::deque<QueuedDemapped> m_demappedQueue;
    std::vector<QueuedDemapped> m_demappedFree;
    bool m_innerBusy;
    bool m_busy;
    // Viterbi threads added as the error correction thread's load needs them
    std::atomic<bool> m_autoViterbi{true};
    int m_viterbiThreads = 0;
    double m_fecBusy = 0.0;                 //!< Seconds decoding since m_fecWindowStart
    std::chrono::steady_clock::time_point m_fecWindowStart;
    double m_fecViterbiSeconds = 0.0;       //!< FECDecoder's Viterbi time at m_fecWindowStart
    int m_fecQuietWindows = 0;              //!< Consecutive windows it would have been comfortable with a thread fewer
    std::vector<uint8_t> m_fecPackets;      //!< Decoded by one call, added to m_packets together
    bool m_stop;
    std::vector<uint8_t> m_packets; //!< Decoded, waiting to be delivered
    uint64_t m_resetEpoch = 0;      //!< Resets queued
    uint64_t m_decodeEpoch = 0;     //!< Of the last reset decoding has reached: packets are dropped until it's m_resetEpoch
    std::vector<uint8_t> m_delivering;
    FECDecoder::Status m_fecStatus;
    uint64_t m_queueWaits;
};

} // namespace dvbt

#endif // MODEMDVB_DVBTRECEIVER_H
