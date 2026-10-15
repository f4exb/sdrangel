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

#include <cmath>

#include <QString>

#include "dsp/fftengine.h"

#include "dvbt2modulator.h"

namespace dvbt2 {

Modulator::Modulator(const Config& config) :
    m_config(config),
    m_frameIdx(0)
{
    m_fft = FFTEngine::create(QString());
    setup();
}

Modulator::~Modulator()
{
    delete m_fft;
}

// S2 field 1 (table 19(a))
int Modulator::s2Field1(int fftSize, GuardInterval guard, bool lite)
{
    bool newGuard = (guard == GuardInterval::G1_128) || (guard == GuardInterval::G19_128) || (guard == GuardInterval::G19_256);

    switch (fftSize)
    {
    case 1024: return 3;
    case 2048: return 0;
    case 4096: return 2;
    case 8192: return newGuard ? 6 : 1;
    case 16384: return lite && newGuard ? 3 : 4;
    default: return newGuard ? 7 : 5;
    }
}

void Modulator::setup()
{
    const Config& c = m_config;
    const int n = c.m_fftSize;
    const int ng = guardSamples(n, c.m_guard);
    const int np2 = numP2(n);
    m_tables = &Tables::get(n, c.m_extended, c.m_pattern, c.m_toneReservation, c.m_miso);
    m_frameClosing = frameClosingSymbol(n, c.m_guard, c.m_pattern, c.m_miso);

    // Frame length: at most 250 ms. 32K frames have an even number of symbols
    int lf = c.m_dataSymbols > 0 ? c.m_dataSymbols + np2 : (int) ((0.25 * sampleRate(c.m_bandwidth) - 2048) / (n + ng));

    if ((n == 32768) && (lf % 2)) {
        lf--;
    }

    m_lf = lf;
    const int ldata = lf - np2;

    m_pre = L1Pre();
    m_pre.m_type = 0;
    m_pre.m_bwtExt = m_tables->m_extended;
    m_pre.m_s1 = c.m_lite ? (int) (c.m_miso ? PreambleType::T2LiteMISO : PreambleType::T2LiteSISO)
        : (int) (c.m_miso ? PreambleType::T2MISO : PreambleType::T2SISO);
    m_pre.m_s2 = (s2Field1(n, c.m_guard, c.m_lite) << 1) | (c.m_fefLength > 0 ? 1 : 0);
    m_pre.m_guard = c.m_guard;
    m_pre.m_papr = c.m_toneReservation ? 2 : 0;
    m_pre.m_l1Mod = c.m_l1Mod;
    m_pre.m_pilotPattern = c.m_pattern;
    m_pre.m_cellId = c.m_cellId;
    m_pre.m_networkId = 0x3001;
    m_pre.m_t2SystemId = 0x8001;
    m_pre.m_numT2Frames = c.m_frames;
    m_pre.m_numDataSymbols = ldata;
    m_pre.m_numRf = 1;
    m_pre.m_t2Version = 2;

    m_ncells = cellsPerFECBlock(c.m_fecType, c.m_constellation);
    PLP plp;
    plp.m_id = 0;
    plp.m_type = 1;
    plp.m_payloadType = 3;
    plp.m_codeRate = c.m_codeRate;
    plp.m_constellation = c.m_constellation;
    plp.m_rotation = c.m_rotation;
    plp.m_fecType = c.m_fecType;
    plp.m_frameInterval = 1;
    plp.m_plpMode = c.m_highEfficiency ? 2 : 1;
    m_post = L1Post();
    m_post.m_frequencies.push_back(0);
    m_post.m_plps.push_back(plp);

    if (c.m_fefLength > 0)
    {
        m_post.m_fefType = 0;
        m_post.m_fefLength = c.m_fefLength & 0x3fffff;
        m_post.m_fefLengthMsb = c.m_fefLength >> 22;
        m_post.m_fefInterval = std::max(1, c.m_fefInterval);
    }

    // Capacity: L1 signalling first, then the PLP, then dummy cells
    std::vector<uint8_t> postBits;
    m_post.encode(postBits, m_pre);
    m_pre.m_l1PostSize = L1Codec::postCells(m_pre.m_l1PostInfoSize, c.m_l1Mod, n);
    const int dl1 = l1PreCells + m_pre.m_l1PostSize;
    const int cdata = (int) m_tables->m_data[0].size();
    const int ctot = np2 * (int) m_tables->m_p2Data.size() + (ldata - 1) * cdata + (m_frameClosing ? m_tables->m_cfc : cdata);
    m_blocksPerFrame = (ctot - dl1) / m_ncells;
    const int pi = std::max(1, c.m_interleavingFrames);
    int nti = c.m_tiBlocks;

    if (pi > 1)
    {
        // PI of an Interleaving Frame's cells must be an integer (8.3.6.3.1)
        m_blocksPerFrame = std::min(pi * m_blocksPerFrame, m_maxTICells / m_ncells);
        m_post.m_plps[0].m_timeIlType = true;
        m_post.m_plps[0].m_timeIlLength = pi;
    }
    else
    {
        if (nti > 0)
        {
            while (((m_blocksPerFrame + nti - 1) / nti) * m_ncells > m_maxTICells) {
                nti++;
            }
        }

        m_post.m_plps[0].m_timeIlLength = nti;
    }

    m_post.m_plps[0].m_frameInterval = std::max(1, c.m_frameInterval);
    m_post.m_plps[0].m_numBlocksMax = m_blocksPerFrame;
    m_post.m_plps[0].m_numBlocks = m_blocksPerFrame;
    m_post.m_plps[0].m_start = 0;
    m_post.m_type2Start = m_blocksPerFrame * m_ncells / pi;

    m_ldpc.reset(new LDPC(c.m_fecType, c.m_codeRate));
    m_bch.reset(new BCH(c.m_fecType, bchT(c.m_fecType, c.m_codeRate)));
    m_bbEncoder.reset(new BBFrameEncoder(c.m_fecType, c.m_codeRate, c.m_highEfficiency));
    m_cellInterleaver.reset(new CellInterleaver(m_ncells));
    cellBitMap(c.m_fecType, c.m_codeRate, c.m_constellation, m_bitMap);
    m_packetsPerFrame = (int) std::ceil(m_blocksPerFrame * (double) (kbch(c.m_fecType, c.m_codeRate) - 80) / (188 * 8)
        / (pi * std::max(1, c.m_frameInterval)));

    makeP1(m_pre.m_s1, m_pre.m_s2, m_p1);

    if (c.m_fefLength > 0) {
        makeP1(c.m_fefS1, 0, m_fefP1);
    }

    m_fft->configure(n, true);
}

// P1 (9.8): the A part from the active carriers, then C and B, frequency shifted copies of its start and end
void Modulator::makeP1(int s1, int s2, std::vector<Cf>& p1Samples)
{
    float values[384];
    const P1Tables& p1 = P1Tables::get();
    p1.modulation(s1, s2, values);
    m_fft->configure(1024, true);
    Cf *in = m_fft->in();
    std::fill(in, in + 1024, Cf(0.0f, 0.0f));

    for (int i = 0; i < 384; i++) {
        in[(p1.m_carriers[i] - 426 + 1024) % 1024] = Cf(values[i] / std::sqrt(384.0f), 0.0f);
    }

    m_fft->transform();
    const Cf *a = m_fft->out();
    p1Samples.resize(2048);

    for (int i = 0; i < 2048; i++)
    {
        Cf shift = std::polar(1.0f, (float) (2.0 * M_PI * i / 1024.0));

        if (i < 542) {
            p1Samples[i] = a[i] * shift;
        } else if (i < 1566) {
            p1Samples[i] = a[i - 542];
        } else {
            p1Samples[i] = a[i - 1024] * shift;
        }
    }
}

// A FEF part (8.4): a P1, then anything, here noise at the T2 signal's power
void Modulator::addFEF(std::vector<Cf>& samples)
{
    const size_t start = samples.size();
    samples.insert(samples.end(), m_fefP1.begin(), m_fefP1.end());
    std::normal_distribution<float> noise(0.0f, (float) std::sqrt(0.5));

    for (int i = (int) m_fefP1.size(); i < m_config.m_fefLength; i++) {
        samples.push_back(Cf(noise(m_fefRng), noise(m_fefRng)));
    }

    if (m_tx2) {
        m_tx2->insert(m_tx2->end(), samples.begin() + start, samples.end());
    }
}

int Modulator::samplesPerFrame() const
{
    return 2048 + m_lf * (m_config.m_fftSize + guardSamples(m_config.m_fftSize, m_config.m_guard));
}

// BCH, LDPC, bit interleaving and mapping (6.1, 6.2), then rotation (6.3)
void Modulator::encodeFECBlock(const std::vector<uint8_t>& bbframe, std::vector<Cf>& cells)
{
    const int kbchBytes = (int) bbframe.size();
    const int k = m_ldpc->k();
    const int n = m_ldpc->n();
    std::vector<uint8_t> packed(k / 8);
    std::copy(bbframe.begin(), bbframe.end(), packed.begin());
    m_bch->encode(packed.data(), kbchBytes, packed.data() + kbchBytes);
    std::vector<uint8_t> codeword(n);

    for (int i = 0; i < k; i++) {
        codeword[i] = (packed[i / 8] >> (7 - (i % 8))) & 1;
    }

    m_ldpc->encode(codeword.data(), codeword.data() + k);

    const int eta = bitsPerCell(m_config.m_constellation);
    std::vector<Cf> f(m_ncells);
    uint8_t bits[8];

    for (int q = 0; q < m_ncells; q++)
    {
        for (int e = 0; e < eta; e++) {
            bits[e] = codeword[m_bitMap[q * eta + e]];
        }

        f[q] = mapCell(bits, eta);
    }

    cells.resize(m_ncells);

    if (m_config.m_rotation)
    {
        Cf r = std::polar(1.0f, rotationAngle(m_config.m_constellation));

        for (int q = 0; q < m_ncells; q++)
        {
            Cf a = r * f[q];
            Cf b = r * f[(q + m_ncells - 1) % m_ncells];
            cells[q] = Cf(a.real(), b.imag());
        }
    }
    else
    {
        cells = f;
    }
}

void Modulator::modulate(const uint8_t *packets, int nbPackets, std::vector<Cf>& samples, std::vector<Cf> *samples2)
{
    m_tx2 = m_config.m_miso ? samples2 : nullptr;

    std::vector<uint8_t> bbframe;

    for (int p = 0; p < nbPackets; p++)
    {
        if (m_bbEncoder->addPacket(packets + 188 * p, bbframe)) {
            m_bbframes.push_back(bbframe);
        }

        while ((int) m_bbframes.size() >= blocksNeeded()) {
            buildFrame(samples);
        }
    }
}

// Blocks the next frame takes: an Interleaving Frame's, if it's the first it's in
int Modulator::blocksNeeded() const
{
    const int ijump = std::max(1, m_config.m_frameInterval);
    const int pi = std::max(1, m_config.m_interleavingFrames);
    return ((m_frameIdx % ijump) == 0) && (((m_frameIdx / ijump) % pi) == 0) ? m_blocksPerFrame : 0;
}

void Modulator::buildFrame(std::vector<Cf>& samples)
{
    const Config& c = m_config;
    const int ijump = std::max(1, c.m_frameInterval);
    const int pi = std::max(1, c.m_interleavingFrames);
    const bool present = (m_frameIdx % ijump) == 0;
    const int part = (m_frameIdx / ijump) % pi;

    // The PLP's cells in this frame: part of the Interleaving Frame (8.3.6.3.1)
    std::vector<Cf> plpCells;

    if (present)
    {
        if (part == 0) {
            encodeInterleavingFrame();
        }

        const size_t d = m_ifCells.size() / pi;
        plpCells.assign(m_ifCells.begin() + part * d, m_ifCells.begin() + (part + 1) * d);
    }

    m_post.m_plps[0].m_start = 0;
    m_post.m_type2Start = (int) plpCells.size();
    buildFrameCells(plpCells, samples);
}

// The PLP's cells: cell and time interleaving of the FEC blocks (6.4, 6.5)
void Modulator::encodeInterleavingFrame()
{
    const PLP& plp = m_post.m_plps[0];
    std::vector<Cf>& plpCells = m_ifCells;
    plpCells.clear();
    plpCells.reserve((size_t) m_blocksPerFrame * m_ncells);
    std::vector<std::vector<Cf>> blocks(m_blocksPerFrame);

    for (int b = 0; b < m_blocksPerFrame; b++) {
        encodeFECBlock(m_bbframes[b], blocks[b]);
    }

    m_bbframes.erase(m_bbframes.begin(), m_bbframes.begin() + m_blocksPerFrame);

    const int nti = plp.m_timeIlType ? 1 : plp.m_timeIlLength;

    if (nti == 0)
    {
        for (int b = 0; b < m_blocksPerFrame; b++)
        {
            std::vector<Cf> d(m_ncells);

            for (int q = 0; q < m_ncells; q++) {
                d[m_cellInterleaver->address(0, q)] = blocks[b][q];
            }

            plpCells.insert(plpCells.end(), d.begin(), d.end());
        }
    }
    else
    {
        int b = 0;

        for (int s = 0; s < nti; s++)
        {
            int nfec = m_blocksPerFrame / nti + (s >= nti - (m_blocksPerFrame % nti) ? 1 : 0);
            std::vector<Cf> d((size_t) nfec * m_ncells);

            for (int r = 0; r < nfec; r++, b++)
            {
                for (int q = 0; q < m_ncells; q++) {
                    d[(size_t) r * m_ncells + m_cellInterleaver->address(r, q)] = blocks[b][q];
                }
            }

            std::vector<int> read;
            timeInterleaver(m_ncells, nfec, read);

            for (size_t i = 0; i < read.size(); i++) {
                plpCells.push_back(d[read[i]]);
            }
        }
    }
}

// L1, the PLP's cells and dummy cells into the frame's symbols
void Modulator::buildFrameCells(std::vector<Cf>& plpCells, std::vector<Cf>& samples)
{
    const Config& c = m_config;
    const int np2 = numP2(c.m_fftSize);

    // L1 signalling for this frame
    m_post.m_frameIdx = m_frameIdx;
    std::vector<uint8_t> postBits;
    m_post.encode(postBits, m_pre);
    std::vector<Cf> postCells, preCells;
    m_l1Codec.encodePost(postBits, m_pre, postCells);
    uint8_t preBits[L1Pre::size];
    m_pre.encode(preBits);
    m_l1Codec.encodePre(preBits, preCells);

    // Dummy cells (8.3.8): the BB scrambling sequence, BPSK
    const int cp2 = (int) m_tables->m_p2Data.size();
    const int cdata = (int) m_tables->m_data[0].size();
    const int dl1 = (int) (preCells.size() + postCells.size());
    const int ctot = np2 * cp2 + (m_lf - np2 - 1) * cdata + (m_frameClosing ? m_tables->m_cfc : cdata);
    const int ndummy = ctot - dl1 - (int) plpCells.size();
    std::vector<uint8_t> dummyBits(ndummy, 0);
    scramble(dummyBits.data(), ndummy);

    for (int i = 0; i < ndummy; i++) {
        plpCells.push_back(Cf(dummyBits[i] ? -1.0f : 1.0f, 0.0f));
    }

    // Map to the symbols (8.3.5, 8.3.6.2): L1 zig-zag across the P2 symbols, then the cells in address order
    addP1(samples);
    size_t address = 0;
    const int preRows = (int) preCells.size() / np2;
    const int postRows = (int) postCells.size() / np2;

    for (int l = 0; l < m_lf; l++)
    {
        std::vector<Cf> cells;

        if (l < np2)
        {
            cells.resize(cp2);

            for (int p = 0; p < preRows; p++) {
                cells[p] = preCells[np2 * p + l];
            }

            for (int p = 0; p < postRows; p++) {
                cells[preRows + p] = postCells[np2 * p + l];
            }

            for (int p = preRows + postRows; p < cp2; p++) {
                cells[p] = plpCells[address++];
            }
        }
        else if (m_frameClosing && (l == m_lf - 1))
        {
            cells.assign(m_tables->m_fcData.size(), Cf(0.0f, 0.0f));

            for (int p = 0; p < m_tables->m_cfc; p++) {
                cells[p] = plpCells[address++];
            }
        }
        else
        {
            cells.resize(cdata);

            for (int p = 0; p < cdata; p++) {
                cells[p] = plpCells[address++];
            }
        }

        addSymbol(l, cells, samples);
    }

    if ((c.m_fefLength > 0) && ((m_frameIdx + 1) % std::max(1, c.m_fefInterval) == 0)) {
        addFEF(samples);
    }

    m_frameIdx = (m_frameIdx + 1) % c.m_frames;
}

void Modulator::addP1(std::vector<Cf>& samples)
{
    samples.insert(samples.end(), m_p1.begin(), m_p1.end());

    if (m_tx2) {
        m_tx2->insert(m_tx2->end(), m_p1.begin(), m_p1.end());
    }
}

// The second group of transmitters' pilot sign (9.2.8)
float Modulator::misoSign(int k, int l) const
{
    const Tables& t = *m_tables;

    if (l < numP2(m_config.m_fftSize)) {
        return (k % 6 == 3) ? -1.0f : 1.0f;
    }

    if ((k == 0) || (k == t.m_kMax)) {
        return (l & 1) ? -1.0f : 1.0f;
    }

    if (k % t.m_dx == 0) {
        return ((k / t.m_dx) & 1) ? -1.0f : 1.0f;
    }

    return 1.0f;
}

// Frequency interleaving, pilots, IFFT and guard interval (8.5, 9.2, 9.5, 9.7)
void Modulator::addSymbol(int l, const std::vector<Cf>& cells, std::vector<Cf>& samples)
{
    const int n = m_config.m_fftSize;
    const int np2 = numP2(n);
    const Tables& t = *m_tables;
    const bool p2 = l < np2;
    const bool fc = !p2 && m_frameClosing && (l == m_lf - 1);
    const std::vector<int>& data = p2 ? t.m_p2Data : fc ? t.m_fcData : t.m_data[l % t.m_dy];
    const std::vector<Tables::Pilot>& pilots = p2 ? t.m_p2Pilots : fc ? t.m_fcPilots : t.m_pilots[l % t.m_dy];
    const std::vector<int>& read = p2 ? t.p2Deinterleave(l) : fc ? t.fcDeinterleave(l) : t.dataDeinterleave(l);
    const int pn = framePN(l);
    const float scale = 5.0f / std::sqrt(27.0f * (t.m_kMax + 1));
    const int ng = guardSamples(n, m_config.m_guard);
    auto bin = [n, &t](int k) { return (k - t.m_kMax / 2 + n) % n; };

    // The cells in carrier order, the frequency interleaver's output
    m_carrierCells.assign(data.size(), Cf(0.0f, 0.0f));

    for (size_t p = 0; p < cells.size(); p++) {
        m_carrierCells[read[p]] = cells[p];
    }

    // The first group of transmitters, or the only one, then in MISO the second: each pair of cells a0, a1 as
    // -a1*, a0* (9.1), and some pilots inverted
    for (int group = 0; group < (m_tx2 ? 2 : 1); group++)
    {
        Cf *in = m_fft->in();
        std::fill(in, in + n, Cf(0.0f, 0.0f));

        for (size_t i = 0; i < data.size(); i++)
        {
            if (group == 0) {
                in[bin(data[i])] = m_carrierCells[i];
            } else if (i & 1) {
                in[bin(data[i])] = std::conj(m_carrierCells[i - 1]);
            } else if (i + 1 < data.size()) {
                in[bin(data[i])] = -std::conj(m_carrierCells[i + 1]);
            }
        }

        for (const Tables::Pilot& pilot : pilots)
        {
            int r = t.m_w[pilot.m_k] ^ pn;
            const float sign = group == 1 ? misoSign(pilot.m_k, l) : 1.0f;
            in[bin(pilot.m_k)] = Cf(sign * (r ? -pilot.m_amplitude : pilot.m_amplitude), 0.0f);
        }

        m_fft->transform();
        const Cf *out = m_fft->out();
        std::vector<Cf>& to = group == 0 ? samples : *m_tx2;

        for (int i = n - ng; i < n; i++) {
            to.push_back(out[i] * scale);
        }

        for (int i = 0; i < n; i++) {
            to.push_back(out[i] * scale);
        }
    }
}

} // namespace dvbt2
