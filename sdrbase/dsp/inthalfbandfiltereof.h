///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// Integer half-band FIR based interpolator and decimator                        //
// This is the even/odd double buffer variant. Really useful only when SIMD is   //
// used                                                                          //
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

#ifndef SDRBASE_DSP_INTHALFBANDFILTEREOF_H_
#define SDRBASE_DSP_INTHALFBANDFILTEREOF_H_

#include <stdint.h>
#include <cstdlib>
#include "dsp/dsptypes.h"
#include "dsp/hbfiltertraits.h"
#include "export.h"

template<uint32_t HBFilterOrder>
class SDRBASE_API IntHalfbandFilterEOF {
public:
    IntHalfbandFilterEOF();

    bool workDecimateCenter(float *x, float *y)
    {
        // insert sample into ring-buffer
        storeSample(*x, *y);

        switch(m_state)
        {
            case 0:
                // advance write-pointer
                advancePointer();
                // next state
                m_state = 1;
                // tell caller we don't have a new sample
                return false;

            default:
                // save result
                doFIR(x, y);
                // advance write-pointer
                advancePointer();
                // next state
                m_state = 0;
                // tell caller we have a new sample
                return true;
        }
    }

    void myDecimate(float x1, float y1, float *x2, float *y2)
    {
        storeSample(x1, y1);
        advancePointer();

        storeSample(*x2, *y2);
        doFIR(x2, y2);
        advancePointer();
    }

    /** Simple zero stuffing and filter */
    void myInterpolateZeroStuffing(float *x1, float *y1, float *x2, float *y2)
    {
        storeSample(*x1, *y1);
        doFIR(x1, y1);
        advancePointer();

        storeSample(0, 0);
        doFIR(x2, y2);
        advancePointer();
    }

    /** Optimized upsampler by 2 not calculating FIR with inserted null samples */
    void myInterpolate(float *x1, float *y1, float *x2, float *y2)
    {
        // insert sample into ring double buffer
        m_samples[m_ptr][0] = *x1;
        m_samples[m_ptr][1] = *y1;
        m_samples[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][0] = *x1;
        m_samples[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][1] = *y1;

        // advance pointer
        if (m_ptr < (HBFIRFilterTraits<HBFilterOrder>::hbOrder/2) - 1) {
            m_ptr++;
        } else {
            m_ptr = 0;
        }

        // first output sample calculated with the middle peak
        *x1 = m_samples[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][0];
        *y1 = m_samples[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][1];

        // second sample calculated with the filter
        doInterpolateFIR(x2, y2);
    }

protected:
    float m_even[2][HBFIRFilterTraits<HBFilterOrder>::hbOrder];    // double buffer technique
    float m_odd[2][HBFIRFilterTraits<HBFilterOrder>::hbOrder];     // double buffer technique
    float m_samples[HBFIRFilterTraits<HBFilterOrder>::hbOrder][2]; // double buffer technique

    int m_ptr;
    int m_size;
    int m_state;

    void storeSample(float x, float y)
    {
        if ((m_ptr % 2) == 0)
        {
            m_even[0][m_ptr/2] = x;
            m_even[1][m_ptr/2] = y;
            m_even[0][m_ptr/2 + m_size] = x;
            m_even[1][m_ptr/2 + m_size] = y;
        }
        else
        {
            m_odd[0][m_ptr/2] = x;
            m_odd[1][m_ptr/2] = y;
            m_odd[0][m_ptr/2 + m_size] = x;
            m_odd[1][m_ptr/2 + m_size] = y;
        }
    }

    void advancePointer()
    {
        m_ptr = m_ptr + 1 < 2*m_size ? m_ptr + 1: 0;
    }

    void doFIR(float *x, float *y)
    {
        float iAcc = 0;
        float qAcc = 0;

//#if defined(USE_SSE4_1) && !defined(NO_DSP_SIMD)
//        IntHalfbandFilterEO1Intrisics<HBFilterOrder>::work(
//                m_ptr,
//                m_even,
//                m_odd,
//                iAcc,
//                qAcc
//        );
//#else
        int a = m_ptr/2 + m_size; // tip pointer
        int b = m_ptr/2 + 1; // tail pointer

        for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 4; i++)
        {
            if ((m_ptr % 2) == 0)
            {
                iAcc += (m_even[0][a] + m_even[0][b]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffsF[i];
                qAcc += (m_even[1][a] + m_even[1][b]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffsF[i];
            }
            else
            {
                iAcc += (m_odd[0][a] + m_odd[0][b]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffsF[i];
                qAcc += (m_odd[1][a] + m_odd[1][b]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffsF[i];
            }

            a -= 1;
            b += 1;
        }
//#endif
        if ((m_ptr % 2) == 0)
        {
            iAcc += m_odd[0][m_ptr/2 + m_size/2] * 0.5f;
            qAcc += m_odd[1][m_ptr/2 + m_size/2] * 0.5f;
        }
        else
        {
            iAcc += m_even[0][m_ptr/2 + m_size/2 + 1] * 0.5f;
            qAcc += m_even[1][m_ptr/2 + m_size/2 + 1] * 0.5f;
        }

        *x = iAcc; // HB_SHIFT incorrect do not loose the gained bit
        *y = qAcc;
    }

    void doInterpolateFIR(float *x, float *y)
    {
        qint32 iAcc = 0;
        qint32 qAcc = 0;

        qint16 a = m_ptr;
        qint16 b = m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder / 2) - 1;

        // go through samples in buffer
        for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 4; i++)
        {
            iAcc += (m_samples[a][0] + m_samples[b][0]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffsF[i];
            qAcc += (m_samples[a][1] + m_samples[b][1]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffsF[i];
            a++;
            b--;
        }

        *x = iAcc * SDR_RX_SCALED;
        *y = qAcc * SDR_RX_SCALED;
    }
};

template<uint32_t HBFilterOrder>
IntHalfbandFilterEOF<HBFilterOrder>::IntHalfbandFilterEOF()
{
    m_size = HBFIRFilterTraits<HBFilterOrder>::hbOrder/2;

    for (int i = 0; i < 2*m_size; i++)
    {
        m_even[0][i] = 0.0f;
        m_even[1][i] = 0.0f;
        m_odd[0][i] = 0.0f;
        m_odd[1][i] = 0.0f;
        m_samples[i][0] = 0.0f;
        m_samples[i][1] = 0.0f;
    }

    m_ptr = 0;
    m_state = 0;
}

#endif /* SDRBASE_DSP_INTHALFBANDFILTEREOF_H_ */
