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

#ifndef MODEMDVB_DVBS2RECEIVER_H
#define MODEMDVB_DVBS2RECEIVER_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "dvbs2.h"
#include "dvbs2pl.h"
#include "dvbsfrontend.h"
#include "dvbt2fec.h"
#include "ldpcpool.h"

class SampleThread;

namespace dvbs2 {

//! DVB-S2 receiver (EN 302 307): samples at the channel rate in, transport stream packets of one input stream out.
//! If threaded, the front end and physical layer decoding run in a thread of their own, and the XFECFRAMEs are queued
//! for error correction in another: it takes the frames waiting, in batches of as many as the LDPC decoder decodes at
//! once (with the same code), each demapped, LDPC and BCH decoded by one of the decoder threads, then the BBFRAMEs in
//! order. So at low symbol rates each frame's decoded on its own, as soon as it arrives, and at high ones the batches
//! fill. Packets are delivered in the thread calling process()
class MODEMDVB_API Receiver
{
public:
    typedef std::function<void(const uint8_t *packet)> PacketCallback;

    enum class State { Spectrum = 0, Searching = 1, Frames = 2, Locked = 3 };

    struct Status
    {
        State m_state = State::Spectrum;
        dvbs::FrontEnd::Status m_frontEnd;
        PLDecoder::Status m_pl;
        double m_symbolRate = 0.0;
        double m_frequencyOffset = 0.0;     //!< Hz, of the carrier from the channel's centre
        double m_clockOffset = 0.0;         //!< ppm
        double m_rollOff = 0.35;            //!< The matched filter's
        //! From the BBHEADERs: 0 0.35, 1 0.25, 2 0.20, S2X's 4 0.15, 5 0.10, 6 0.05, -1 not yet
        int m_matypeRollOff = -1;
        bool m_multipleStreams = false;
        bool m_acm = false;                 //!< VCM or ACM, from the BBHEADER
        int m_isi = -1;                     //!< Input stream being decoded (time slicing's: the time slice's TSN)
        uint64_t m_streamPackets = 0;       //!< Packets of it, since it was chosen: Locked needs some (m_bb's are all time's)
        int m_isiCount = 0;                 //!< Input streams seen
        std::vector<int> m_isis;            //!< Their identifiers (or TSNs), in order
        uint64_t m_fecFrames = 0;           //!< Decoded
        uint64_t m_ldpcFailures = 0;        //!< Frames LDPC didn't converge on (BCH may still correct them)
        uint64_t m_fecErrors = 0;           //!< Decodes that failed (an exception, as running out of memory): their frames lost
        uint64_t m_bchFailures = 0;
        uint64_t m_bchCorrected = 0;        //!< Bits
        double m_ldpcIterations = 0.0;      //!< Recent average
        uint64_t m_otherStreams = 0;        //!< BBFRAMEs of other input streams, skipped
        dvbt2::BBFrameDecoder::Stats m_bb;
        uint64_t m_queueWaits = 0;
        uint64_t m_restarts = 0;            //!< Acquisitions started again (the spectrum measured again), without lock
        uint64_t m_searchedSymbols = 0;     //!< Searched for headers, not locked
        uint64_t m_skippedSymbols = 0;      //!< Not searched, to keep to the search's budget
        double m_seconds[4] = {0.0, 0.0, 0.0, 0.0}; //!< Front end, physical layer, demapping with LDPC and BCH, BBFRAMEs
    };

    //! decoderThreads: for LDPC decoding in all, 0 for all but two of the cores, up to 4
    explicit Receiver(bool threaded = true, int decoderThreads = 0);
    ~Receiver();
    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    //! The channel's sample rate, from 1.5 (1 + roll-off, for the matched filter) to 64 times the symbol rate (twice
    //! or more is best). Acquires again
    void configure(double inputRate, double symbolRate);
    //! The matched filter's roll-off, or 0 to follow the BBHEADER's (starting at 0.35). From any thread
    void setRollOff(double rollOff) { m_rollOffSetting = rollOff; }
    //! Input stream to decode, in a multiple input stream (or time slice, by its TSN), or -1 for the first. From any thread
    void setISI(int isi) { m_isiSetting = isi; }
    //! Gold code n of the physical layer scrambling. Acquires again
    void setScramblingCode(int n);
    void setMaxIterations(int iterations) { m_maxIterations = iterations; }
    void setPacketCallback(PacketCallback callback) { m_packetCallback = callback; }
    void reset();
    void process(const Cf *samples, int nbSamples);
    //! The front end shared with the DVB-S receiver (by dvbs::SharedFrontEnd, which runs it and gives this its symbols
    //! by processSymbols()), or this receiver's own again (nullptr). With neither demodulating
    void setSharedFrontEnd(dvbs::FrontEnd *frontEnd);
    //! The shared front end's symbols, in this receiver's thread (the front end's own, or one of its own beside it):
    //! first, the front end's index of the first of them (of all it's made); frontEnd, its status as it made them;
    //! corrections, how many of the frequency corrections this asked for it had made by then, the last from symbol
    //! boundary; otherLoad, the cores the rest of this thread uses (the front end and DVB-S, if in it), so the search's
    //! budget is what's left
    void processSymbols(const Cf *symbols, int nbSymbols, uint64_t first, const dvbs::FrontEnd::Status& frontEnd,
        uint64_t corrections, uint64_t boundary, double otherLoad);
    //! Shared: whether this has asked for the spectrum to be measured again (taken), and has a lock it'd lose. From
    //! any thread
    bool takeFrontEndRestart() { return m_frontEndRestart.exchange(false); }
    bool frontEndLocked() const { return m_plLocked; }
    double frontEndEsN0() const { return m_esn0; }
    //! The timing loop's bandwidth while searching: wide to acquire, but below -3 dB Es/N0 (from the spectrum), as it
    //! would track at (at VL-SNR's lowest SNRs, the wide loop wanders thousands of ppm, too far for any header to be found)
    static double searchTimingBandwidth(double spectralEsN0);
    //! Seconds without a lock after which the receiver starts again (the spectrum measured again): 3, or at low symbol
    //! rates as long as the slowest acquisitions take (VL-SNR's, time slicing's near threshold). A restart sooner,
    //! here or by the receiver's owner, never locks to them
    static double relockSeconds(double symbolRate);
    //! Shared, for the front end's owner to make, from its thread: the matched filter's roll-off wanted (0 for none
    //! yet), and the large frequency corrections asked for (how many, and their sum, Hz: both only grow)
    double frontEndRollOff() const { return m_frontEndRollOff; }
    uint64_t frequencyRequests() const { return m_frequencyRequests; }
    double frequencyRequested() const { return m_frequencyRequested; }
    //! A shared front end's owner: the clock correction asked for (ppm), taken
    double takeClockRequest() { return m_clockRequested.exchange(0.0); }
    //! Shared, in its thread: the symbols from here don't follow on from the last (samples were dropped)
    void discontinuity();
    //! Its own front end's equaliser on (the default) or off (a shared front end's is set there)
    void setEqualiser(bool enabled) { m_frontEnd.setEqualiser(enabled); }
    //! The packets decoded, to the callback, in the caller's thread (process() does it unshared)
    void deliverPackets();
    //! Wait for everything queued to be decoded, and deliver the packets
    void flush();
    Status getStatus() const;
    //! Up to maxCells of the last frame's data cells, and the input's spectrum (dB, points from -rate/2 to +rate/2)
    void getDisplayData(std::vector<Cf>& cells, int maxCells, std::vector<float>& spectrumDb, int points) const;

private:
    struct FrameSlot
    {
        bool m_reset = false;           //!< Not a frame: decoding starts again
        bool m_skip = false;            //!< Not decodable
        uint64_t m_epoch = 0;
        XFECFrame m_frame;
        std::vector<int8_t> m_llrs;
        std::vector<uint8_t> m_packed;  //!< BCH codeword, hard decisions, corrected
        int m_corrected = 0;            //!< By BCH, -1 if it couldn't
    };

    //! What a code's frames need, shared by the decoder threads
    struct Code
    {
        std::vector<int> m_map;
        std::vector<Cf> m_points;
        DemapTable m_demapTable;        //!< For 128 and 256APSK
        int m_eta = 2;
        std::unique_ptr<dvbt2::BCH> m_bch;
        int m_kldpc = 0;
        int m_kbch = 0;                 //!< A VL-SNR frame's less its shortening
        int m_vlsnr = -1;               //!< A VL-SNR frame's header's index
    };

    struct Scratch : public dvbt2::LDPCPool::Scratch
    {
        std::vector<int8_t> m_cellLLRs;     //!< A VL-SNR QPSK frame's, of its bits sent
    };

    void demodulate(const Cf *samples, int nbSamples);
    //! The front end's symbols in m_symbols: the physical layer, and the frames to the error correction
    void demodulateSymbols(double start, double mid);
    void restartFrontEnd();
    void onFrame(XFECFrame& frame);
    void decodeFrames(std::vector<FrameSlot*>& frames);
    //! decodeFrames, an exception (LDPCPool::run rethrows a decoder thread's, as bad_alloc) caught: the frames lost,
    //! the BBFRAMEs discontinuous, a reset among them made, counted (m_fecErrors)
    void decodeFramesCaught(std::vector<FrameSlot*>& frames);
    //! The error correction's state reset, where a reset slot is reached
    void resetDecoding(const FrameSlot& slot);
    void decodeBatch(dvbt2::LDPC& ldpc, dvbt2::LDPCPool::Context& context, std::vector<FrameSlot*>& frames, size_t first,
        size_t count, const Code& code);
    const Code& code(const XFECFrame& frame);
    //! A VL-SNR frame's LLRs: its shortened bits known zeros, its punctured ones unknown, its spread ones summed
    void vlsnrLLRs(const XFECFrame& frame, const Code& code, int8_t *llrs, int n, Scratch& scratch);
    void deframe(FrameSlot& slot);
    //! No input stream chosen, none seen (after a reset)
    void clearStreams();
    void runFEC();
    bool idle() const;
    FrameSlot *freeSlot();
    static double now();
    static double rollOffValue(int ro);

    static constexpr int m_defaultMaxIterations = 50;
    static constexpr int m_vlsnrIterations = 75;
    static constexpr float m_outlierThreshold = 16.0f;  //!< As DVB-T2's
    static constexpr double m_timingAcquire = 2e-3;
    static constexpr double m_timingTrack = 5e-4;
    static constexpr double m_minRelockSeconds = 3.0;   //!< Without frames, before the spectrum's measured again (at least)
    static constexpr int m_maxFailedFrames = 32;            //!< Locked, but frames failing in a row: perhaps a false lock, if
    static constexpr double m_searchBudget = 0.4;       //!< Of a core, at most, for the physical layer's search (symbols skipped)
    static constexpr double m_threadBudget = 0.6;       //!< Of a core, for the thread as a whole: the search has what's left,
                                                        //!< after the front end (and in Auto, the DVB-S receiver)
    static constexpr double m_minSearchBudget = 0.1;    //!< Even so
    static constexpr double m_steadyLockSeconds = 2.0;  //!< Locked this long, the search's budget starts again from nothing
    static constexpr double m_maxSkipSeconds = 0.25;    //!< Of symbols the budget skips after a window, at most: one that cost
                                                        //!< far too much (a fault) mustn't stop the search for long
    static constexpr int64_t m_searchWindow = 131072;       //!< Symbols searched at a time: a few of the longest frames, so a header
                                                        //!< and the next are found in it
    static constexpr int m_lowRollOffFrames = 16;           //!< Trusted frames without an RO of 11, after which S2X's low roll-offs have ended
    static constexpr double m_failingSeconds = 1.0;     //!< Failing for this long too
    static constexpr uint64_t m_nullOnlyPackets = 1000;     //!< Of an input stream chosen automatically, all null: another's chosen
    static constexpr uint64_t m_missingStreamFrames = 2000; //!< Of other streams, without the one chosen automatically: it's
                                                        //!< gone (the transmitter's changed its streams), another's chosen
    static constexpr size_t m_minTakeBatches = 4;           //!< The FEC thread takes at most whole rounds of its decoders' batches,
                                                        //!< at least this many: more, and the last round leaves decoders idle
    static constexpr size_t m_maxLanes = 32;                //!< The LDPC decoder's widest batch (AVX2's), for the slots made at first

    bool m_threaded;
    double m_inputRate;
    double m_symbolRate;
    std::atomic<double> m_rollOffSetting;
    std::atomic<int> m_isiSetting;
    std::atomic<int> m_maxIterations;
    std::atomic<int> m_matypeRollOff;   //!< From the BBHEADERs, for the front end
    // The FEC thread's (reset where it reaches a reset, not by the caller of reset())
    bool m_lowRollOffs;                 //!< A BBHEADER's RO was 11: S2X's low roll-offs, the others' codes theirs
    int m_sinceLowRollOffs;             //!< Trusted frames since an RO of 11: many, the low roll-offs have ended
    PacketCallback m_packetCallback;

    // Demodulation, in its own thread if threaded
    dvbs::FrontEnd m_frontEnd;
    dvbs::FrontEnd *m_fe;               //!< m_frontEnd, or a shared one
    std::atomic<bool> m_frontEndRestart;    //!< Shared: asked for
    std::atomic<bool> m_plLocked;
    std::atomic<double> m_esn0{0.0};    //!< The physical layer's, for the shared front end's equaliser
    // Shared: the front end's status with the symbols, and what's asked of its owner (the front end's in another thread)
    dvbs::FrontEnd::Status m_feStatus;
    std::atomic<double> m_frontEndRollOff{0.0};
    std::atomic<uint64_t> m_frequencyRequests{0};
    std::atomic<double> m_frequencyRequested{0.0};
    std::atomic<double> m_clockRequested{0.0};      //!< Of a shared front end, by its owner
    uint64_t m_awaitClock = 0;          //!< A shared front end's clock adjustment count that says it's made (0: none)
    uint64_t m_awaitCorrection = 0;     //!< The frequency request whose corrected symbols are awaited, 0 for none
    double m_pendingCorrection = 0.0;   //!< Cycles a symbol: turned here, till then
    double m_pendingPhase = 0.0;        //!< Radians, of that turning
    bool shared() const { return m_fe != &m_frontEnd; }
    const dvbs::FrontEnd::Status& frontEndStatus() const { return shared() ? m_feStatus : m_frontEnd.getStatus(); }
    PLDecoder m_pl;
    std::vector<Cf> m_symbols;
    uint64_t m_unlockedSymbols;         //!< Since the physical layer was last locked
    // The search's CPU budget: its time and symbols over a window, and the symbols still to skip
    double m_searchSeconds = 0.0;
    double m_otherLoad = 0.0;           //!< Cores the rest of the thread uses (its own front end's, or as set)
    int64_t m_searchSymbols = 0;
    int64_t m_skipSymbols = 0;
    int64_t m_lockedSymbols = 0;        //!< Since the search last ran, for its budget to start again after a steady lock
    std::atomic<int> m_failedFrames;    //!< FECFRAMEs in a row that BCH or the BBHEADER's CRC failed (FEC thread)
    uint64_t m_failingSymbols;          //!< Locked, since that reached m_maxFailedFrames
    uint64_t m_restarts;                //!< Acquisitions started again (demodulation thread)
    std::unique_ptr<SampleThread> m_demodThread;
    mutable std::mutex m_demodMutex;    //!< For these
    Status m_demodStatus;
    mutable std::atomic<bool> m_displayRequested;
    mutable int m_displayMaxCells;
    mutable int m_displayPoints;
    std::vector<Cf> m_displayCells;
    std::vector<float> m_displaySpectrum;

    // Error correction
    std::unique_ptr<dvbt2::LDPCPool> m_pool;
    dvbt2::LDPCPool::Context m_context;     //!< The worker's
    std::map<int, std::unique_ptr<Code>> m_codes;   //!< By MODCOD and frame size
    std::thread m_fecThread;
    mutable std::mutex m_mutex;         //!< For the queue, packets and status below
    std::condition_variable m_available;
    std::condition_variable m_space;
    std::condition_variable m_idle;
    std::deque<FrameSlot*> m_queue;
    std::vector<std::unique_ptr<FrameSlot>> m_slots;
    std::vector<FrameSlot*> m_free;
    std::vector<FrameSlot*> m_pending;  //!< Unthreaded: frames from this call
    bool m_busy;
    bool m_stop;
    bool m_draining;                    //!< Flushing: what's queued decoded without waiting for more
    std::atomic<uint64_t> m_resetEpoch; //!< Set with m_mutex, read without it by the FEC thread too
    uint64_t m_decodeEpoch;
    dvbt2::BBFrameDecoder m_bbDecoder;
    std::atomic<int> m_isi;             //!< Being decoded, -1 until one's chosen (written in the error correction thread)
    std::vector<bool> m_isisSeen;
    std::vector<bool> m_isisNullOnly;   //!< Chosen automatically, and found to carry only null packets
    uint64_t m_streamPackets;           //!< Of the stream being decoded, since it was chosen (FEC thread)
    uint64_t m_isiPackets;              //!< Of the stream chosen automatically, since chosen, and of them null
    uint64_t m_isiNulls;
    uint64_t m_isiMissing;              //!< Trusted frames of other streams since the one chosen automatically was seen
    std::vector<uint8_t> m_bbPackets;
    std::vector<uint8_t> m_packets;     //!< For the caller's thread
    std::vector<uint8_t> m_delivering;
    Status m_fecStatus;
    uint64_t m_queueWaits;

    static constexpr size_t m_maxQueued = 160; //!< 5 batches of 32 lanes: across the decoder threads and this one when the frames are coming faster than they're decoded (3 left a laptop's fourth decoder thread idle, and DVB-S2 waiting a quarter of its time)
    size_t m_takeBatches = 1;           //!< The most the FEC thread takes from the queue at once, in batches
    static constexpr int m_batchWaitMs = 100;   //!< The longest a frame waits for a batch to fill
};

} // namespace dvbs2

#endif // MODEMDVB_DVBS2RECEIVER_H
