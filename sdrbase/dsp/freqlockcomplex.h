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
#include "export.h"

/** General purpose Phase-locked loop using complex analytic signal input. */
class SDRBASE_API FreqLockComplex
{
public:
    FreqLockComplex();
    ~FreqLockComplex();

    void reset();
    void setSampleRate(unsigned int sampleRate);
    /** Feed PLL with a new signa sample */
    void feed(float re, float im);
    const std::complex<float>& getComplex() const { return m_y; }
    float getReal() const { return m_yRe; }
    float getImag() const { return m_yIm; }

private:
    /** Normalize angle in radians into the [-pi,+pi] region */
    static float normalizeAngle(float angle);

    float m_a0;
    float m_a1;
    std::complex<float> m_y;
    float m_yRe;
    float m_yIm;
    float m_freq;
    float m_phi;
    float m_phiX0;
    float m_phiX1;
    float m_y1;
};


#endif /* SDRBASE_DSP_FREQLOCKCOMPLEX_H_ */
