///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>          //
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

#ifndef INCLUDE_PACKETDEMODTONECORRELATOR_H
#define INCLUDE_PACKETDEMODTONECORRELATOR_H

// A sliding correlation of the discriminator output against the two AFSK tones.
//
// Two consumers want this and used to carry a copy each: the discriminator demodulator
// slices the filtered magnitudes to recover bits, and - in MLSE mode, where that path is
// idle - the symbol rate estimator reads the same correlations to find tone transitions.
// The two copies were identical up to brace style, which is the kind of duplication that
// stays correct right up until one of them is changed.

#include <complex>
#include <vector>
#include <algorithm>
#include <cmath>

#include "dsp/dsptypes.h"

class PacketDemodToneCorrelator
{
public:
    // length is one symbol period in samples; the tones are the AFSK pair in Hz
    void create(int length, int sampleRate, double f0Hz, double f1Hz)
    {
        m_length = std::max(1, length);
        m_f0.assign(m_length, Complex(0.0f, 0.0f));
        m_f1.assign(m_length, Complex(0.0f, 0.0f));
        m_buf.assign(m_length, Complex(0.0f, 0.0f));

        Real p0 = 0.0f;
        Real p1 = 0.0f;

        for (int i = 0; i < m_length; i++)
        {
            m_f0[i] = Complex(cos(p0), sin(p0));
            m_f1[i] = Complex(cos(p1), sin(p1));
            p0 += 2.0f * (Real) M_PI * (Real) f0Hz / sampleRate;
            p1 += 2.0f * (Real) M_PI * (Real) f1Hz / sampleRate;
        }

        reset();
    }

    void reset()
    {
        m_index = 0;
        m_count = 0;
        std::fill(m_buf.begin(), m_buf.end(), Complex(0.0f, 0.0f));
    }

    bool valid() const { return !m_buf.empty(); }

    // Push one discriminator sample. Returns true once a full symbol period is buffered,
    // in which case corrF0 and corrF1 hold this sample's correlations.
    bool push(Real sample, Complex& corrF0, Complex& corrF1)
    {
        if (m_buf.empty()) {
            return false;
        }

        m_buf[m_index] = sample;
        bool ready = (m_count >= m_length);

        if (ready)
        {
            corrF0 = Complex(0.0f, 0.0f);
            corrF1 = Complex(0.0f, 0.0f);

            for (int i = 0; i < m_length; i++)
            {
                int j = m_index - i;

                if (j < 0) {
                    j += m_length;
                }

                corrF0 += m_f0[i] * m_buf[j];
                corrF1 += m_f1[i] * m_buf[j];
            }

            m_count--;      // avoid overflow in the increment below
        }

        m_index = (m_index + 1) % m_length;
        m_count++;

        return ready;
    }

private:
    std::vector<Complex> m_f0;
    std::vector<Complex> m_f1;
    std::vector<Complex> m_buf;
    int m_length = 0;
    int m_index = 0;
    int m_count = 0;
};

#endif // INCLUDE_PACKETDEMODTONECORRELATOR_H
