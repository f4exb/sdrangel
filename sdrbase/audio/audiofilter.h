///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef _SDRBASE_AUDIO_AUDIOFILTER_H_
#define _SDRBASE_AUDIO_AUDIOFILTER_H_

#include "export.h"
#include "dsp/iirfilter.h"

/**
 * By default this is a 2 pole lowpass Chebyshev (recursive) filter at fc=0.075 using coefficients found in table 20-1 of
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
 * It can be generalized using the program found in tables 20-4 and 20-5 of the same document. This form is used as a
 * decimation filter and can be set with the setDecimFilters method
 */

class SDRBASE_API AudioFilter {
public:
    AudioFilter();
    ~AudioFilter();

    void useHP(bool useHP) { m_useHP = useHP; }
    bool usesHP() const { return m_useHP; }
    void setDecimFilters(int srHigh, int srLow, float fcHigh, float fcLow, float gain = 1.0f);
    float run(const float& sample);
    float runHP(const float& sample);
    float runLP(const float& sample);

private:
    void calculate2(bool highPass, double fc, float *a, float *b, float fgain); // two pole Chebyshev calculation
    void cheby(bool highPass, double fc, float pr, int np, double *a, double *b, float fgain);
    void cheby_sub(bool highPass, double fc, float pr, int np, int stage,
            double& a0, double& a1, double& a2, double& b1, double& b2);

    IIRFilter<float, 2> m_filterLP;
    IIRFilter<float, 2> m_filterHP;
    bool m_useHP;
    float m_lpva[3];
    float m_lpvb[3];
    float m_hpva[3];
    float m_hpvb[3];
    static const float m_lpa[3];
    static const float m_lpb[3];
    static const float m_hpa[3];
    static const float m_hpb[3];

};

#endif // _SDRBASE_AUDIO_AUDIOFILTER_H_
