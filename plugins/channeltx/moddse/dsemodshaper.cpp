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

#include "dsemodshaper.h"

void DSEModShaper::shapeWithGaps(const int8_t *levels, int nbLevels, std::vector<float>& out)
{
    int i = 0;

    while (i < nbLevels)
    {
        int j = i;

        if (levels[i] == m_levelOff)
        {
            while ((j < nbLevels) && (levels[j] == m_levelOff)) {
                j++;
            }

            // The filter tail of the burst just ended takes the first part of the gap
            size_t before = out.size();
            flush(out);
            int offSamples = std::max(0, (j - i) * m_samplesPerSymbol - (int) (out.size() - before));
            out.insert(out.end(), offSamples, m_sampleOff);
        }
        else
        {
            while ((j < nbLevels) && (levels[j] != m_levelOff)) {
                j++;
            }

            shape(&levels[i], j - i, out);
        }

        i = j;
    }
}

// ---- GMSK -------------------------------------------------------------------

DSEModGMSKShaper::DSEModGMSKShaper(int samplesPerSymbol, double bt, int symbolSpan) :
    DSEModShaper(samplesPerSymbol),
    m_bt(bt),
    m_symbolSpan(symbolSpan)
{
    reset();
}

void DSEModGMSKShaper::reset()
{
    m_filter.create(m_bt, m_symbolSpan, m_samplesPerSymbol);
}

void DSEModGMSKShaper::shape(const int8_t *levels, int nbLevels, std::vector<float>& out)
{
    for (int i = 0; i < nbLevels; i++)
    {
        float v = levels[i] / 3.0f;

        for (int j = 0; j < m_samplesPerSymbol; j++) {
            out.push_back(m_filter.filter(v)); // NRZ input, unity DC gain
        }
    }
}

void DSEModGMSKShaper::flush(std::vector<float>& out)
{
    for (int i = 0; i < m_symbolSpan * m_samplesPerSymbol; i++) {
        out.push_back(m_filter.filter(0.0f));
    }
}

// ---- RRC 4FSK ---------------------------------------------------------------

DSEModRRC4FSKShaper::DSEModRRC4FSKShaper(int samplesPerSymbol, double alpha, int symbolSpan) :
    DSEModShaper(samplesPerSymbol),
    m_alpha(alpha),
    m_symbolSpan(symbolSpan),
    m_scale(1.0f)
{
    create();
    reset();
}

void DSEModRRC4FSKShaper::create()
{
    m_filter.create(m_alpha, m_symbolSpan, m_samplesPerSymbol, RootRaisedCosine<float>::Gain);

    // Calibrate: a constant stream of unit impulses (one per symbol) must give unity
    // output at the symbol instants (the sum of the symbol spaced taps).
    float acc = 0.0f;
    int nbSymbols = 4 * m_symbolSpan;

    for (int s = 0; s < nbSymbols; s++)
    {
        for (int j = 0; j < m_samplesPerSymbol; j++)
        {
            float y = m_filter.filter(j == 0 ? 1.0f : 0.0f);

            if ((s == nbSymbols - 1) && (j == 0)) {
                acc = y;
            }
        }
    }

    m_scale = (std::fabs(acc) > 1e-6f) ? 1.0f / acc : 1.0f;
}

void DSEModRRC4FSKShaper::reset()
{
    m_filter.create(m_alpha, m_symbolSpan, m_samplesPerSymbol, RootRaisedCosine<float>::Gain); // clears memory
}

void DSEModRRC4FSKShaper::shape(const int8_t *levels, int nbLevels, std::vector<float>& out)
{
    for (int i = 0; i < nbLevels; i++)
    {
        float v = (levels[i] / 3.0f) * m_scale;

        for (int j = 0; j < m_samplesPerSymbol; j++) {
            out.push_back(m_filter.filter(j == 0 ? v : 0.0f)); // impulse train input
        }
    }
}

void DSEModRRC4FSKShaper::flush(std::vector<float>& out)
{
    for (int i = 0; i < m_symbolSpan * m_samplesPerSymbol; i++) {
        out.push_back(m_filter.filter(0.0f));
    }
}

// ---- Linear -----------------------------------------------------------------------------------

DSEModLinearShaper::DSEModLinearShaper(int samplesPerSymbol) :
    m_sps(0),
    m_history(DVAngel61::RrcSpan + 1, std::complex<float>(0.0f, 0.0f)),
    m_level(0.5f)
{
    setSamplesPerSymbol(samplesPerSymbol);
}

void DSEModLinearShaper::setSamplesPerSymbol(int samplesPerSymbol)
{
    m_sps = samplesPerSymbol;
    m_taps.resize(DVAngel61::rrcTapCount(samplesPerSymbol));
    DVAngel61::rrcTaps(samplesPerSymbol, m_taps.data());
    reset();
}

void DSEModLinearShaper::reset()
{
    std::fill(m_history.begin(), m_history.end(), std::complex<float>(0.0f, 0.0f));
}

void DSEModLinearShaper::shape(const std::complex<float> *symbols, int nbSymbols, std::vector<float>& iq)
{
    // Polyphase: output sample p of a symbol period is the sum over the symbols in the filter's
    // span of symbol j back times tap sps j + p (impulse train through the causal RRC). Unit power
    // symbols through the unit energy filter have a mean output power of 1/sps per sample, so the
    // level is scaled by sqrt(sps).
    const int sps = m_sps;
    const int nbTaps = (int) m_taps.size();
    const int span = DVAngel61::RrcSpan;
    const float gain = m_level * std::sqrt((float) sps);

    for (int i = 0; i < nbSymbols; i++)
    {
        for (int j = span; j > 0; j--) {
            m_history[j] = m_history[j - 1];
        }

        m_history[0] = symbols[i] * gain;

        for (int p = 0; p < sps; p++)
        {
            std::complex<float> acc(0.0f, 0.0f);

            for (int j = 0; j <= span; j++)
            {
                int tap = j * sps + p;

                if (tap < nbTaps) {
                    acc += m_history[j] * m_taps[tap];
                }
            }

            iq.push_back(acc.real());
            iq.push_back(acc.imag());
        }
    }
}

void DSEModLinearShaper::flush(std::vector<float>& iq)
{
    std::vector<std::complex<float>> zeros(DVAngel61::RrcSpan, std::complex<float>(0.0f, 0.0f));
    shape(zeros.data(), (int) zeros.size(), iq);
}
