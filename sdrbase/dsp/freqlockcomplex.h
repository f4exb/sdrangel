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

#ifndef SDRBASE_DSP_FREQLOCKCOMPLEX_H_
#define SDRBASE_DSP_FREQLOCKCOMPLEX_H_

#include "dsp/dsptypes.h"
#include "iirfilter.h"
#include "export.h"

/** General purpose Phase-locked loop using complex analytic signal input. */
class SDRBASE_API FreqLockComplex
{
public:
    FreqLockComplex();
    ~FreqLockComplex();

    void reset();
    void computeCoefficients(float wn);
    /** Feed PLL with a new signa sample */
    void feed(float re, float im);

private:
    float m_a0;
    float m_a1;
    float m_a2;
    float m_b0;
    float m_b1;
    float m_b2;
    float m_v0;
    float m_v1;
    float m_v2;
    std::complex<float> m_y;
    std::complex<float> m_prod;
    float m_yRe;
    float m_yIm;
    float m_freq;
    float m_phi;
    IIRFilter<float, 2> *m_iir;
};


#endif /* SDRBASE_DSP_FREQLOCKCOMPLEX_H_ */
