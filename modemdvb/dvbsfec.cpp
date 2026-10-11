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

#include "dvbsfec.h"

namespace dvbs {

double FECDecoder::now()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

FECDecoder::FECDecoder() :
    m_codeRateSetting(-1),
    m_conjugateFirst(false),
    m_period(1),
    m_periodFill(0),
    m_encoderState(0),
    m_unsyncedBits(0),
    m_failedRun(0),
    m_berErrors(0),
    m_berTotal(0)
{
    reset();
}

void FECDecoder::setCodeRate(int rate)
{
    // Out of range: found (rather than taken as the nearest)
    if ((rate < (int) CodeRate::R1_2) || (rate > (int) CodeRate::R7_8)) {
        rate = -1;
    }

    if (rate != m_codeRateSetting)
    {
        m_codeRateSetting = rate;
        reset();
    }
}

void FECDecoder::reset()
{
    const uint64_t searches = m_status.m_searches;
    const uint64_t locks = m_status.m_locks;
    double seconds[3];
    std::copy(m_status.m_seconds, m_status.m_seconds + 3, seconds);
    m_status = Status();
    m_status.m_searches = searches;
    m_status.m_locks = locks;
    std::copy(seconds, seconds + 3, m_status.m_seconds);
    m_search.clear();
    m_history.clear();
    // m_skipSymbols isn't: the receiver resets the FEC at each restart, hundreds of times a second without a signal
    m_outer.reset();
    m_viterbi.reset();
    m_hard.clear();
    m_periodFill = 0;
    m_encoderState = 0;
    m_unsyncedBits = 0;
    m_failedRun = 0;
    m_berErrors = 0;
    m_berTotal = 0;
    m_berOuter = m_outer.getStats();
    // The outer decoder's counts go on from reset to reset (as the searches and locks do): what's new is measured from
    // them, so a lock doesn't take the totals for new packets (and unlock at once, after packets that failed before)
    m_status.m_outer = m_outer.getStats();
}

void FECDecoder::rotate(const int8_t *iq, int nbSymbols, int rotation, bool conjugate, int8_t *out)
{
    // Undoing a quarter turn: (I + jQ)(-j) = Q - jI. The soft values are positive for 0, so for +1. Conjugated first:
    // Q and -Q swapped
    for (int i = 0; i < nbSymbols; i++)
    {
        const int8_t a = iq[2 * i], q = iq[2 * i + 1];
        const int8_t na = (int8_t) (a == -128 ? 127 : -a), nq = (int8_t) (q == -128 ? 127 : -q);
        const int8_t b = conjugate ? nq : q, nb = conjugate ? q : nq;

        switch (rotation & 3)
        {
        case 0: out[2 * i] = a; out[2 * i + 1] = b; break;
        case 1: out[2 * i] = b; out[2 * i + 1] = na; break;
        case 2: out[2 * i] = na; out[2 * i + 1] = nb; break;
        default: out[2 * i] = nb; out[2 * i + 1] = a; break;
        }
    }
}

void FECDecoder::decode(const int8_t *iq, int nbSymbols)
{
    if (nbSymbols <= 0) {
        return;
    }

    // A symbol rate set since: the skip measured at the last isn't this one's
    const double rate = m_requestedSymbolRate;

    if (rate != m_symbolRate)
    {
        m_symbolRate = rate;
        m_skipSymbols = 0;
    }

    if (m_status.m_state == State::Searching)
    {
        // Skipped, to keep to the search's budget
        if (m_skipSymbols > 0)
        {
            const int skip = (int) std::min<int64_t>(m_skipSymbols, nbSymbols);
            m_skipSymbols -= skip;
            iq += 2 * skip;
            nbSymbols -= skip;
        }

        m_search.insert(m_search.end(), iq, iq + 2 * nbSymbols);

        while ((m_status.m_state == State::Searching) && ((int) m_search.size() >= 2 * m_searchSymbols)) {
            search();
        }

        return;
    }

    // The last block's soft bits, to search again from at once
    m_history.insert(m_history.end(), iq, iq + 2 * nbSymbols);

    if ((int) m_history.size() > 2 * m_searchSymbols) {
        m_history.erase(m_history.begin(), m_history.end() - 2 * m_searchSymbols);
    }

    m_rotated.resize(2 * nbSymbols);
    rotate(iq, nbSymbols, m_hypothesis.m_rotation, m_hypothesis.m_conjugate, m_rotated.data());
    decodeSerial(m_rotated.data(), 2 * nbSymbols);

    if (m_status.m_state == State::Searching)
    {
        // Lost: search the recent symbols
        m_search.swap(m_history);
        m_history.clear();

        while ((m_status.m_state == State::Searching) && ((int) m_search.size() >= 2 * m_searchSymbols)) {
            search();
        }
    }
}

double FECDecoder::test(const int8_t *serial, int nbBits, CodeRate rate, int offset, std::vector<uint8_t> *bits)
{
    std::vector<PunctureEntry> pattern;
    int period;
    puncturePattern(rate, pattern, period);
    const int length = (int) pattern.size();
    const int periods = (nbBits - offset) / length;
    const int steps = periods * period;
    m_xy.assign(2 * steps, 0);

    for (int g = 0; g < periods; g++)
    {
        const int8_t *s = serial + offset + g * length;
        int8_t *xy = m_xy.data() + 2 * g * period;

        for (int j = 0; j < length; j++) {
            xy[2 * pattern[j].m_bit + pattern[j].m_y] = s[j];
        }
    }

    m_testViterbi.reset();
    m_bits.clear();
    m_testViterbi.decode(m_xy.data(), steps, m_bits);
    const int decoded = (int) m_bits.size();
    uint8_t state = 0;
    m_encoded.clear();
    dvbt::Viterbi27::encode(m_bits.data(), decoded, state, m_encoded);
    int matches = 0, total = 0;

    for (int i = m_skipSteps; i < decoded; i++)
    {
        for (int y = 0; y < 2; y++)
        {
            const int8_t soft = m_xy[2 * i + y];

            if (soft != 0)
            {
                total++;
                matches += (soft < 0 ? 1 : 0) == m_encoded[2 * i + y];
            }
        }
    }

    if (bits) {
        *bits = m_bits;
    }

    return total > 0 ? (double) matches / total : 0.0;
}

int FECDecoder::syncBytes(const std::vector<uint8_t>& bits, bool& inverted, int& positions)
{
    const int packetBits = dvbt::rsPacketSize * 8;
    const int n = (int) bits.size();
    int best = 0;
    inverted = false;
    positions = 0;

    for (int offset = 0; offset < packetBits; offset++)
    {
        int c47 = 0, cB8 = 0, count = 0;

        for (int k = offset; k + 8 <= n; k += packetBits, count++)
        {
            uint8_t byte = 0;

            for (int b = 0; b < 8; b++) {
                byte = (uint8_t) ((byte << 1) | (bits[k + b] & 1));
            }

            c47 += byte == 0x47;
            cB8 += byte == 0xB8;
        }

        if (c47 + cB8 > best)
        {
            best = c47 + cB8;
            inverted = cB8 > c47;
            positions = count;
        }
    }

    return best;
}

void FECDecoder::search()
{
    const double start = now();
    m_status.m_searches++;
    const int nbSymbols = m_searchSymbols;
    const int nbBits = 2 * nbSymbols;
    std::vector<CodeRate> rates;

    if (m_codeRateSetting >= 0) {
        rates.push_back((CodeRate) m_codeRateSetting);
    } else {
        rates = {CodeRate::R1_2, CodeRate::R2_3, CodeRate::R3_4, CodeRate::R5_6, CodeRate::R7_8};
    }

    // Each hypothesis decoded, and the agreement of what it decoded with what was received (for the status, and to skip
    // the hopeless). Agreement can't choose between code rates: at low SNR a heavily punctured code, whose codewords are
    // dense, can match a rate 1/2 stream more closely than its own code does. So the decoded bits' sync bytes decide:
    // 0x47 (or 0xB8) every 204 bytes, which only the right hypothesis decodes
    Hypothesis best;
    double bestFraction = 0.0;
    double bestAgreement = 0.0;
    bool bestInverted = false;
    std::vector<int8_t> rotated(nbBits);
    std::vector<uint8_t> bits;
    m_status.m_lastSyncs = 0;
    m_status.m_lastPositions = 0;

    // The spectrum conjugated only if nothing's found the other way (the way last locked first)
    for (int pass = 0; (pass < 2) && (bestFraction == 0.0); pass++)
    {
        const bool conjugate = pass == 0 ? m_conjugateFirst : !m_conjugateFirst;

        for (int rotation = 0; rotation < 2; rotation++)
        {
            rotate(m_search.data(), nbSymbols, rotation, conjugate, rotated.data());

            for (CodeRate rate : rates)
            {
                const int length = punctureLength(rate);

                for (int offset = 0; offset < length; offset++)
                {
                    const double agreement = test(rotated.data(), nbBits, rate, offset, &bits);
                    bestAgreement = std::max(bestAgreement, agreement);

                    if (agreement < m_minAgreement) {
                        continue;
                    }

                    bool inverted;
                    int positions;
                    const int syncs = syncBytes(bits, inverted, positions);
                    const double fraction = positions > 0 ? (double) syncs / positions : 0.0;

                    if ((syncs >= 3) && (fraction >= 0.6)
                        && ((fraction > bestFraction) || ((fraction == bestFraction) && (agreement > best.m_agreement))))
                    {
                        best.m_rotation = rotation;
                        best.m_conjugate = conjugate;
                        best.m_rate = rate;
                        best.m_offset = offset;
                        best.m_agreement = agreement;
                        bestFraction = fraction;
                        bestInverted = inverted;
                        m_status.m_lastSyncs = syncs;
                        m_status.m_lastPositions = positions;
                    }
                }
            }
        }
    }

    m_status.m_lastAgreement = bestAgreement;
    const bool found = bestFraction > 0.0;

    if (found && bestInverted) {
        best.m_rotation += 2;
    }

    m_status.m_seconds[0] += now() - start;

    if (found)
    {
        lock(best);
        // From the start of the block, rotated as found, from the start of its first whole period
        std::vector<int8_t> serial(m_search.size());
        rotate(m_search.data(), (int) m_search.size() / 2, best.m_rotation, best.m_conjugate, serial.data());
        m_search.clear();
        decodeSerial(serial.data() + best.m_offset, (int) serial.size() - best.m_offset);
    }
    else
    {
        // Try again with half of the block, and as many new symbols. Unless searching would take more than its share of
        // real time at that rate (at high symbol rates): then with symbols further on, those between skipped
        const double rate = m_symbolRate;
        const double seconds = now() - start;
        const int64_t wanted = rate > 0.0 ? (int64_t) (seconds * rate / m_searchBudget) : 0;
        const int64_t advance = std::max<int64_t>(nbSymbols / 2, wanted);
        const int64_t held = (int64_t) m_search.size() / 2;

        if (advance < held)
        {
            m_search.erase(m_search.begin(), m_search.begin() + 2 * advance);
        }
        else
        {
            m_search.clear();
            m_skipSymbols = advance - held;
        }
    }
}

void FECDecoder::lock(const Hypothesis& hypothesis)
{
    m_hypothesis = hypothesis;
    m_conjugateFirst = hypothesis.m_conjugate;
    puncturePattern(hypothesis.m_rate, m_pattern, m_period);
    m_periodBits.assign(m_pattern.size(), 0);
    m_periodFill = 0;
    m_viterbi.reset();
    m_outer.reset();
    m_hard.clear();
    m_encoderState = 0;
    m_unsyncedBits = 0;
    m_failedRun = 0;
    m_berErrors = 0;
    m_berTotal = 0;
    m_berOuter = m_outer.getStats();
    m_status.m_state = State::Locked;
    m_status.m_codeRateKnown = true;
    m_status.m_codeRate = hypothesis.m_rate;
    m_status.m_rotation = hypothesis.m_rotation;
    m_status.m_inverted = hypothesis.m_conjugate;
    m_status.m_agreement = hypothesis.m_agreement;
    m_status.m_locks++;
}

void FECDecoder::decodeSerial(const int8_t *serial, int nbBits)
{
    const int length = (int) m_pattern.size();
    // The periods completed, sized at once (the punctured bits left 0)
    const int periods = (m_periodFill + std::max(0, nbBits)) / length;
    m_xy.assign(2 * (size_t) periods * m_period, 0);
    int8_t *xy = m_xy.data();
    const PunctureEntry *pattern = m_pattern.data();

    for (int i = 0; i < nbBits; i++)
    {
        m_periodBits[m_periodFill++] = serial[i];

        if (m_periodFill == length)
        {
            for (int j = 0; j < length; j++) {
                xy[2 * pattern[j].m_bit + pattern[j].m_y] = m_periodBits[j];
            }

            xy += 2 * m_period;
            m_periodFill = 0;
        }
    }

    const int steps = (int) m_xy.size() / 2;

    if (steps == 0) {
        return;
    }

    // Hard decisions, for the BER before Viterbi
    for (int i = 0; i < steps; i++)
    {
        const int8_t x = m_xy[2 * i], y = m_xy[2 * i + 1];
        m_hard.push_back((uint8_t) ((x < 0 ? 1 : 0) | (y < 0 ? 2 : 0) | (x != 0 ? 4 : 0) | (y != 0 ? 8 : 0)));
    }

    double start = now();
    m_bits.clear();
    m_viterbi.decode(m_xy.data(), steps, m_bits);
    measureBER(m_bits);
    m_status.m_viterbiBits += m_bits.size();
    double mid = now();
    m_status.m_seconds[1] += mid - start;
    const uint64_t packetsBefore = m_status.m_outer.m_packets, failedBefore = m_status.m_outer.m_rsFailed;
    m_outer.decode(m_bits.data(), (int) m_bits.size());
    m_status.m_seconds[2] += now() - mid;
    m_status.m_outer = m_outer.getStats();
    m_status.m_tsSync = m_outer.synced();
    const uint64_t newPackets = m_status.m_outer.m_packets - packetsBefore, newFailed = m_status.m_outer.m_rsFailed - failedBefore;

    // RS failures are counted for packets dropped before the outer decoder's synchronised too, which aren't counted
    // as packets: as many failures as packets, or more, is a run of them
    if (newPackets > 0) {
        m_failedRun = newFailed >= newPackets ? m_failedRun + (int) newPackets : 0;
    }

    // Searching again if the outer decoder can't synchronise, or RS fails packet after packet
    if (m_outer.synced()) {
        m_unsyncedBits = 0;
    } else {
        m_unsyncedBits += m_bits.size();
    }

    if ((m_unsyncedBits > m_maxUnsyncedPackets * dvbt::rsPacketSize * 8) || (m_failedRun >= m_maxFailedRun))
    {
        m_status.m_state = State::Searching;
        m_status.m_tsSync = false;
        m_search.clear();
        m_unsyncedBits = 0;
        m_failedRun = 0;
    }

    // BER after Viterbi, from the bits Reed-Solomon corrected, over the packets since the window started
    const dvbt::OuterDecoder::Stats& stats = m_status.m_outer;
    const uint64_t packets = (stats.m_packets - m_berOuter.m_packets);

    if (packets >= 1000)
    {
        const uint64_t bits = stats.m_rsBitsCorrected - m_berOuter.m_rsBitsCorrected;
        const uint64_t failed = stats.m_rsFailed - m_berOuter.m_rsFailed;
        // A packet RS couldn't correct had more than 8 bytes wrong: count it as 9 bytes of 4 bits
        m_status.m_postViterbiBER = (bits + failed * 36.0) / (packets * dvbt::rsPacketSize * 8.0);
        m_berOuter = stats;
    }
}

// Re-encode the decoded bits and compare them with the hard decisions of what was received
void FECDecoder::measureBER(const std::vector<uint8_t>& bits)
{
    const int n = std::min((int) bits.size(), (int) m_hard.size());

    if (n <= 0) {
        return;
    }

    m_encoded.clear();
    dvbt::Viterbi27::encode(bits.data(), n, m_encoderState, m_encoded);
    uint64_t errors = 0, total = 0;

    for (int i = 0; i < n; i++)
    {
        const uint8_t h = m_hard[i];

        if (h & 4)
        {
            total++;
            errors += (h & 1) != m_encoded[2 * i];
        }

        if (h & 8)
        {
            total++;
            errors += ((h >> 1) & 1) != m_encoded[2 * i + 1];
        }
    }

    m_hard.erase(m_hard.begin(), m_hard.begin() + n);
    m_berErrors += errors;
    m_berTotal += total;

    if (m_berTotal >= m_berWindow)
    {
        m_status.m_preViterbiBER = (double) m_berErrors / m_berTotal;
        m_berErrors = 0;
        m_berTotal = 0;
    }
    else if (m_status.m_preViterbiBER == 0.0 && m_berTotal >= 20000)
    {
        m_status.m_preViterbiBER = (double) m_berErrors / m_berTotal;
    }
}

} // namespace dvbs
