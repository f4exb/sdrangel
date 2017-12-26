///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_DSP_FILTERMBE_H_
#define SDRBASE_DSP_FILTERMBE_H_

/**
 * Uses the generic IIR filter internally
 *
 * Low pass / High pass:
 *
 * This is a 2 pole Chebyshev (recursive) filter using coefficients found in table 20-1 (low pass)
 * or table 20-2 (high pass) of http://www.analog.com/media/en/technical-documentation/dsp-book/dsp_book_Ch20.pdf
 *
 * For low pass fc = 0.075
 * For high oass fc = 0.01
 *
 * Convention taken here exchanges A and B coefficients as shown in this image:
 * https://cdn.mikroe.com/ebooks/img/8/2016/02/digital-filter-design-chapter-03-image-2-9.gif
 * So A applies to Y and B to X
 *
 * At the interpolated sampling frequency of 48 kHz the -3 dB corner is at 48 * .075 = 3.6 kHz which is perfect for voice
 * The high pass has a 3 dB corner of 48 * 0.01 = 0.48 kHz
 *
 * Low pass:
 *
 * b0 = 3.869430E-02 (a0 = 1.0)
 * b1 = 7.738860E-02 a1 =  1.392667E+00
 * b2 = 3.869430E-02 a2 = -5.474446E-01
 *
 * High pass:
 *
 * b0 =  9.567529E-01 (a0 = 1.0)
 * b1 = -1.913506E+00  a1 =  1.911437E+00
 * b2 =  9.567529E-01  a2 = -9.155749E-01
 *
 * given x[n] is the new input sample and y[n] the returned output sample:
 *
 * y[n] = b0*x[n] + b1*x[n] + b2*x[n] + a1*y[n-1] + a2*y[n-2]
 *
 * This one works directly with floats
 *
 *
 */

#include "iirfilter.h"

class MBEAudioInterpolatorFilter
{
public:
    MBEAudioInterpolatorFilter();
    ~MBEAudioInterpolatorFilter();

    void useHP(bool useHP) { m_useHP = useHP; }
    float run(const float& sample);

private:
    IIRFilter<float, 2> m_filterLP;
    IIRFilter<float, 2> m_filterHP;
    bool m_useHP;
    static const float m_lpa[3], m_lpb[3]; // low pass coefficients
    static const float m_hpa[3], m_hpb[3]; // band pass coefficients
};


#endif /* SDRBASE_DSP_FILTERMBE_H_ */
