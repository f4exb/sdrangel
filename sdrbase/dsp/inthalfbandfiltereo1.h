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

#include <stdint.h>
#include <cstdlib>
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
    bool workInterpolateCenterZeroStuffing(Sample* sampleIn, Sample *SampleOut)
    {
        switch(m_state)
        {
            case 0:
                // insert sample into ring-buffer
                storeSample((FixReal) 0, (FixReal) 0);
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

    /** Optimized upsampler by 2 not calculating FIR with inserted null samples */
    bool workInterpolateCenter(Sample* sampleIn, Sample *SampleOut)
    {
        switch(m_state)
        {
        case 0:
            // return the middle peak
            SampleOut->setReal(m_samples[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][0]);
            SampleOut->setImag(m_samples[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][1]);
            m_state = 1;  // next state
            return false; // tell caller we didn't consume the sample

        default:
            // calculate with non null samples
            doInterpolateFIR(SampleOut);

            // insert sample into ring double buffer
            m_samples[m_ptr][0] = sampleIn->real();
            m_samples[m_ptr][1] = sampleIn->imag();
            m_samples[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][0] = sampleIn->real();
            m_samples[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][1] = sampleIn->imag();

            // advance pointer
            if (m_ptr < (HBFIRFilterTraits<HBFilterOrder>::hbOrder/2) - 1) {
                m_ptr++;
            } else {
                m_ptr = 0;
            }

            m_state = 0; // next state
            return true; // tell caller we consumed the sample
        }
    }

    bool workDecimateCenter(int32_t *x, int32_t *y)
    {
        // insert sample into ring-buffer
        storeSample32(*x, *y);

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
    bool workInterpolateLowerHalfZeroStuffing(Sample* sampleIn, Sample *sampleOut)
    {
        Sample s;

        switch(m_state)
        {
        case 0:
            // insert sample into ring-buffer
            storeSample((FixReal) 0, (FixReal) 0);

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
            storeSample((FixReal) 0, (FixReal) 0);

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

    /** Optimized upsampler by 2 not calculating FIR with inserted null samples */
    bool workInterpolateLowerHalf(Sample* sampleIn, Sample *sampleOut)
    {
        Sample s;

        switch(m_state)
        {
        case 0:
            // return the middle peak
            sampleOut->setReal(m_samples[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][1]);  // imag
            sampleOut->setImag(-m_samples[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][0]); // - real
            m_state = 1;  // next state
            return false; // tell caller we didn't consume the sample

        case 1:
            // calculate with non null samples
            doInterpolateFIR(&s);
            sampleOut->setReal(-s.real());
            sampleOut->setImag(-s.imag());

            // insert sample into ring double buffer
            m_samples[m_ptr][0] = sampleIn->real();
            m_samples[m_ptr][1] = sampleIn->imag();
            m_samples[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][0] = sampleIn->real();
            m_samples[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][1] = sampleIn->imag();

            // advance pointer
            if (m_ptr < (HBFIRFilterTraits<HBFilterOrder>::hbOrder/2) - 1) {
                m_ptr++;
            } else {
                m_ptr = 0;
            }

            m_state = 2; // next state
            return true; // tell caller we consumed the sample

        case 2:
            // return the middle peak
            sampleOut->setReal(-m_samples[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][1]);  // - imag
            sampleOut->setImag(m_samples[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][0]); // real
            m_state = 3;  // next state
            return false; // tell caller we didn't consume the sample

        default:
            // calculate with non null samples
            doInterpolateFIR(&s);
            sampleOut->setReal(s.real());
            sampleOut->setImag(s.imag());

            // insert sample into ring double buffer
            m_samples[m_ptr][0] = sampleIn->real();
            m_samples[m_ptr][1] = sampleIn->imag();
            m_samples[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][0] = sampleIn->real();
            m_samples[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][1] = sampleIn->imag();

            // advance pointer
            if (m_ptr < (HBFIRFilterTraits<HBFilterOrder>::hbOrder/2) - 1) {
                m_ptr++;
            } else {
                m_ptr = 0;
            }

            m_state = 0; // next state
            return true; // tell caller we consumed the sample
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
    bool workInterpolateUpperHalfZeroStuffing(Sample* sampleIn, Sample *sampleOut)
    {
        Sample s;

        switch(m_state)
        {
        case 0:
            // insert sample into ring-buffer
            storeSample((FixReal) 0, (FixReal) 0);

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
            storeSample((FixReal) 0, (FixReal) 0);

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

    /** Optimized upsampler by 2 not calculating FIR with inserted null samples */
    bool workInterpolateUpperHalf(Sample* sampleIn, Sample *sampleOut)
    {
        Sample s;

        switch(m_state)
        {
        case 0:
            // return the middle peak
            sampleOut->setReal(-m_samples[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][1]); // - imag
            sampleOut->setImag(m_samples[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][0]); // + real
            m_state = 1;  // next state
            return false; // tell caller we didn't consume the sample

        case 1:
            // calculate with non null samples
            doInterpolateFIR(&s);
            sampleOut->setReal(-s.real());
            sampleOut->setImag(-s.imag());

            // insert sample into ring double buffer
            m_samples[m_ptr][0] = sampleIn->real();
            m_samples[m_ptr][1] = sampleIn->imag();
            m_samples[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][0] = sampleIn->real();
            m_samples[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][1] = sampleIn->imag();

            // advance pointer
            if (m_ptr < (HBFIRFilterTraits<HBFilterOrder>::hbOrder/2) - 1) {
                m_ptr++;
            } else {
                m_ptr = 0;
            }

            m_state = 2; // next state
            return true; // tell caller we consumed the sample

        case 2:
            // return the middle peak
            sampleOut->setReal(m_samples[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][1]);  // + imag
            sampleOut->setImag(-m_samples[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][0]);   // - real
            m_state = 3;  // next state
            return false; // tell caller we didn't consume the sample

        default:
            // calculate with non null samples
            doInterpolateFIR(&s);
            sampleOut->setReal(s.real());
            sampleOut->setImag(s.imag());

            // insert sample into ring double buffer
            m_samples[m_ptr][0] = sampleIn->real();
            m_samples[m_ptr][1] = sampleIn->imag();
            m_samples[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][0] = sampleIn->real();
            m_samples[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][1] = sampleIn->imag();

            // advance pointer
            if (m_ptr < (HBFIRFilterTraits<HBFilterOrder>::hbOrder/2) - 1) {
                m_ptr++;
            } else {
                m_ptr = 0;
            }

            m_state = 0; // next state
            return true; // tell caller we consumed the sample
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

    void myDecimate(int32_t x1, int32_t y1, int32_t *x2, int32_t *y2)
    {
        storeSample32(x1, y1);
        advancePointer();

        storeSample32(*x2, *y2);
        doFIR(x2, y2);
        advancePointer();
    }

    /** Simple zero stuffing and filter */
    void myInterpolateZeroStuffing(Sample* sample1, Sample* sample2)
    {
        storeSample((FixReal) sample1->real(), (FixReal) sample1->imag());
        doFIR(sample1);
        advancePointer();

        storeSample((FixReal) 0, (FixReal) 0);
        doFIR(sample2);
        advancePointer();
    }

    /** Simple zero stuffing and filter */
    void myInterpolateZeroStuffing(int32_t *x1, int32_t *y1, int32_t *x2, int32_t *y2)
    {
        storeSample32(*x1, *y1);
        doFIR(x1, y1);
        advancePointer();

        storeSample32(0, 0);
        doFIR(x2, y2);
        advancePointer();
    }

    /** Optimized upsampler by 2 not calculating FIR with inserted null samples */
    void myInterpolate(qint32 *x1, qint32 *y1, qint32 *x2, qint32 *y2)
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
    int32_t m_even[2][HBFIRFilterTraits<HBFilterOrder>::hbOrder]; // double buffer technique
    int32_t m_odd[2][HBFIRFilterTraits<HBFilterOrder>::hbOrder]; // double buffer technique
    int32_t m_samples[HBFIRFilterTraits<HBFilterOrder>::hbOrder][2]; // double buffer technique

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

    void storeSample32(int32_t x, int32_t y)
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

    int32_t rand(int32_t mod)
    {
        return (RAND_MAX/2 - std::rand()) % mod;
    }

    void doFIR(Sample* sample)
    {
        int32_t iAcc = 0;
        int32_t qAcc = 0;

#ifdef USE_SSE4_1
        IntHalfbandFilterEO1Intrisics<HBFilterOrder>::work(
                m_ptr,
                m_even,
                m_odd,
                iAcc,
                qAcc
        );
#else
        int a = m_ptr/2 + m_size; // tip pointer
        int b = m_ptr/2 + 1; // tail pointer

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
            iAcc += ((int32_t)m_odd[0][m_ptr/2 + m_size/2]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
            qAcc += ((int32_t)m_odd[1][m_ptr/2 + m_size/2]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
        }
        else
        {
            iAcc += ((int32_t)m_even[0][m_ptr/2 + m_size/2 + 1]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
            qAcc += ((int32_t)m_even[1][m_ptr/2 + m_size/2 + 1]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
        }

        sample->setReal(iAcc >> (HBFIRFilterTraits<HBFilterOrder>::hbShift -1));
        sample->setImag(qAcc >> (HBFIRFilterTraits<HBFilterOrder>::hbShift -1));
    }

    void doFIR(int32_t *x, int32_t *y)
    {
        int32_t iAcc = 0;
        int32_t qAcc = 0;

#ifdef USE_SSE4_1
        IntHalfbandFilterEO1Intrisics<HBFilterOrder>::work(
                m_ptr,
                m_even,
                m_odd,
                iAcc,
                qAcc
        );
#else
        int a = m_ptr/2 + m_size; // tip pointer
        int b = m_ptr/2 + 1; // tail pointer

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
            iAcc += ((int32_t)m_odd[0][m_ptr/2 + m_size/2]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
            qAcc += ((int32_t)m_odd[1][m_ptr/2 + m_size/2]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
        }
        else
        {
            iAcc += ((int32_t)m_even[0][m_ptr/2 + m_size/2 + 1]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
            qAcc += ((int32_t)m_even[1][m_ptr/2 + m_size/2 + 1]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
        }

        *x = iAcc >> (HBFIRFilterTraits<HBFilterOrder>::hbShift -1); // HB_SHIFT incorrect do not loose the gained bit
        *y = qAcc >> (HBFIRFilterTraits<HBFilterOrder>::hbShift -1);
    }

    void doInterpolateFIR(Sample* sample)
    {
        qint32 iAcc = 0;
        qint32 qAcc = 0;

        qint16 a = m_ptr;
        qint16 b = m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder / 2) - 1;

        // go through samples in buffer
        for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 4; i++)
        {
            iAcc += (m_samples[a][0] + m_samples[b][0]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
            qAcc += (m_samples[a][1] + m_samples[b][1]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
            a++;
            b--;
        }

        sample->setReal(iAcc >> (HBFIRFilterTraits<HBFilterOrder>::hbShift -1));
        sample->setImag(qAcc >> (HBFIRFilterTraits<HBFilterOrder>::hbShift -1));
    }

    void doInterpolateFIR(qint32 *x, qint32 *y)
    {
        qint32 iAcc = 0;
        qint32 qAcc = 0;

        qint16 a = m_ptr;
        qint16 b = m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder / 2) - 1;

        // go through samples in buffer
        for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 4; i++)
        {
            iAcc += (m_samples[a][0] + m_samples[b][0]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
            qAcc += (m_samples[a][1] + m_samples[b][1]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
            a++;
            b--;
        }

        *x = iAcc >> (HBFIRFilterTraits<HBFilterOrder>::hbShift -1);
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
        m_samples[i][0] = 0;
        m_samples[i][1] = 0;
    }

    m_ptr = 0;
    m_state = 0;
}

#endif /* SDRBASE_DSP_INTHALFBANDFILTEREO_H_ */
