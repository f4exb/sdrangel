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
#include <limits>

#include "dvbits.h"
#include "dvconvolutional.h"

DVConvolutional::DVConvolutional(int k, uint32_t poly0, uint32_t poly1) :
    m_k(k),
    m_nbStates(1 << (k - 1))
{
    // Trellis state = the K - 1 most recent input bits, newest in bit K - 2 (the register shifted right by one)
    m_outputs.resize(2 * m_nbStates);

    for (int state = 0; state < m_nbStates; state++)
    {
        for (int bit = 0; bit < 2; bit++)
        {
            uint32_t r = (uint32_t) state | ((uint32_t) bit << (k - 1));
            m_outputs[(state << 1) | bit] = (uint8_t) ((parity(r & poly0) << 1) | parity(r & poly1));
        }
    }
}

int DVConvolutional::parity(uint32_t v)
{
    return DVBits::popcount(v) & 1;
}

void DVConvolutional::encode(const uint8_t *bits, int nbBits, uint8_t *coded) const
{
    int state = 0;

    for (int i = 0; i < nbBits; i++)
    {
        int bit = bits[i] & 1;
        uint8_t out = m_outputs[(state << 1) | bit];
        coded[2 * i] = out >> 1;
        coded[2 * i + 1] = out & 1;
        state = (state >> 1) | (bit << (m_k - 2));
    }
}

void DVConvolutional::encodeDibits(const uint8_t *bits, int nbBits, uint8_t *dibits) const
{
    int state = 0;

    for (int i = 0; i < nbBits; i++)
    {
        int bit = bits[i] & 1;
        dibits[i] = m_outputs[(state << 1) | bit];
        state = (state >> 1) | (bit << (m_k - 2));
    }
}

int DVConvolutional::decodeSoft(const float *llr, int nbBits, uint8_t *bits, bool terminated) const
{
    const float minusInfinity = -std::numeric_limits<float>::infinity();
    std::vector<float> metric(m_nbStates, minusInfinity), next(m_nbStates);
    std::vector<uint8_t> decisions((size_t) nbBits * m_nbStates); //!< LSB of the predecessor state
    metric[0] = 0.0f;

    for (int i = 0; i < nbBits; i++)
    {
        float l0 = llr[2 * i];
        float l1 = llr[2 * i + 1];
        // Branch metrics for the four output pairs: correlation with the bipolar outputs
        float branch[4] = { l0 + l1, l0 - l1, -l0 + l1, -l0 - l1 };
        std::fill(next.begin(), next.end(), minusInfinity);
        uint8_t *decision = &decisions[(size_t) i * m_nbStates];

        for (int state = 0; state < m_nbStates; state++)
        {
            if (metric[state] == minusInfinity) {
                continue;
            }

            for (int bit = 0; bit < 2; bit++)
            {
                int ns = (state >> 1) | (bit << (m_k - 2));
                float m = metric[state] + branch[m_outputs[(state << 1) | bit]];

                if (m > next[ns])
                {
                    next[ns] = m;
                    decision[ns] = (uint8_t) (state & 1);
                }
            }
        }

        metric.swap(next);
    }

    int state = 0;

    if (!terminated)
    {
        state = (int) (std::max_element(metric.begin(), metric.end()) - metric.begin());
    }

    // Trace back: the input bit that led to state ns is its MSB, the predecessor is ns shifted up with the stored LSB
    for (int i = nbBits - 1; i >= 0; i--)
    {
        bits[i] = (uint8_t) (state >> (m_k - 2));
        state = ((state << 1) & (m_nbStates - 1)) | decisions[(size_t) i * m_nbStates + state];
    }

    // Disagreements between the survivor and the hard decisions
    std::vector<uint8_t> coded(2 * nbBits);
    encode(bits, nbBits, coded.data());
    int differences = 0;

    for (int i = 0; i < 2 * nbBits; i++)
    {
        if ((llr[i] != 0.0f) && (coded[i] != (llr[i] < 0.0f ? 1 : 0))) {
            differences++;
        }
    }

    return differences;
}

int DVConvolutional::decodeHard(const uint8_t *coded, int nbBits, uint8_t *bits, bool terminated) const
{
    std::vector<float> llr(2 * nbBits);
    DVBits::bitsToLlr(coded, 2 * nbBits, llr.data());
    return decodeSoft(llr.data(), nbBits, bits, terminated);
}

int DVConvolutional::decodeDibits(const uint8_t *dibits, int nbBits, uint8_t *bits, bool terminated) const
{
    std::vector<uint8_t> coded(2 * nbBits);
    DVBits::dibitsToBits(dibits, nbBits, coded.data());
    return decodeHard(coded.data(), nbBits, bits, terminated);
}

const DVConvolutional& DVConvolutional::dstar()
{
    static const DVConvolutional code(3, 0x7, 0x5);
    return code;
}

const DVConvolutional& DVConvolutional::k5()
{
    static const DVConvolutional code(5, 0x13, 0x1D);
    return code;
}
