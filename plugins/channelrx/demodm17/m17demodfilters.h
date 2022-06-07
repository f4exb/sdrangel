///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB.                                  //
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

#ifndef INCLUDE_M17DEMODFILTERS_H
#define INCLUDE_M17DEMODFILTERS_H

#define NZEROS 60
#define NXZEROS 134

#include "dsp/iirfilter.h"
#include "export.h"

/**
 * This is a 2 pole lowpass Chebyshev (recursive) filter at fc=0.075 using coefficients found in table 20-1 of
 * http://www.analog.com/media/en/technical-documentation/dsp-book/dsp_book_Ch20.pdf
 *
 * At the interpolated sampling frequency of 48 kHz the -3 dB corner is at 48 * .075 = 3.6 kHz which is perfect for voice
 *
 * a0= 3.869430E-02
 * a1= 7.738860E-02 b1= 1.392667E+00
 * a2= 3.869430E-02 b2= -5.474446E-01
 *
 * given x[n] is the new input sample and y[n] the returned output sample:
 *
 * y[n] = a0*x[n] + a1*x[n] + a2*x[n] + b1*y[n-1] + b2*y[n-2]
 *
 * This one works directly with floats
 *
 */

class M17DemodAudioInterpolatorFilter
{
public:
    M17DemodAudioInterpolatorFilter();
    ~M17DemodAudioInterpolatorFilter();

    void useHP(bool useHP) { m_useHP = useHP; }
    bool usesHP() const { return m_useHP; }
    float run(const float& sample);
    float runHP(const float& sample);
    float runLP(const float& sample);

private:
    IIRFilter<float, 2> m_filterLP;
    IIRFilter<float, 2> m_filterHP;
    bool m_useHP;
    static const float m_lpa[3];
    static const float m_lpb[3];
    static const float m_hpa[3];
    static const float m_hpb[3];
};

#endif
