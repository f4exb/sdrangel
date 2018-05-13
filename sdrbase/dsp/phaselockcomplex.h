///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// See: http://liquidsdr.org/blog/pll-howto/                                     //
// Fixes filter registers saturation                                             //
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

#ifndef SDRBASE_DSP_PHASELOCKCOMPLEX_H_
#define SDRBASE_DSP_PHASELOCKCOMPLEX_H_

#include "dsp/dsptypes.h"
#include "export.h"

/** General purpose Phase-locked loop using complex analytic signal input. */
class SDRBASE_API PhaseLockComplex
{
public:
    PhaseLockComplex();
    /** Compute loop filter parameters (active PI design)
     * \param wn   PLL bandwidth relative to Nyquist frequency
     * \param zeta PLL damping factor
     * \param K    PLL loop gain
     * */
    void computeCoefficients(Real wn, Real zeta, Real K);
    void reset();
    void feed(float re, float im);
    const std::complex<float>& getComplex() const { return m_y; }
    float getReal() const { return m_yRe; }
    float getImag() const { return m_yIm; }
    bool locked() const { return (m_deltaPhi > -0.1) && (m_deltaPhi < 0.1); }
    float getFrequency() const { return m_freq; }
    float getDeltaPhi() const { return m_deltaPhi; }
    float getPhiHat() const { return m_phiHat; }

private:
    // a0 = 1 is implied
    float m_a1;
    float m_a2;
    float m_b0;
    float m_b1;
    float m_b2;
    float m_v0;
    float m_v1;
    float m_v2;
    float m_deltaPhi;
    float m_phiHatLast;
    float m_phiHat;
    std::complex<float> m_y;
    float m_yRe;
    float m_yIm;
    float m_freq;

};



#endif /* SDRBASE_DSP_PHASELOCKCOMPLEX_H_ */
