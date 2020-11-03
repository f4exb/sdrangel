///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2005,2007,2012 Free Software Foundation, Inc.
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

#include <cmath>
#include <QDebug>
#include "dsp/fmpreemphasis.h"

FMPreemphasis::FMPreemphasis(int sampleRate, Real tau, Real highFreq)
{
    configure(sampleRate, tau, highFreq);
}

void FMPreemphasis::configure(int sampleRate, Real tau, Real highFreq)
{
    // Based on: https://github.com/gnuradio/gnuradio/blob/master/gr-analog/python/analog/fm_emph.py
    // Compare to freq response in https://www.mathworks.com/help/comm/ref/comm.fmbroadcastmodulator-system-object.html

    // High frequency corner at which to flatten the gain
    double fh = std::min((double)highFreq, 0.925 * sampleRate/2.0);

    // Digital corner frequencies
    double w_cl = 1.0 / tau;
    double w_ch = 2.0 * M_PI * fh;

    // Prewarped analog corner frequencies
    double w_cla = 2.0 * sampleRate * std::tan(w_cl / (2.0 * sampleRate));
    double w_cha = 2.0 * sampleRate * std::tan(w_ch / (2.0 * sampleRate));

    // Resulting digital pole, zero, and gain term from the bilinear
    // transformation of H(s) = (s + w_cla) / (s + w_cha) to
    // H(z) = b0 (1 - z1 z^-1)/(1 - p1 z^-1)
    double kl = -w_cla / (2.0 * sampleRate);
    double kh = -w_cha / (2.0 * sampleRate);
    double z1 = (1.0 + kl) / (1.0 - kl);
    double p1 = (1.0 + kh) / (1.0 - kh);
    double b0 = (1.0 - kl) / (1.0 - kh);

    // Adjust with a gain, g, so 0 dB gain at DC
    double g = std::abs(1.0 - p1) / (b0 * std::abs(1.0 - z1));

    // Caclulate IIR taps
    m_b0 = (Real)(g * b0 * 1.0);
    m_b1 = (Real)(g * b0 * -z1);
    m_a1 = (Real)-p1;
    // Zero delay line so we get reproducible results
    m_z = 0;

    qDebug() << "FMPreemphasis::configure: tau: " << tau
                << " sampleRate: " << sampleRate
                << " b0: " << m_b0
                << " b1: " << m_b1
                << " a1: " << m_a1;
}

Real FMPreemphasis::filter(const Real sampleIn)
{
    Real sampleOut;

    // See Transposed Direct form 2 - https://en.wikipedia.org/wiki/Digital_biquad_filter
    sampleOut = sampleIn * m_b0 + m_z;
    m_z = sampleIn * m_b1 + sampleOut * -m_a1;

    return sampleOut;
}
