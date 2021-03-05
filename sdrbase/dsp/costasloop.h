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

#ifndef SDRBASE_DSP_COSTASLOOP_H_
#define SDRBASE_DSP_COSTASLOOP_H_

#include <QDebug>

#include "dsp/dsptypes.h"
#include "export.h"

/** Costas Loop for phase and frequency tracking. */
class SDRBASE_API CostasLoop
{
public:
    CostasLoop(float loopBW, unsigned int pskOrder);
    ~CostasLoop();

    void computeCoefficients(float loopBW);
    void setPskOrder(unsigned int pskOrder) { m_pskOrder = pskOrder; }
    void reset();
    void setSampleRate(unsigned int sampleRate);
    void feed(float re, float im);
    const std::complex<float>& getComplex() const { return m_y; }
    float getReal() const { return m_y.real(); }
    float getImag() const { return m_y.imag(); }
    float getFreq() const { return m_freq; }
    float getPhiHat() const { return m_phase; }

private:

    std::complex<float> m_y;
    float m_phase;
    float m_freq;
    float m_error;
    float m_maxFreq;
    float m_minFreq;
    float m_alpha;
    float m_beta;
    unsigned int m_pskOrder;

    void advanceLoop(float error)
    {
        m_freq = m_freq + m_beta * error;
        m_phase = m_phase + m_freq + m_alpha * error;
    }

    void phaseWrap()
    {
        const float two_pi = (float)(2.0 * M_PI);

        while (m_phase > two_pi)
            m_phase -= two_pi;
        while (m_phase < -two_pi)
            m_phase += two_pi;
    }

    void frequencyLimit()
    {
        if (m_freq > m_maxFreq)
            m_freq = m_maxFreq;
        else if (m_freq < m_minFreq)
            m_freq = m_minFreq;
    }

    void setMaxFreq(float freq)
    {
        m_maxFreq = freq;
    }

    void setMinFreq(float freq)
    {
        m_minFreq = freq;
    }

    float phaseDetector2(std::complex<float> sample) const // for BPSK
    {
        return (sample.real() * sample.imag());
    }

    float phaseDetector4(std::complex<float> sample) const // for QPSK
    {
        return ((sample.real() > 0.0f ? 1.0f : -1.0f) * sample.imag() -
                (sample.imag() > 0.0f ? 1.0f : -1.0f) * sample.real());
    };

    float phaseDetector8(std::complex<float> sample) const // for 8PSK
    {
        const float K = (sqrtf(2.0) - 1);
        if (fabsf(sample.real()) >= fabsf(sample.imag()))
        {
            return ((sample.real() > 0.0f ? 1.0f : -1.0f) * sample.imag() -
                    (sample.imag() > 0.0f ? 1.0f : -1.0f) * sample.real() * K);
        }
        else
        {
            return ((sample.real() > 0.0f ? 1.0f : -1.0f) * sample.imag() * K -
                    (sample.imag() > 0.0f ? 1.0f : -1.0f) * sample.real());
        }
    };

};

#endif /* SDRBASE_DSP_COSTASLOOP_H_ */
