#ifndef INCLUDE_INTHALFBANDFILTER_H
#define INCLUDE_INTHALFBANDFILTER_H

#include <stdint.h>
#include "dsp/dsptypes.h"
#include "util/export.h"

// uses Q1.14 format internally, input and output are S16

/*
 * supported filter orders: 96, 80, 64, 48, 32
 * any usage of another value will be prevented by compilation errors
 */
template<uint32_t HBFilterOrder>
struct HBFIRFilterTraits
{
};

template<>
struct HBFIRFilterTraits<32>
{
    static const qint32 hbOrder = 32;
    static const qint32 hbShift = 14;
    static const qint16 hbMod[32+6];
    static const qint32 hbCoeffs[8];
};

template<>
struct HBFIRFilterTraits<48>
{
    static const qint32 hbOrder = 48;
    static const qint32 hbShift = 14;
    static const qint16 hbMod[48+6];
    static const qint32 hbCoeffs[12];
};

template<>
struct HBFIRFilterTraits<64>
{
    static const qint32 hbOrder = 64;
    static const qint32 hbShift = 14;
    static const qint16 hbMod[64+6];
    static const qint32 hbCoeffs[16];
};

template<>
struct HBFIRFilterTraits<80>
{
    static const qint32 hbOrder = 80;
    static const qint32 hbShift = 14;
    static const qint16 hbMod[80+6];
    static const qint32 hbCoeffs[20];
};

template<>
struct HBFIRFilterTraits<96>
{
    static const qint32 hbOrder = 96;
    static const qint32 hbShift = 16;
    static const qint16 hbMod[96+6];
    static const qint32 hbCoeffs[24];
};

template<uint32_t HBFilterOrder>
class SDRANGEL_API IntHalfbandFilter {
public:
	IntHalfbandFilter();

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
    bool workInterpolateCenter(Sample* sampleIn, Sample *SampleOut)
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

    // upsample by 2, return center part of original spectrum - double buffer variant
    bool workInterpolateCenterDB(Sample* sampleIn, Sample *SampleOut)
    {
        switch(m_state)
        {
            case 0:
                // insert sample into ring-buffer
                m_samplesDB[m_ptrDB][0] = 0;
                m_samplesDB[m_ptrDB][1] = 0;
                m_samplesDB[m_ptrDB + m_sizeDB][0] = 0;
                m_samplesDB[m_ptrDB + m_sizeDB][1] = 0;

                // save result
                doFIRDB(SampleOut);

                // advance write-pointer
                m_ptrDB = (m_ptrDB + 1) % m_sizeDB;

                // next state
                m_state = 1;

                // tell caller we didn't consume the sample
                return false;

            default:
                // insert sample into ring-buffer
                m_samplesDB[m_ptrDB][0] = sampleIn->real();
                m_samplesDB[m_ptrDB][1] = sampleIn->imag();
                m_samplesDB[m_ptrDB + m_sizeDB][0] = sampleIn->real();
                m_samplesDB[m_ptrDB + m_sizeDB][1] = sampleIn->imag();

                // save result
                doFIRDB(SampleOut);

                // advance write-pointer
                m_ptrDB = (m_ptrDB + 1) % m_sizeDB;

                // next state
                m_state = 0;

                // tell caller we consumed the sample
                return true;
        }
    }

	bool workDecimateCenter(qint32 *x, qint32 *y)
	{
		// insert sample into ring-buffer
		m_samples[m_ptr][0] = *x;
		m_samples[m_ptr][1] = *y;

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
				doFIR(x, y);

				// advance write-pointer
				m_ptr = (m_ptr + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

				// next state
				m_state = 0;

				// tell caller we have a new sample
				return true;
		}
	}

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
    bool workInterpolateLowerHalf(Sample* sampleIn, Sample *sampleOut)
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

    // upsample by 2, from lower half of original spectrum - double buffer variant
    bool workInterpolateLowerHalfDB(Sample* sampleIn, Sample *sampleOut)
    {
        Sample s;

        switch(m_state)
        {
        case 0:
            // insert sample into ring-buffer
            m_samplesDB[m_ptrDB][0] = 0;
            m_samplesDB[m_ptrDB][1] = 0;
            m_samplesDB[m_ptrDB + m_sizeDB][0] = 0;
            m_samplesDB[m_ptrDB + m_sizeDB][1] = 0;

            // save result
            doFIRDB(&s);
            sampleOut->setReal(s.imag());
            sampleOut->setImag(-s.real());

            // advance write-pointer
            m_ptrDB = (m_ptrDB + 1) % m_sizeDB;

            // next state
            m_state = 1;

            // tell caller we didn't consume the sample
            return false;

        case 1:
            // insert sample into ring-buffer
            m_samplesDB[m_ptrDB][0] = sampleIn->real();
            m_samplesDB[m_ptrDB][1] = sampleIn->imag();
            m_samplesDB[m_ptrDB + m_sizeDB][0] = sampleIn->real();
            m_samplesDB[m_ptrDB + m_sizeDB][1] = sampleIn->imag();

            // save result
            doFIRDB(&s);
            sampleOut->setReal(-s.real());
            sampleOut->setImag(-s.imag());

            // advance write-pointer
            m_ptrDB = (m_ptrDB + 1) % m_sizeDB;

            // next state
            m_state = 2;

            // tell caller we consumed the sample
            return true;

        case 2:
            // insert sample into ring-buffer
            m_samplesDB[m_ptrDB][0] = 0;
            m_samplesDB[m_ptrDB][1] = 0;
            m_samplesDB[m_ptrDB + m_sizeDB][0] = 0;
            m_samplesDB[m_ptrDB + m_sizeDB][1] = 0;

            // save result
            doFIRDB(&s);
            sampleOut->setReal(-s.imag());
            sampleOut->setImag(s.real());

            // advance write-pointer
            m_ptrDB = (m_ptrDB + 1) % m_sizeDB;

            // next state
            m_state = 3;

            // tell caller we didn't consume the sample
            return false;

        default:
            // insert sample into ring-buffer
            m_samplesDB[m_ptrDB][0] = sampleIn->real();
            m_samplesDB[m_ptrDB][1] = sampleIn->imag();
            m_samplesDB[m_ptrDB + m_sizeDB][0] = sampleIn->real();
            m_samplesDB[m_ptrDB + m_sizeDB][1] = sampleIn->imag();

            // save result
            doFIRDB(&s);
            sampleOut->setReal(s.real());
            sampleOut->setImag(s.imag());

            // advance write-pointer
            m_ptrDB = (m_ptrDB + 1) % m_sizeDB;

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
    bool workInterpolateUpperHalf(Sample* sampleIn, Sample *sampleOut)
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

    // upsample by 2, move original spectrum to upper half - double buffer variant
    bool workInterpolateUpperHalfDB(Sample* sampleIn, Sample *sampleOut)
    {
        Sample s;

        switch(m_state)
        {
        case 0:
            // insert sample into ring-buffer
            m_samplesDB[m_ptrDB][0] = 0;
            m_samplesDB[m_ptrDB][1] = 0;
            m_samplesDB[m_ptrDB + m_sizeDB][0] = 0;
            m_samplesDB[m_ptrDB + m_sizeDB][1] = 0;

            // save result
            doFIRDB(&s);
            sampleOut->setReal(-s.imag());
            sampleOut->setImag(s.real());

            // advance write-pointer
            m_ptrDB = (m_ptrDB + 1) % m_sizeDB;

            // next state
            m_state = 1;

            // tell caller we didn't consume the sample
            return false;

        case 1:
            // insert sample into ring-buffer
            m_samplesDB[m_ptrDB][0] = sampleIn->real();
            m_samplesDB[m_ptrDB][1] = sampleIn->imag();
            m_samplesDB[m_ptrDB + m_sizeDB][0] = sampleIn->real();
            m_samplesDB[m_ptrDB + m_sizeDB][1] = sampleIn->imag();

            // save result
            doFIRDB(&s);
            sampleOut->setReal(-s.real());
            sampleOut->setImag(-s.imag());

            // advance write-pointer
            m_ptrDB = (m_ptrDB + 1) % m_sizeDB;

            // next state
            m_state = 2;

            // tell caller we consumed the sample
            return true;

        case 2:
            // insert sample into ring-buffer
            m_samplesDB[m_ptrDB][0] = 0;
            m_samplesDB[m_ptrDB][1] = 0;
            m_samplesDB[m_ptrDB + m_sizeDB][0] = 0;
            m_samplesDB[m_ptrDB + m_sizeDB][1] = 0;

            // save result
            doFIRDB(&s);
            sampleOut->setReal(s.imag());
            sampleOut->setImag(-s.real());

            // advance write-pointer
            m_ptrDB = (m_ptrDB + 1) % m_sizeDB;

            // next state
            m_state = 3;

            // tell caller we didn't consume the sample
            return false;

        default:
            // insert sample into ring-buffer
            m_samplesDB[m_ptrDB][0] = sampleIn->real();
            m_samplesDB[m_ptrDB][1] = sampleIn->imag();
            m_samplesDB[m_ptrDB + m_sizeDB][0] = sampleIn->real();
            m_samplesDB[m_ptrDB + m_sizeDB][1] = sampleIn->imag();

            // save result
            doFIRDB(&s);
            sampleOut->setReal(s.real());
            sampleOut->setImag(s.imag());

            // advance write-pointer
            m_ptrDB = (m_ptrDB + 1) % m_sizeDB;

            // next state
            m_state = 0;

            // tell caller we consumed the sample
            return true;
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

protected:
	qint32 m_samples[HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1][2];     // Valgrind optim (from qint16)
	qint32 m_samplesDB[2*(HBFIRFilterTraits<HBFilterOrder>::hbOrder - 1)][2]; // double buffer technique
	qint16 m_ptr;
	int m_ptrDB;
	int m_sizeDB;
	int m_state;

    void doFIRDB(Sample* sample)
    {
        int a = m_ptrDB + m_sizeDB; // tip pointer
        int b = m_ptrDB + 1; // tail pointer
        qint32 iAcc = 0;
        qint32 qAcc = 0;

        for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 4; i++)
        {
            iAcc += (m_samplesDB[a][0] + m_samplesDB[b][0]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
            qAcc += (m_samplesDB[a][1] + m_samplesDB[b][1]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
            a -= 2;
            b += 2;
        }

        iAcc += ((qint32)m_samplesDB[b-1][0]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
        qAcc += ((qint32)m_samplesDB[b-1][1]) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);

        sample->setReal(iAcc >> HBFIRFilterTraits<HBFilterOrder>::hbShift -1);
        sample->setImag(qAcc >> HBFIRFilterTraits<HBFilterOrder>::hbShift -1);
    }

	void doFIR(Sample* sample)
	{
		// init read-pointer
//		int a = HBFIRFilterTraits<HBFilterOrder>::hbMod[m_ptr + 2 + 1]; // 0 + 1
//		int b = HBFIRFilterTraits<HBFilterOrder>::hbMod[m_ptr + 2 - 2]; //-1 - 1
		// ancient way:
		int a = (m_ptr + 1) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);
		int b = (m_ptr + (HBFIRFilterTraits<HBFilterOrder>::hbOrder - 1)) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

		// go through samples in buffer
		qint32 iAcc = 0;
		qint32 qAcc = 0;

		for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 4; i++)
		{
			// do multiply-accumulate
			//qint32 iTmp = m_samples[a][0] + m_samples[b][0]; // Valgrind optim
			//qint32 qTmp = m_samples[a][1] + m_samples[b][1]; // Valgrind optim
			iAcc += (m_samples[a][0] + m_samples[b][0]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];
			qAcc += (m_samples[a][1] + m_samples[b][1]) * HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[i];

			// update read-pointer
//			a = HBFIRFilterTraits<HBFilterOrder>::hbMod[a + 2 + 2];
//			b = HBFIRFilterTraits<HBFilterOrder>::hbMod[b + 2 - 2];
			// ancient way:
			a = (a + 2) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);
			b = (b + (HBFIRFilterTraits<HBFilterOrder>::hbOrder - 1)) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);
		}

//		a = HBFIRFilterTraits<HBFilterOrder>::hbMod[a + 2 - 1];
		// ancient way:
		a = (a + HBFIRFilterTraits<HBFilterOrder>::hbOrder) % (HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1);

		iAcc += ((qint32)m_samples[a][0] + 1) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);
		qAcc += ((qint32)m_samples[a][1] + 1) << (HBFIRFilterTraits<HBFilterOrder>::hbShift - 1);

		sample->setReal(iAcc >> HBFIRFilterTraits<HBFilterOrder>::hbShift -1);
		sample->setImag(qAcc >> HBFIRFilterTraits<HBFilterOrder>::hbShift -1);
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
		qint32 iAcc = 0;
		qint32 qAcc = 0;

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

template<uint32_t HBFilterOrder>
IntHalfbandFilter<HBFilterOrder>::IntHalfbandFilter()
{
    m_sizeDB = HBFIRFilterTraits<HBFilterOrder>::hbOrder - 1;

    for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder + 1; i++)
    {
        m_samples[i][0] = 0;
        m_samples[i][1] = 0;

        if (i < m_sizeDB)
        {
            m_samplesDB[i][0] = 0;
            m_samplesDB[i][1] = 0;
        }
    }

    m_ptr = 0;
    m_ptrDB = 0;

    m_state = 0;
}

#endif // INCLUDE_INTHALFBANDFILTER_H
