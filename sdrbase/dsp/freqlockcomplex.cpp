///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// See: http://liquidsdr.org/blog/pll-howto/                                     //
// Fixed filter registers saturation                                             //
// Added order for PSK locking. This brilliant idea actually comes from this     //
// post: https://www.dsprelated.com/showthread/comp.dsp/36356-1.php              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "freqlockcomplex.h"
#include <math.h>

FreqLockComplex::FreqLockComplex() :
    m_a0(0.998),
    m_a1(0.002),
    m_y(1.0, 0.0),
    m_yRe(1.0),
    m_yIm(0.0),
    m_freq(0.0),
    m_phi(0.0),
    m_phiX0(0.0),
    m_phiX1(0.0),
    m_y1(0.0f)
{
}

FreqLockComplex::~FreqLockComplex()
{
}

void FreqLockComplex::reset()
{
    m_y.real(1.0);
    m_y.imag(0.0);
    m_yRe = 1.0f;
    m_yIm = 0.0f;
    m_freq = 0.0f;
    m_phi = 0.0f;
    m_phiX0 = 0.0f;
    m_phiX1 = 0.0f;
    m_y1 = 0.0f;
}

void FreqLockComplex::setSampleRate(unsigned int sampleRate)
{
    m_a1 = 10.0f / sampleRate; // 1 - alpha
    m_a0 = 1.0f - m_a1; // alpha
    reset();
}

void FreqLockComplex::feed(float re, float im)
{
    m_yRe = cos(m_phi);
    m_yIm = sin(m_phi);
    m_y.real(m_yRe);
    m_y.imag(m_yIm);
    std::complex<float> x(re, im);
    m_phiX0 = std::arg(x);

    float eF = normalizeAngle(m_phiX0 - m_phiX1);
    float fHat = m_a1*eF + m_a0*m_y1;
    m_y1 = fHat;

    m_freq = fHat; // correct instantaneous frequency
    m_phi += fHat; // advance phase with instantaneous frequency
    m_phiX1 = m_phiX0;
}

float FreqLockComplex::normalizeAngle(float angle)
{
    while (angle <= -M_PI) {
        angle += 2.0*M_PI;
    }
    while (angle > M_PI) {
        angle -= 2.0*M_PI;
    }
    return angle;
}

