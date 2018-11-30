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

#ifndef INCLUDE_INTHALFBANDFILTER_DBFF_H
#define INCLUDE_INTHALFBANDFILTER_DBFF_H

#include <stdint.h>
#include "dsp/dsptypes.h"
#include "dsp/hbfiltertraits.h"
#include "export.h"

template<typename AccuType, typename SampleType, uint32_t HBFilterOrder>
class SDRBASE_API IntHalfbandFilterDBFF {
public:
    IntHalfbandFilterDBFF();

	// downsample by 2, return center part of original spectrum
	bool workDecimateCenter(FSample* sample)
	{
		// insert sample into ring-buffer
	    storeSampleReal((Real) sample->real(), (Real) sample->imag());

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
    bool workInterpolateCenterZeroStuffing(FSample* sampleIn, FSample *SampleOut)
    {
        switch(m_state)
        {
            case 0:
                // insert sample into ring-buffer
                storeSampleReal((Real) 0, (Real) 0);
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
                storeSampleReal((Real) sampleIn->real(), (Real) sampleIn->imag());
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
    bool workInterpolateCenter(FSample* sampleIn, FSample *SampleOut)
    {
        switch(m_state)
        {
        case 0:
            // return the middle peak
            SampleOut->setReal(m_samplesDB[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][0]);
            SampleOut->setImag(m_samplesDB[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][1]);
            m_state = 1;  // next state
            return false; // tell caller we didn't consume the sample

        default:
            // calculate with non null samples
            doInterpolateFIR(SampleOut);

            // insert sample into ring double buffer
            m_samplesDB[m_ptr][0] = sampleIn->real();
            m_samplesDB[m_ptr][1] = sampleIn->imag();
            m_samplesDB[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][0] = sampleIn->real();
            m_samplesDB[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][1] = sampleIn->imag();

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

	// downsample by 2, return lower half of original spectrum
	bool workDecimateLowerHalf(FSample* sample)
	{
		switch(m_state)
		{
			case 0:
				// insert sample into ring-buffer
			    storeSampleReal((Real) -sample->imag(), (Real) sample->real());
				// advance write-pointer
                advancePointer();
				// next state
				m_state = 1;
				// tell caller we don't have a new sample
				return false;

			case 1:
				// insert sample into ring-buffer
                storeSampleReal((Real) -sample->real(), (Real) -sample->imag());
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
                storeSampleReal((Real) sample->imag(), (Real) -sample->real());
				// advance write-pointer
                advancePointer();
				// next state
				m_state = 3;
				// tell caller we don't have a new sample
				return false;

			default:
				// insert sample into ring-buffer
                storeSampleReal((Real) sample->real(), (Real) sample->imag());
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

    /** Optimized upsampler by 2 not calculating FIR with inserted null samples */
    bool workInterpolateLowerHalf(FSample* sampleIn, FSample *sampleOut)
    {
        FSample s;

        switch(m_state)
        {
        case 0:
            // return the middle peak
            sampleOut->setReal(m_samplesDB[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][1]);  // imag
            sampleOut->setImag(-m_samplesDB[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][0]); // - real
            m_state = 1;  // next state
            return false; // tell caller we didn't consume the sample

        case 1:
            // calculate with non null samples
            doInterpolateFIR(&s);
            sampleOut->setReal(-s.real());
            sampleOut->setImag(-s.imag());

            // insert sample into ring double buffer
            m_samplesDB[m_ptr][0] = sampleIn->real();
            m_samplesDB[m_ptr][1] = sampleIn->imag();
            m_samplesDB[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][0] = sampleIn->real();
            m_samplesDB[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][1] = sampleIn->imag();

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
            sampleOut->setReal(-m_samplesDB[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][1]);  // - imag
            sampleOut->setImag(m_samplesDB[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][0]); // real
            m_state = 3;  // next state
            return false; // tell caller we didn't consume the sample

        default:
            // calculate with non null samples
            doInterpolateFIR(&s);
            sampleOut->setReal(s.real());
            sampleOut->setImag(s.imag());

            // insert sample into ring double buffer
            m_samplesDB[m_ptr][0] = sampleIn->real();
            m_samplesDB[m_ptr][1] = sampleIn->imag();
            m_samplesDB[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][0] = sampleIn->real();
            m_samplesDB[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][1] = sampleIn->imag();

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

    // upsample by 2, from lower half of original spectrum - double buffer variant
    bool workInterpolateLowerHalfZeroStuffing(FSample* sampleIn, FSample *sampleOut)
    {
        FSample s;

        switch(m_state)
        {
        case 0:
            // insert sample into ring-buffer
            storeSampleReal((Real) 0, (Real) 0);

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
            storeSampleReal((Real) sampleIn->real(), (Real) sampleIn->imag());

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
            storeSampleReal((Real) 0, (Real) 0);

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
            storeSampleReal((Real) sampleIn->real(), (Real) sampleIn->imag());

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
	bool workDecimateUpperHalf(FSample* sample)
	{
		switch(m_state)
		{
			case 0:
				// insert sample into ring-buffer
	            storeSampleReal((Real) sample->imag(), (Real) -sample->real());
				// advance write-pointer
                advancePointer();
				// next state
				m_state = 1;
				// tell caller we don't have a new sample
				return false;

			case 1:
				// insert sample into ring-buffer
                storeSampleReal((Real) -sample->real(), (Real) -sample->imag());
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
                storeSampleReal((Real) -sample->imag(), (Real) sample->real());
				// advance write-pointer
                advancePointer();
				// next state
				m_state = 3;
				// tell caller we don't have a new sample
				return false;

			default:
				// insert sample into ring-buffer
                storeSampleReal((Real) sample->real(), (Real) sample->imag());
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

    /** Optimized upsampler by 2 not calculating FIR with inserted null samples */
    bool workInterpolateUpperHalf(FSample* sampleIn, FSample *sampleOut)
    {
        FSample s;

        switch(m_state)
        {
        case 0:
            // return the middle peak
            sampleOut->setReal(-m_samplesDB[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][1]); // - imag
            sampleOut->setImag(m_samplesDB[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][0]); // + real
            m_state = 1;  // next state
            return false; // tell caller we didn't consume the sample

        case 1:
            // calculate with non null samples
            doInterpolateFIR(&s);
            sampleOut->setReal(-s.real());
            sampleOut->setImag(-s.imag());

            // insert sample into ring double buffer
            m_samplesDB[m_ptr][0] = sampleIn->real();
            m_samplesDB[m_ptr][1] = sampleIn->imag();
            m_samplesDB[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][0] = sampleIn->real();
            m_samplesDB[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][1] = sampleIn->imag();

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
            sampleOut->setReal(m_samplesDB[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][1]);  // + imag
            sampleOut->setImag(-m_samplesDB[m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder/4) - 1][0]);   // - real
            m_state = 3;  // next state
            return false; // tell caller we didn't consume the sample

        default:
            // calculate with non null samples
            doInterpolateFIR(&s);
            sampleOut->setReal(s.real());
            sampleOut->setImag(s.imag());

            // insert sample into ring double buffer
            m_samplesDB[m_ptr][0] = sampleIn->real();
            m_samplesDB[m_ptr][1] = sampleIn->imag();
            m_samplesDB[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][0] = sampleIn->real();
            m_samplesDB[m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2][1] = sampleIn->imag();

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

    // upsample by 2, move original spectrum to upper half - double buffer variant
    bool workInterpolateUpperHalfZeroStuffing(FSample* sampleIn, FSample *sampleOut)
    {
        FSample s;

        switch(m_state)
        {
        case 0:
            // insert sample into ring-buffer
            storeSampleReal((Real) 0, (Real) 0);

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
            storeSampleReal((Real) sampleIn->real(), (Real) sampleIn->imag());

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
            storeSampleReal((Real) 0, (Real) 0);

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
            storeSampleReal((Real) sampleIn->real(), (Real) sampleIn->imag());

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

    void myDecimate(const FSample* sample1, FSample* sample2)
    {
        storeSampleReal((Real) sample1->real(), (Real) sample1->imag());
        advancePointer();

        storeSampleReal((Real) sample2->real(), (Real) sample2->imag());
        doFIR(sample2);
        advancePointer();
    }

    void myDecimate(AccuType x1, AccuType y1, AccuType *x2, AccuType *y2)
    {
        storeSampleAccu(x1, y1);
        advancePointer();

        storeSampleAccu(*x2, *y2);
        doFIRAccu(x2, y2);
        advancePointer();
    }

    /** Simple zero stuffing and filter */
    void myInterpolateZeroStuffing(FSample* sample1, FSample* sample2)
    {
        storeSampleReal((Real) sample1->real(), (Real) sample1->imag());
        doFIR(sample1);
        advancePointer();

        storeSampleReal((Real) 0, (Real) 0);
        doFIR(sample2);
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

    void storeSampleReal(const Real& sampleI, const Real& sampleQ)
    {
        m_samplesDB[m_ptr][0] = sampleI;
        m_samplesDB[m_ptr][1] = sampleQ;
        m_samplesDB[m_ptr + m_size][0] = sampleI;
        m_samplesDB[m_ptr + m_size][1] = sampleQ;
    }

    void storeSampleAccu(AccuType x, AccuType y)
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

    void doFIR(FSample* sample)
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

        iAcc += m_samplesDB[b-1][0] << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
        qAcc += m_samplesDB[b-1][1] << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);

        sample->setReal(iAcc);
        sample->setImag(qAcc);
    }

    void doFIRAccu(AccuType *x, AccuType *y)
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

    void doInterpolateFIR(FSample* sample)
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

        sample->setReal(iAcc);
        sample->setImag(qAcc);
    }

    void doInterpolateFIR(Real *x, Real *y)
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

        *x = iAcc;
        *y = qAcc;
    }
};

template<typename AccuType, typename SampleType, uint32_t HBFilterOrder>
IntHalfbandFilterDBFF<AccuType, SampleType, HBFilterOrder>::IntHalfbandFilterDBFF()
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

#endif // INCLUDE_INTHALFBANDFILTER_DBFF_H
