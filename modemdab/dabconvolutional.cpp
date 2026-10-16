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
#include <cstdlib>
#include <limits>

#include "dabconvolutional.h"

namespace modemdab
{

namespace
{

// Generator polynomials applied to the register (a_i << 6) | (a_i-1 << 5) | ... | a_i-6.
const int generators[4] = {0133, 0171, 0145, 0133};

int parity(int v)
{
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return v & 1;
}

int registerOutputs(int reg)
{
    int out = 0;

    for (int j = 0; j < 4; j++) {
        out = (out << 1) | parity(reg & generators[j]);
    }
    return out;
}

} // anonymous namespace

namespace
{

// The bits of a mother codeword that a profile transmits, in order, then its padding
template <typename T>
std::vector<T> puncture(const std::vector<T>& mother, const ProtectionProfile& profile, T padding)
{
    std::vector<T> out;
    std::size_t m = 0;

    out.reserve(profile.puncturedBits());
    for (const auto& part : profile.parts)
    {
        const auto& v = DABTables::puncturingVector(part.index);
        for (int block = 0; block < part.blocks * 4; block++)
        {
            for (int i = 0; i < 32; i++, m++)
            {
                if (v[i] && (m < mother.size())) {
                    out.push_back(mother[m]);
                }
            }
        }
    }
    const auto& vt = DABTables::tailPuncturingVector();
    for (int i = 0; i < 24; i++, m++)
    {
        if (vt[i] && (m < mother.size())) {
            out.push_back(mother[m]);
        }
    }
    out.insert(out.end(), profile.paddingBits, padding);
    return out;
}

} // anonymous namespace

std::vector<uint8_t> DABConvolutional::encodeMother(const std::vector<uint8_t>& bits)
{
    std::vector<uint8_t> mother;
    int state = 0;

    mother.reserve(4 * (bits.size() + TailBits));
    for (std::size_t i = 0; i < bits.size() + TailBits; i++)
    {
        int b = i < bits.size() ? (bits[i] & 1) : 0;
        int reg = (b << 6) | state;
        int out = registerOutputs(reg);

        for (int j = 3; j >= 0; j--) {
            mother.push_back(static_cast<uint8_t>((out >> j) & 1));
        }
        state = reg >> 1;
    }
    return mother;
}

std::vector<uint8_t> DABConvolutional::encode(const std::vector<uint8_t>& bits, const ProtectionProfile& profile)
{
    return puncture(encodeMother(bits), profile, static_cast<uint8_t>(0));
}

std::vector<uint8_t> DABConvolutional::codedReliable(const std::vector<uint8_t>& reliable, const ProtectionProfile& profile)
{
    const int bits = static_cast<int>(reliable.size());
    std::vector<uint8_t> mother(4 * (bits + TailBits));
    int unreliable = 0;     // Unreliable bits in the register

    for (int t = 0; t < bits + TailBits; t++)
    {
        if ((t < bits) && !reliable[t]) {
            unreliable++;
        }
        if ((t - ConstraintLength >= 0) && (t - ConstraintLength < bits) && !reliable[t - ConstraintLength]) {
            unreliable--;
        }
        const uint8_t r = unreliable == 0 ? 1 : 0;
        for (int j = 0; j < 4; j++) {
            mother[4 * t + j] = r;
        }
    }
    return puncture(mother, profile, static_cast<uint8_t>(1));
}

void DABConvolutional::encodeKnown(const std::vector<uint8_t>& bits, const std::vector<uint8_t>& reliable,
    const ProtectionProfile& profile, std::vector<int8_t>& codeword)
{
    // The four outputs of each register value, unreliable or not
    static const std::array<std::array<int8_t, 4>, 128> outputs = [] {
        std::array<std::array<int8_t, 4>, 128> o;
        for (int reg = 0; reg < 128; reg++)
        {
            const int out = registerOutputs(reg);
            for (int j = 0; j < 4; j++) {
                o[reg][j] = static_cast<int8_t>((out >> (3 - j)) & 1);
            }
        }
        return o;
    }();
    const int count = static_cast<int>(bits.size());
    const bool haveReliability = reliable.size() == bits.size();
    codeword.clear();
    codeword.reserve(profile.puncturedBits());

    int state = 0;
    int unreliable = 0;     // Unreliable bits in the register
    int t = 0;              // Step
    int j = 4;              // Output of the step
    int8_t step[4] = {};

    // The next mother codeword bit
    auto next = [&]() -> int8_t {
        if (j == 4)
        {
            const int b = t < count ? (bits[t] & 1) : 0;
            const int reg = (b << 6) | state;
            if (haveReliability)
            {
                if ((t < count) && !reliable[t]) {
                    unreliable++;
                }
                if ((t >= ConstraintLength) && (t - ConstraintLength < count) && !reliable[t - ConstraintLength]) {
                    unreliable--;
                }
            }
            for (int k = 0; k < 4; k++) {
                step[k] = unreliable ? static_cast<int8_t>(-1) : outputs[reg][k];
            }
            state = reg >> 1;
            t++;
            j = 0;
        }
        return step[j++];
    };

    for (const auto& part : profile.parts)
    {
        const auto& v = DABTables::puncturingVector(part.index);
        for (int block = 0; block < part.blocks * 4; block++)
        {
            for (int i = 0; i < 32; i++)
            {
                const int8_t bit = next();
                if (v[i]) {
                    codeword.push_back(bit);
                }
            }
        }
    }
    const auto& vt = DABTables::tailPuncturingVector();
    for (int i = 0; i < 24; i++)
    {
        const int8_t bit = next();
        if (vt[i]) {
            codeword.push_back(bit);
        }
    }
    codeword.insert(codeword.end(), profile.paddingBits, 0);
}

bool DABConvolutional::depuncture(const float *soft, int softCount, const ProtectionProfile& profile, std::vector<float>& mother)
{
    const int informationBits = profile.informationBits();
    const int needed = profile.puncturedBits() - profile.paddingBits;

    if (softCount < needed) {
        return false;
    }

    mother.assign(4 * (informationBits + TailBits), 0.0f);
    std::size_t m = 0;
    int s = 0;

    for (const auto& part : profile.parts)
    {
        const auto& v = DABTables::puncturingVector(part.index);
        for (int block = 0; block < part.blocks * 4; block++)
        {
            for (int i = 0; i < 32; i++, m++)
            {
                if (v[i]) {
                    mother[m] = soft[s++];
                }
            }
        }
    }
    const auto& vt = DABTables::tailPuncturingVector();
    for (int i = 0; i < 24; i++, m++)
    {
        if (vt[i]) {
            mother[m] = soft[s++];
        }
    }
    return true;
}

DABViterbi::DABViterbi() :
    m_core(modemdr::Viterbi::dabGenerators()),
    m_reliabilityLimit(0.0f),
    m_closeLimit(0.0f)
{
}

void DABViterbi::decode(const float *mother, int informationBits, std::vector<uint8_t>& bits)
{
    const int steps = informationBits + DABConvolutional::TailBits;

    m_decisions.resize(steps);
    if (m_reliabilityLimit > 0.0f)
    {
        // The limit in metric units, from the mean magnitude of the soft values
        double sumAbs = 0.0;
        int count = 0;
        for (int i = 0; i < 4 * steps; i++)
        {
            if (mother[i] != 0.0f)
            {
                sumAbs += std::fabs(mother[i]);
                count++;
            }
        }
        m_closeLimit = count > 0 ? static_cast<float>(m_reliabilityLimit * sumAbs / count) : 0.0f;
        m_close.resize(steps);
    }
    m_core.forward(mother, steps, m_decisions.data(), m_reliabilityLimit > 0.0f ? m_close.data() : nullptr, m_closeLimit);

    // Trace back from the all-zero terminating state
    bits.resize(informationBits);
    m_states.resize(steps);
    modemdr::Viterbi::traceback(m_decisions.data(), steps, informationBits, bits.data(), m_states.data());

    if (m_reliabilityLimit <= 0.0f) {
        return;
    }

    // SOVA: at each step, the path that lost at the decoded path's state decides the bits
    // before differently until the two merge. When it lost by less than the limit, those
    // bits are unreliable. Paths merge within a few constraint lengths.
    const int window = 8 * DABConvolutional::ConstraintLength;
    m_reliable.assign(informationBits, 1);
    for (int t = 1; t < steps; t++)
    {
        const int s = m_states[t];
        if (!((m_close[t] >> s) & 1)) {
            continue;
        }
        const int d = (m_decisions[t] >> s) & 1;
        // The losing path's state before step t, and the bits it gives from there back
        int competitor = ((s & 31) << 1) | (d ^ 1);
        for (int k = t - 1; (k >= 0) && (k >= t - window); k--)
        {
            const int decoded = m_states[k];
            if (competitor == decoded) {
                break;
            }
            if (((competitor ^ decoded) & 32) && (k < informationBits)) {
                m_reliable[k] = 0;
            }
            const int cd = (m_decisions[k] >> competitor) & 1;
            competitor = ((competitor & 31) << 1) | cd;
        }
    }
}

bool DABViterbi::decodePunctured(
    const float *soft,
    int softCount,
    const ProtectionProfile& profile,
    std::vector<uint8_t>& bits,
    int *bitErrors,
    int *codedBits
)
{
    if (!DABConvolutional::depuncture(soft, softCount, profile, m_mother)) {
        return false;
    }

    decode(m_mother.data(), profile.informationBits(), bits);

    if (bitErrors)
    {
        // Compare the hard decisions of the transmitted bits with the re-encoded codeword
        std::vector<uint8_t> reencoded = DABConvolutional::encodeMother(bits);
        int errors = 0;
        int compared = 0;

        for (std::size_t i = 0; i < reencoded.size(); i++)
        {
            const float v = m_mother[i];
            if (v != 0.0f)
            {
                compared++;
                if ((v < 0.0f) != (reencoded[i] != 0)) {
                    errors++;
                }
            }
        }
        *bitErrors = errors;
        if (codedBits) {
            *codedBits = compared;
        }
    }
    return true;
}

} // namespace modemdab
