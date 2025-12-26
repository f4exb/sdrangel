///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include "interpolatorsifnorm.h"

void InterpolatorsIFNormalized::interpolate1(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
    if (invertIQ)
    {
        for (int pos = 0; pos < len - 1; pos += 2)
        {
            buf[pos+1] = (float) ((**it).m_real / SDR_TX_SCALED) * zeroDBm;
            buf[pos+0] = (float) ((**it).m_imag / SDR_TX_SCALED) * zeroDBm;
            ++(*it);
        }
    }
    else
    {
        for (int pos = 0; pos < len - 1; pos += 2)
        {
            buf[pos+0] = (float) ((**it).m_real / SDR_TX_SCALED) * zeroDBm;
            buf[pos+1] = (float) ((**it).m_imag / SDR_TX_SCALED) * zeroDBm;
            ++(*it);
        }
    }
}

void InterpolatorsIFNormalized::interpolate2_cen(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
	qint32 intbuf[4];
    qint32 *bufI, *bufQ;

    if (invertIQ)
    {
        bufI = &intbuf[1];
        bufQ = &intbuf[0];
    }
    else
    {
        bufI = &intbuf[0];
        bufQ = &intbuf[1];
    }

    for (int pos = 0; pos < len - 3; pos += 4)
    {
        *bufI = (**it).m_real;
        *bufQ = (**it).m_imag;
//        intbuf[2] = 0;
//        intbuf[3] = 0;

        m_interpolator2.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3]);

        buf[pos+0] = (float) (intbuf[0] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+1] = (float) (intbuf[1] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+2] = (float) (intbuf[2] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+3] = (float) (intbuf[3] / SDR_TX_SCALED) * zeroDBm;

        ++(*it);
    }
}

void InterpolatorsIFNormalized::interpolate2_inf(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
	qint32 intbuf[8];
    qint32 *bufI0, *bufQ0, *bufI1, *bufQ1;

    if (invertIQ)
    {
        bufI0 = &intbuf[1];
        bufQ0 = &intbuf[0];
        bufI1 = &intbuf[5];
        bufQ1 = &intbuf[4];
    }
    else
    {
        bufI0 = &intbuf[0];
        bufQ0 = &intbuf[1];
        bufI1 = &intbuf[4];
        bufQ1 = &intbuf[5];
    }

    for (int pos = 0; pos < len - 7; pos += 8)
    {
        std::fill(intbuf, intbuf + 8, 0);

        *bufI0 = (**it).m_real;
        *bufQ0 = (**it).m_imag;
        ++(*it);
        *bufI1 = (**it).m_real;
        *bufQ1 = (**it).m_imag;
        ++(*it);

        m_interpolator2.myInterpolateInf(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3], &intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);

        buf[pos+0] = (float) (intbuf[0] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+1] = (float) (intbuf[1] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+2] = (float) (intbuf[2] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+3] = (float) (intbuf[3] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+4] = (float) (intbuf[4] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+5] = (float) (intbuf[5] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+6] = (float) (intbuf[6] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+7] = (float) (intbuf[7] / SDR_TX_SCALED) * zeroDBm;
    }
}

void InterpolatorsIFNormalized::interpolate2_sup(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
	qint32 intbuf[8];
    qint32 *bufI0, *bufQ0, *bufI1, *bufQ1;

    if (invertIQ)
    {
        bufI0 = &intbuf[1];
        bufQ0 = &intbuf[0];
        bufI1 = &intbuf[5];
        bufQ1 = &intbuf[4];
    }
    else
    {
        bufI0 = &intbuf[0];
        bufQ0 = &intbuf[1];
        bufI1 = &intbuf[4];
        bufQ1 = &intbuf[5];
    }

    for (int pos = 0; pos < len - 7; pos += 8)
    {
        std::fill(intbuf, intbuf + 8, 0);

        *bufI0 = (**it).m_real;
        *bufQ0 = (**it).m_imag;
        ++(*it);
        *bufI1 = (**it).m_real;
        *bufQ1 = (**it).m_imag;
        ++(*it);

        m_interpolator2.myInterpolateSup(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3], &intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);

        buf[pos+0] = (float) (intbuf[0] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+1] = (float) (intbuf[1] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+2] = (float) (intbuf[2] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+3] = (float) (intbuf[3] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+4] = (float) (intbuf[4] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+5] = (float) (intbuf[5] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+6] = (float) (intbuf[6] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+7] = (float) (intbuf[7] / SDR_TX_SCALED) * zeroDBm;
    }
}

void InterpolatorsIFNormalized::interpolate4_cen(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
	qint32 intbuf[8];
    qint32 *bufI, *bufQ;

    if (invertIQ)
    {
        bufI = &intbuf[1];
        bufQ = &intbuf[0];
    }
    else
    {
        bufI = &intbuf[0];
        bufQ = &intbuf[1];
    }

	for (int pos = 0; pos < len - 7; pos += 8)
	{
        std::fill(intbuf, intbuf + 8, 0);
		*bufI  = (**it).m_real;
		*bufQ  = (**it).m_imag;

        m_interpolator2.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[4], &intbuf[5]);

        m_interpolator4.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3]);
        m_interpolator4.myInterpolate(&intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);

        buf[pos+0] = (float) (intbuf[0] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+1] = (float) (intbuf[1] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+2] = (float) (intbuf[2] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+3] = (float) (intbuf[3] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+4] = (float) (intbuf[4] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+5] = (float) (intbuf[5] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+6] = (float) (intbuf[6] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+7] = (float) (intbuf[7] / SDR_TX_SCALED) * zeroDBm;

		++(*it);
	}
}

void InterpolatorsIFNormalized::interpolate4_inf(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
    qint32 intbuf[16];
    qint32 *bufI0, *bufQ0, *bufI1, *bufQ1;

    if (invertIQ)
    {
        bufI0 = &intbuf[1];
        bufQ0 = &intbuf[0];
        bufI1 = &intbuf[9];
        bufQ1 = &intbuf[8];
    }
    else
    {
        bufI0 = &intbuf[0];
        bufQ0 = &intbuf[1];
        bufI1 = &intbuf[8];
        bufQ1 = &intbuf[9];
    }

    for (int pos = 0; pos < len - 15; pos += 16)
    {
        std::fill(intbuf, intbuf + 16, 0);
		*bufI0  = (**it).m_real;
		*bufQ0  = (**it).m_imag;
        ++(*it);
		*bufI1  = (**it).m_real;
		*bufQ1  = (**it).m_imag;
        ++(*it);

        m_interpolator2.myInterpolateInf(&intbuf[0], &intbuf[1], &intbuf[4], &intbuf[5], &intbuf[8], &intbuf[9], &intbuf[12], &intbuf[13]);

        m_interpolator4.myInterpolateInf(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3], &intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);
        m_interpolator4.myInterpolateInf(&intbuf[8], &intbuf[9], &intbuf[10], &intbuf[11], &intbuf[12], &intbuf[13], &intbuf[14], &intbuf[15]);

        for (int i = 0; i < 16; i++) {
            buf[pos+i] = (float) (intbuf[i] / SDR_TX_SCALED) * zeroDBm;
        }
    }
}

void InterpolatorsIFNormalized::interpolate4_sup(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
    qint32 intbuf[16];
    qint32 *bufI0, *bufQ0, *bufI1, *bufQ1;

    if (invertIQ)
    {
        bufI0 = &intbuf[1];
        bufQ0 = &intbuf[0];
        bufI1 = &intbuf[9];
        bufQ1 = &intbuf[8];
    }
    else
    {
        bufI0 = &intbuf[0];
        bufQ0 = &intbuf[1];
        bufI1 = &intbuf[8];
        bufQ1 = &intbuf[9];
    }

    for (int pos = 0; pos < len - 15; pos += 16)
    {
        std::fill(intbuf, intbuf + 16, 0);
		*bufI0  = (**it).m_real;
		*bufQ0  = (**it).m_imag;
        ++(*it);
		*bufI1  = (**it).m_real;
		*bufQ1  = (**it).m_imag;
        ++(*it);

        m_interpolator2.myInterpolateSup(&intbuf[0], &intbuf[1], &intbuf[4], &intbuf[5], &intbuf[8], &intbuf[9], &intbuf[12], &intbuf[13]);

        m_interpolator4.myInterpolateSup(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3], &intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);
        m_interpolator4.myInterpolateSup(&intbuf[8], &intbuf[9], &intbuf[10], &intbuf[11], &intbuf[12], &intbuf[13], &intbuf[14], &intbuf[15]);

        for (int i = 0; i < 16; i++) {
            buf[pos+i] = (float) (intbuf[i] / SDR_TX_SCALED) * zeroDBm;
        }
    }
}

void InterpolatorsIFNormalized::interpolate8_cen(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
	qint32 intbuf[16];
    qint32 *bufI, *bufQ;

    if (invertIQ)
    {
        bufI = &intbuf[1];
        bufQ = &intbuf[0];
    }
    else
    {
        bufI = &intbuf[0];
        bufQ = &intbuf[1];
    }

	for (int pos = 0; pos < len - 15; pos += 16)
	{
        std::fill(intbuf, intbuf + 16, 0);
        *bufI  = (**it).m_real;
        *bufQ  = (**it).m_imag;

        m_interpolator2.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[8], &intbuf[9]);

        m_interpolator4.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[4], &intbuf[5]);
        m_interpolator4.myInterpolate(&intbuf[8], &intbuf[9], &intbuf[12], &intbuf[13]);

        m_interpolator8.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3]);
        m_interpolator8.myInterpolate(&intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);
        m_interpolator8.myInterpolate(&intbuf[8], &intbuf[9], &intbuf[10], &intbuf[11]);
        m_interpolator8.myInterpolate(&intbuf[12], &intbuf[13], &intbuf[14], &intbuf[15]);

        buf[pos+0]  = (float) (intbuf[0]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+1]  = (float) (intbuf[1]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+2]  = (float) (intbuf[2]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+3]  = (float) (intbuf[3]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+4]  = (float) (intbuf[4]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+5]  = (float) (intbuf[5]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+6]  = (float) (intbuf[6]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+7]  = (float) (intbuf[7]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+8]  = (float) (intbuf[8]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+9]  = (float) (intbuf[9]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+10] = (float) (intbuf[10] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+11] = (float) (intbuf[11] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+12] = (float) (intbuf[12] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+13] = (float) (intbuf[13] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+14] = (float) (intbuf[14] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+15] = (float) (intbuf[15] / SDR_TX_SCALED) * zeroDBm;

        ++(*it);
	}
}

void InterpolatorsIFNormalized::interpolate8_inf(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
	qint32 intbuf[32];
    qint32 *bufI0, *bufQ0, *bufI1, *bufQ1;

    if (invertIQ)
    {
        bufI0 = &intbuf[1];
        bufQ0 = &intbuf[0];
        bufI1 = &intbuf[17];
        bufQ1 = &intbuf[16];
    }
    else
    {
        bufI0 = &intbuf[0];
        bufQ0 = &intbuf[1];
        bufI1 = &intbuf[16];
        bufQ1 = &intbuf[17];
    }

	for (int pos = 0; pos < len - 31; pos += 32)
	{
        std::fill(intbuf, intbuf + 32, 0);
		*bufI0   = (**it).m_real;
		*bufQ0   = (**it).m_imag;
        ++(*it);
		*bufI1  = (**it).m_real;
		*bufQ1  = (**it).m_imag;
        ++(*it);

        m_interpolator2.myInterpolateSup(&intbuf[0], &intbuf[1], &intbuf[8], &intbuf[9], &intbuf[16], &intbuf[17], &intbuf[24], &intbuf[25]);

        m_interpolator4.myInterpolateInf(&intbuf[0], &intbuf[1], &intbuf[4], &intbuf[5], &intbuf[8], &intbuf[9], &intbuf[12], &intbuf[13]);
        m_interpolator4.myInterpolateInf(&intbuf[16], &intbuf[17], &intbuf[20], &intbuf[21], &intbuf[24], &intbuf[25], &intbuf[28], &intbuf[29]);

        m_interpolator8.myInterpolateInf(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3], &intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);
        m_interpolator8.myInterpolateInf(&intbuf[8], &intbuf[9], &intbuf[10], &intbuf[11], &intbuf[12], &intbuf[13], &intbuf[14], &intbuf[15]);
        m_interpolator8.myInterpolateInf(&intbuf[16], &intbuf[17], &intbuf[18], &intbuf[19], &intbuf[20], &intbuf[21], &intbuf[22], &intbuf[23]);
        m_interpolator8.myInterpolateInf(&intbuf[24], &intbuf[25], &intbuf[26], &intbuf[27], &intbuf[28], &intbuf[29], &intbuf[30], &intbuf[31]);

        for (int i = 0; i < 32; i++) {
            buf[pos+i] = (float) (intbuf[i] / SDR_TX_SCALED) * zeroDBm;
        }
    }
}

void InterpolatorsIFNormalized::interpolate8_sup(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
	qint32 intbuf[32];
    qint32 *bufI0, *bufQ0, *bufI1, *bufQ1;

    if (invertIQ)
    {
        bufI0 = &intbuf[1];
        bufQ0 = &intbuf[0];
        bufI1 = &intbuf[17];
        bufQ1 = &intbuf[16];
    }
    else
    {
        bufI0 = &intbuf[0];
        bufQ0 = &intbuf[1];
        bufI1 = &intbuf[16];
        bufQ1 = &intbuf[17];
    }

	for (int pos = 0; pos < len - 31; pos += 32)
	{
        std::fill(intbuf, intbuf + 32, 0);
		*bufI0   = (**it).m_real;
		*bufQ0   = (**it).m_imag;
        ++(*it);
		*bufI1  = (**it).m_real;
		*bufQ1  = (**it).m_imag;
        ++(*it);

        m_interpolator2.myInterpolateInf(&intbuf[0], &intbuf[1], &intbuf[8], &intbuf[9], &intbuf[16], &intbuf[17], &intbuf[24], &intbuf[25]);

        m_interpolator4.myInterpolateSup(&intbuf[0], &intbuf[1], &intbuf[4], &intbuf[5], &intbuf[8], &intbuf[9], &intbuf[12], &intbuf[13]);
        m_interpolator4.myInterpolateSup(&intbuf[16], &intbuf[17], &intbuf[20], &intbuf[21], &intbuf[24], &intbuf[25], &intbuf[28], &intbuf[29]);

        m_interpolator8.myInterpolateSup(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3], &intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);
        m_interpolator8.myInterpolateSup(&intbuf[8], &intbuf[9], &intbuf[10], &intbuf[11], &intbuf[12], &intbuf[13], &intbuf[14], &intbuf[15]);
        m_interpolator8.myInterpolateSup(&intbuf[16], &intbuf[17], &intbuf[18], &intbuf[19], &intbuf[20], &intbuf[21], &intbuf[22], &intbuf[23]);
        m_interpolator8.myInterpolateSup(&intbuf[24], &intbuf[25], &intbuf[26], &intbuf[27], &intbuf[28], &intbuf[29], &intbuf[30], &intbuf[31]);

        for (int i = 0; i < 32; i++) {
            buf[pos+i] = (float) (intbuf[i] / SDR_TX_SCALED) * zeroDBm;
        }
    }
}

void InterpolatorsIFNormalized::interpolate16_cen(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
	qint32 intbuf[32];
    qint32 *bufI, *bufQ;

    if (invertIQ)
    {
        bufI = &intbuf[1];
        bufQ = &intbuf[0];
    }
    else
    {
        bufI = &intbuf[0];
        bufQ = &intbuf[1];
    }

	for (int pos = 0; pos < len - 31; pos += 32)
	{
        std::fill(intbuf, intbuf + 32, 0);
        *bufI  = (**it).m_real;
        *bufQ  = (**it).m_imag;

        m_interpolator2.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[16], &intbuf[17]);

        m_interpolator4.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[8], &intbuf[9]);
        m_interpolator4.myInterpolate(&intbuf[16], &intbuf[17], &intbuf[24], &intbuf[25]);

        m_interpolator8.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[4], &intbuf[5]);
        m_interpolator8.myInterpolate(&intbuf[8], &intbuf[9], &intbuf[12], &intbuf[13]);
        m_interpolator8.myInterpolate(&intbuf[16], &intbuf[17], &intbuf[20], &intbuf[21]);
        m_interpolator8.myInterpolate(&intbuf[24], &intbuf[25], &intbuf[28], &intbuf[29]);

        m_interpolator16.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3]);
        m_interpolator16.myInterpolate(&intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);
        m_interpolator16.myInterpolate(&intbuf[8], &intbuf[9], &intbuf[10], &intbuf[11]);
        m_interpolator16.myInterpolate(&intbuf[12], &intbuf[13], &intbuf[14], &intbuf[15]);
        m_interpolator16.myInterpolate(&intbuf[16], &intbuf[17], &intbuf[18], &intbuf[19]);
        m_interpolator16.myInterpolate(&intbuf[20], &intbuf[21], &intbuf[22], &intbuf[23]);
        m_interpolator16.myInterpolate(&intbuf[24], &intbuf[25], &intbuf[26], &intbuf[27]);
        m_interpolator16.myInterpolate(&intbuf[28], &intbuf[29], &intbuf[30], &intbuf[31]);

        buf[pos+0]  = (float) (intbuf[0]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+1]  = (float) (intbuf[1]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+2]  = (float) (intbuf[2]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+3]  = (float) (intbuf[3]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+4]  = (float) (intbuf[4]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+5]  = (float) (intbuf[5]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+6]  = (float) (intbuf[6]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+7]  = (float) (intbuf[7]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+8]  = (float) (intbuf[8]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+9]  = (float) (intbuf[9]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+10] = (float) (intbuf[10] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+11] = (float) (intbuf[11] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+12] = (float) (intbuf[12] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+13] = (float) (intbuf[13] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+14] = (float) (intbuf[14] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+15] = (float) (intbuf[15] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+16] = (float) (intbuf[16] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+17] = (float) (intbuf[17] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+18] = (float) (intbuf[18] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+19] = (float) (intbuf[19] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+20] = (float) (intbuf[20] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+21] = (float) (intbuf[21] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+22] = (float) (intbuf[22] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+23] = (float) (intbuf[23] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+24] = (float) (intbuf[24] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+25] = (float) (intbuf[25] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+26] = (float) (intbuf[26] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+27] = (float) (intbuf[27] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+28] = (float) (intbuf[28] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+29] = (float) (intbuf[29] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+30] = (float) (intbuf[30] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+31] = (float) (intbuf[31] / SDR_TX_SCALED) * zeroDBm;

		++(*it);
	}
}

void InterpolatorsIFNormalized::interpolate16_inf(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
	qint32 intbuf[64];
    qint32 *bufI0, *bufQ0, *bufI1, *bufQ1;

    if (invertIQ)
    {
        bufI0 = &intbuf[1];
        bufQ0 = &intbuf[0];
        bufI1 = &intbuf[33];
        bufQ1 = &intbuf[32];
    }
    else
    {
        bufI0 = &intbuf[0];
        bufQ0 = &intbuf[1];
        bufI1 = &intbuf[32];
        bufQ1 = &intbuf[33];
    }

	for (int pos = 0; pos < len - 63; pos += 64)
	{
        std::fill(intbuf, intbuf + 64, 0);
		*bufI0   = (**it).m_real;
		*bufQ0   = (**it).m_imag;
        ++(*it);
		*bufI1  = (**it).m_real;
		*bufQ1  = (**it).m_imag;
        ++(*it);

        m_interpolator2.myInterpolateInf(&intbuf[0], &intbuf[1], &intbuf[16], &intbuf[17], &intbuf[32], &intbuf[33], &intbuf[48], &intbuf[49]);

        m_interpolator4.myInterpolateSup(&intbuf[0], &intbuf[1], &intbuf[8], &intbuf[9], &intbuf[16], &intbuf[17], &intbuf[24], &intbuf[25]);
        m_interpolator4.myInterpolateSup(&intbuf[32], &intbuf[33], &intbuf[40], &intbuf[41], &intbuf[48], &intbuf[49], &intbuf[56], &intbuf[57]);

        m_interpolator8.myInterpolateInf(&intbuf[0],  &intbuf[1],  &intbuf[4],  &intbuf[5],  &intbuf[8],  &intbuf[9],  &intbuf[12], &intbuf[13]);
        m_interpolator8.myInterpolateInf(&intbuf[16], &intbuf[17], &intbuf[20], &intbuf[21], &intbuf[24], &intbuf[25], &intbuf[28], &intbuf[29]);
        m_interpolator8.myInterpolateInf(&intbuf[32], &intbuf[33], &intbuf[36], &intbuf[37], &intbuf[40], &intbuf[41], &intbuf[44], &intbuf[45]);
        m_interpolator8.myInterpolateInf(&intbuf[48], &intbuf[49], &intbuf[52], &intbuf[53], &intbuf[56], &intbuf[57], &intbuf[60], &intbuf[61]);

        m_interpolator16.myInterpolateInf(&intbuf[0],  &intbuf[1],  &intbuf[2],  &intbuf[3],  &intbuf[4],  &intbuf[5],  &intbuf[6],  &intbuf[7]);
        m_interpolator16.myInterpolateInf(&intbuf[8],  &intbuf[9],  &intbuf[10], &intbuf[11], &intbuf[12], &intbuf[13], &intbuf[14], &intbuf[15]);
        m_interpolator16.myInterpolateInf(&intbuf[16], &intbuf[17], &intbuf[18], &intbuf[19], &intbuf[20], &intbuf[21], &intbuf[22], &intbuf[23]);
        m_interpolator16.myInterpolateInf(&intbuf[24], &intbuf[25], &intbuf[26], &intbuf[27], &intbuf[28], &intbuf[29], &intbuf[30], &intbuf[31]);
        m_interpolator16.myInterpolateInf(&intbuf[32], &intbuf[33], &intbuf[34], &intbuf[35], &intbuf[36], &intbuf[37], &intbuf[38], &intbuf[39]);
        m_interpolator16.myInterpolateInf(&intbuf[40], &intbuf[41], &intbuf[42], &intbuf[43], &intbuf[44], &intbuf[45], &intbuf[46], &intbuf[47]);
        m_interpolator16.myInterpolateInf(&intbuf[48], &intbuf[49], &intbuf[50], &intbuf[51], &intbuf[52], &intbuf[53], &intbuf[54], &intbuf[55]);
        m_interpolator16.myInterpolateInf(&intbuf[56], &intbuf[57], &intbuf[58], &intbuf[59], &intbuf[60], &intbuf[61], &intbuf[62], &intbuf[63]);

        for (int i = 0; i < 64; i++) {
            buf[pos+i] = (float) (intbuf[i] / SDR_TX_SCALED) * zeroDBm;
        }
    }
}

void InterpolatorsIFNormalized::interpolate16_sup(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
	qint32 intbuf[64];
    qint32 *bufI0, *bufQ0, *bufI1, *bufQ1;

    if (invertIQ)
    {
        bufI0 = &intbuf[1];
        bufQ0 = &intbuf[0];
        bufI1 = &intbuf[33];
        bufQ1 = &intbuf[32];
    }
    else
    {
        bufI0 = &intbuf[0];
        bufQ0 = &intbuf[1];
        bufI1 = &intbuf[32];
        bufQ1 = &intbuf[33];
    }

	for (int pos = 0; pos < len - 63; pos += 64)
	{
        std::fill(intbuf, intbuf + 64, 0);
		*bufI0   = (**it).m_real;
		*bufQ0   = (**it).m_imag;
        ++(*it);
		*bufI1  = (**it).m_real;
		*bufQ1  = (**it).m_imag;
        ++(*it);

        m_interpolator2.myInterpolateSup(&intbuf[0], &intbuf[1], &intbuf[16], &intbuf[17], &intbuf[32], &intbuf[33], &intbuf[48], &intbuf[49]);

        m_interpolator4.myInterpolateInf(&intbuf[0], &intbuf[1], &intbuf[8], &intbuf[9], &intbuf[16], &intbuf[17], &intbuf[24], &intbuf[25]);
        m_interpolator4.myInterpolateInf(&intbuf[32], &intbuf[33], &intbuf[40], &intbuf[41], &intbuf[48], &intbuf[49], &intbuf[56], &intbuf[57]);

        m_interpolator8.myInterpolateSup(&intbuf[0],  &intbuf[1],  &intbuf[4],  &intbuf[5],  &intbuf[8],  &intbuf[9],  &intbuf[12], &intbuf[13]);
        m_interpolator8.myInterpolateSup(&intbuf[16], &intbuf[17], &intbuf[20], &intbuf[21], &intbuf[24], &intbuf[25], &intbuf[28], &intbuf[29]);
        m_interpolator8.myInterpolateSup(&intbuf[32], &intbuf[33], &intbuf[36], &intbuf[37], &intbuf[40], &intbuf[41], &intbuf[44], &intbuf[45]);
        m_interpolator8.myInterpolateSup(&intbuf[48], &intbuf[49], &intbuf[52], &intbuf[53], &intbuf[56], &intbuf[57], &intbuf[60], &intbuf[61]);

        m_interpolator16.myInterpolateSup(&intbuf[0],  &intbuf[1],  &intbuf[2],  &intbuf[3],  &intbuf[4],  &intbuf[5],  &intbuf[6],  &intbuf[7]);
        m_interpolator16.myInterpolateSup(&intbuf[8],  &intbuf[9],  &intbuf[10], &intbuf[11], &intbuf[12], &intbuf[13], &intbuf[14], &intbuf[15]);
        m_interpolator16.myInterpolateSup(&intbuf[16], &intbuf[17], &intbuf[18], &intbuf[19], &intbuf[20], &intbuf[21], &intbuf[22], &intbuf[23]);
        m_interpolator16.myInterpolateSup(&intbuf[24], &intbuf[25], &intbuf[26], &intbuf[27], &intbuf[28], &intbuf[29], &intbuf[30], &intbuf[31]);
        m_interpolator16.myInterpolateSup(&intbuf[32], &intbuf[33], &intbuf[34], &intbuf[35], &intbuf[36], &intbuf[37], &intbuf[38], &intbuf[39]);
        m_interpolator16.myInterpolateSup(&intbuf[40], &intbuf[41], &intbuf[42], &intbuf[43], &intbuf[44], &intbuf[45], &intbuf[46], &intbuf[47]);
        m_interpolator16.myInterpolateSup(&intbuf[48], &intbuf[49], &intbuf[50], &intbuf[51], &intbuf[52], &intbuf[53], &intbuf[54], &intbuf[55]);
        m_interpolator16.myInterpolateSup(&intbuf[56], &intbuf[57], &intbuf[58], &intbuf[59], &intbuf[60], &intbuf[61], &intbuf[62], &intbuf[63]);

        for (int i = 0; i < 64; i++) {
            buf[pos+i] = (float) (intbuf[i] / SDR_TX_SCALED) * zeroDBm;
        }
    }
}

void InterpolatorsIFNormalized::interpolate32_cen(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
	qint32 intbuf[64];
    qint32 *bufI, *bufQ;

    if (invertIQ)
    {
        bufI = &intbuf[1];
        bufQ = &intbuf[0];
    }
    else
    {
        bufI = &intbuf[0];
        bufQ = &intbuf[1];
    }

	for (int pos = 0; pos < len - 63; pos += 64)
	{
        std::fill(intbuf, intbuf + 64, 0);
        *bufI  = (**it).m_real;
        *bufQ  = (**it).m_imag;
        m_interpolator2.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[32], &intbuf[33]);

        m_interpolator4.myInterpolate(&intbuf[0],  &intbuf[1],  &intbuf[16], &intbuf[17]);
        m_interpolator4.myInterpolate(&intbuf[32], &intbuf[33], &intbuf[48], &intbuf[49]);

        m_interpolator8.myInterpolate(&intbuf[0],  &intbuf[1],  &intbuf[8],  &intbuf[9]);
        m_interpolator8.myInterpolate(&intbuf[16], &intbuf[17], &intbuf[24], &intbuf[25]);
        m_interpolator8.myInterpolate(&intbuf[32], &intbuf[33], &intbuf[40], &intbuf[41]);
        m_interpolator8.myInterpolate(&intbuf[48], &intbuf[49], &intbuf[56], &intbuf[57]);

        m_interpolator16.myInterpolate(&intbuf[0],  &intbuf[1],  &intbuf[4],  &intbuf[5]);
        m_interpolator16.myInterpolate(&intbuf[8],  &intbuf[9],  &intbuf[12], &intbuf[13]);
        m_interpolator16.myInterpolate(&intbuf[16], &intbuf[17], &intbuf[20], &intbuf[21]);
        m_interpolator16.myInterpolate(&intbuf[24], &intbuf[25], &intbuf[28], &intbuf[29]);
        m_interpolator16.myInterpolate(&intbuf[32], &intbuf[33], &intbuf[36], &intbuf[37]);
        m_interpolator16.myInterpolate(&intbuf[40], &intbuf[41], &intbuf[44], &intbuf[45]);
        m_interpolator16.myInterpolate(&intbuf[48], &intbuf[49], &intbuf[52], &intbuf[53]);
        m_interpolator16.myInterpolate(&intbuf[56], &intbuf[57], &intbuf[60], &intbuf[61]);

        m_interpolator32.myInterpolate(&intbuf[0],  &intbuf[1],  &intbuf[2],  &intbuf[3]);
        m_interpolator32.myInterpolate(&intbuf[4],  &intbuf[5],  &intbuf[6],  &intbuf[7]);
        m_interpolator32.myInterpolate(&intbuf[8],  &intbuf[9],  &intbuf[10], &intbuf[11]);
        m_interpolator32.myInterpolate(&intbuf[12], &intbuf[13], &intbuf[14], &intbuf[15]);
        m_interpolator32.myInterpolate(&intbuf[16], &intbuf[17], &intbuf[18], &intbuf[19]);
        m_interpolator32.myInterpolate(&intbuf[20], &intbuf[21], &intbuf[22], &intbuf[23]);
        m_interpolator32.myInterpolate(&intbuf[24], &intbuf[25], &intbuf[26], &intbuf[27]);
        m_interpolator32.myInterpolate(&intbuf[28], &intbuf[29], &intbuf[30], &intbuf[31]);
        m_interpolator32.myInterpolate(&intbuf[32], &intbuf[33], &intbuf[34], &intbuf[35]);
        m_interpolator32.myInterpolate(&intbuf[36], &intbuf[37], &intbuf[38], &intbuf[39]);
        m_interpolator32.myInterpolate(&intbuf[40], &intbuf[41], &intbuf[42], &intbuf[43]);
        m_interpolator32.myInterpolate(&intbuf[44], &intbuf[45], &intbuf[46], &intbuf[47]);
        m_interpolator32.myInterpolate(&intbuf[48], &intbuf[49], &intbuf[50], &intbuf[51]);
        m_interpolator32.myInterpolate(&intbuf[52], &intbuf[53], &intbuf[54], &intbuf[55]);
        m_interpolator32.myInterpolate(&intbuf[56], &intbuf[57], &intbuf[58], &intbuf[59]);
        m_interpolator32.myInterpolate(&intbuf[60], &intbuf[61], &intbuf[62], &intbuf[63]);

        buf[pos+0]  = (float) (intbuf[0]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+1]  = (float) (intbuf[1]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+2]  = (float) (intbuf[2]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+3]  = (float) (intbuf[3]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+4]  = (float) (intbuf[4]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+5]  = (float) (intbuf[5]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+6]  = (float) (intbuf[6]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+7]  = (float) (intbuf[7]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+8]  = (float) (intbuf[8]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+9]  = (float) (intbuf[9]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+10] = (float) (intbuf[10] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+11] = (float) (intbuf[11] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+12] = (float) (intbuf[12] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+13] = (float) (intbuf[13] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+14] = (float) (intbuf[14] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+15] = (float) (intbuf[15] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+16] = (float) (intbuf[16] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+17] = (float) (intbuf[17] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+18] = (float) (intbuf[18] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+19] = (float) (intbuf[19] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+20] = (float) (intbuf[20] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+21] = (float) (intbuf[21] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+22] = (float) (intbuf[22] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+23] = (float) (intbuf[23] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+24] = (float) (intbuf[24] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+25] = (float) (intbuf[25] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+26] = (float) (intbuf[26] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+27] = (float) (intbuf[27] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+28] = (float) (intbuf[28] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+29] = (float) (intbuf[29] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+30] = (float) (intbuf[30] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+31] = (float) (intbuf[31] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+32] = (float) (intbuf[32] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+33] = (float) (intbuf[33] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+34] = (float) (intbuf[34] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+35] = (float) (intbuf[35] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+36] = (float) (intbuf[36] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+37] = (float) (intbuf[37] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+38] = (float) (intbuf[38] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+39] = (float) (intbuf[39] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+40] = (float) (intbuf[40] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+41] = (float) (intbuf[41] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+42] = (float) (intbuf[42] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+43] = (float) (intbuf[43] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+44] = (float) (intbuf[44] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+45] = (float) (intbuf[45] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+46] = (float) (intbuf[46] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+47] = (float) (intbuf[47] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+48] = (float) (intbuf[48] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+49] = (float) (intbuf[49] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+50] = (float) (intbuf[50] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+51] = (float) (intbuf[51] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+52] = (float) (intbuf[52] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+53] = (float) (intbuf[53] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+54] = (float) (intbuf[54] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+55] = (float) (intbuf[55] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+56] = (float) (intbuf[56] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+57] = (float) (intbuf[57] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+58] = (float) (intbuf[58] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+59] = (float) (intbuf[59] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+60] = (float) (intbuf[60] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+61] = (float) (intbuf[61] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+62] = (float) (intbuf[62] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+63] = (float) (intbuf[63] / SDR_TX_SCALED) * zeroDBm;

        ++(*it);
	}
}

void InterpolatorsIFNormalized::interpolate32_inf(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
	qint32 intbuf[128];
    qint32 *bufI0, *bufQ0, *bufI1, *bufQ1;

    if (invertIQ)
    {
        bufI0 = &intbuf[1];
        bufQ0 = &intbuf[0];
        bufI1 = &intbuf[65];
        bufQ1 = &intbuf[64];
    }
    else
    {
        bufI0 = &intbuf[0];
        bufQ0 = &intbuf[1];
        bufI1 = &intbuf[64];
        bufQ1 = &intbuf[65];
    }

	for (int pos = 0; pos < len - 127; pos += 128)
	{
        std::fill(intbuf, intbuf + 128, 0);
        *bufI0  = (**it).m_real;
        *bufQ0  = (**it).m_imag;
        ++(*it);
        *bufI1  = (**it).m_real;
        *bufQ1  = (**it).m_imag;
        ++(*it);

        m_interpolator2.myInterpolateSup(&intbuf[0],   &intbuf[1],  &intbuf[32], &intbuf[33], &intbuf[64], &intbuf[65], &intbuf[96], &intbuf[97]);

        m_interpolator4.myInterpolateInf(&intbuf[0],   &intbuf[1],  &intbuf[16], &intbuf[17], &intbuf[32], &intbuf[33], &intbuf[48],  &intbuf[49]);
        m_interpolator4.myInterpolateInf(&intbuf[64],  &intbuf[65], &intbuf[80], &intbuf[81], &intbuf[96], &intbuf[97], &intbuf[112], &intbuf[113]);

        m_interpolator8.myInterpolateSup(&intbuf[0],   &intbuf[1],  &intbuf[8],   &intbuf[9],   &intbuf[16],  &intbuf[17],  &intbuf[24],  &intbuf[25]);
        m_interpolator8.myInterpolateSup(&intbuf[32],  &intbuf[33], &intbuf[40],  &intbuf[41],  &intbuf[48],  &intbuf[49],  &intbuf[56],  &intbuf[57]);
        m_interpolator8.myInterpolateSup(&intbuf[64],  &intbuf[65], &intbuf[72],  &intbuf[73],  &intbuf[80],  &intbuf[81],  &intbuf[88],  &intbuf[89]);
        m_interpolator8.myInterpolateSup(&intbuf[96],  &intbuf[97], &intbuf[104], &intbuf[105], &intbuf[112], &intbuf[113], &intbuf[120], &intbuf[121]);

        m_interpolator16.myInterpolateInf(&intbuf[0],   &intbuf[1],   &intbuf[4],   &intbuf[5],   &intbuf[8],   &intbuf[9],   &intbuf[12],  &intbuf[13]);
        m_interpolator16.myInterpolateInf(&intbuf[16],  &intbuf[17],  &intbuf[20],  &intbuf[21],  &intbuf[24],  &intbuf[25],  &intbuf[28],  &intbuf[29]);
        m_interpolator16.myInterpolateInf(&intbuf[32],  &intbuf[33],  &intbuf[36],  &intbuf[37],  &intbuf[40],  &intbuf[41],  &intbuf[44],  &intbuf[45]);
        m_interpolator16.myInterpolateInf(&intbuf[48],  &intbuf[49],  &intbuf[52],  &intbuf[53],  &intbuf[56],  &intbuf[57],  &intbuf[60],  &intbuf[61]);
        m_interpolator16.myInterpolateInf(&intbuf[64],  &intbuf[65],  &intbuf[68],  &intbuf[69],  &intbuf[72],  &intbuf[73],  &intbuf[76],  &intbuf[77]);
        m_interpolator16.myInterpolateInf(&intbuf[80],  &intbuf[81],  &intbuf[84],  &intbuf[85],  &intbuf[88],  &intbuf[89],  &intbuf[92],  &intbuf[93]);
        m_interpolator16.myInterpolateInf(&intbuf[96],  &intbuf[97],  &intbuf[100], &intbuf[101], &intbuf[104], &intbuf[105], &intbuf[108], &intbuf[109]);
        m_interpolator16.myInterpolateInf(&intbuf[112], &intbuf[113], &intbuf[116], &intbuf[117], &intbuf[120], &intbuf[121], &intbuf[124], &intbuf[125]);

        m_interpolator32.myInterpolateInf(&intbuf[0],   &intbuf[1],   &intbuf[2],   &intbuf[3],   &intbuf[4],   &intbuf[5],   &intbuf[6],   &intbuf[7]);
        m_interpolator32.myInterpolateInf(&intbuf[8],   &intbuf[9],   &intbuf[10],  &intbuf[11],  &intbuf[12],  &intbuf[13],  &intbuf[14],  &intbuf[15]);
        m_interpolator32.myInterpolateInf(&intbuf[16],  &intbuf[17],  &intbuf[18],  &intbuf[19],  &intbuf[20],  &intbuf[21],  &intbuf[22],  &intbuf[23]);
        m_interpolator32.myInterpolateInf(&intbuf[24],  &intbuf[25],  &intbuf[26],  &intbuf[27],  &intbuf[28],  &intbuf[29],  &intbuf[30],  &intbuf[31]);
        m_interpolator32.myInterpolateInf(&intbuf[32],  &intbuf[33],  &intbuf[34],  &intbuf[35],  &intbuf[36],  &intbuf[37],  &intbuf[38],  &intbuf[39]);
        m_interpolator32.myInterpolateInf(&intbuf[40],  &intbuf[41],  &intbuf[42],  &intbuf[43],  &intbuf[44],  &intbuf[45],  &intbuf[46],  &intbuf[47]);
        m_interpolator32.myInterpolateInf(&intbuf[48],  &intbuf[49],  &intbuf[50],  &intbuf[51],  &intbuf[52],  &intbuf[53],  &intbuf[54],  &intbuf[55]);
        m_interpolator32.myInterpolateInf(&intbuf[56],  &intbuf[57],  &intbuf[58],  &intbuf[59],  &intbuf[60],  &intbuf[61],  &intbuf[62],  &intbuf[63]);
        m_interpolator32.myInterpolateInf(&intbuf[64],  &intbuf[65],  &intbuf[66],  &intbuf[67],  &intbuf[68],  &intbuf[69],  &intbuf[70],  &intbuf[71]);
        m_interpolator32.myInterpolateInf(&intbuf[72],  &intbuf[73],  &intbuf[74],  &intbuf[75],  &intbuf[76],  &intbuf[77],  &intbuf[78],  &intbuf[79]);
        m_interpolator32.myInterpolateInf(&intbuf[80],  &intbuf[81],  &intbuf[82],  &intbuf[83],  &intbuf[84],  &intbuf[85],  &intbuf[86],  &intbuf[87]);
        m_interpolator32.myInterpolateInf(&intbuf[88],  &intbuf[89],  &intbuf[90],  &intbuf[91],  &intbuf[92],  &intbuf[93],  &intbuf[94],  &intbuf[95]);
        m_interpolator32.myInterpolateInf(&intbuf[96],  &intbuf[97],  &intbuf[98],  &intbuf[99],  &intbuf[100], &intbuf[101], &intbuf[102], &intbuf[103]);
        m_interpolator32.myInterpolateInf(&intbuf[104], &intbuf[105], &intbuf[106], &intbuf[107], &intbuf[108], &intbuf[109], &intbuf[110], &intbuf[111]);
        m_interpolator32.myInterpolateInf(&intbuf[112], &intbuf[113], &intbuf[114], &intbuf[115], &intbuf[116], &intbuf[117], &intbuf[118], &intbuf[119]);
        m_interpolator32.myInterpolateInf(&intbuf[120], &intbuf[121], &intbuf[122], &intbuf[123], &intbuf[124], &intbuf[125], &intbuf[126], &intbuf[127]);

        for (int i = 0; i < 128; i++) {
            buf[pos+i] = (float) (intbuf[i] / SDR_TX_SCALED) * zeroDBm;
        }
    }
}

void InterpolatorsIFNormalized::interpolate32_sup(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
	qint32 intbuf[128];
    qint32 *bufI0, *bufQ0, *bufI1, *bufQ1;

    if (invertIQ)
    {
        bufI0 = &intbuf[1];
        bufQ0 = &intbuf[0];
        bufI1 = &intbuf[65];
        bufQ1 = &intbuf[64];
    }
    else
    {
        bufI0 = &intbuf[0];
        bufQ0 = &intbuf[1];
        bufI1 = &intbuf[64];
        bufQ1 = &intbuf[65];
    }

	for (int pos = 0; pos < len - 127; pos += 128)
	{
	    std::fill(intbuf, intbuf + 128, 0);
        *bufI0  = (**it).m_real;
        *bufQ0  = (**it).m_imag;
        ++(*it);
        *bufI1  = (**it).m_real;
        *bufQ1  = (**it).m_imag;
        ++(*it);

        m_interpolator2.myInterpolateInf(&intbuf[0],   &intbuf[1],  &intbuf[32], &intbuf[33], &intbuf[64], &intbuf[65], &intbuf[96], &intbuf[97]);

        m_interpolator4.myInterpolateSup(&intbuf[0],   &intbuf[1],  &intbuf[16], &intbuf[17], &intbuf[32], &intbuf[33], &intbuf[48],  &intbuf[49]);
        m_interpolator4.myInterpolateSup(&intbuf[64],  &intbuf[65], &intbuf[80], &intbuf[81], &intbuf[96], &intbuf[97], &intbuf[112], &intbuf[113]);

        m_interpolator8.myInterpolateInf(&intbuf[0],   &intbuf[1],  &intbuf[8],   &intbuf[9],   &intbuf[16],  &intbuf[17],  &intbuf[24],  &intbuf[25]);
        m_interpolator8.myInterpolateInf(&intbuf[32],  &intbuf[33], &intbuf[40],  &intbuf[41],  &intbuf[48],  &intbuf[49],  &intbuf[56],  &intbuf[57]);
        m_interpolator8.myInterpolateInf(&intbuf[64],  &intbuf[65], &intbuf[72],  &intbuf[73],  &intbuf[80],  &intbuf[81],  &intbuf[88],  &intbuf[89]);
        m_interpolator8.myInterpolateInf(&intbuf[96],  &intbuf[97], &intbuf[104], &intbuf[105], &intbuf[112], &intbuf[113], &intbuf[120], &intbuf[121]);

        m_interpolator16.myInterpolateSup(&intbuf[0],   &intbuf[1],   &intbuf[4],   &intbuf[5],   &intbuf[8],   &intbuf[9],   &intbuf[12],  &intbuf[13]);
        m_interpolator16.myInterpolateSup(&intbuf[16],  &intbuf[17],  &intbuf[20],  &intbuf[21],  &intbuf[24],  &intbuf[25],  &intbuf[28],  &intbuf[29]);
        m_interpolator16.myInterpolateSup(&intbuf[32],  &intbuf[33],  &intbuf[36],  &intbuf[37],  &intbuf[40],  &intbuf[41],  &intbuf[44],  &intbuf[45]);
        m_interpolator16.myInterpolateSup(&intbuf[48],  &intbuf[49],  &intbuf[52],  &intbuf[53],  &intbuf[56],  &intbuf[57],  &intbuf[60],  &intbuf[61]);
        m_interpolator16.myInterpolateSup(&intbuf[64],  &intbuf[65],  &intbuf[68],  &intbuf[69],  &intbuf[72],  &intbuf[73],  &intbuf[76],  &intbuf[77]);
        m_interpolator16.myInterpolateSup(&intbuf[80],  &intbuf[81],  &intbuf[84],  &intbuf[85],  &intbuf[88],  &intbuf[89],  &intbuf[92],  &intbuf[93]);
        m_interpolator16.myInterpolateSup(&intbuf[96],  &intbuf[97],  &intbuf[100], &intbuf[101], &intbuf[104], &intbuf[105], &intbuf[108], &intbuf[109]);
        m_interpolator16.myInterpolateSup(&intbuf[112], &intbuf[113], &intbuf[116], &intbuf[117], &intbuf[120], &intbuf[121], &intbuf[124], &intbuf[125]);

        m_interpolator32.myInterpolateSup(&intbuf[0],   &intbuf[1],   &intbuf[2],   &intbuf[3],   &intbuf[4],   &intbuf[5],   &intbuf[6],   &intbuf[7]);
        m_interpolator32.myInterpolateSup(&intbuf[8],   &intbuf[9],   &intbuf[10],  &intbuf[11],  &intbuf[12],  &intbuf[13],  &intbuf[14],  &intbuf[15]);
        m_interpolator32.myInterpolateSup(&intbuf[16],  &intbuf[17],  &intbuf[18],  &intbuf[19],  &intbuf[20],  &intbuf[21],  &intbuf[22],  &intbuf[23]);
        m_interpolator32.myInterpolateSup(&intbuf[24],  &intbuf[25],  &intbuf[26],  &intbuf[27],  &intbuf[28],  &intbuf[29],  &intbuf[30],  &intbuf[31]);
        m_interpolator32.myInterpolateSup(&intbuf[32],  &intbuf[33],  &intbuf[34],  &intbuf[35],  &intbuf[36],  &intbuf[37],  &intbuf[38],  &intbuf[39]);
        m_interpolator32.myInterpolateSup(&intbuf[40],  &intbuf[41],  &intbuf[42],  &intbuf[43],  &intbuf[44],  &intbuf[45],  &intbuf[46],  &intbuf[47]);
        m_interpolator32.myInterpolateSup(&intbuf[48],  &intbuf[49],  &intbuf[50],  &intbuf[51],  &intbuf[52],  &intbuf[53],  &intbuf[54],  &intbuf[55]);
        m_interpolator32.myInterpolateSup(&intbuf[56],  &intbuf[57],  &intbuf[58],  &intbuf[59],  &intbuf[60],  &intbuf[61],  &intbuf[62],  &intbuf[63]);
        m_interpolator32.myInterpolateSup(&intbuf[64],  &intbuf[65],  &intbuf[66],  &intbuf[67],  &intbuf[68],  &intbuf[69],  &intbuf[70],  &intbuf[71]);
        m_interpolator32.myInterpolateSup(&intbuf[72],  &intbuf[73],  &intbuf[74],  &intbuf[75],  &intbuf[76],  &intbuf[77],  &intbuf[78],  &intbuf[79]);
        m_interpolator32.myInterpolateSup(&intbuf[80],  &intbuf[81],  &intbuf[82],  &intbuf[83],  &intbuf[84],  &intbuf[85],  &intbuf[86],  &intbuf[87]);
        m_interpolator32.myInterpolateSup(&intbuf[88],  &intbuf[89],  &intbuf[90],  &intbuf[91],  &intbuf[92],  &intbuf[93],  &intbuf[94],  &intbuf[95]);
        m_interpolator32.myInterpolateSup(&intbuf[96],  &intbuf[97],  &intbuf[98],  &intbuf[99],  &intbuf[100], &intbuf[101], &intbuf[102], &intbuf[103]);
        m_interpolator32.myInterpolateSup(&intbuf[104], &intbuf[105], &intbuf[106], &intbuf[107], &intbuf[108], &intbuf[109], &intbuf[110], &intbuf[111]);
        m_interpolator32.myInterpolateSup(&intbuf[112], &intbuf[113], &intbuf[114], &intbuf[115], &intbuf[116], &intbuf[117], &intbuf[118], &intbuf[119]);
        m_interpolator32.myInterpolateSup(&intbuf[120], &intbuf[121], &intbuf[122], &intbuf[123], &intbuf[124], &intbuf[125], &intbuf[126], &intbuf[127]);

        for (int i = 0; i < 128; i++) {
            buf[pos+i] = (float) (intbuf[i] / SDR_TX_SCALED) * zeroDBm;
        }
    }
}

void InterpolatorsIFNormalized::interpolate64_cen(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
	qint32 intbuf[128];
    qint32 *bufI, *bufQ;

    if (invertIQ)
    {
        bufI = &intbuf[1];
        bufQ = &intbuf[0];
    }
    else
    {
        bufI = &intbuf[0];
        bufQ = &intbuf[1];
    }

	for (int pos = 0; pos < len - 127; pos += 128)
	{
        std::fill(intbuf, intbuf + 128, 0);
        *bufI  = (**it).m_real;
        *bufQ  = (**it).m_imag;
        m_interpolator2.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[64], &intbuf[65]);

        m_interpolator4.myInterpolate(&intbuf[0],  &intbuf[1],  &intbuf[32], &intbuf[33]);
        m_interpolator4.myInterpolate(&intbuf[64], &intbuf[65], &intbuf[96], &intbuf[97]);

        m_interpolator8.myInterpolate(&intbuf[0],   &intbuf[1],   &intbuf[16],  &intbuf[17]);
        m_interpolator8.myInterpolate(&intbuf[32],  &intbuf[33],  &intbuf[48],  &intbuf[49]);
        m_interpolator8.myInterpolate(&intbuf[64],  &intbuf[65],  &intbuf[80],  &intbuf[81]);
        m_interpolator8.myInterpolate(&intbuf[96],  &intbuf[97],  &intbuf[112], &intbuf[113]);

        m_interpolator16.myInterpolate(&intbuf[0],   &intbuf[1],    &intbuf[8],   &intbuf[9]);
        m_interpolator16.myInterpolate(&intbuf[16],  &intbuf[17],   &intbuf[24],  &intbuf[25]);
        m_interpolator16.myInterpolate(&intbuf[32],  &intbuf[33],   &intbuf[40],  &intbuf[41]);
        m_interpolator16.myInterpolate(&intbuf[48],  &intbuf[49],   &intbuf[56],  &intbuf[57]);
        m_interpolator16.myInterpolate(&intbuf[64],  &intbuf[65],   &intbuf[72],  &intbuf[73]);
        m_interpolator16.myInterpolate(&intbuf[80],  &intbuf[81],   &intbuf[88],  &intbuf[89]);
        m_interpolator16.myInterpolate(&intbuf[96],  &intbuf[97],   &intbuf[104], &intbuf[105]);
        m_interpolator16.myInterpolate(&intbuf[112], &intbuf[113],  &intbuf[120], &intbuf[121]);

        m_interpolator32.myInterpolate(&intbuf[0],   &intbuf[1],    &intbuf[4],   &intbuf[5]);
        m_interpolator32.myInterpolate(&intbuf[8],   &intbuf[9],    &intbuf[12],  &intbuf[13]);
        m_interpolator32.myInterpolate(&intbuf[16],  &intbuf[17],   &intbuf[20],  &intbuf[21]);
        m_interpolator32.myInterpolate(&intbuf[24],  &intbuf[25],   &intbuf[28],  &intbuf[29]);
        m_interpolator32.myInterpolate(&intbuf[32],  &intbuf[33],   &intbuf[36],  &intbuf[37]);
        m_interpolator32.myInterpolate(&intbuf[40],  &intbuf[41],   &intbuf[44],  &intbuf[45]);
        m_interpolator32.myInterpolate(&intbuf[48],  &intbuf[49],   &intbuf[52],  &intbuf[53]);
        m_interpolator32.myInterpolate(&intbuf[56],  &intbuf[57],   &intbuf[60],  &intbuf[61]);
        m_interpolator32.myInterpolate(&intbuf[64],  &intbuf[65],   &intbuf[68],  &intbuf[69]);
        m_interpolator32.myInterpolate(&intbuf[72],  &intbuf[73],   &intbuf[76],  &intbuf[77]);
        m_interpolator32.myInterpolate(&intbuf[80],  &intbuf[81],   &intbuf[84],  &intbuf[85]);
        m_interpolator32.myInterpolate(&intbuf[88],  &intbuf[89],   &intbuf[92],  &intbuf[93]);
        m_interpolator32.myInterpolate(&intbuf[96],  &intbuf[97],   &intbuf[100], &intbuf[101]);
        m_interpolator32.myInterpolate(&intbuf[104], &intbuf[105],  &intbuf[108], &intbuf[109]);
        m_interpolator32.myInterpolate(&intbuf[112], &intbuf[113],  &intbuf[116], &intbuf[117]);
        m_interpolator32.myInterpolate(&intbuf[120], &intbuf[121],  &intbuf[124], &intbuf[125]);

        m_interpolator64.myInterpolate(&intbuf[0],   &intbuf[1],    &intbuf[2],   &intbuf[3]);
        m_interpolator64.myInterpolate(&intbuf[4],   &intbuf[5],    &intbuf[6],   &intbuf[7]);
        m_interpolator64.myInterpolate(&intbuf[8],   &intbuf[9],    &intbuf[10],  &intbuf[11]);
        m_interpolator64.myInterpolate(&intbuf[12],  &intbuf[13],   &intbuf[14],  &intbuf[15]);
        m_interpolator64.myInterpolate(&intbuf[16],  &intbuf[17],   &intbuf[18],  &intbuf[19]);
        m_interpolator64.myInterpolate(&intbuf[20],  &intbuf[21],   &intbuf[22],  &intbuf[23]);
        m_interpolator64.myInterpolate(&intbuf[24],  &intbuf[25],   &intbuf[26],  &intbuf[27]);
        m_interpolator64.myInterpolate(&intbuf[28],  &intbuf[29],   &intbuf[30],  &intbuf[31]);
        m_interpolator64.myInterpolate(&intbuf[32],  &intbuf[33],   &intbuf[34],  &intbuf[35]);
        m_interpolator64.myInterpolate(&intbuf[36],  &intbuf[37],   &intbuf[38],  &intbuf[39]);
        m_interpolator64.myInterpolate(&intbuf[40],  &intbuf[41],   &intbuf[42],  &intbuf[43]);
        m_interpolator64.myInterpolate(&intbuf[44],  &intbuf[45],   &intbuf[46],  &intbuf[47]);
        m_interpolator64.myInterpolate(&intbuf[48],  &intbuf[49],   &intbuf[50],  &intbuf[51]);
        m_interpolator64.myInterpolate(&intbuf[52],  &intbuf[53],   &intbuf[54],  &intbuf[55]);
        m_interpolator64.myInterpolate(&intbuf[56],  &intbuf[57],   &intbuf[58],  &intbuf[59]);
        m_interpolator64.myInterpolate(&intbuf[60],  &intbuf[61],   &intbuf[62],  &intbuf[63]);
        m_interpolator64.myInterpolate(&intbuf[64],  &intbuf[65],   &intbuf[66],  &intbuf[67]);
        m_interpolator64.myInterpolate(&intbuf[68],  &intbuf[69],   &intbuf[70],  &intbuf[71]);
        m_interpolator64.myInterpolate(&intbuf[72],  &intbuf[73],   &intbuf[74],  &intbuf[75]);
        m_interpolator64.myInterpolate(&intbuf[76],  &intbuf[77],   &intbuf[78],  &intbuf[79]);
        m_interpolator64.myInterpolate(&intbuf[80],  &intbuf[81],   &intbuf[82],  &intbuf[83]);
        m_interpolator64.myInterpolate(&intbuf[84],  &intbuf[85],   &intbuf[86],  &intbuf[87]);
        m_interpolator64.myInterpolate(&intbuf[88],  &intbuf[89],   &intbuf[90],  &intbuf[91]);
        m_interpolator64.myInterpolate(&intbuf[92],  &intbuf[93],   &intbuf[94],  &intbuf[95]);
        m_interpolator64.myInterpolate(&intbuf[96],  &intbuf[97],   &intbuf[98],  &intbuf[99]);
        m_interpolator64.myInterpolate(&intbuf[100], &intbuf[101],  &intbuf[102], &intbuf[103]);
        m_interpolator64.myInterpolate(&intbuf[104], &intbuf[105],  &intbuf[106], &intbuf[107]);
        m_interpolator64.myInterpolate(&intbuf[108], &intbuf[109],  &intbuf[110], &intbuf[111]);
        m_interpolator64.myInterpolate(&intbuf[112], &intbuf[113],  &intbuf[114], &intbuf[115]);
        m_interpolator64.myInterpolate(&intbuf[116], &intbuf[117],  &intbuf[118], &intbuf[119]);
        m_interpolator64.myInterpolate(&intbuf[120], &intbuf[121],  &intbuf[122], &intbuf[123]);
        m_interpolator64.myInterpolate(&intbuf[124], &intbuf[125],  &intbuf[126], &intbuf[127]);

        buf[pos+0]   = (float) (intbuf[0]   / SDR_TX_SCALED) * zeroDBm;
        buf[pos+1]   = (float) (intbuf[1]   / SDR_TX_SCALED) * zeroDBm;
        buf[pos+2]   = (float) (intbuf[2]   / SDR_TX_SCALED) * zeroDBm;
        buf[pos+3]   = (float) (intbuf[3]   / SDR_TX_SCALED) * zeroDBm;
        buf[pos+4]   = (float) (intbuf[4]   / SDR_TX_SCALED) * zeroDBm;
        buf[pos+5]   = (float) (intbuf[5]   / SDR_TX_SCALED) * zeroDBm;
        buf[pos+6]   = (float) (intbuf[6]   / SDR_TX_SCALED) * zeroDBm;
        buf[pos+7]   = (float) (intbuf[7]   / SDR_TX_SCALED) * zeroDBm;
        buf[pos+8]   = (float) (intbuf[8]   / SDR_TX_SCALED) * zeroDBm;
        buf[pos+9]   = (float) (intbuf[9]   / SDR_TX_SCALED) * zeroDBm;
        buf[pos+10]  = (float) (intbuf[10]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+11]  = (float) (intbuf[11]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+12]  = (float) (intbuf[12]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+13]  = (float) (intbuf[13]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+14]  = (float) (intbuf[14]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+15]  = (float) (intbuf[15]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+16]  = (float) (intbuf[16]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+17]  = (float) (intbuf[17]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+18]  = (float) (intbuf[18]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+19]  = (float) (intbuf[19]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+20]  = (float) (intbuf[20]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+21]  = (float) (intbuf[21]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+22]  = (float) (intbuf[22]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+23]  = (float) (intbuf[23]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+24]  = (float) (intbuf[24]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+25]  = (float) (intbuf[25]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+26]  = (float) (intbuf[26]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+27]  = (float) (intbuf[27]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+28]  = (float) (intbuf[28]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+29]  = (float) (intbuf[29]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+30]  = (float) (intbuf[30]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+31]  = (float) (intbuf[31]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+32]  = (float) (intbuf[32]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+33]  = (float) (intbuf[33]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+34]  = (float) (intbuf[34]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+35]  = (float) (intbuf[35]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+36]  = (float) (intbuf[36]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+37]  = (float) (intbuf[37]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+38]  = (float) (intbuf[38]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+39]  = (float) (intbuf[39]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+40]  = (float) (intbuf[40]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+41]  = (float) (intbuf[41]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+42]  = (float) (intbuf[42]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+43]  = (float) (intbuf[43]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+44]  = (float) (intbuf[44]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+45]  = (float) (intbuf[45]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+46]  = (float) (intbuf[46]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+47]  = (float) (intbuf[47]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+48]  = (float) (intbuf[48]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+49]  = (float) (intbuf[49]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+50]  = (float) (intbuf[50]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+51]  = (float) (intbuf[51]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+52]  = (float) (intbuf[52]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+53]  = (float) (intbuf[53]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+54]  = (float) (intbuf[54]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+55]  = (float) (intbuf[55]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+56]  = (float) (intbuf[56]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+57]  = (float) (intbuf[57]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+58]  = (float) (intbuf[58]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+59]  = (float) (intbuf[59]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+60]  = (float) (intbuf[60]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+61]  = (float) (intbuf[61]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+62]  = (float) (intbuf[62]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+63]  = (float) (intbuf[63]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+64]  = (float) (intbuf[64]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+65]  = (float) (intbuf[65]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+66]  = (float) (intbuf[66]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+67]  = (float) (intbuf[67]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+68]  = (float) (intbuf[68]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+69]  = (float) (intbuf[69]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+70]  = (float) (intbuf[70]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+71]  = (float) (intbuf[71]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+72]  = (float) (intbuf[72]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+73]  = (float) (intbuf[73]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+74]  = (float) (intbuf[74]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+75]  = (float) (intbuf[75]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+76]  = (float) (intbuf[76]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+77]  = (float) (intbuf[77]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+78]  = (float) (intbuf[78]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+79]  = (float) (intbuf[79]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+80]  = (float) (intbuf[80]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+81]  = (float) (intbuf[81]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+82]  = (float) (intbuf[82]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+83]  = (float) (intbuf[83]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+84]  = (float) (intbuf[84]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+85]  = (float) (intbuf[85]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+86]  = (float) (intbuf[86]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+87]  = (float) (intbuf[87]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+88]  = (float) (intbuf[88]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+89]  = (float) (intbuf[89]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+90]  = (float) (intbuf[90]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+91]  = (float) (intbuf[91]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+92]  = (float) (intbuf[92]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+93]  = (float) (intbuf[93]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+94]  = (float) (intbuf[94]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+95]  = (float) (intbuf[95]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+96]  = (float) (intbuf[96]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+97]  = (float) (intbuf[97]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+98]  = (float) (intbuf[98]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+99]  = (float) (intbuf[99]  / SDR_TX_SCALED) * zeroDBm;
        buf[pos+100] = (float) (intbuf[100] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+101] = (float) (intbuf[101] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+102] = (float) (intbuf[102] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+103] = (float) (intbuf[103] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+104] = (float) (intbuf[104] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+105] = (float) (intbuf[105] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+106] = (float) (intbuf[106] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+107] = (float) (intbuf[107] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+108] = (float) (intbuf[108] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+109] = (float) (intbuf[109] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+110] = (float) (intbuf[110] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+111] = (float) (intbuf[111] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+112] = (float) (intbuf[112] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+113] = (float) (intbuf[113] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+114] = (float) (intbuf[114] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+115] = (float) (intbuf[115] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+116] = (float) (intbuf[116] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+117] = (float) (intbuf[117] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+118] = (float) (intbuf[118] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+119] = (float) (intbuf[119] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+120] = (float) (intbuf[120] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+121] = (float) (intbuf[121] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+122] = (float) (intbuf[122] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+123] = (float) (intbuf[123] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+124] = (float) (intbuf[124] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+125] = (float) (intbuf[125] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+126] = (float) (intbuf[126] / SDR_TX_SCALED) * zeroDBm;
        buf[pos+127] = (float) (intbuf[127] / SDR_TX_SCALED) * zeroDBm;

        ++(*it);
	}
}

void InterpolatorsIFNormalized::interpolate64_inf(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
	qint32 intbuf[256];
    qint32 *bufI0, *bufQ0, *bufI1, *bufQ1;

    if (invertIQ)
    {
        bufI0 = &intbuf[1];
        bufQ0 = &intbuf[0];
        bufI1 = &intbuf[129];
        bufQ1 = &intbuf[128];
    }
    else
    {
        bufI0 = &intbuf[0];
        bufQ0 = &intbuf[1];
        bufI1 = &intbuf[128];
        bufQ1 = &intbuf[129];
    }

	for (int pos = 0; pos < len - 255; pos += 256)
	{
        std::fill(intbuf, intbuf + 256, 0);
        *bufI0  = (**it).m_real;
        *bufQ0  = (**it).m_imag;
        ++(*it);
        *bufI1  = (**it).m_real;
        *bufQ1  = (**it).m_imag;
        ++(*it);

        m_interpolator2.myInterpolateSup(&intbuf[0],   &intbuf[1],  &intbuf[64], &intbuf[65], &intbuf[128], &intbuf[129], &intbuf[192], &intbuf[193]);

        m_interpolator4.myInterpolateInf(&intbuf[0],    &intbuf[1],   &intbuf[32],  &intbuf[33],  &intbuf[64],  &intbuf[65],  &intbuf[96],  &intbuf[97]);
        m_interpolator4.myInterpolateInf(&intbuf[128],  &intbuf[129], &intbuf[160], &intbuf[161], &intbuf[192], &intbuf[193], &intbuf[224], &intbuf[225]);

        m_interpolator8.myInterpolateSup(&intbuf[0],   &intbuf[1],   &intbuf[16],  &intbuf[17],  &intbuf[32],  &intbuf[33],  &intbuf[48],  &intbuf[49]);
        m_interpolator8.myInterpolateSup(&intbuf[64],  &intbuf[65],  &intbuf[80],  &intbuf[81],  &intbuf[96],  &intbuf[97],  &intbuf[112], &intbuf[113]);
        m_interpolator8.myInterpolateSup(&intbuf[128], &intbuf[129], &intbuf[144], &intbuf[145], &intbuf[160], &intbuf[161], &intbuf[176], &intbuf[177]);
        m_interpolator8.myInterpolateSup(&intbuf[192], &intbuf[193], &intbuf[208], &intbuf[209], &intbuf[224], &intbuf[225], &intbuf[240], &intbuf[241]);

        m_interpolator16.myInterpolateInf(&intbuf[0],   &intbuf[1],   &intbuf[8],   &intbuf[9],   &intbuf[16],  &intbuf[17],  &intbuf[24],  &intbuf[25]);
        m_interpolator16.myInterpolateInf(&intbuf[32],  &intbuf[33],  &intbuf[40],  &intbuf[41],  &intbuf[48],  &intbuf[49],  &intbuf[56],  &intbuf[57]);
        m_interpolator16.myInterpolateInf(&intbuf[64],  &intbuf[65],  &intbuf[72],  &intbuf[73],  &intbuf[80],  &intbuf[81],  &intbuf[88],  &intbuf[89]);
        m_interpolator16.myInterpolateInf(&intbuf[96],  &intbuf[97],  &intbuf[104], &intbuf[105], &intbuf[112], &intbuf[113], &intbuf[120], &intbuf[121]);
        m_interpolator16.myInterpolateInf(&intbuf[128], &intbuf[129], &intbuf[136], &intbuf[137], &intbuf[144], &intbuf[145], &intbuf[152], &intbuf[153]);
        m_interpolator16.myInterpolateInf(&intbuf[160], &intbuf[161], &intbuf[168], &intbuf[169], &intbuf[176], &intbuf[177], &intbuf[184], &intbuf[185]);
        m_interpolator16.myInterpolateInf(&intbuf[192], &intbuf[193], &intbuf[200], &intbuf[201], &intbuf[208], &intbuf[209], &intbuf[216], &intbuf[217]);
        m_interpolator16.myInterpolateInf(&intbuf[224], &intbuf[225], &intbuf[232], &intbuf[233], &intbuf[240], &intbuf[241], &intbuf[248], &intbuf[249]);

        for (int i = 0; i < 16; i++) {
            m_interpolator32.myInterpolateSup(
                &intbuf[16*i+0],
                &intbuf[16*i+1],
                &intbuf[16*i+4],
                &intbuf[16*i+5],
                &intbuf[16*i+8],
                &intbuf[16*i+9],
                &intbuf[16*i+12],
                &intbuf[16*i+13]);
        }

        for (int i = 0; i < 32; i++) {
            m_interpolator64.myInterpolateInf(
                &intbuf[8*i+0],
                &intbuf[8*i+1],
                &intbuf[8*i+2],
                &intbuf[8*i+3],
                &intbuf[8*i+4],
                &intbuf[8*i+5],
                &intbuf[8*i+6],
                &intbuf[8*i+7]);
        }

        for (int i = 0; i < 256; i++) {
            buf[pos+i] = (float) (intbuf[i] / SDR_TX_SCALED) * zeroDBm;
        }
    }
}

void InterpolatorsIFNormalized::interpolate64_sup(SampleVector::iterator* it, float* buf, qint32 len, float zeroDBm, bool invertIQ)
{
	qint32 intbuf[256];
    qint32 *bufI0, *bufQ0, *bufI1, *bufQ1;

    if (invertIQ)
    {
        bufI0 = &intbuf[1];
        bufQ0 = &intbuf[0];
        bufI1 = &intbuf[129];
        bufQ1 = &intbuf[128];
    }
    else
    {
        bufI0 = &intbuf[0];
        bufQ0 = &intbuf[1];
        bufI1 = &intbuf[128];
        bufQ1 = &intbuf[129];
    }

	for (int pos = 0; pos < len - 255; pos += 256)
	{
	    std::fill(intbuf, intbuf + 256, 0);
        *bufI0  = (**it).m_real;
        *bufQ0  = (**it).m_imag;
        ++(*it);
        *bufI1  = (**it).m_real;
        *bufQ1  = (**it).m_imag;
        ++(*it);

        m_interpolator2.myInterpolateInf(&intbuf[0],   &intbuf[1],  &intbuf[64], &intbuf[65], &intbuf[128], &intbuf[129], &intbuf[192], &intbuf[193]);

        m_interpolator4.myInterpolateSup(&intbuf[0],    &intbuf[1],   &intbuf[32],  &intbuf[33],  &intbuf[64],  &intbuf[65],  &intbuf[96],  &intbuf[97]);
        m_interpolator4.myInterpolateSup(&intbuf[128],  &intbuf[129], &intbuf[160], &intbuf[161], &intbuf[192], &intbuf[193], &intbuf[224], &intbuf[225]);

        m_interpolator8.myInterpolateInf(&intbuf[0],   &intbuf[1],   &intbuf[16],  &intbuf[17],  &intbuf[32],  &intbuf[33],  &intbuf[48],  &intbuf[49]);
        m_interpolator8.myInterpolateInf(&intbuf[64],  &intbuf[65],  &intbuf[80],  &intbuf[81],  &intbuf[96],  &intbuf[97],  &intbuf[112], &intbuf[113]);
        m_interpolator8.myInterpolateInf(&intbuf[128], &intbuf[129], &intbuf[144], &intbuf[145], &intbuf[160], &intbuf[161], &intbuf[176], &intbuf[177]);
        m_interpolator8.myInterpolateInf(&intbuf[192], &intbuf[193], &intbuf[208], &intbuf[209], &intbuf[224], &intbuf[225], &intbuf[240], &intbuf[241]);

        m_interpolator16.myInterpolateSup(&intbuf[0],   &intbuf[1],   &intbuf[8],   &intbuf[9],   &intbuf[16],  &intbuf[17],  &intbuf[24],  &intbuf[25]);
        m_interpolator16.myInterpolateSup(&intbuf[32],  &intbuf[33],  &intbuf[40],  &intbuf[41],  &intbuf[48],  &intbuf[49],  &intbuf[56],  &intbuf[57]);
        m_interpolator16.myInterpolateSup(&intbuf[64],  &intbuf[65],  &intbuf[72],  &intbuf[73],  &intbuf[80],  &intbuf[81],  &intbuf[88],  &intbuf[89]);
        m_interpolator16.myInterpolateSup(&intbuf[96],  &intbuf[97],  &intbuf[104], &intbuf[105], &intbuf[112], &intbuf[113], &intbuf[120], &intbuf[121]);
        m_interpolator16.myInterpolateSup(&intbuf[128], &intbuf[129], &intbuf[136], &intbuf[137], &intbuf[144], &intbuf[145], &intbuf[152], &intbuf[153]);
        m_interpolator16.myInterpolateSup(&intbuf[160], &intbuf[161], &intbuf[168], &intbuf[169], &intbuf[176], &intbuf[177], &intbuf[184], &intbuf[185]);
        m_interpolator16.myInterpolateSup(&intbuf[192], &intbuf[193], &intbuf[200], &intbuf[201], &intbuf[208], &intbuf[209], &intbuf[216], &intbuf[217]);
        m_interpolator16.myInterpolateSup(&intbuf[224], &intbuf[225], &intbuf[232], &intbuf[233], &intbuf[240], &intbuf[241], &intbuf[248], &intbuf[249]);

        for (int i = 0; i < 16; i++) {
            m_interpolator32.myInterpolateInf(
                &intbuf[16*i+0],
                &intbuf[16*i+1],
                &intbuf[16*i+4],
                &intbuf[16*i+5],
                &intbuf[16*i+8],
                &intbuf[16*i+9],
                &intbuf[16*i+12],
                &intbuf[16*i+13]);
        }

        for (int i = 0; i < 32; i++) {
            m_interpolator64.myInterpolateSup(
                &intbuf[8*i+0],
                &intbuf[8*i+1],
                &intbuf[8*i+2],
                &intbuf[8*i+3],
                &intbuf[8*i+4],
                &intbuf[8*i+5],
                &intbuf[8*i+6],
                &intbuf[8*i+7]);
        }

        for (int i = 0; i < 256; i++) {
            buf[pos+i] = (float) (intbuf[i] / SDR_TX_SCALED) * zeroDBm;
        }
    }
}
