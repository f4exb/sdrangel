///////////////////////////////////////////////////////////////////////////////////
// Copyright 2006-2021 Free Software Foundation, Inc.                            //
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
//                                                                               //
// Based on the Costas Loop from GNU Radio                                       //
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

#include "costasloop.h"
#include <cmath>

// Loop bandwidth supposedly ~ 2pi/100 rads/sample
// pskOrder 2, 4 or 8
CostasLoop::CostasLoop(float loopBW, unsigned int pskOrder) :
    m_maxFreq(1.0f),
    m_minFreq(-1.0f),
    m_pskOrder(pskOrder)
{
    computeCoefficients(loopBW);
    reset();
}

CostasLoop::~CostasLoop()
{
}

void CostasLoop::reset()
{
    m_y.real(1.0f);
    m_y.imag(0.0f);
    m_freq = 0.0f;
    m_phase = 0.0f;
    m_freq = 0.0f;
    m_error = 0.0f;
}

// 2nd order loop with critical damping
void CostasLoop::computeCoefficients(float loopBW)
{
    float damping = sqrtf(2.0f) / 2.0f;
    float denom = (1.0 + 2.0 * damping * loopBW + loopBW * loopBW);
    m_alpha = (4 * damping * loopBW) / denom;
    m_beta = (4 * loopBW * loopBW) / denom;
}

void CostasLoop::setSampleRate(unsigned int sampleRate)
{
    (void) sampleRate;

    reset();
}

static float branchlessClip(float x, float clip)
{
    return 0.5f * (std::abs(x + clip) - std::abs(x - clip));
}

// Don't use built-in complex.h multiply to avoid NaN/INF checking
static void fastComplexMultiply(std::complex<float> &out, const std::complex<float> cc1, const std::complex<float> cc2)
{
    float o_r, o_i;

    o_r = (cc1.real() * cc2.real()) - (cc1.imag() * cc2.imag());
    o_i = (cc1.real() * cc2.imag()) + (cc1.imag() * cc2.real());

    out.real(o_r);
    out.imag(o_i);
}

void CostasLoop::feed(float re, float im)
{
    std::complex<float> nco(::cosf(-m_phase), ::sinf(-m_phase));

    std::complex<float> in, out;
    in.real(re);
    in.imag(im);
    fastComplexMultiply(out, in, nco);

    switch (m_pskOrder)
    {
    case 2:
        m_error = phaseDetector2(out);
        break;
    case 4:
        m_error = phaseDetector4(out);
        break;
    case 8:
        m_error = phaseDetector8(out);
        break;
    }
    m_error = branchlessClip(m_error, 1.0f);

    advanceLoop(m_error);
    phaseWrap();
    frequencyLimit();

    m_y.real(-nco.real());
    m_y.imag(nco.imag());
}
