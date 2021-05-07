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

#ifndef INCLUDE_GAUSSIAN_H
#define INCLUDE_GAUSSIAN_H

#include <math.h>
#include "dsp/dsptypes.h"

// Standard values for bt
#define GAUSSIAN_BT_BLUETOOTH   0.5
#define GAUSSIAN_BT_GSM         0.3
#define GAUSSIAN_BT_CCSDS       0.25
#define GAUSSIAN_BT_802_15_4    0.5
#define GAUSSIAN_BT_AIS         0.5

// Gaussian low-pass filter for pulse shaping
// https://onlinelibrary.wiley.com/doi/pdf/10.1002/9780470041956.app2
// Unlike raisedcosine.h, this should be feed NRZ values rather than impulse stream, as described here:
// https://www.mathworks.com/matlabcentral/answers/107231-why-does-the-pulse-shape-generated-by-gaussdesign-differ-from-that-used-in-the-comm-gmskmodulator-ob
template <class Type> class Gaussian {
public:
    Gaussian() : m_ptr(0) { }

    // bt - 3dB bandwidth symbol time product
    // symbolSpan - number of symbols over which the filter is spread
    // samplesPerSymbol - number of samples per symbol
    void create(double bt, int symbolSpan, int samplesPerSymbol)
    {
        int nTaps = symbolSpan * samplesPerSymbol + 1;
        int i;

        // check constraints
        if(!(nTaps & 1)) {
            qDebug("Gaussian filter has to have an odd number of taps");
            nTaps++;
        }

        // make room
        m_samples.resize(nTaps);
        for(int i = 0; i < nTaps; i++)
            m_samples[i] = 0;
        m_ptr = 0;
        m_taps.resize(nTaps / 2 + 1);

        // See eq B.2 - this is alpha over Ts
        double alpha_t = std::sqrt(std::log(2.0) / 2.0) / (bt);
        double sqrt_pi_alpha_t = std::sqrt(M_PI) / alpha_t;

        // calculate filter taps
        for(i = 0; i < nTaps / 2 + 1; i++)
        {
            double t = (i - (nTaps / 2)) / (double)samplesPerSymbol;

            // See eq B.5
            m_taps[i] = sqrt_pi_alpha_t * std::exp(-std::pow(t * M_PI / alpha_t, 2.0));
        }

        // normalize
        double sum = 0;
        for(i = 0; i < (int)m_taps.size() - 1; i++)
            sum += m_taps[i] * 2;
        sum += m_taps[i];
        for(i = 0; i < (int)m_taps.size(); i++)
            m_taps[i] /= sum;
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
/*
    void printTaps()
    {
        for (int i = 0; i < m_taps.size(); i++)
            printf("%.4f ", m_taps[i]);
        printf("\n");
    }
*/
private:
    std::vector<Real> m_taps;
    std::vector<Type> m_samples;
    unsigned int m_ptr;
};

#endif // INCLUDE_GAUSSIAN_H
