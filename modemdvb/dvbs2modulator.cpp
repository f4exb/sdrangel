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
#include <cmath>

#include "dvbs2modulator.h"

namespace dvbs2 {

const Cf Modulator::m_turns[4] = {Cf(1.0f, 0.0f), Cf(0.0f, 1.0f), Cf(-1.0f, 0.0f), Cf(0.0f, -1.0f)};

Modulator::Modulator(const Config& config) :
    m_config(config),
    m_scrambler(config.m_scramblingCode),
    m_frameCount(0)
{
    for (int s = 0; s < 2; s++) {
        m_streams[s].m_isi = config.m_isi[s];
    }
}

Modulator::~Modulator()
{
}

Modulator::Code& Modulator::code(int modcod)
{
    auto it = m_codes.find(modcod);

    if (it != m_codes.end()) {
        return *it->second;
    }

    std::unique_ptr<Code> c(new Code());
    FECType type;

    if (isVLSNR(modcod))
    {
        const VLSNRModcod& m = vlsnrModcod(vlsnrIndex(modcod));
        c->m_modulation = m.m_modulation;
        c->m_rate = m.m_rate;
        type = m.m_type;
        c->m_kbch = vlsnrKbch(vlsnrIndex(modcod));
    }
    else
    {
        modcodInfo(modcod, c->m_modulation, c->m_rate);
        type = modcodFECType(modcod, m_config.m_fecType);
        c->m_kbch = dvbt2::kbch(type, c->m_rate, CodeFamily::S2);
    }

    c->m_type = type;
    c->m_kldpc = dvbt2::kldpc(type, c->m_rate, CodeFamily::S2);
    c->m_bch.reset(new dvbt2::BCH(type, dvbt2::bchT(type, c->m_rate, CodeFamily::S2)));
    c->m_ldpc.reset(new dvbt2::LDPC(type, c->m_rate, CodeFamily::S2));
    bitMap(type, c->m_modulation, c->m_rate, c->m_map);
    constellation(c->m_modulation, c->m_rate, c->m_points, type);

    if (m_config.m_unitOuterRing && (c->m_modulation != Modulation::QPSK) && (c->m_modulation != Modulation::PSK8))
    {
        float outer = 0.0f;

        for (const Cf& p : c->m_points) {
            outer = std::max(outer, std::abs(p));
        }

        for (Cf& p : c->m_points) {
            p /= outer;
        }
    }

    Code& result = *c;
    m_codes[modcod] = std::move(c);
    return result;
}

double Modulator::packetsPerFrame() const
{
    return dataBytes(m_config.m_vcm.empty() ? m_config.m_modcod : m_config.m_vcm[0]) / 188.0;
}

int Modulator::dataBytes(int modcod) const
{
    if (isVLSNR(modcod)) {
        return (vlsnrKbch(vlsnrIndex(modcod)) - 80) / 8;
    }

    Modulation modulation;
    CodeRate rate;
    modcodInfo(modcod, modulation, rate);
    return (dvbt2::kbch(modcodFECType(modcod, m_config.m_fecType), rate, CodeFamily::S2) - 80) / 8;
}

int Modulator::nextModcod() const
{
    return m_config.m_vcm.empty() ? m_config.m_modcod : m_config.m_vcm[m_frameCount % m_config.m_vcm.size()];
}

void Modulator::addPacket(Stream& stream, const uint8_t *packet)
{
    stream.m_data.push_back(stream.m_crc);
    stream.m_data.insert(stream.m_data.end(), packet + 1, packet + 188);
    stream.m_crc = dvbt2::crc8(packet + 1, 187);
}

void Modulator::modulate(const uint8_t *packets, int nbPackets, std::vector<Cf>& symbols)
{
    uint8_t null[188] = {0x47, 0x1f, 0xff, 0x10};

    for (int p = 0; p < nbPackets; p++)
    {
        addPacket(m_streams[0], packets + 188 * p);

        if (m_config.m_streams > 1) {
            addPacket(m_streams[1], null);
        }

        while (true)
        {
            Stream& stream = m_streams[m_config.m_streams > 1 ? m_frameCount % 2 : 0];
            const int modcod = nextModcod();

            if (!frameReady(stream, modcod)) {
                break;
            }

            if (isVLSNR(modcod)) {
                vlsnrFrame(stream, vlsnrIndex(modcod), symbols);
            } else {
                frame(stream, modcod, symbols);
            }
        }
    }
}

void Modulator::bbFrame(Stream& stream, int kbch, std::vector<uint8_t>& bb)
{
    const int dataBytes = (kbch - 80) / 8;
    bb.assign((kbch + 7) / 8, 0);
    // Time slices: each its own single stream, told apart by its TSN alone
    const bool timeSlicing = !m_config.m_tsns.empty();
    const bool single = (m_config.m_streams <= 1) || timeSlicing;
    // TS/GS: transport stream, or generic continuous (no user packets: UPL, SYNC and SYNCD 0)
    const bool generic = m_config.m_generic;
    const int ro = m_config.m_rollOff < 4 ? (m_config.m_rollOff & 3) : (m_frameCount & 1) ? (m_config.m_rollOff & 3) : 3;
    bb[0] = (uint8_t) ((generic ? 0x40 : 0xc0) | (single ? 0x20 : 0) | (m_config.m_vcm.empty() ? 0x10 : 0) | ro);
    bb[1] = (uint8_t) (single ? 0 : stream.m_isi);
    bb[2] = (uint8_t) (generic ? 0 : (188 * 8) >> 8);
    bb[3] = (uint8_t) (generic ? 0 : 188 * 8);
    bb[4] = (uint8_t) ((dataBytes * 8) >> 8);
    bb[5] = (uint8_t) (dataBytes * 8);
    bb[6] = generic ? 0 : 0x47;
    const int syncd = generic ? 0 : stream.m_syncd < dataBytes ? stream.m_syncd * 8 : 65535;
    bb[7] = (uint8_t) (syncd >> 8);
    bb[8] = (uint8_t) syncd;
    bb[9] = dvbt2::crc8(bb.data(), 9);
    std::copy(stream.m_data.begin(), stream.m_data.begin() + dataBytes, bb.begin() + 10);
    stream.m_data.erase(stream.m_data.begin(), stream.m_data.begin() + dataBytes);
    const int consumed = dataBytes - stream.m_syncd;
    stream.m_syncd = (188 - (consumed % 188)) % 188;
    dvbt2::scrambleBytes(bb.data(), (int) bb.size());
}

void Modulator::frame(Stream& stream, int modcod, std::vector<Cf>& symbols)
{
    Code& c = code(modcod);
    const bool timeSlicing = !m_config.m_tsns.empty();
    const int streamIndex = &stream == &m_streams[0] ? 0 : 1;
    std::vector<uint8_t> bb;
    bbFrame(stream, c.m_kbch, bb);

    // BCH, then LDPC, of the bits MSB first
    std::vector<uint8_t> parity((c.m_bch->parityBits() + 7) / 8);
    c.m_bch->encode(bb.data(), (int) bb.size(), parity.data());
    const int n = c.m_ldpc->n();
    m_bits.assign(n, 0);

    for (int i = 0; i < c.m_kbch; i++) {
        m_bits[i] = (bb[i / 8] >> (7 - i % 8)) & 1;
    }

    for (int i = 0; i < c.m_bch->parityBits(); i++) {
        m_bits[c.m_kbch + i] = (parity[i / 8] >> (7 - i % 8)) & 1;
    }

    c.m_ldpc->encode(m_bits.data(), m_bits.data() + c.m_kldpc);

    // Interleaved and mapped (with 128APSK's padding)
    const int eta = bitsPerSymbol(c.m_modulation);
    const int nbCells = (int) c.m_map.size() / eta;
    m_cells.resize(nbCells);

    for (int i = 0; i < nbCells; i++)
    {
        int index = 0;

        for (int b = 0; b < eta; b++)
        {
            const int bit = c.m_map[i * eta + b];
            index = (index << 1) | (bit >= 0 ? m_bits[bit] : bit == mapOne ? 1 : 0);
        }

        m_cells[i] = c.m_points[index];
    }

    // The PLFRAME: header (time slicing's two slots), slots with a pilot block after every 16 (not at the end), all
    // but the header scrambled
    const int type = frameType(c.m_type, m_config.m_pilots);
    Cf header[timeSlicingHeaderSymbols];
    const int length = timeSlicing ? timeSlicingHeaderSymbols : headerSymbols;

    if (timeSlicing) {
        timeSlicingHeader(modcod, type, m_config.m_tsns[streamIndex % m_config.m_tsns.size()], header);
    } else {
        plHeaderSymbols(modcod, type, header);
    }

    symbols.insert(symbols.end(), header, header + length);
    const int s = nbCells / slotSymbols;
    const std::vector<uint8_t>& r = m_scrambler.sequence();
    int k = 0;
    static const Cf pilot(0.70710678f, 0.70710678f);

    for (int slot = 0; slot < s; slot++)
    {
        for (int i = 0; i < slotSymbols; i++, k++) {
            symbols.push_back(m_cells[slot * slotSymbols + i] * m_turns[r[k]]);
        }

        if (m_config.m_pilots && ((slot + 1) % pilotInterval == 0) && (slot + 1 < s))
        {
            for (int i = 0; i < pilotSymbols; i++, k++) {
                symbols.push_back(pilot * m_turns[r[k]]);
            }
        }
    }

    m_frameCount++;
}

// EN 302 307-2 5.5.2: the PLHEADER (PLS code 129 or 131), the VL-SNR header, then the XFECFRAME, its first Xs
// information bits (zeros) and Xp of its parity bits not sent, each bit sent spreading times; with the pilots of
// figures 17 and 18, the scrambling restarted after the VL-SNR header
void Modulator::vlsnrFrame(Stream& stream, int index, std::vector<Cf>& symbols)
{
    const VLSNRModcod& m = vlsnrModcod(index);
    Code& c = code(vlsnr(index));
    std::vector<uint8_t> bb;
    bbFrame(stream, c.m_kbch, bb);
    std::vector<uint8_t> parity((c.m_bch->parityBits() + 7) / 8);
    c.m_bch->encodeBits(bb.data(), c.m_kbch, parity.data());
    const int n = c.m_ldpc->n();
    m_bits.assign(n, 0);

    for (int i = 0; i < c.m_kbch; i++) {
        m_bits[m.m_xs + i] = (bb[i / 8] >> (7 - i % 8)) & 1;
    }

    for (int i = 0; i < c.m_bch->parityBits(); i++) {
        m_bits[m.m_xs + c.m_kbch + i] = (parity[i / 8] >> (7 - i % 8)) & 1;
    }

    c.m_ldpc->encode(m_bits.data(), m_bits.data() + c.m_kldpc);

    // The bits sent, each spreading times
    std::vector<uint8_t> sent;
    sent.reserve((size_t) n * m.m_spreading);

    for (int i = m.m_xs; i < n; i++)
    {
        if ((i >= c.m_kldpc) && vlsnrPunctured(m, i - c.m_kldpc)) {
            continue;
        }

        sent.insert(sent.end(), (size_t) m.m_spreading, m_bits[i]);
    }

    const int set = m.m_set;
    Cf header[headerSymbols];
    plHeaderSymbols(vlsnrSetModcod(set), 1, header);
    symbols.insert(symbols.end(), header, header + headerSymbols);
    Cf vlHeader[vlsnrHeaderSymbols];
    vlsnrHeader(index, vlHeader);
    symbols.insert(symbols.end(), vlHeader, vlHeader + vlsnrHeaderSymbols);

    const std::vector<uint8_t>& pilots = vlsnrPilots(set);
    const std::vector<uint8_t>& r = m_scrambler.sequence();
    static const Cf pilot(0.70710678f, 0.70710678f);
    const bool qpsk = m.m_modulation == Modulation::QPSK;
    size_t d = 0;   // Data symbols

    for (size_t k = 0; k < pilots.size(); k++)
    {
        if (pilots[k])
        {
            symbols.push_back(pilot * m_turns[r[k]]);
        }
        else if (qpsk)
        {
            symbols.push_back(c.m_points[(sent[2 * d] << 1) | sent[2 * d + 1]] * m_turns[r[k]]);
            d++;
        }
        else
        {
            // pi/2 BPSK, scrambled by (-1)^R
            const float sign = (sent[d] ? -1.0f : 1.0f) * ((r[k] & 1) ? -1.0f : 1.0f);
            symbols.push_back(headerSymbolZero((int) d) * sign);
            d++;
        }
    }

    m_frameCount++;
}

void Modulator::vlsnrDummyFrame(std::vector<Cf>& symbols)
{
    Cf header[headerSymbols];
    plHeaderSymbols(vlsnrSetModcod(2), 1, header);
    symbols.insert(symbols.end(), header, header + headerSymbols);
    Cf vlHeader[vlsnrHeaderSymbols];
    vlsnrHeader(vlsnrDummy, vlHeader);
    symbols.insert(symbols.end(), vlHeader, vlHeader + vlsnrHeaderSymbols);
    const std::vector<uint8_t>& r = m_scrambler.sequence();
    static const Cf unmodulated(0.70710678f, 0.70710678f);

    for (int k = 0; k < vlsnrFrameSymbols(2) - vlsnrHeaderLength; k++) {
        symbols.push_back(unmodulated * m_turns[r[k]]);
    }
}

void Modulator::dummyFrame(std::vector<Cf>& symbols)
{
    Cf header[timeSlicingHeaderSymbols];
    const bool timeSlicing = !m_config.m_tsns.empty();

    if (timeSlicing) {
        timeSlicingHeader(dummyModcod, 0, m_config.m_tsns[0], header);
    } else {
        plHeaderSymbols(dummyModcod, 0, header);
    }

    symbols.insert(symbols.end(), header, header + (timeSlicing ? timeSlicingHeaderSymbols : headerSymbols));
    const std::vector<uint8_t>& r = m_scrambler.sequence();
    static const Cf unmodulated(0.70710678f, 0.70710678f);

    for (int k = 0; k < dummySlots * slotSymbols; k++) {
        symbols.push_back(unmodulated * m_turns[r[k]]);
    }
}

} // namespace dvbs2
