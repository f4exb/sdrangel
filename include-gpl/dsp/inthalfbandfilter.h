#ifndef INCLUDE_INTHALFBANDFILTER_H
#define INCLUDE_INTHALFBANDFILTER_H

#include <QtGlobal>
#include "dsp/dsptypes.h"
#include "util/export.h"

// uses Q1.14 format internally, input and output are S16

/*
 * supported filter orders: 64, 48, 32
 */
#define HB_FILTERORDER 32
#define HB_SHIFT 14

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
				m_ptr = (m_ptr + HB_FILTERORDER) % (HB_FILTERORDER + 1);

				// next state
				m_state = 1;

				// tell caller we don't have a new sample
				return false;

			default:
				// save result
				doFIR(sample);

				// advance write-pointer
				m_ptr = (m_ptr + HB_FILTERORDER) % (HB_FILTERORDER + 1);

				// next state
				m_state = 0;

				// tell caller we have a new sample
				return true;
		}
	}

	bool workDecimateCenter(qint16 *x, qint16 *y)
	{
		// insert sample into ring-buffer
		m_samples[m_ptr][0] = *x;
		m_samples[m_ptr][1] = *y;

		switch(m_state)
		{
			case 0:
				// advance write-pointer
				m_ptr = (m_ptr + HB_FILTERORDER) % (HB_FILTERORDER + 1);

				// next state
				m_state = 1;

				// tell caller we don't have a new sample
				return false;

			default:
				// save result
				doFIR(x, y);

				// advance write-pointer
				m_ptr = (m_ptr + HB_FILTERORDER) % (HB_FILTERORDER + 1);

				// next state
				m_state = 0;

				// tell caller we have a new sample
				return true;
		}
	}

	// downsample by 2, return edges of spectrum rotated into center
	bool workDecimateFullRotate(Sample* sample)
	{
		switch(m_state)
		{
			case 0:
				// insert sample into ring-buffer
				m_samples[m_ptr][0] = sample->real();
				m_samples[m_ptr][1] = sample->imag();

				// advance write-pointer
				m_ptr = (m_ptr + HB_FILTERORDER) % (HB_FILTERORDER + 1);

				// next state
				m_state = 1;

				// tell caller we don't have a new sample
				return false;

			default:
				// insert sample into ring-buffer
				m_samples[m_ptr][0] = -sample->real();
				m_samples[m_ptr][1] = sample->imag();

				// save result
				doFIR(sample);

				// advance write-pointer
				m_ptr = (m_ptr + HB_FILTERORDER) % (HB_FILTERORDER + 1);

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
				m_samples[m_ptr][0] = -sample->imag();
				m_samples[m_ptr][1] = sample->real();

				// advance write-pointer
				m_ptr = (m_ptr + HB_FILTERORDER) % (HB_FILTERORDER + 1);

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
				m_ptr = (m_ptr + HB_FILTERORDER) % (HB_FILTERORDER + 1);

				// next state
				m_state = 2;

				// tell caller we have a new sample
				return true;

			case 2:
				// insert sample into ring-buffer
				m_samples[m_ptr][0] = sample->imag();
				m_samples[m_ptr][1] = -sample->real();

				// advance write-pointer
				m_ptr = (m_ptr + HB_FILTERORDER) % (HB_FILTERORDER + 1);

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
				m_ptr = (m_ptr + HB_FILTERORDER) % (HB_FILTERORDER + 1);

				// next state
				m_state = 0;

				// tell caller we have a new sample
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
				m_ptr = (m_ptr + HB_FILTERORDER) % (HB_FILTERORDER + 1);

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
				m_ptr = (m_ptr + HB_FILTERORDER) % (HB_FILTERORDER + 1);

				// next state
				m_state = 2;

				// tell caller we have a new sample
				return true;

			case 2:
				// insert sample into ring-buffer
				m_samples[m_ptr][0] = -sample->imag();
				m_samples[m_ptr][1] = sample->real();

				// advance write-pointer
				m_ptr = (m_ptr + HB_FILTERORDER) % (HB_FILTERORDER + 1);

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
				m_ptr = (m_ptr + HB_FILTERORDER) % (HB_FILTERORDER + 1);

				// next state
				m_state = 0;

				// tell caller we have a new sample
				return true;
		}
	}

	void myDecimate(const Sample* sample1, Sample* sample2)
    {
		static const qint16 mod33[38] = { 31,32,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,
							20,21,22,23,24,25,26,27,28,29,30,31,32,0,1,2};

		m_samples[m_ptr][0] = sample1->real();
		m_samples[m_ptr][1] = sample1->imag();
		m_ptr = mod33[m_ptr + 2 - 1];

		m_samples[m_ptr][0] = sample2->real();
		m_samples[m_ptr][1] = sample2->imag();

		doFIR(sample2);

		m_ptr = mod33[m_ptr + 2 - 1];
	}

	void myDecimate(qint16 x1, qint16 y1, qint16 *x2, qint16 *y2)
	{
		static const qint16 mod33[38] = { 31,32,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,
							20,21,22,23,24,25,26,27,28,29,30,31,32,0,1,2};

		m_samples[m_ptr][0] = x1;
		m_samples[m_ptr][1] = y1;
		m_ptr = mod33[m_ptr + 2 - 1];

		m_samples[m_ptr][0] = *x2;
		m_samples[m_ptr][1] = *y2;

		doFIR(x2, y2);

		m_ptr = mod33[m_ptr + 2 - 1];
	}

protected:
	qint32 m_samples[HB_FILTERORDER + 1][2]; // Valgrind optim (from qint16)
	qint16 m_ptr;
	int m_state;

	void doFIR(Sample* sample)
	{
		// coefficents

#if HB_FILTERORDER == 64
		static const qint32 COEFF[16] = {
			-0.001114417441601693505720538368564120901 * (1 << HB_SHIFT),
			 0.001268007827185253051302527005361753254 * (1 << HB_SHIFT),
			-0.001959831378850490895410230152151598304 * (1 << HB_SHIFT),
			 0.002878308307661380308073439948657323839 * (1 << HB_SHIFT),
			-0.004071361818258721100571850826099762344 * (1 << HB_SHIFT),
			 0.005597288494657440618973431867289036745 * (1 << HB_SHIFT),
			-0.007532345003308904551886371336877346039 * (1 << HB_SHIFT),
			 0.009980346844667375288961963519795972388 * (1 << HB_SHIFT),
			-0.013092614174300500062830820979797863401 * (1 << HB_SHIFT),
			 0.01710934914871829748417297878404497169  * (1 << HB_SHIFT),
			-0.022443558692997273018576720460259821266 * (1 << HB_SHIFT),
			 0.029875811511593811098386197500076377764 * (1 << HB_SHIFT),
			-0.041086352085710403647667021687084343284 * (1 << HB_SHIFT),
			 0.060465467462665789533104998554335907102 * (1 << HB_SHIFT),
			-0.104159517495977321788203084906854201108 * (1 << HB_SHIFT),
			 0.317657589850154464805598308885237202048 * (1 << HB_SHIFT),
		};
#elif HB_FILTERORDER == 48
		static const qint32 COEFF[12] = {
		   -0.004102576237611492253332112767338912818 * (1 << HB_SHIFT),
			0.003950551047979387886410762575906119309 * (1 << HB_SHIFT),
		   -0.005807875789391703583164350277456833282 * (1 << HB_SHIFT),
			0.00823497890520805998770814682075069868  * (1 << HB_SHIFT),
		   -0.011372226513199541059195851744334504474 * (1 << HB_SHIFT),
			0.015471557140973646315984524335362948477 * (1 << HB_SHIFT),
		   -0.020944996398689276484450516591095947661 * (1 << HB_SHIFT),
			0.028568078132034283034279553703527199104 * (1 << HB_SHIFT),
		   -0.040015143905614086738964374490024056286 * (1 << HB_SHIFT),
			0.059669519431831075095828964549582451582 * (1 << HB_SHIFT),
		   -0.103669138691865420076609893840213771909 * (1 << HB_SHIFT),
			0.317491986549921390015072120149852707982 * (1 << HB_SHIFT)
		};
#elif HB_FILTERORDER == 32
		static const qint16 mod33[38] = { 31,32,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,
							20,21,22,23,24,25,26,27,28,29,30,31,32,0,1,2} ;
		static const qint32 COEFF[8] = {
			(qint32)(-0.015956912844043127236437484839370881673 * (1 << HB_SHIFT)),
			(qint32)( 0.013023031678944928940522274274371739011 * (1 << HB_SHIFT)),
			(qint32)(-0.01866942273717486777684371190844103694  * (1 << HB_SHIFT)),
			(qint32)( 0.026550887571157304190005987720724078827 * (1 << HB_SHIFT)),
			(qint32)(-0.038350314277854319344740474662103224546 * (1 << HB_SHIFT)),
			(qint32)( 0.058429248652825838128421764849917963147 * (1 << HB_SHIFT)),
			(qint32)(-0.102889802028955756885153505209018476307 * (1 << HB_SHIFT)),
			(qint32)( 0.317237706405931241260276465254719369113 * (1 << HB_SHIFT))
		};
#else
#error unsupported filter order
#endif


		// init read-pointer
		int a = mod33[m_ptr + 2 + 1]; // 0 + 1
		int b = mod33[m_ptr + 2 - 2]; //-1 - 1

		// go through samples in buffer
		qint32 iAcc = 0;
		qint32 qAcc = 0;

		for (int i = 0; i < HB_FILTERORDER / 4; i++)
		{
			// do multiply-accumulate
			//qint32 iTmp = m_samples[a][0] + m_samples[b][0]; // Valgrind optim
			//qint32 qTmp = m_samples[a][1] + m_samples[b][1]; // Valgrind optim
			iAcc += (m_samples[a][0] + m_samples[b][0]) * COEFF[i];
			qAcc += (m_samples[a][1] + m_samples[b][1]) * COEFF[i];

			// update read-pointer
			a = mod33[a + 2 + 2];
			b = mod33[b + 2 - 2];
		}

		a = mod33[a + 2 - 1];

		iAcc += ((qint32)m_samples[a][0] + 1) << (HB_SHIFT - 1);
		qAcc += ((qint32)m_samples[a][1] + 1) << (HB_SHIFT - 1);

		sample->setReal(iAcc >> HB_SHIFT);
		sample->setImag(qAcc >> HB_SHIFT);
	}

	void doFIR(qint16 *x, qint16 *y)
	{
		// coefficents

	#if HB_FILTERORDER == 64
		static const qint32 COEFF[16] = {
			-0.001114417441601693505720538368564120901 * (1 << HB_SHIFT),
			 0.001268007827185253051302527005361753254 * (1 << HB_SHIFT),
			-0.001959831378850490895410230152151598304 * (1 << HB_SHIFT),
			 0.002878308307661380308073439948657323839 * (1 << HB_SHIFT),
			-0.004071361818258721100571850826099762344 * (1 << HB_SHIFT),
			 0.005597288494657440618973431867289036745 * (1 << HB_SHIFT),
			-0.007532345003308904551886371336877346039 * (1 << HB_SHIFT),
			 0.009980346844667375288961963519795972388 * (1 << HB_SHIFT),
			-0.013092614174300500062830820979797863401 * (1 << HB_SHIFT),
			 0.01710934914871829748417297878404497169  * (1 << HB_SHIFT),
			-0.022443558692997273018576720460259821266 * (1 << HB_SHIFT),
			 0.029875811511593811098386197500076377764 * (1 << HB_SHIFT),
			-0.041086352085710403647667021687084343284 * (1 << HB_SHIFT),
			 0.060465467462665789533104998554335907102 * (1 << HB_SHIFT),
			-0.104159517495977321788203084906854201108 * (1 << HB_SHIFT),
			 0.317657589850154464805598308885237202048 * (1 << HB_SHIFT),
		};
	#elif HB_FILTERORDER == 48
		static const qint32 COEFF[12] = {
		   -0.004102576237611492253332112767338912818 * (1 << HB_SHIFT),
			0.003950551047979387886410762575906119309 * (1 << HB_SHIFT),
		   -0.005807875789391703583164350277456833282 * (1 << HB_SHIFT),
			0.00823497890520805998770814682075069868  * (1 << HB_SHIFT),
		   -0.011372226513199541059195851744334504474 * (1 << HB_SHIFT),
			0.015471557140973646315984524335362948477 * (1 << HB_SHIFT),
		   -0.020944996398689276484450516591095947661 * (1 << HB_SHIFT),
			0.028568078132034283034279553703527199104 * (1 << HB_SHIFT),
		   -0.040015143905614086738964374490024056286 * (1 << HB_SHIFT),
			0.059669519431831075095828964549582451582 * (1 << HB_SHIFT),
		   -0.103669138691865420076609893840213771909 * (1 << HB_SHIFT),
			0.317491986549921390015072120149852707982 * (1 << HB_SHIFT)
		};
	#elif HB_FILTERORDER == 32
		static const qint16 mod33[38] = { 31,32,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,
							20,21,22,23,24,25,26,27,28,29,30,31,32,0,1,2} ;
		static const qint32 COEFF[8] = {
			(qint32)(-0.015956912844043127236437484839370881673 * (1 << HB_SHIFT)),
			(qint32)( 0.013023031678944928940522274274371739011 * (1 << HB_SHIFT)),
			(qint32)(-0.01866942273717486777684371190844103694  * (1 << HB_SHIFT)),
			(qint32)( 0.026550887571157304190005987720724078827 * (1 << HB_SHIFT)),
			(qint32)(-0.038350314277854319344740474662103224546 * (1 << HB_SHIFT)),
			(qint32)( 0.058429248652825838128421764849917963147 * (1 << HB_SHIFT)),
			(qint32)(-0.102889802028955756885153505209018476307 * (1 << HB_SHIFT)),
			(qint32)( 0.317237706405931241260276465254719369113 * (1 << HB_SHIFT))
		};
	#else
	#error unsupported filter order
	#endif


		// init read-pointer
		int a = mod33[m_ptr + 2 + 1]; // 0 + 1
		int b = mod33[m_ptr + 2 - 2]; //-1 - 1

		// go through samples in buffer
		qint32 iAcc = 0;
		qint32 qAcc = 0;

		for (int i = 0; i < HB_FILTERORDER / 4; i++)
		{
			// do multiply-accumulate
			//qint32 iTmp = m_samples[a][0] + m_samples[b][0]; // Valgrind optim
			//qint32 qTmp = m_samples[a][1] + m_samples[b][1]; // Valgrind optim
			iAcc += (m_samples[a][0] + m_samples[b][0]) * COEFF[i];
			qAcc += (m_samples[a][1] + m_samples[b][1]) * COEFF[i];

			// update read-pointer
			a = mod33[a + 2 + 2];
			b = mod33[b + 2 - 2];
		}

		a = mod33[a + 2 - 1];

		iAcc += ((qint32)m_samples[a][0] + 1) << (HB_SHIFT - 1);
		qAcc += ((qint32)m_samples[a][1] + 1) << (HB_SHIFT - 1);

		*x = iAcc >> HB_SHIFT;
		*y = qAcc >> HB_SHIFT;
	}

};

#endif // INCLUDE_INTHALFBANDFILTER_H
