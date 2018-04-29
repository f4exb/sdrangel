///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// Float half-band FIR based interpolator and decimator                        //
// This is the double buffer variant                                             //
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

#ifndef INCLUDE_INTHALFBANDFILTER_DBF_H
#define INCLUDE_INTHALFBANDFILTER_DBF_H

#include <stdint.h>
#include "dsp/dsptypes.h"
#include "dsp/hbfiltertraits.h"
#include "export.h"

template<typename AccuType, typename SampleType, uint32_t HBFilterOrder>
class SDRBASE_API IntHalfbandFilterDBF {
public:
    IntHalfbandFilterDBF();

    void myDecimate(AccuType x1, AccuType y1, AccuType *x2, AccuType *y2)
    {
        storeSample(x1, y1);
        advancePointer();

        storeSample(*x2, *y2);
        doFIR(x2, y2);
        advancePointer();
    }

    /** Optimized upsampler by 2 not calculating FIR with inserted null samples */
    void myInterpolate(qint32 *x1, qint32 *y1, qint32 *x2, qint32 *y2)
    {
        // insert sample into ring double buffer
        m_samplesDB[m_ptr][0] = *x1;
        m_samplesDB[m_ptr][1] = *y1;
        m_samplesDB[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][0] = *x1;
        m_samplesDB[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][1] = *y1;

        // advance pointer
        if (m_ptr < (HBFIRFilterTraits<HBFilterOrder>::hbOrder/2) - 1) {
            m_ptr++;
        } else {
            m_ptr = 0;
        }

        // first output sample calculated with the middle peak
        *x1 = m_samplesDB[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][0];
        *y1 = m_samplesDB[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][1];

        // second sample calculated with the filter
        doInterpolateFIR(x2, y2);
    }

protected:
    SampleType m_samplesDB[2*(HBFIRFilterTraits<HBFilterOrder>::hbOrder - 1)][2]; // double buffer technique
	int m_ptr;
	int m_size;
	int m_state;

    void storeSample(AccuType x, AccuType y)
    {
        m_samplesDB[m_ptr][0] = x;
        m_samplesDB[m_ptr][1] = y;
        m_samplesDB[m_ptr + m_size][0] = x;
        m_samplesDB[m_ptr + m_size][1] = y;
    }

    void advancePointer()
    {
        m_ptr = m_ptr + 1 < m_size ? m_ptr + 1: 0;
    }

    void doFIR(AccuType *x, AccuType *y)
    {
        int a = m_ptr + m_size; // tip pointer
        int b = m_ptr + 1; // tail pointer
        AccuType iAcc = 0;
        AccuType qAcc = 0;

        for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 4; i++)
        {
            iAcc += (m_samplesDB[a][0] + m_samplesDB[b][0]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffsF[i];
            qAcc += (m_samplesDB[a][1] + m_samplesDB[b][1]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffsF[i];
            a -= 2;
            b += 2;
        }

        iAcc += m_samplesDB[b-1][0] / 2.0;
        qAcc += m_samplesDB[b-1][1] / 2.0;

        *x = iAcc; // HB_SHIFT incorrect do not loose the gained bit
        *y = qAcc;
    }

    void doInterpolateFIR(qint32 *x, qint32 *y)
    {
        qint16 a = m_ptr;
        qint16 b = m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder / 2) - 1;

        // go through samples in buffer
        AccuType iAcc = 0;
        AccuType qAcc = 0;

        for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 4; i++)
        {
            iAcc += (m_samplesDB[a][0] + m_samplesDB[b][0]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffsF[i];
            qAcc += (m_samplesDB[a][1] + m_samplesDB[b][1]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffsF[i];
            a++;
            b--;
        }

        *x = iAcc * SDR_RX_SCALED;
        *y = qAcc * SDR_RX_SCALED;
    }
};

template<typename AccuType, typename SampleType, uint32_t HBFilterOrder>
IntHalfbandFilterDBF<AccuType, SampleType, HBFilterOrder>::IntHalfbandFilterDBF()
{
    m_size = HBFIRFilterTraits<HBFilterOrder>::hbOrder - 1;

    for (int i = 0; i < m_size; i++)
    {
        m_samplesDB[i][0] = 0;
        m_samplesDB[i][1] = 0;
    }

    m_ptr = 0;
    m_state = 0;
}

#endif // INCLUDE_INTHALFBANDFILTER_DBF_H
