///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_INTHALFBANDFILTER_H
#define INCLUDE_INTHALFBANDFILTER_H

#include <stdint.h>
#include "dsp/dsptypes.h"
#include "dsp/hbfiltertraits.h"
#include "util/export.h"

template<typename AccuType, uint32_t HBFilterOrder>
class SDRANGEL_API IntHalfbandFilter {
public:
	IntHalfbandFilter() :
	    m_ptr(0),
	    m_state(0)
    {
	    for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1; i++)
	    {
	        m_samples[i][0] = 0;
	        m_samples[i][1] = 0;
	    }
    }

	// downsample by 2, return center part of original spectrum
	bool workDecimateCenter(Sample* sample)
	{
		// insert sample into ring-buffer
		m_samples[m_ptr][0] = sample->real();
		m_samples[m_ptr][1] = sample->imag();

		switch(m_state)
		{
			case 0:
				// advance write-pointer
				m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

				// next state
				m_state = 1;

				// tell caller we don't have a new sample
				return false;

			default:
				// save result
				doFIR(sample);

				// advance write-pointer
				m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

				// next state
				m_state = 0;

				// tell caller we have a new sample
				return true;
		}
	}

    // upsample by 2, return center part of original spectrum
    bool workInterpolateCenterZeroStuffing(Sample* sampleIn, Sample *SampleOut)
    {
        switch(m_state)
        {
            case 0:
                // insert sample into ring-buffer
                m_samples[m_ptr][0] = 0;
                m_samples[m_ptr][1] = 0;

                // save result
                doFIR(SampleOut);

                // advance write-pointer
                m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

                // next state
                m_state = 1;

                // tell caller we didn't consume the sample
                return false;

            default:
                // insert sample into ring-buffer
                m_samples[m_ptr][0] = sampleIn->real();
                m_samples[m_ptr][1] = sampleIn->imag();

                // save result
                doFIR(SampleOut);

                // advance write-pointer
                m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

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

//	bool workDecimateCenter(qint32 *x, qint32 *y)
//	{
//		// insert sample into ring-buffer
//		m_samples[m_ptr][0] = *x;
//		m_samples[m_ptr][1] = *y;
//
//		switch(m_state)
//		{
//			case 0:
//				// advance write-pointer
//				m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);
//
//				// next state
//				m_state = 1;
//
//				// tell caller we don't have a new sample
//				return false;
//
//			default:
//				// save result
//				doFIR(x, y);
//
//				// advance write-pointer
//				m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);
//
//				// next state
//				m_state = 0;
//
//				// tell caller we have a new sample
//				return true;
//		}
//	}

	// downsample by 2, return edges of spectrum rotated into center - unused
//	bool workDecimateFullRotate(Sample* sample)
//	{
//		switch(m_state)
//		{
//			case 0:
//				// insert sample into ring-buffer
//				m_samples[m_ptr][0] = sample->real();
//				m_samples[m_ptr][1] = sample->imag();
//
//				// advance write-pointer
//				m_ptr = (m_ptr + HB_FILTERORDER) % (HB_FILTERORDER + 1);
//
//				// next state
//				m_state = 1;
//
//				// tell caller we don't have a new sample
//				return false;
//
//			default:
//				// insert sample into ring-buffer
//				m_samples[m_ptr][0] = -sample->real();
//				m_samples[m_ptr][1] = sample->imag();
//
//				// save result
//				doFIR(sample);
//
//				// advance write-pointer
//				m_ptr = (m_ptr + HB_FILTERORDER) % (HB_FILTERORDER + 1);
//
//				// next state
//				m_state = 0;
//
//				// tell caller we have a new sample
//				return true;
//		}
//	}

	// downsample by 2, return lower half of original spectrum
	bool workDecimateLowerHalf(Sample* sample)
	{
		switch(m_state)
		{
			case 0:
				// insert sample into ring-buffer
				m_samples[m_ptr][0] = -sample->imag();
				m_samples[m_ptr][1] = sample->real();

				// advance write-pointer
				m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

				// next state
				m_state = 1;

				// tell caller we don't have a new sample
				return false;

			case 1:
				// insert sample into ring-buffer
				m_samples[m_ptr][0] = -sample->real();
				m_samples[m_ptr][1] = -sample->imag();

				// save result
				doFIR(sample);

				// advance write-pointer
				m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

				// next state
				m_state = 2;

				// tell caller we have a new sample
				return true;

			case 2:
				// insert sample into ring-buffer
				m_samples[m_ptr][0] = sample->imag();
				m_samples[m_ptr][1] = -sample->real();

				// advance write-pointer
				m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

				// next state
				m_state = 3;

				// tell caller we don't have a new sample
				return false;

			default:
				// insert sample into ring-buffer
				m_samples[m_ptr][0] = sample->real();
				m_samples[m_ptr][1] = sample->imag();

				// save result
				doFIR(sample);

				// advance write-pointer
				m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

				// next state
				m_state = 0;

				// tell caller we have a new sample
				return true;
		}
	}

    // upsample by 2, from lower half of original spectrum
    bool workInterpolateLowerHalfZeroStuffing(Sample* sampleIn, Sample *sampleOut)
    {
        Sample s;

        switch(m_state)
        {
        case 0:
            // insert sample into ring-buffer
            m_samples[m_ptr][0] = 0;
            m_samples[m_ptr][1] = 0;

            // save result
            doFIR(&s);
            sampleOut->setReal(s.imag());
            sampleOut->setImag(-s.real());

            // advance write-pointer
            m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

            // next state
            m_state = 1;

            // tell caller we didn't consume the sample
            return false;

        case 1:
            // insert sample into ring-buffer
            m_samples[m_ptr][0] = sampleIn->real();
            m_samples[m_ptr][1] = sampleIn->imag();

            // save result
            doFIR(&s);
            sampleOut->setReal(-s.real());
            sampleOut->setImag(-s.imag());

            // advance write-pointer
            m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

            // next state
            m_state = 2;

            // tell caller we consumed the sample
            return true;

        case 2:
            // insert sample into ring-buffer
            m_samples[m_ptr][0] = 0;
            m_samples[m_ptr][1] = 0;

            // save result
            doFIR(&s);
            sampleOut->setReal(-s.imag());
            sampleOut->setImag(s.real());

            // advance write-pointer
            m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

            // next state
            m_state = 3;

            // tell caller we didn't consume the sample
            return false;

        default:
            // insert sample into ring-buffer
            m_samples[m_ptr][0] = sampleIn->real();
            m_samples[m_ptr][1] = sampleIn->imag();

            // save result
            doFIR(&s);
            sampleOut->setReal(s.real());
            sampleOut->setImag(s.imag());

            // advance write-pointer
            m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

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
				m_samples[m_ptr][0] = sample->imag();
				m_samples[m_ptr][1] = -sample->real();

				// advance write-pointer
				m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

				// next state
				m_state = 1;

				// tell caller we don't have a new sample
				return false;

			case 1:
				// insert sample into ring-buffer
				m_samples[m_ptr][0] = -sample->real();
				m_samples[m_ptr][1] = -sample->imag();

				// save result
				doFIR(sample);

				// advance write-pointer
				m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

				// next state
				m_state = 2;

				// tell caller we have a new sample
				return true;

			case 2:
				// insert sample into ring-buffer
				m_samples[m_ptr][0] = -sample->imag();
				m_samples[m_ptr][1] = sample->real();

				// advance write-pointer
				m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

				// next state
				m_state = 3;

				// tell caller we don't have a new sample
				return false;

			default:
				// insert sample into ring-buffer
				m_samples[m_ptr][0] = sample->real();
				m_samples[m_ptr][1] = sample->imag();

				// save result
				doFIR(sample);

				// advance write-pointer
				m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

				// next state
				m_state = 0;

				// tell caller we have a new sample
				return true;
		}
	}

    // upsample by 2, move original spectrum to upper half
    bool workInterpolateUpperHalfZeroStuffing(Sample* sampleIn, Sample *sampleOut)
    {
        Sample s;

        switch(m_state)
        {
        case 0:
            // insert sample into ring-buffer
            m_samples[m_ptr][0] = 0;
            m_samples[m_ptr][1] = 0;

            // save result
            doFIR(&s);
            sampleOut->setReal(-s.imag());
            sampleOut->setImag(s.real());

            // advance write-pointer
            m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

            // next state
            m_state = 1;

            // tell caller we didn't consume the sample
            return false;

        case 1:
            // insert sample into ring-buffer
            m_samples[m_ptr][0] = sampleIn->real();
            m_samples[m_ptr][1] = sampleIn->imag();

            // save result
            doFIR(&s);
            sampleOut->setReal(-s.real());
            sampleOut->setImag(-s.imag());

            // advance write-pointer
            m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

            // next state
            m_state = 2;

            // tell caller we consumed the sample
            return true;

        case 2:
            // insert sample into ring-buffer
            m_samples[m_ptr][0] = 0;
            m_samples[m_ptr][1] = 0;

            // save result
            doFIR(&s);
            sampleOut->setReal(s.imag());
            sampleOut->setImag(-s.real());

            // advance write-pointer
            m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

            // next state
            m_state = 3;

            // tell caller we didn't consume the sample
            return false;

        default:
            // insert sample into ring-buffer
            m_samples[m_ptr][0] = sampleIn->real();
            m_samples[m_ptr][1] = sampleIn->imag();

            // save result
            doFIR(&s);
            sampleOut->setReal(s.real());
            sampleOut->setImag(s.imag());

            // advance write-pointer
            m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

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
        m_samples[m_ptr][0] = sample1->real();
        m_samples[m_ptr][1] = sample1->imag();
        m_ptr = HBFIRFilterTraits<HBFilterOrder>::hbMod[m_ptr + 2 - 1];

        m_samples[m_ptr][0] = sample2->real();
        m_samples[m_ptr][1] = sample2->imag();

        doFIR(sample2);

        m_ptr = HBFIRFilterTraits<HBFilterOrder>::hbMod[m_ptr + 2 - 1];
    }

    void myDecimate(qint32 x1, qint32 y1, qint32 *x2, qint32 *y2)
    {
        m_samples[m_ptr][0] = x1;
        m_samples[m_ptr][1] = y1;
        m_ptr = HBFIRFilterTraits<HBFilterOrder>::hbMod[m_ptr + 2 - 1];

        m_samples[m_ptr][0] = *x2;
        m_samples[m_ptr][1] = *y2;

        doFIR(x2, y2);

        m_ptr = HBFIRFilterTraits<HBFilterOrder>::hbMod[m_ptr + 2 - 1];
    }

    /** Simple zero stuffing and filter */
    void myInterpolateZeroStuffing(Sample* sample1, Sample* sample2)
    {
        m_samples[m_ptr][0] = sample1->real();
        m_samples[m_ptr][1] = sample1->imag();

        doFIR(sample1);

        m_ptr = HBFIRFilterTraits<HBFilterOrder>::hbMod[m_ptr + 2 - 1];

        m_samples[m_ptr][0] = 0;
        m_samples[m_ptr][1] = 0;

        doFIR(sample2);

        m_ptr = HBFIRFilterTraits<HBFilterOrder>::hbMod[m_ptr + 2 - 1];
    }

    /** Simple zero stuffing and filter */
    void myInterpolateZeroStuffing(qint32 *x1, qint32 *y1, qint32 *x2, qint32 *y2)
    {
        m_samples[m_ptr][0] = *x1;
        m_samples[m_ptr][1] = *y1;

        doFIR(x1, y1);

        m_ptr = HBFIRFilterTraits<HBFilterOrder>::hbMod[m_ptr + 2 - 1];

        m_samples[m_ptr][0] = 0;
        m_samples[m_ptr][1] = 0;

        doFIR(x2, y2);

        m_ptr = HBFIRFilterTraits<HBFilterOrder>::hbMod[m_ptr + 2 - 1];
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
	AccuType m_samples[HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1][2];     // Valgrind optim (from qint16)
	qint16 m_ptr;
	int m_state;

	void doFIR(Sample* sample)
	{
		// init read-pointer
		int a = HBFIRFilterTraits<HBFilterOrder>::hbMod[m_ptr + 2 + 1]; // 0 + 1
		int b = HBFIRFilterTraits<HBFilterOrder>::hbMod[m_ptr + 2 - 2]; //-1 - 1

		// go through samples in buffer
		AccuType iAcc = 0;
		AccuType qAcc = 0;

		for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 4; i++)
		{
			// do multiply-accumulate
			//qint32 iTmp = m_samples[a][0] + m_samples[b][0]; // Valgrind optim
			//qint32 qTmp = m_samples[a][1] + m_samples[b][1]; // Valgrind optim
			iAcc += (m_samples[a][0] + m_samples[b][0]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
			qAcc += (m_samples[a][1] + m_samples[b][1]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];

			// update read-pointer
			a = HBFIRFilterTraits<HBFilterOrder>::hbMod[a + 2 + 2];
			b = HBFIRFilterTraits<HBFilterOrder>::hbMod[b + 2 - 2];
		}

		a = HBFIRFilterTraits<HBFilterOrder>::hbMod[a + 2 - 1];

		iAcc += ((qint32)m_samples[a][0] + 1) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
		qAcc += ((qint32)m_samples[a][1] + 1) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);

		sample->setReal(iAcc >> (HBFIRFilterTraits<HBFilterOrder>::hbShift -1));
		sample->setImag(qAcc >> (HBFIRFilterTraits<HBFilterOrder>::hbShift -1));
	}

	void doInterpolateFIR(Sample* sample)
	{
	    qint16 a = m_ptr;
	    qint16 b = m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder / 2) - 1;

        // go through samples in buffer
	    AccuType iAcc = 0;
	    AccuType qAcc = 0;

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
        qint16 a = m_ptr;
        qint16 b = m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder / 2) - 1;

        // go through samples in buffer
        AccuType iAcc = 0;
        AccuType qAcc = 0;

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

	void doFIR(qint32 *x, qint32 *y)
	{
		// Coefficents. This is a sinc function:
	    // Half of the half of coefficients are stored because:
	    // - half of the coefficients are 0
	    // - there is a symmertry around the central 0.5 coefficient (not stored either)
	    // There are actually order+1 coefficients

		// init read-pointer
		int a = HBFIRFilterTraits<HBFilterOrder>::hbMod[m_ptr + 2 + 1]; // 0 + 1
		int b = HBFIRFilterTraits<HBFilterOrder>::hbMod[m_ptr + 2 - 2]; //-1 - 1

		// go through samples in buffer
		AccuType iAcc = 0;
		AccuType qAcc = 0;

		for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 4; i++)
		{
			// do multiply-accumulate
			//qint32 iTmp = m_samples[a][0] + m_samples[b][0]; // Valgrind optim
			//qint32 qTmp = m_samples[a][1] + m_samples[b][1]; // Valgrind optim
			iAcc += (m_samples[a][0] + m_samples[b][0]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
			qAcc += (m_samples[a][1] + m_samples[b][1]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];

			// update read-pointer
			a = HBFIRFilterTraits<HBFilterOrder>::hbMod[a + 2 + 2];
			b = HBFIRFilterTraits<HBFilterOrder>::hbMod[b + 2 - 2];
		}

		a = HBFIRFilterTraits<HBFilterOrder>::hbMod[a + 2 - 1];

		iAcc += ((qint32)m_samples[a][0] + 1) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
		qAcc += ((qint32)m_samples[a][1] + 1) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);

		*x = iAcc >> (HBFIRFilterTraits<HBFilterOrder>::hbShift -1); // HB_SHIFT incorrect do not loose the gained bit
		*y = qAcc >> (HBFIRFilterTraits<HBFilterOrder>::hbShift -1);
	}

};

//template<uint32_t HBFilterOrder>
//IntHalfbandFilter<HBFilterOrder>::IntHalfbandFilter()
//{
//    for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1; i++)
//    {
//        m_samples[i][0] = 0;
//        m_samples[i][1] = 0;
//    }
//
//    m_ptr = 0;
//    m_state = 0;
//}

#endif // INCLUDE_INTHALFBANDFILTER_H
