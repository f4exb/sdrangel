///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// Integer half-band FIR based interpolator and decimator                        //
// This is the even/odd and I/Q stride with double buffering variant             //
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

#ifndef INCLUDE_INTHALFBANDFILTER_ST_H
#define INCLUDE_INTHALFBANDFILTER_ST_H

#include <stdint.h>
#include "dsp/dsptypes.h"
#include "dsp/hbfiltertraits.h"
#include "dsp/inthalfbandfiltersti.h"
#include "export.h"

template<uint32_t HBFilterOrder>
class SDRANGEL_API IntHalfbandFilterST {
public:
    IntHalfbandFilterST();

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

	bool workDecimateCenter(int32_t *x, int32_t *y)
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

    void myDecimate(int32_t x1, int32_t y1, int32_t *x2, int32_t *y2)
    {
        storeSample(x1, y1);
        advancePointer();

        storeSample(*x2, *y2);
        doFIR(x2, y2);
        advancePointer();
    }

    void myInterpolate(Sample* sample1, Sample* sample2)
    {
        storeSample((FixReal) sample1->real(), (FixReal) sample1->imag());
        doFIR(sample1);
        advancePointer();

        storeSample(0, 0);
        doFIR(sample2);
        advancePointer();
    }

    void myInterpolate(int32_t *x1, int32_t *y1, int32_t *x2, int32_t *y2)
    {
        storeSample(*x1, *y1);
        doFIR(x1, y1);
        advancePointer();

        storeSample(0, 0);
        doFIR(x2, y2);
        advancePointer();
    }

protected:
	int32_t m_samplesDB[2*HBFilterOrder][2]; // double buffer technique with even/odd amnd I/Q stride
	int32_t m_samplesAligned[HBFilterOrder][2] __attribute__ ((aligned (16)));
	int m_ptr;
	int m_size;
	int m_state;
    int32_t m_iEvenAcc;
    int32_t m_qEvenAcc;
    int32_t m_iOddAcc;
    int32_t m_qOddAcc;


    void storeSample(const FixReal& sampleI, const FixReal& sampleQ)
    {
        m_samplesDB[m_ptr][0] = sampleI;
        m_samplesDB[m_ptr][1] = sampleQ;
        m_samplesDB[m_ptr + m_size][0] = sampleI;
        m_samplesDB[m_ptr + m_size][1] = sampleQ;
    }

    void storeSample(int32_t x, int32_t y)
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

    void doFIR(Sample* sample)
    {
        // calculate on odd values

        if ((m_ptr % 2) == 1)
        {
            m_iEvenAcc = 0;
            m_qEvenAcc = 0;
            m_iOddAcc = 0;
            m_qOddAcc = 0;
#ifdef USE_SSE4_1
//            memcpy((void *) m_samplesAligned, (const void *) &(m_samplesDB[ m_ptr + 1][0]), HBFilterOrder*2*sizeof(int32_t));
            IntHalfbandFilterSTIntrinsics<HBFilterOrder>::workNA(
                    m_ptr + 1,
                    m_samplesDB,
					m_iEvenAcc,
					m_qEvenAcc,
					m_iOddAcc,
					m_qOddAcc);
#else
            int a = m_ptr + m_size; // tip pointer - odd
            int b = m_ptr + 1; // tail pointer - aven

            for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 4; i++)
            {
                m_iEvenAcc += (m_samplesDB[a-1][0] + m_samplesDB[b][0])   * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
                m_iOddAcc  += (m_samplesDB[a][0]   + m_samplesDB[b+1][0]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
                m_qEvenAcc += (m_samplesDB[a-1][1] + m_samplesDB[b][1])   * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
                m_qOddAcc  += (m_samplesDB[a][1]   + m_samplesDB[b+1][1]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
                a -= 2;
                b += 2;
            }
#endif
            m_iEvenAcc += ((int32_t)m_samplesDB[m_ptr + m_size/2][0]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
            m_qEvenAcc += ((int32_t)m_samplesDB[m_ptr + m_size/2][1]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
            m_iOddAcc += ((int32_t)m_samplesDB[m_ptr + m_size/2 + 1][0]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
            m_qOddAcc += ((int32_t)m_samplesDB[m_ptr + m_size/2 + 1][1]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);

            sample->setReal(m_iEvenAcc >> HBFIRFilterTraits<HBFilterOrder>::hbShift -1);
            sample->setImag(m_qEvenAcc >> HBFIRFilterTraits<HBFilterOrder>::hbShift -1);
        }
        else
        {
            sample->setReal(m_iOddAcc >> HBFIRFilterTraits<HBFilterOrder>::hbShift -1);
            sample->setImag(m_qOddAcc >> HBFIRFilterTraits<HBFilterOrder>::hbShift -1);
        }
    }

    void doFIR(int32_t *x, int32_t *y)
    {
        // calculate on odd values

        if ((m_ptr % 2) == 1)
        {
            m_iEvenAcc = 0;
            m_qEvenAcc = 0;
            m_iOddAcc = 0;
            m_qOddAcc = 0;

#ifdef USE_SSE4_1
//            memcpy((void *) m_samplesAligned, (const void *) &(m_samplesDB[ m_ptr + 1][0]), HBFilterOrder*2*sizeof(int32_t));
            IntHalfbandFilterSTIntrinsics<HBFilterOrder>::workNA(
                    m_ptr + 1,
                    m_samplesDB,
					m_iEvenAcc,
					m_qEvenAcc,
					m_iOddAcc,
					m_qOddAcc);
#else
            int a = m_ptr + m_size; // tip pointer - odd
            int b = m_ptr + 1; // tail pointer - aven

            for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 4; i++)
            {
                m_iEvenAcc += (m_samplesDB[a-1][0] + m_samplesDB[b][0])   * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
                m_iOddAcc  += (m_samplesDB[a][0]   + m_samplesDB[b+1][0]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
                m_qEvenAcc += (m_samplesDB[a-1][1] + m_samplesDB[b][1])   * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
                m_qOddAcc  += (m_samplesDB[a][1]   + m_samplesDB[b+1][1]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
                a -= 2;
                b += 2;
            }
#endif
            m_iEvenAcc += ((int32_t)m_samplesDB[m_ptr + m_size/2][0]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
            m_qEvenAcc += ((int32_t)m_samplesDB[m_ptr + m_size/2][1]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
            m_iOddAcc += ((int32_t)m_samplesDB[m_ptr + m_size/2 + 1][0]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
            m_qOddAcc += ((int32_t)m_samplesDB[m_ptr + m_size/2 + 1][1]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);

            *x = m_iEvenAcc >> HBFIRFilterTraits<HBFilterOrder>::hbShift -1;
            *y = m_qEvenAcc >> HBFIRFilterTraits<HBFilterOrder>::hbShift -1;
        }
        else
        {
            *x = m_iOddAcc >> HBFIRFilterTraits<HBFilterOrder>::hbShift -1;
            *y = m_qOddAcc >> HBFIRFilterTraits<HBFilterOrder>::hbShift -1;
        }
    }
};

template<uint32_t HBFilterOrder>
IntHalfbandFilterST<HBFilterOrder>::IntHalfbandFilterST()
{
    m_size = HBFIRFilterTraits<HBFilterOrder>::hbOrder;

    for (int i = 0; i < m_size; i++)
    {
        m_samplesDB[i][0] = 0;
        m_samplesDB[i][1] = 0;
    }

    m_ptr = 0;
    m_state = 0;
    m_iEvenAcc = 0;
    m_qEvenAcc = 0;
    m_iOddAcc = 0;
    m_qOddAcc = 0;
}

#endif // INCLUDE_INTHALFBANDFILTER_DB_H
