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
class MBEAudioInterpolatorFilter
{
public:
    MBEAudioInterpolatorFilter();
    ~MBEAudioInterpolatorFilter();

    void init();
    float run(float sample);

private:
    float m_x[2];
    float m_y[2];
    static const float m_a0, m_a1, m_a2, m_b1, m_b2;
};


#endif /* SDRBASE_DSP_FILTERMBE_H_ */
