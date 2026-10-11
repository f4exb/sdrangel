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

#ifndef MODEMDVB_DVBT2RECEIVER_H
#define MODEMDVB_DVBT2RECEIVER_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "dvbt2.h"
#include "dvbt2fec.h"
#include "dvbt2ofdm.h"
#include "cwcanceller.h"
#include "ldpcpool.h"
#include "resampler.h"

class SampleThread;

namespace dvbt2 {

//! DVB-T2 receiver: samples at 1/T in, transport stream packets of one PLP out.
//! If threaded, resampling runs in the thread calling process(), and OFDM demodulation (with the frames' assembly
//! and L1 decoding) in a thread of its own. De-interleaving and FEC decoding run in worker threads, each decoding an
//! interleaving frame, its batches of FEC blocks shared with decoder threads common to the workers, so several frames
//! are decoded at once
class MODEMDVB_API Receiver
{
public:
    typedef std::function<void(const uint8_t *packet)> PacketCallback;

    struct Status
    {
        OFDMDemod::Status m_ofdm;
        bool m_l1Valid = false;
        L1Pre m_pre;
        L1Post m_post;
        int m_plpId = -1;               //!< PLP being decoded
        uint64_t m_l1Errors = 0;
        uint64_t m_fecBlocks = 0;
        uint64_t m_ldpcFailures = 0;    //!< Blocks LDPC didn't converge on (BCH may still correct them)
        uint64_t m_bchFailures = 0;
        uint64_t m_bchCorrected = 0;    //!< Bits
        uint64_t m_bchCorrectedBlocks = 0;
        double m_ldpcIterations = 0.0;  //!< Average
        BBFrameDecoder::Stats m_bb;
        double m_mer = 0.0;             //!< dB, of the PLP's cells (unrotated constellations)
        uint64_t m_queueWaits = 0;
        std::vector<double> m_notches;  //!< Frequencies of narrowband interferers notched out, Hz from the centre
        double m_seconds[4] = {0.0, 0.0, 0.0, 0.0}; //!< Demodulation, time de-interleaving, demapping, LDPC and BCH, BBFRAMEs
        // Analysis, of the cells of blocks that decoded: error relative to what was sent, scaled by the noise the
        // LLRs assume, so each dimension should have unit variance. Separately for the real and imaginary parts
        uint64_t m_analysedCells = 0;
        double m_normalisedError[2] = {0.0, 0.0};
        uint64_t m_outliers[3] = {0, 0, 0};     //!< Normalised error power per dimension over 9, 16, 25
        double m_errorByScale[8] = {0.0};       //!< Normalised error power, by LLR scale octave relative to the median
        uint64_t m_cellsByScale[8] = {0};
    };

    //! decoderThreads: for LDPC decoding in all, 0 for all but two of the cores, up to 8
    explicit Receiver(int bandwidthHz = 8000000, FFTFactory *fftFactory = nullptr, bool threaded = true, int decoderThreads = 0);
    //! Notching out strong narrowband interferers, on by default. From the thread that calls process()
    void setNotchEnabled(bool enabled) { m_canceller.setEnabled(enabled); }
    ~Receiver();

    void setPacketCallback(PacketCallback callback) { m_packetCallback = callback; }
    void setLogCallback(OFDMDemod::LogCallback callback);
    //! PLP to decode, or -1 for the first data PLP (also used if the one selected isn't there)
    void selectPLP(int id) { m_selectedPLP = id; }
    //! T2-base or T2-Lite. From the thread that calls process(): when it changes, the signal is acquired again
    void setProfile(Profile profile);
    //! LDPC iterations limit. BCH corrects the few errors LDPC leaves near the threshold, so this needn't be high
    void setMaxIterations(int iterations) { m_maxIterations = iterations; }
    //! Reduce the reliability of cells much further from the constellation than noise would put them. On by default
    void setOutlierLimiting(bool enable) { m_outlierLimiting = enable; }
    void setAnalysis(bool enable) { m_analysis = enable; }
    void reset();
    //! Input sample rate, if not 1/T (64/7 MS/s for 8 MHz): resampled, from about 0.5 to 2 times
    void setInputSampleRate(double sampleRate);
    void process(const Cf *samples, int nbSamples);
    //! Wait for the error correction of everything received to finish
    void flush();
    Status getStatus();
    //! For display: up to maxCells of the PLP's cells, with any constellation rotation undone, and the channel
    //! frequency response magnitude in dB relative to its mean. The response is from the demodulator a moment ago
    //! (none the first time), as it gives it between calls to process(), in that thread
    void getDisplayData(std::vector<Cf>& cells, int maxCells, std::vector<float>& responseDb, int points) const;
    //! As OFDMDemod::getImpulseResponse(), from when getDisplayData() was last satisfied
    void getImpulseResponse(ImpulseResponse& response) const;
    //! Only to be used after flush()
    const OFDMDemod& ofdm() const { return m_ofdm; }

private:
    //! The cells of one interleaving frame of the PLP, for the worker
    struct InterleavingFrame
    {
        uint64_t m_sequence;            //!< To finish in order
        bool m_gap = false;             //!< Not following the last decoded, so a packet across them can't be completed
        uint64_t m_acquisition = 0;     //!< OFDM acquisitions, and frame count, at its first T2-frame
        int m_firstFrame = 0;
        PLP m_plp;
        std::vector<Cf> m_cells;
        std::vector<float> m_scales;
    };

    static constexpr size_t m_maxQueuedFrames = 4;
    static constexpr int m_defaultMaxIterations = 50;   //!< LDPC. 0.2 dB better than 25 near the threshold, 100 only 0.05 dB more
    static constexpr float m_outlierThreshold = 16.0f;  //!< Error power of a cell, relative to the noise per dimension, beyond which it's limited
    static constexpr double m_clockCorrectionThreshold = 1.0;   //!< ppm, to start resampling. 32K symbols are long enough for 3 ppm to matter
    static constexpr uint64_t m_clockCorrectionFrames = 2;
    //! MTI (6.5.2): an Interleaving Frame over several T2-frames has at most this
    static constexpr size_t m_maxTICells = (1 << 19) + (1 << 15);

    static double now();
    void onSymbol(const OFDMDemod::Symbol& symbol);
    void decodeL1();
    void addCells(const Cf *cells, const float *scales, int nbCells);
    void frameComplete();
    struct FEC;
    //! An interleaving frame's turn to finish (its BBFRAMEs, statistics and display, in the order the frames were
    //! received): taken when needed, and passed on to the next however the frame's decoding ends, so an exception (out
    //! of memory) doesn't leave the other worker, reset() and flush() waiting for it forever
    class Turn
    {
    public:
        Turn(Receiver& receiver, uint64_t sequence) : m_receiver(receiver), m_sequence(sequence) {}
        ~Turn();
        void take();

    private:
        Receiver& m_receiver;
        uint64_t m_sequence;
        bool m_taken = false;
    };
    void decodeInterleavingFrame(InterleavingFrame& frame, FEC& fec);
    void decodeFrame(InterleavingFrame& frame, FEC& fec, Turn& turn);
    struct DemapScratch : public LDPCPool::Scratch
    {
        std::vector<Cf> m_g;
        std::vector<float> m_gs, m_re, m_im, m_scales, m_scalesQ, m_errors;   //!< m_scalesQ: of rotated cells' imaginary parts
        std::vector<float> m_merScales;     //!< Each 16th cell's reliability before outlier limiting, for the MER
        std::vector<int8_t *> m_blocks;     //!< A batch's LLRs, for the LDPC decoder
    };
    int decodeLDPC(FEC& fec, LDPC& ldpc, int batch, int nbBlocks, DemapScratch& scratch);
    void demapBlock(FEC& fec, const PLP& plp, int block, float merThreshold, int displayStep, DemapScratch& scratch,
        double& errorPower, int& errorCount);
    void decodeBCH(FEC& fec, int first, int nbBlocks);
    void decodeBlocks(FEC& fec, int nbBlocks);
    void analyseBlock(FEC& fec, int block, const uint8_t *packed);
    void worker(int index);
    void deliverPackets();
    void correctClock();

    OFDMDemod m_ofdm;
    double m_outputRate;
    double m_inputRate;
    bool m_resampling;
    dvbt::Resampler m_resampler;
    std::atomic<double> m_ratioCorrection{1.0};    //!< The resampler's, for getStatus() in another thread
    dvbt::CWCanceller m_canceller;
    std::vector<Cf> m_notched;
    std::vector<double> m_notchFrequencies; //!< With m_ofdmMutex
    struct NotchChange
    {
        int64_t m_sample;                       //!< The first sample notched with them
        std::vector<double> m_frequencies;
    };
    std::deque<NotchChange> m_notchChanges; //!< With m_ofdmMutex
    std::vector<double> m_notchesQueued;    //!< The last in m_notchChanges, or given the demodulator
    std::vector<Cf> m_resampled;
    std::vector<Cf> m_lastInput;    //!< To start resampling without a gap
    uint64_t m_clockFrames;         //!< Frames when the clock was last corrected
    bool m_clockMeasured;           //!< Corrected by a measurement since the reset, so later ones are tracked
    L1Codec m_l1Codec;
    PacketCallback m_packetCallback;
    OFDMDemod::LogCallback m_log;
    std::atomic<int> m_selectedPLP; //!< Read by the demodulator thread
    Profile m_profile = Profile::Auto;
    bool m_threaded;

    // Current frame
    std::vector<std::vector<Cf>> m_p2Cells;
    std::vector<std::vector<float>> m_p2Scales;
    bool m_frameValid;
    L1Pre m_pre;
    L1Post m_post;
    PLP m_plp;
    int m_address;                  //!< Of the next cell of the frame
    std::vector<std::pair<int, int>> m_ranges; //!< Cell address ranges [start, end) of the PLP in this frame
    size_t m_rangeCursor = 0;       //!< The first of them not yet passed
    InterleavingFrame m_current;
    // An Interleaving Frame over PI T2-frames (TIME_IL_TYPE 1) is collected from consecutive ones carrying the PLP
    int m_symbolFrame = 0;          //!< OFDM frame count of the T2-frame being received
    int m_currentParts = 0;         //!< Of m_current's PI, received
    int m_currentFrame = 0;         //!< OFDM frame count of the last
    uint64_t m_lastAcquisition = ~0ULL; //!< Of the first T2-frame of the last Interleaving Frame decoded
    int m_lastFirstFrame = 0;
    int m_lastPLPId = -1;

    // Error correction, one for each worker
    struct FEC
    {
        FEC() : m_type(FECType::Normal), m_rate(CodeRate::R1_2), m_constellation(Constellation::QPSK), m_analysisRotation(false) {}
        FECType m_type;
        CodeRate m_rate;
        Constellation m_constellation;
        LDPCPool::Context m_context;                //!< The worker's own decoders and scratch, for the pool
        LDPC *m_ldpc = nullptr;                     //!< Its decoder for the interleaving frame's code
        std::unique_ptr<BCH> m_bch;
        std::unique_ptr<CellInterleaver> m_cellInterleaver;
        std::vector<int> m_bitMap;
        std::vector<std::vector<int8_t>> m_llrs;    //!< One per FEC block of the interleaving frame
        std::vector<Cf> m_cells;                    //!< Of each FEC block in turn, time de-interleaved
        std::vector<float> m_scales;
        std::vector<int> m_blockR;                  //!< Each block's index in its TI block, for the cell interleaver
        std::vector<std::vector<Cf>> m_blockDisplay;    //!< Cells of each block to display
        std::vector<std::vector<uint8_t>> m_packed; //!< Hard decisions of each block's BCH codeword, corrected
        std::vector<int> m_corrected;               //!< Bits BCH corrected in each block, -1 if it couldn't
        std::vector<int> m_iterations;              //!< Per batch
        std::vector<int> m_unconverged;             //!< Per batch, the blocks LDPC didn't converge on
        // The settings for the interleaving frame being decoded, as they were when it started
        int m_maxIterations = 50;
        bool m_outlierLimiting = true;
        bool m_analysis = false;
        bool m_analysisRotation;
        std::vector<std::vector<Cf>> m_analysisCells;   //!< Of each block, de-interleaved, before demapping
        std::vector<std::vector<float>> m_analysisScales;
        std::vector<Cf> m_displayCells;     //!< Some of the PLP's cells, ready to demap
    };
    std::vector<std::unique_ptr<FEC>> m_fecs;
    int m_workers;
    // Decoder threads, shared by the workers: each worker's batches of FEC blocks, each demapped, LDPC and BCH decoded
    // by one thread, oldest first, a batch at a time as they finish one, so a batch that takes longer (more iterations)
    // doesn't hold up the rest, and while a worker de-interleaves or delivers a frame, the other's batches use them all.
    // The workers decode their own batches too
    std::unique_ptr<LDPCPool> m_pool;
    // Settings, which may be changed from other threads. Each interleaving frame's FEC uses them as they were at its start
    std::atomic<int> m_maxIterations;
    std::atomic<bool> m_analysis;
    std::atomic<bool> m_outlierLimiting;
    BBFrameDecoder m_bbDecoder;     //!< Used by one worker at a time, in frame order
    std::vector<uint8_t> m_packets; //!< Decoded, for the caller's thread
    std::vector<uint8_t> m_bbPackets;   //!< From m_bbDecoder, added to m_packets a frame at a time
    std::vector<uint8_t> m_delivered;   //!< Being delivered, in the caller's thread

    // Workers
    std::vector<std::thread> m_threads;
    std::mutex m_mutex;
    std::condition_variable m_frameAvailable;
    std::condition_variable m_idle;
    std::condition_variable m_turn;
    std::deque<InterleavingFrame> m_queue;
    std::vector<InterleavingFrame> m_freeFrames;    //!< Decoded, so their buffers can be reused without page faults
    int m_busy;                     //!< Workers decoding
    uint64_t m_nextSequence;        //!< Of the next interleaving frame queued
    uint64_t m_finishSequence;      //!< Of the next to finish
    bool m_stop;

    std::mutex m_statusMutex;
    Status m_status;

    mutable std::mutex m_displayMutex;
    std::vector<Cf> m_ofdmCells;            //!< From the demodulator, for display until the PLP's cells are known
    std::vector<float> m_ofdmResponse;
    ImpulseResponse m_ofdmImpulse;
    mutable bool m_displayRequested = false;
    mutable int m_displayMaxCells = 0;
    mutable int m_displayPoints = 0;

    // Demodulation, in its own thread if threaded, with what's needed of it from other threads
    void demodulate(const Cf *samples, int nbSamples);
    OFDMDemod::Status ofdmStatus() const;
    void resetDemodulator();
    std::unique_ptr<SampleThread> m_demodThread;
    mutable std::mutex m_ofdmMutex;
    OFDMDemod::Status m_ofdmStatus;
    // Clock corrections reach the demodulator with the samples they apply to, so its estimate follows them
    struct ClockAdjustment
    {
        int64_t m_sample;           //!< From this sample pushed to the demodulator
        double m_ppm;
    };
    int64_t m_samplesPushed = 0;
    int64_t m_samplesDemodulated = 0;
    std::deque<ClockAdjustment> m_clockAdjustments;    //!< With m_ofdmMutex
    uint64_t m_clockLandedFrames = 0;   //!< OFDM frames when the last of them reached the demodulator. With m_ofdmMutex
    void setRatioCorrection(double ratio);
    std::vector<Cf> m_displayCells;         //!< From the last interleaving frame decoded
};

} // namespace dvbt2

#endif // MODEMDVB_DVBT2RECEIVER_H
