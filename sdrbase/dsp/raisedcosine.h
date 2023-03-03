///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_RAISEDCOSINE_H
#define INCLUDE_RAISEDCOSINE_H

#include <cmath>
#include <cstdio>
#include "dsp/dsptypes.h"

// Raised-cosine low-pass filter for pulse shaping, without intersymbol interference (ISI)
// https://en.wikipedia.org/wiki/Raised-cosine_filter
// This could be optimised in to a polyphase filter, as samplesPerSymbol-1 inputs
// to filter() should be zero, as the data is upsampled to the sample rate
template <class Type> class RaisedCosine {
public:
    RaisedCosine() : m_ptr(0) { }

    // beta - roll-off factor
    // symbolSpan - number of symbols over which the filter is spread
    // samplesPerSymbol - number of samples per symbol
    // normaliseUpsampledAmplitude - when true, scale the filter such that an upsampled
    // (by samplesPerSymbol) bipolar sequence (E.g. [1 0 0 -1 0 0..]) has maximum
    // output values close to (1,-1)
    void create(double beta, int symbolSpan, int samplesPerSymbol, bool normaliseUpsampledAmplitude = false)
    {
        int nTaps = symbolSpan * samplesPerSymbol + 1;
        int i, j;

        // check constraints
        if(!(nTaps & 1)) {
            qDebug("Raised cosine filter has to have an odd number of taps");
            nTaps++;
        }

        // make room
        m_samples.resize(nTaps);
        for(int i = 0; i < nTaps; i++)
            m_samples[i] = 0;
        m_ptr = 0;
        m_taps.resize(nTaps / 2 + 1);

        // calculate filter taps
        for(i = 0; i < nTaps / 2 + 1; i++)
        {
            double t = (i - (nTaps / 2)) / (double)samplesPerSymbol;
            double denominator = 1.0 - std::pow(2.0 * beta * t, 2.0);
            double sinc;

            if (denominator != 0.0)
            {
                if (t == 0)
                    sinc = 1.0;
                else
                    sinc = sin(M_PI*t)/(M_PI*t);
                m_taps[i] = sinc * (cos(M_PI*beta*t) / denominator) / (double)samplesPerSymbol;
            }
            else
                m_taps[i] = beta * sin(M_PI/(2.0*beta)) / (2.0*samplesPerSymbol);
        }

        // normalize
        if (!normaliseUpsampledAmplitude)
        {
            // normalize energy
            double sum = 0;
            for(i = 0; i < (int)m_taps.size() - 1; i++)
                sum += std::pow(m_taps[i], 2.0) * 2;
            sum += std::pow(m_taps[i], 2.0);
            sum = std::sqrt(sum);
            for(i = 0; i < (int)m_taps.size(); i++)
                m_taps[i] /= sum;
        }
        else
        {
            // Calculate maximum output of filter, assuming upsampled bipolar input E.g. [1 0 0 -1 0 0..]
            // This doesn't necessarily include the centre tap, so we try each offset
            double maxGain = 0.0;
            for (i = 0; i < samplesPerSymbol; i++)
            {
                double g = 0.0;
                for (j = 0; j < (int)m_taps.size() - 1; j += samplesPerSymbol)
                    g += std::fabs(2.0 * m_taps[j]);
                if ((i & 1) == 0)
                    g += std::fabs(m_taps[j]);
                if (g > maxGain)
                    maxGain = g;
            }
            // Scale up so maximum out is 1
            for(i = 0; i < (int)m_taps.size(); i++)
                m_taps[i] /= maxGain;
        }
    }

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

private:
    std::vector<Real> m_taps;
    std::vector<Type> m_samples;
    unsigned int m_ptr;
};

#endif // INCLUDE_RAISEDCOSINE_H
