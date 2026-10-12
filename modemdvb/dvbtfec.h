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

#ifndef MODEMDVB_DVBTFEC_H
#define MODEMDVB_DVBTFEC_H

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "dvbt.h"
#include "dvbtinner.h"
#include "dvbtouter.h"
#include "viterbi27.h"

namespace dvbt {

// Everything after OFDM demodulation: de-mapping and inner de-interleaving, Viterbi decoding and outer decoding,
// from the equalised data cells of each symbol to transport stream packets. Also measures the BER. In two stages,
// which can run in two threads: demap() (inner de-interleaving and de-mapping, to soft bits), then decodeDemapped()
// (Viterbi, BER and outer decoding), each always called from the same thread, with demap()'s output in order
class MODEMDVB_API FECDecoder
{
public:
    typedef std::function<void(const uint8_t *packet)> PacketCallback; //!< 188-byte TS packet

    struct Status
    {
        OuterDecoder::Stats m_outer;
        bool m_tsSync;
        bool m_supported;           //!< False for a mode that can't be decoded (DVB-H's 2K in-depth interleaver)
        bool m_lowPriority = false; //!< Decoding a hierarchical mode's low priority stream
        double m_mer;               //!< dB
        double m_preViterbiBER;     //!< Recent average, from re-encoding the decoded bits
        double m_postViterbiBER;    //!< Recent average, from the bits Reed-Solomon corrects. QEF needs < 2e-4
        uint64_t m_viterbiBits;
        double m_seconds[4];        //!< Processing time: inner, Viterbi, BER, outer

        Status() :
            m_tsSync(false),
            m_supported(true),
            m_mer(0.0),
            m_preViterbiBER(0.0),
            m_postViterbiBER(0.0),
            m_viterbiBits(0),
            m_seconds{0.0, 0.0, 0.0, 0.0}
        {
        }
    };

    //! A symbol's soft bits, from demap()
    struct Demapped
    {
        bool m_reconfigured = false;    //!< The parameters changed, so decoding starts again
        bool m_supported = true;
        double m_mer = 0.0;
        double m_seconds = 0.0;         //!< To demap
        std::vector<int8_t> m_llrs;     //!< X, Y per information bit, positive for 0, 0 for erased
    };

    FECDecoder();
    ~FECDecoder();

    void setPacketCallback(PacketCallback callback) { m_outer.setPacketCallback(callback); }
    void reset();                       //!< Both stages
    void resetDemap();
    void resetDecode();
    //! Both stages. A symbol without dataCarriers(tps.m_mode) cells is skipped, and decoding starts again after it
    void decode(int symbolIndex, const TPS& tps, const Cf *cells, const float *csi, int nbCells);
    void demap(int symbolIndex, const TPS& tps, const Cf *cells, const float *csi, int nbCells, Demapped& out);
    void decodeDemapped(const Demapped& in);
    //! In hierarchical modes, decode the low priority stream rather than the high priority one. From any thread
    void setLowPriority(bool lowPriority) { m_lowPriority = lowPriority; }
    //! Threads, besides the one calling decodeDemapped(), to share Viterbi decoding, for slow CPUs. From any thread
    //! Clamped to what startViterbiThreads() starts, so a request beyond doesn't restart them every symbol
    void setViterbiThreads(int threads) { m_viterbiThreadsWanted = std::max(0, std::min(threads, 7)); }
    //! From the thread that calls decodeDemapped()
    Status getStatus() const;

private:
    static double now();
    static void decodeChunk(Viterbi27& viterbi, const int8_t *llrs, int steps, int skip, int count, std::vector<uint8_t>& out);
    static void appendHardDecisions(const int8_t *llrs, int nbInfoBits, std::vector<uint8_t>& hardDecisions);

    void measureBER(const int8_t *llrs, int nbInfoBits, const uint8_t *bits, int nbBits);
    void measurePostViterbiBER();
    void viterbiParallel(const int8_t *llrs, int nbInfoBits, std::vector<uint8_t>& out);
    void startViterbiThreads(int threads);
    void stopViterbiThreads();
    void viterbiWorker(int index, uint64_t generation);

    // Viterbi decoding shared between threads: the stream's undecoded steps (and those before them that start each
    // chunk's decoder) are split into chunks, each decoded on its own from 192 steps before it to 192 after, so the
    // result is as the single decoder's, with the same lag
    struct ViterbiChunk
    {
        Viterbi27 m_viterbi;
        const int8_t *m_llrs = nullptr;
        int m_steps = 0;            //!< To decode, from m_llrs
        int m_skip = 0;             //!< Of the decoded bits, the first not wanted
        int m_count = 0;            //!< Wanted
        std::vector<uint8_t> m_out;
    };
    std::atomic<int> m_viterbiThreadsWanted{0};
    int m_viterbiThreads = 0;
    std::vector<std::thread> m_workers;
    std::vector<std::unique_ptr<ViterbiChunk>> m_chunks;    //!< 0 for this thread, then one per worker
    std::mutex m_workMutex;
    std::condition_variable m_workAvailable;
    std::condition_variable m_workDone;
    uint64_t m_workGeneration = 0;
    int m_workPending = 0;
    bool m_workStop = false;
    std::vector<int8_t> m_stream;   //!< Soft bits from step m_streamStart
    int64_t m_streamStart = 0;
    int64_t m_streamDecoded = 0;    //!< Steps output

    // Demapping
    InnerDecoder m_inner;
    bool m_configured;
    bool m_demapSupported;
    std::atomic<bool> m_lowPriority{false};
    bool m_configuredLowPriority = false;
    TPS m_tps;
    Demapped m_demapped;        //!< For decode()

    // Decoding
    Viterbi27 m_viterbi;
    OuterDecoder m_outer;
    bool m_supported;
    double m_mer;
    std::vector<uint8_t> m_bits;

    // Pre-Viterbi BER
    std::vector<uint8_t> m_hardDecisions;   //!< Per information bit not yet decoded: X, Y, X valid, Y valid in bits 0..3
    std::vector<uint8_t> m_xy;
    uint8_t m_encoderState;
    double m_ber;
    bool m_berValid;
    uint64_t m_berErrors;
    uint64_t m_berTotal;
    unsigned int m_berCalls;
    static constexpr uint64_t m_berWindow = 2000000;    //!< Bits compared
    static const unsigned int m_berInterval = 8;    //!< Calls, of which one is measured, to save time

    // Post-Viterbi BER
    double m_postBER;
    uint64_t m_postBERPackets;      //!< Outer decoder counts at the start of the window
    uint64_t m_postBERBits;

    uint64_t m_viterbiBits;
    double m_seconds[4];
};

} // namespace dvbt

#endif // MODEMDVB_DVBTFEC_H
