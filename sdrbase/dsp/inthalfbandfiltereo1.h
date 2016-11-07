///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
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

#ifndef SDRBASE_DSP_INTHALFBANDFILTEREO_H_
#define SDRBASE_DSP_INTHALFBANDFILTEREO_H_

#ifdef USE_SSE4_1
#include <smmintrin.h>
#endif

#include <stdint.h>
#include "dsp/dsptypes.h"
#include "dsp/hbfiltertraits.h"
#include "dsp/inthalfbandfiltereo1i.h"
#include "util/export.h"

template<uint32_t HBFilterOrder>
class SDRANGEL_API IntHalfbandFilterEO1 {
public:
    IntHalfbandFilterEO1();

    // downsample by 2, return center part of original spectrum
    bool workDecimateCenter(Sample* sample)
    {
        // insert sample into ring-buffer
        storeSample((FixReal) sample->real(), (FixReal) sample->imag());

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
                doFIR(sample);
                // advance write-pointer
                advancePointer();
                // next state
                m_state = 0;

                // tell caller we have a new sample
                return true;
        }
    }

    // upsample by 2, return center part of original spectrum - double buffer variant
    bool workInterpolateCenter(Sample* sampleIn, Sample *SampleOut)
    {
        switch(m_state)
        {
            case 0:
                // insert sample into ring-buffer
                storeSample(0, 0);
                // save result
                doFIR(SampleOut);
                // advance write-pointer
                advancePointer();
                // next state
                m_state = 1;
                // tell caller we didn't consume the sample
                return false;

            default:
                // insert sample into ring-buffer
                storeSample((FixReal) sampleIn->real(), (FixReal) sampleIn->imag());
                // save result
                doFIR(SampleOut);
                // advance write-pointer
                advancePointer();
                // next state
                m_state = 0;
                // tell caller we consumed the sample
                return true;
        }
    }

    bool workDecimateCenter(qint32 *x, qint32 *y)
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

    // downsample by 2, return lower half of original spectrum
    bool workDecimateLowerHalf(Sample* sample)
    {
        switch(m_state)
        {
            case 0:
                // insert sample into ring-buffer
                storeSample((FixReal) -sample->imag(), (FixReal) sample->real());
                // advance write-pointer
                advancePointer();
                // next state
                m_state = 1;
                // tell caller we don't have a new sample
                return false;

            case 1:
                // insert sample into ring-buffer
                storeSample((FixReal) -sample->real(), (FixReal) -sample->imag());
                // save result
                doFIR(sample);
                // advance write-pointer
                advancePointer();
                // next state
                m_state = 2;
                // tell caller we have a new sample
                return true;

            case 2:
                // insert sample into ring-buffer
                storeSample((FixReal) sample->imag(), (FixReal) -sample->real());
                // advance write-pointer
                advancePointer();
                // next state
                m_state = 3;
                // tell caller we don't have a new sample
                return false;

            default:
                // insert sample into ring-buffer
                storeSample((FixReal) sample->real(), (FixReal) sample->imag());
                // save result
                doFIR(sample);
                // advance write-pointer
                advancePointer();
                // next state
                m_state = 0;
                // tell caller we have a new sample
                return true;
        }
    }

    // upsample by 2, from lower half of original spectrum - double buffer variant
    bool workInterpolateLowerHalf(Sample* sampleIn, Sample *sampleOut)
    {
        Sample s;

        switch(m_state)
        {
        case 0:
            // insert sample into ring-buffer
            storeSample(0, 0);

            // save result
            doFIR(&s);
            sampleOut->setReal(s.imag());
            sampleOut->setImag(-s.real());

            // advance write-pointer
            advancePointer();

            // next state
            m_state = 1;

            // tell caller we didn't consume the sample
            return false;

        case 1:
            // insert sample into ring-buffer
            storeSample((FixReal) sampleIn->real(), (FixReal) sampleIn->imag());

            // save result
            doFIR(&s);
            sampleOut->setReal(-s.real());
            sampleOut->setImag(-s.imag());

            // advance write-pointer
            advancePointer();

            // next state
            m_state = 2;

            // tell caller we consumed the sample
            return true;

        case 2:
            // insert sample into ring-buffer
            storeSample(0, 0);

            // save result
            doFIR(&s);
            sampleOut->setReal(-s.imag());
            sampleOut->setImag(s.real());

            // advance write-pointer
            advancePointer();

            // next state
            m_state = 3;

            // tell caller we didn't consume the sample
            return false;

        default:
            // insert sample into ring-buffer
            storeSample((FixReal) sampleIn->real(), (FixReal) sampleIn->imag());

            // save result
            doFIR(&s);
            sampleOut->setReal(s.real());
            sampleOut->setImag(s.imag());

            // advance write-pointer
            advancePointer();

            // next state
            m_state = 0;

            // tell caller we consumed the sample
            return true;
        }
    }

    // downsample by 2, return upper half of original spectrum
    bool workDecimateUpperHalf(Sample* sample)
    {
        switch(m_state)
        {
            case 0:
                // insert sample into ring-buffer
                storeSample((FixReal) sample->imag(), (FixReal) -sample->real());
                // advance write-pointer
                advancePointer();
                // next state
                m_state = 1;
                // tell caller we don't have a new sample
                return false;

            case 1:
                // insert sample into ring-buffer
                storeSample((FixReal) -sample->real(), (FixReal) -sample->imag());
                // save result
                doFIR(sample);
                // advance write-pointer
                advancePointer();
                // next state
                m_state = 2;
                // tell caller we have a new sample
                return true;

            case 2:
                // insert sample into ring-buffer
                storeSample((FixReal) -sample->imag(), (FixReal) sample->real());
                // advance write-pointer
                advancePointer();
                // next state
                m_state = 3;
                // tell caller we don't have a new sample
                return false;

            default:
                // insert sample into ring-buffer
                storeSample((FixReal) sample->real(), (FixReal) sample->imag());
                // save result
                doFIR(sample);
                // advance write-pointer
                advancePointer();
                // next state
                m_state = 0;
                // tell caller we have a new sample
                return true;
        }
    }

    // upsample by 2, move original spectrum to upper half - double buffer variant
    bool workInterpolateUpperHalf(Sample* sampleIn, Sample *sampleOut)
    {
        Sample s;

        switch(m_state)
        {
        case 0:
            // insert sample into ring-buffer
            storeSample(0, 0);

            // save result
            doFIR(&s);
            sampleOut->setReal(-s.imag());
            sampleOut->setImag(s.real());

            // advance write-pointer
            advancePointer();

            // next state
            m_state = 1;

            // tell caller we didn't consume the sample
            return false;

        case 1:
            // insert sample into ring-buffer
            storeSample((FixReal) sampleIn->real(), (FixReal) sampleIn->imag());

            // save result
            doFIR(&s);
            sampleOut->setReal(-s.real());
            sampleOut->setImag(-s.imag());

            // advance write-pointer
            advancePointer();

            // next state
            m_state = 2;

            // tell caller we consumed the sample
            return true;

        case 2:
            // insert sample into ring-buffer
            storeSample(0, 0);

            // save result
            doFIR(&s);
            sampleOut->setReal(s.imag());
            sampleOut->setImag(-s.real());

            // advance write-pointer
            advancePointer();

            // next state
            m_state = 3;

            // tell caller we didn't consume the sample
            return false;

        default:
            // insert sample into ring-buffer
            storeSample((FixReal) sampleIn->real(), (FixReal) sampleIn->imag());

            // save result
            doFIR(&s);
            sampleOut->setReal(s.real());
            sampleOut->setImag(s.imag());

            // advance write-pointer
            advancePointer();

            // next state
            m_state = 0;

            // tell caller we consumed the sample
            return true;
        }
    }

    void myDecimate(const Sample* sample1, Sample* sample2)
    {
        storeSample((FixReal) sample1->real(), (FixReal) sample1->imag());
        advancePointer();

        storeSample((FixReal) sample2->real(), (FixReal) sample2->imag());
        doFIR(sample2);
        advancePointer();
    }

    void myDecimate(qint32 x1, qint32 y1, qint32 *x2, qint32 *y2)
    {
        storeSample(x1, y1);
        advancePointer();

        storeSample(*x2, *y2);
        doFIR(x2, y2);
        advancePointer();
    }

protected:
    qint32 m_even[2][HBFIRFilterTraits<HBFilterOrder>::hbOrder]; // double buffer technique
    qint32 m_odd[2][HBFIRFilterTraits<HBFilterOrder>::hbOrder]; // double buffer technique

    int m_ptr;
    int m_size;
    int m_state;

    void storeSample(const FixReal& sampleI, const FixReal& sampleQ)
    {
        if ((m_ptr % 2) == 0)
        {
            m_even[0][m_ptr/2] = sampleI;
            m_even[1][m_ptr/2] = sampleQ;
            m_even[0][m_ptr/2 + m_size] = sampleI;
            m_even[1][m_ptr/2 + m_size] = sampleQ;
        }
        else
        {
            m_odd[0][m_ptr/2] = sampleI;
            m_odd[1][m_ptr/2] = sampleQ;
            m_odd[0][m_ptr/2 + m_size] = sampleI;
            m_odd[1][m_ptr/2 + m_size] = sampleQ;
        }
    }

    void storeSample(qint32 x, qint32 y)
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
        m_ptr = (m_ptr + 1) % (2*m_size);
    }

    void doFIR(Sample* sample)
    {
        int a = m_ptr/2 + m_size; // tip pointer
        int b = m_ptr/2 + 1; // tail pointer

        qint32 iAcc = 0;
        qint32 qAcc = 0;

#ifdef USE_SSE4_1
        IntHalfbandFilterEO1Intrisics<HBFilterOrder>::work(
                m_ptr,
                m_even,
                m_odd,
                iAcc,
                qAcc
        );
#else
        for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 4; i++)
        {
            if ((m_ptr % 2) == 0)
            {
                iAcc += (m_even[0][a] + m_even[0][b]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
                qAcc += (m_even[1][a] + m_even[1][b]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
            }
            else
            {
                iAcc += (m_odd[0][a] + m_odd[0][b]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
                qAcc += (m_odd[1][a] + m_odd[1][b]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
            }

            a -= 1;
            b += 1;
        }
#endif

        if ((m_ptr % 2) == 0)
        {
            iAcc += ((qint32)m_odd[0][m_ptr/2 + m_size/2]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
            qAcc += ((qint32)m_odd[1][m_ptr/2 + m_size/2]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
        }
        else
        {
            iAcc += ((qint32)m_even[0][m_ptr/2 + m_size/2 + 1]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
            qAcc += ((qint32)m_even[1][m_ptr/2 + m_size/2 + 1]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
        }

        sample->setReal(iAcc >> HBFIRFilterTraits<HBFilterOrder>::hbShift -1);
        sample->setImag(qAcc >> HBFIRFilterTraits<HBFilterOrder>::hbShift -1);
    }

    void doFIR(qint32 *x, qint32 *y)
    {
        int a = m_ptr/2 + m_size; // tip pointer
        int b = m_ptr/2 + 1; // tail pointer

        qint32 iAcc = 0;
        qint32 qAcc = 0;

#ifdef USE_SSE4_1
        IntHalfbandFilterEO1Intrisics<HBFilterOrder>::work(
                m_ptr,
                m_even,
                m_odd,
                iAcc,
                qAcc
        );
#else
        for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 4; i++)
        {
            if ((m_ptr % 2) == 0)
            {
                iAcc += (m_even[0][a] + m_even[0][b]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
                qAcc += (m_even[1][a] + m_even[1][b]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
            }
            else
            {
                iAcc += (m_odd[0][a] + m_odd[0][b]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
                qAcc += (m_odd[1][a] + m_odd[1][b]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
            }

            a -= 1;
            b += 1;
        }
#endif
        if ((m_ptr % 2) == 0)
        {
            iAcc += ((qint32)m_odd[0][m_ptr/2 + m_size/2]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
            qAcc += ((qint32)m_odd[1][m_ptr/2 + m_size/2]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
        }
        else
        {
            iAcc += ((qint32)m_even[0][m_ptr/2 + m_size/2 + 1]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
            qAcc += ((qint32)m_even[1][m_ptr/2 + m_size/2 + 1]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
        }

        *x = iAcc >> (HBFIRFilterTraits<HBFilterOrder>::hbShift -1); // HB_SHIFT incorrect do not loose the gained bit
        *y = qAcc >> (HBFIRFilterTraits<HBFilterOrder>::hbShift -1);
    }
};

template<uint32_t HBFilterOrder>
IntHalfbandFilterEO1<HBFilterOrder>::IntHalfbandFilterEO1()
{
    m_size = HBFIRFilterTraits<HBFilterOrder>::hbOrder/2;

    for (int i = 0; i < 2*m_size; i++)
    {
        m_even[0][i] = 0;
        m_even[1][i] = 0;
        m_odd[0][i] = 0;
        m_odd[1][i] = 0;
    }

    m_ptr = 0;
    m_state = 0;
}

#endif /* SDRBASE_DSP_INTHALFBANDFILTEREO_H_ */
