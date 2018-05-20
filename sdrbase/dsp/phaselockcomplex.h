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
    /** Set the PSK order for the phase comparator
     * \param order 0,1: no PSK (CW), 2: BPSK, 4: QPSK, 8: 8-PSK, ... use powers of two for real cases
     */
    void setPskOrder(unsigned int order);
    /** Set sample rate information only for frequency and lock condition calculation */
    void setSampleRate(unsigned int sampleRate);
    void reset();
    /** Feed PLL with a new signa sample */
    void feed(float re, float im);
    const std::complex<float>& getComplex() const { return m_y; }
    float getReal() const { return m_yRe; }
    float getImag() const { return m_yIm; }
    bool locked() const { return m_pskOrder > 1 ? m_lockCount > 10 : m_lockCount > m_lockTime-2; }
    float getFreq() const { return m_freq; }
    float getDeltaPhi() const { return m_deltaPhi; }
    float getPhiHat() const { return m_phiHat; }

private:
    class ExpAvg
    {
    public:
        ExpAvg() : m_a0(0.999), m_a1(0.001), m_y1(0.0f)
        {}
        void setAlpha(const float& alpha)
        {
            m_a0 = alpha;
            m_a1 = 1.0 - alpha;
        }
        float feed(const float& x)
        {
            float y = m_a1*x + m_a0*m_y1;
            m_y1 = y;
            return y;
        }
    private:
        float m_a0; //!< alpha
        float m_a1; //!< 1 - alpha
        float m_y1;
    };

    /** Normalize angle in radians into the [-pi,+pi] region */
    static float normalizeAngle(float angle);

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
    float m_phiHat;
    float m_phiHatPrev;
    std::complex<float> m_y;
    std::complex<float> m_p;
    float m_yRe;
    float m_yIm;
    float m_freq;
    float m_freqPrev;
    float m_freqTest;
    int m_lockCount;
    float m_lockFreq;
    unsigned int m_pskOrder;
    int m_lockTime;
    int m_lockTimeCount;
    ExpAvg m_expAvg;
};

#endif /* SDRBASE_DSP_PHASELOCKCOMPLEX_H_ */
