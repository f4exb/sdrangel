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
#include <chrono>

#include "dvbtfec.h"

namespace dvbt {

double FECDecoder::now()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

FECDecoder::FECDecoder() :
    m_configured(false),
    m_demapSupported(true),
    m_supported(true),
    m_mer(0.0),
    m_encoderState(0),
    m_ber(0.0),
    m_berValid(false),
    m_berErrors(0),
    m_berTotal(0),
    m_berCalls(0),
    m_postBER(0.0),
    m_postBERPackets(0),
    m_postBERBits(0),
    m_viterbiBits(0),
    m_seconds{0.0, 0.0, 0.0, 0.0}
{
}

FECDecoder::~FECDecoder()
{
    stopViterbiThreads();
}

void FECDecoder::reset()
{
    resetDemap();
    resetDecode();
}

void FECDecoder::resetDemap()
{
    m_configured = false;
    m_demapSupported = true;
}

void FECDecoder::resetDecode()
{
    m_viterbi.reset();
    m_stream.clear();
    m_streamStart = 0;
    m_streamDecoded = 0;
    m_outer.reset();
    m_supported = true;
    m_hardDecisions.clear();
    m_encoderState = 0;
    m_ber = 0.0;
    m_berValid = false;
    m_berErrors = 0;
    m_berTotal = 0;
    m_postBER = 0.0;
    m_postBERPackets = m_outer.getStats().m_packets;
    m_postBERBits = m_outer.getStats().m_rsBitsCorrected;
}

void FECDecoder::decode(int symbolIndex, const TPS& tps, const Cf *cells, const float *csi, int nbCells)
{
    demap(symbolIndex, tps, cells, csi, nbCells, m_demapped);
    decodeDemapped(m_demapped);
}

void FECDecoder::demap(int symbolIndex, const TPS& tps, const Cf *cells, const float *csi, int nbCells, Demapped& out)
{
    out.m_reconfigured = false;

    // Cells not for the mode signalled (as before the demodulator acquires a new mode): skipped, with no soft bits,
    // and the next symbol that has the right number starts decoding again
    if (nbCells != dataCarriers(tps.m_mode))
    {
        m_configured = false;
        out.m_supported = m_demapSupported;
        out.m_llrs.clear();
        out.m_seconds = 0.0;
        return;
    }

    const bool lowPriority = m_lowPriority;

    if (!m_configured || !tps.sameParameters(m_tps) || (lowPriority != m_configuredLowPriority))
    {
        // In hierarchical modes, the stream chosen, with its code rate
        const bool hierarchical = (tps.m_hierarchy != Hierarchy::None) && (tps.m_constellation != Constellation::QPSK);
        const bool lp = hierarchical && lowPriority;
        m_tps = tps;
        m_configured = true;
        m_configuredLowPriority = lowPriority;
        // DVB-H's in-depth interleaver isn't, in 2K. In 8K only the native one is allowed (F.3.4.2.2), so the flag
        // is ignored
        m_demapSupported = !(tps.m_inDepth && (tps.m_mode == Mode::Mode2K));
        m_inner.configure(tps.m_mode, tps.m_constellation, lp ? tps.m_lpCodeRate : tps.m_hpCodeRate, tps.m_hierarchy, lp);
        out.m_reconfigured = true;
    }

    out.m_supported = m_demapSupported;
    out.m_llrs.clear();
    out.m_seconds = 0.0;

    if (!m_demapSupported) {
        return;
    }

    double t0 = now();
    m_inner.decode(symbolIndex, cells, csi, out.m_llrs);
    out.m_mer = m_inner.getMER();
    out.m_seconds = now() - t0;
}

void FECDecoder::decodeDemapped(const Demapped& in)
{
    if (in.m_reconfigured)
    {
        m_viterbi.reset();
        m_stream.clear();
        m_streamStart = 0;
        m_streamDecoded = 0;
        m_outer.reset();
        m_postBERPackets = m_outer.getStats().m_packets;
        m_postBERBits = m_outer.getStats().m_rsBitsCorrected;
        m_hardDecisions.clear();
        m_encoderState = 0;
    }

    m_supported = in.m_supported;

    if (!m_supported) {
        return;
    }

    m_mer = in.m_mer;
    double t1 = now();
    m_bits.clear();
    int nbInfoBits = (int) in.m_llrs.size() / 2;
    const int threads = m_viterbiThreadsWanted;

    if (threads != m_viterbiThreads)
    {
        // Seamlessly: the stream's last steps are kept either way, so the chunks continue from what the single
        // decoder output, or it starts from 192 steps before what they output (bits it outputs again)
        const bool toSingle = (threads <= 0) && (m_viterbiThreads > 0);
        stopViterbiThreads();
        startViterbiThreads(threads);

        if (toSingle)
        {
            static const int roll = Viterbi27::m_tracebackLength;
            const int64_t from = std::max(m_streamStart, m_streamDecoded - roll);
            const int64_t end = m_streamStart + (int64_t) m_stream.size() / 2;
            std::vector<uint8_t> again;
            m_viterbi.reset();
            m_viterbi.decode(m_stream.data() + 2 * (from - m_streamStart), (int) (end - from), again);
        }
    }

    if (m_viterbiThreads > 0)
    {
        viterbiParallel(in.m_llrs.data(), nbInfoBits, m_bits);
    }
    else
    {
        m_viterbi.decode(in.m_llrs.data(), nbInfoBits, m_bits);

        // Its last steps, in case Viterbi threads are added: those from keep, normally all in this call's soft bits,
        // so only they are copied (not the whole symbol's, 48 kB in 8K)
        static const int roll = Viterbi27::m_tracebackLength;
        const int64_t inputStart = m_streamStart + (int64_t) m_stream.size() / 2;
        const int64_t end = inputStart + nbInfoBits;
        m_streamDecoded += (int64_t) m_bits.size();
        const int64_t keep = std::max(m_streamStart, std::min(m_streamDecoded - roll, end));

        if (keep >= inputStart)
        {
            m_stream.assign(in.m_llrs.begin() + 2 * (keep - inputStart), in.m_llrs.begin() + 2 * nbInfoBits);
        }
        else
        {
            m_stream.erase(m_stream.begin(), m_stream.begin() + 2 * (keep - m_streamStart));
            m_stream.insert(m_stream.end(), in.m_llrs.begin(), in.m_llrs.begin() + 2 * nbInfoBits);
        }

        m_streamStart = keep;
    }

    m_viterbiBits += m_bits.size();

    double t2 = now();
    measureBER(in.m_llrs.data(), nbInfoBits, m_bits.data(), (int) m_bits.size());

    double t3 = now();
    m_outer.decode(m_bits.data(), (int) m_bits.size());

    measurePostViterbiBER();

    double t4 = now();
    m_seconds[0] += in.m_seconds;
    m_seconds[1] += t2 - t1;
    m_seconds[2] += t3 - t2;
    m_seconds[3] += t4 - t3;
}

void FECDecoder::startViterbiThreads(int threads)
{
    m_viterbiThreads = std::max(0, std::min(threads, 7));
    m_workStop = false;
    m_chunks.clear();

    for (int i = 0; i <= m_viterbiThreads; i++) {
        m_chunks.emplace_back(new ViterbiChunk());
    }

    // Each from the current round, so it waits for the next rather than running one of earlier workers'
    uint64_t generation;

    {
        std::lock_guard<std::mutex> lock(m_workMutex);
        generation = m_workGeneration;
    }

    for (int i = 1; i <= m_viterbiThreads; i++) {
        m_workers.emplace_back(&FECDecoder::viterbiWorker, this, i, generation);
    }
}

void FECDecoder::stopViterbiThreads()
{
    {
        std::lock_guard<std::mutex> lock(m_workMutex);
        m_workStop = true;
    }

    m_workAvailable.notify_all();

    for (std::thread& worker : m_workers) {
        worker.join();
    }

    m_workers.clear();
    m_viterbiThreads = 0;
}

void FECDecoder::decodeChunk(Viterbi27& viterbi, const int8_t *llrs, int steps, int skip, int count, std::vector<uint8_t>& out)
{
    out.clear();
    viterbi.reset();
    viterbi.decode(llrs, steps, out);
    out.erase(out.begin(), out.begin() + std::min((int) out.size(), skip));
    out.resize(std::min((int) out.size(), count));
}

void FECDecoder::viterbiWorker(int index, uint64_t generation)
{
    while (true)
    {
        {
            std::unique_lock<std::mutex> lock(m_workMutex);
            m_workAvailable.wait(lock, [&] { return m_workStop || (m_workGeneration != generation); });

            if (m_workStop) {
                return;
            }

            generation = m_workGeneration;
        }

        ViterbiChunk& chunk = *m_chunks[index];

        if (chunk.m_count > 0) {
            decodeChunk(chunk.m_viterbi, chunk.m_llrs, chunk.m_steps, chunk.m_skip, chunk.m_count, chunk.m_out);
        }

        {
            std::lock_guard<std::mutex> lock(m_workMutex);
            m_workPending--;
        }

        m_workDone.notify_one();
    }
}

// The steps that can be decoded (all but the last traceback length, as the single decoder holds back), in a chunk
// for each thread, each decoder starting 192 steps before its chunk (from equal metrics, which converge well within
// that) and tracing back from 192 steps after it
void FECDecoder::viterbiParallel(const int8_t *llrs, int nbInfoBits, std::vector<uint8_t>& out)
{
    static const int roll = Viterbi27::m_tracebackLength;
    m_stream.insert(m_stream.end(), llrs, llrs + 2 * nbInfoBits);
    const int64_t end = m_streamStart + (int64_t) m_stream.size() / 2;
    const int64_t decodable = end - roll - m_streamDecoded;

    if (decodable <= 0) {
        return;
    }

    // Not so small a chunk that the extra steps cost more than they save
    const int nbChunks = (int) std::max<int64_t>(1, std::min<int64_t>(m_viterbiThreads + 1, decodable / 4096));
    const int64_t first = m_streamDecoded;

    for (int c = 0; c <= m_viterbiThreads; c++)
    {
        ViterbiChunk& chunk = *m_chunks[c];
        const int64_t a = first + decodable * c / nbChunks;
        const int64_t b = first + decodable * (c + 1) / nbChunks;

        if (c >= nbChunks)
        {
            chunk.m_count = 0;
            continue;
        }

        const int64_t from = std::max(m_streamStart, a - roll);
        chunk.m_llrs = m_stream.data() + 2 * (from - m_streamStart);
        chunk.m_steps = (int) (b + roll - from);
        chunk.m_skip = (int) (a - from);
        chunk.m_count = (int) (b - a);
    }

    {
        std::lock_guard<std::mutex> lock(m_workMutex);
        m_workPending = m_viterbiThreads;
        m_workGeneration++;
    }

    m_workAvailable.notify_all();
    ViterbiChunk& own = *m_chunks[0];
    decodeChunk(own.m_viterbi, own.m_llrs, own.m_steps, own.m_skip, own.m_count, own.m_out);

    {
        std::unique_lock<std::mutex> lock(m_workMutex);
        m_workDone.wait(lock, [this] { return m_workPending == 0; });
    }

    for (int c = 0; c < nbChunks; c++) {
        out.insert(out.end(), m_chunks[c]->m_out.begin(), m_chunks[c]->m_out.end());
    }

    m_streamDecoded = first + decodable;

    // Keep what the next chunks start from
    const int64_t keep = std::max(m_streamStart, m_streamDecoded - roll);
    m_stream.erase(m_stream.begin(), m_stream.begin() + 2 * (keep - m_streamStart));
    m_streamStart = keep;
}

void FECDecoder::appendHardDecisions(const int8_t *llrs, int nbInfoBits, std::vector<uint8_t>& hardDecisions)
{
    size_t base = hardDecisions.size();
    hardDecisions.resize(base + nbInfoBits);
    uint8_t *hard = &hardDecisions[base];

    for (int i = 0; i < nbInfoBits; i++)
    {
        int8_t x = llrs[2 * i];
        int8_t y = llrs[2 * i + 1];
        hard[i] = (x < 0) | ((y < 0) << 1) | ((x != 0) << 2) | ((y != 0) << 3);
    }
}

// Re-encode the decoded bits and compare with hard decisions on the unpunctured received bits.
// Decoded bits lag the soft input (by the Viterbi decoder's traceback), so the hard decisions for the bits not yet
// decoded are kept for the next call. Re-encoding takes as long as an eighth of the error correction, so only one
// call in m_berInterval is measured, and the others only make hard decisions for the bits they leave undecoded
void FECDecoder::measureBER(const int8_t *llrs, int nbInfoBits, const uint8_t *bits, int nbBits)
{
    const int pending = (int) m_hardDecisions.size();

    if ((m_berCalls++ % m_berInterval) != 0)
    {
        // Not measured: just keep the encoder's state, which is the last 6 bits
        for (int i = std::max(0, nbBits - 6); i < nbBits; i++) {
            m_encoderState = (uint8_t) (((m_encoderState << 1) | (bits[i] & 1)) & 63);
        }

        if (nbBits >= pending)
        {
            int skip = std::min(nbBits - pending, nbInfoBits);
            m_hardDecisions.clear();
            appendHardDecisions(llrs + 2 * skip, nbInfoBits - skip, m_hardDecisions);
        }
        else
        {
            m_hardDecisions.erase(m_hardDecisions.begin(), m_hardDecisions.begin() + nbBits);
            appendHardDecisions(llrs, nbInfoBits, m_hardDecisions);
        }

        return;
    }

    appendHardDecisions(llrs, nbInfoBits, m_hardDecisions);

    static const uint8_t bitCount[4] = {0, 1, 1, 2};
    int errors = 0;
    int total = 0;
    m_xy.clear();
    Viterbi27::encode(bits, nbBits, m_encoderState, m_xy);
    int n = std::min(nbBits, (int) m_hardDecisions.size());
    const uint8_t *hard = m_hardDecisions.data();

    for (int i = 0; i < n; i++)
    {
        int valid = hard[i] >> 2;
        int sent = m_xy[2 * i] | (m_xy[2 * i + 1] << 1);
        errors += bitCount[(hard[i] ^ sent) & valid & 3];
        total += bitCount[valid];
    }

    m_hardDecisions.erase(m_hardDecisions.begin(), m_hardDecisions.begin() + n);

    // BER over windows of m_berWindow bits, the first estimate as soon as there are some
    m_berErrors += errors;
    m_berTotal += total;

    if ((m_berTotal >= m_berWindow) || !m_berValid)
    {
        m_ber = m_berTotal > 0 ? (double) m_berErrors / m_berTotal : 0.0;
        m_berValid = m_berTotal > 0;

        if (m_berTotal >= m_berWindow)
        {
            m_berErrors = 0;
            m_berTotal = 0;
        }
    }
}

void FECDecoder::measurePostViterbiBER()
{
    const OuterDecoder::Stats& stats = m_outer.getStats();

    if (stats.m_packets < m_postBERPackets) // Stats were reset
    {
        m_postBERPackets = stats.m_packets;
        m_postBERBits = stats.m_rsBitsCorrected;
    }

    uint64_t bits = (stats.m_packets - m_postBERPackets) * rsPacketSize * 8;

    if (bits >= m_berWindow)
    {
        m_postBER = (double) (stats.m_rsBitsCorrected - m_postBERBits) / bits;
        m_postBERPackets = stats.m_packets;
        m_postBERBits = stats.m_rsBitsCorrected;
    }
}

FECDecoder::Status FECDecoder::getStatus() const
{
    Status status;
    status.m_outer = m_outer.getStats();
    status.m_tsSync = m_outer.synced();
    status.m_supported = m_supported;
    status.m_lowPriority = m_lowPriority;
    status.m_mer = m_mer;
    status.m_preViterbiBER = m_ber;
    status.m_postViterbiBER = m_postBER;
    status.m_viterbiBits = m_viterbiBits;
    std::copy(m_seconds, m_seconds + 4, status.m_seconds);
    return status;
}

} // namespace dvbt
