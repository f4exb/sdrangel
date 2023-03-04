///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 kasper93                                                   //
// written by Kacper Michajłow and Edouard Griffiths                             //
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

#pragma once

#include <cmath>
#include <cstdio>
#include "dsp/dsptypes.h"
#include "dsp/misc.h"
#include "export.h"

namespace FirFilterGenerators
{
    SDRBASE_API void generateLowPassFilter(int nTaps, double sampleRate, double cutoff, std::vector<Real> &taps);
};

template <class Type>
class FirFilter
{
public:
    Type filter(Type sample)
    {
        Type acc = 0;
        unsigned int n_samples = m_samples.size();
        unsigned int n_taps = m_taps.size() - 1;
        unsigned int a = m_ptr;
        unsigned int b = a == n_samples - 1 ? 0 : a + 1;

        m_samples[m_ptr] = sample;

        for (unsigned int i = 0; i < n_taps; ++i)
        {
            acc += (m_samples[a] + m_samples[b]) * m_taps[i];

            a = (a == 0)             ? n_samples - 1 : a - 1;
            b = (b == n_samples - 1) ? 0             : b + 1;
        }

        acc += m_samples[a] * m_taps[n_taps];

        m_ptr = (m_ptr == n_samples - 1) ? 0 : m_ptr + 1;

        return acc;
    }

    // Print taps as a Matlab vector
    void printTaps(const char *name)
    {
        printf("%s = [", name);
        for (int i = 0; i <= m_taps.size() - 1; ++i) {
            printf("%g ", m_taps[i]);
        }
        for (int i = m_taps.size() - 2; i >= 0; --i) {
            printf("%g ", m_taps[i]);
        }
        printf("];\n");
    }

protected:
    void init(int nTaps)
    {
        m_ptr = 0;
        m_samples.resize(nTaps);

        for (int i = 0; i < nTaps; i++) {
            m_samples[i] = 0;
        }
    }

protected:
    std::vector<Real> m_taps;
    std::vector<Type> m_samples;
    size_t m_ptr;
};

template <class T>
struct Lowpass : public FirFilter<T>
{
public:
    void create(int nTaps, double sampleRate, double cutoff)
    {
        this->init(nTaps);
        FirFilterGenerators::generateLowPassFilter(nTaps, sampleRate, cutoff, this->m_taps);
    }
};

template <class T>
struct Bandpass : public FirFilter<T>
{
    void create(int nTaps, double sampleRate, double lowCutoff, double highCutoff)
    {
        this->init(nTaps);
        FirFilterGenerators::generateLowPassFilter(nTaps, sampleRate, highCutoff, this->m_taps);
        std::vector<Real> highPass;
        FirFilterGenerators::generateLowPassFilter(nTaps, sampleRate, lowCutoff, highPass);

        for (size_t i = 0; i < highPass.size(); ++i) {
            highPass[i] = -highPass[i];
        }

        highPass[highPass.size() - 1] += 1;

        for (size_t i = 0; i < this->m_taps.size(); ++i) {
            this->m_taps[i] = -(this->m_taps[i] + highPass[i]);
        }

        this->m_taps[this->m_taps.size() - 1] += 1;
    }
};

template <class T>
struct Highpass : public FirFilter<T>
{
    void create(int nTaps, double sampleRate, double cutoff)
    {
        this->init(nTaps);
        FirFilterGenerators::generateLowPassFilter(nTaps, sampleRate, cutoff, this->m_taps);

        for (size_t i = 0; i < this->m_taps.size(); ++i) {
            this->m_taps[i] = -this->m_taps[i];
        }

        this->m_taps[this->m_taps.size() - 1] += 1;
    }
};
