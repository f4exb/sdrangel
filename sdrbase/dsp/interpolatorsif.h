///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_SDRBASE_DSP_INTERPOLATORSIF_H_
#define INCLUDE_SDRBASE_DSP_INTERPOLATORSIF_H_

#include "dsp/dsptypes.h"
#ifdef USE_SSE4_1
#include "dsp/inthalfbandfiltereo1.h"
#else
#include "dsp/inthalfbandfilterdb.h"
#endif

#define INTERPOLATORS_HB_FILTER_ORDER_FIRST  64
#define INTERPOLATORS_HB_FILTER_ORDER_SECOND 32
#define INTERPOLATORS_HB_FILTER_ORDER_NEXT   16

template<uint OutBits, uint InBits>
struct interpolation_shifts_float
{
    static constexpr float post1  = 1.0f;
    static const uint pre2   = 0;
    static constexpr float post2  = 1.0f;
    static const uint pre4   = 0;
    static constexpr float post4  = 1.0f;
    static const uint pre8   = 0;
    static constexpr float post8  = 1.0f;
    static const uint pre16  = 0;
    static constexpr float post16 = 1.0f;
    static const uint pre32  = 0;
    static constexpr float post32 = 1.0f;
    static const uint pre64  = 0;
    static constexpr float post64 = 1.0f;
};

template<>
struct interpolation_shifts_float<16, 16>
{
    static constexpr float post1  = (1<<0)*32768.0f;
    static const uint pre2   = 1;
    static constexpr float post2  = (1<<1)*32768.0f;
    static const uint pre4   = 2;
    static constexpr float post4  = (1<<2)*32768.0f;
    static const uint pre8   = 3;
    static constexpr float post8  = (1<<3)*32768.0f;
    static const uint pre16  = 3;
    static constexpr float post16 = (1<<3)*32768.0f;
    static const uint pre32  = 3;
    static constexpr float post32 = (1<<3)*32768.0f;
    static const uint pre64  = 3;
    static constexpr float post64 = (1<<3)*32768.0f;
};

template<>
struct interpolation_shifts_float<16, 12> // never used, just an example
{
    static constexpr float post1  = (1<<4)*2048.0f;
    static const uint pre2   = 1;
    static constexpr float post2  = (1<<5)*2048.0f;
    static const uint pre4   = 2;
    static constexpr float post4  = (1<<6)*2048.0f;
    static const uint pre8   = 3;
    static constexpr float post8  = (1<<7)*2048.0f;
    static const uint pre16  = 3;
    static constexpr float post16 = (1<<7)*2048.0f;
    static const uint pre32  = 3;
    static constexpr float post32 = (1<<7)*2048.0f;
    static const uint pre64  = 3;
    static constexpr float post64 = (1<<7)*2048.0f;
};

template<uint OutBits, uint InBits>
class InterpolatorsIF
{
public:
    // interleaved I/Q input buffer
	void interpolate1(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);

    void interpolate2_cen(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);
	void interpolate2_inf(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);
	void interpolate2_sup(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);

    void interpolate4_cen(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);
	void interpolate4_inf(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);
	void interpolate4_sup(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);

    void interpolate8_cen(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);
    void interpolate8_inf(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);
    void interpolate8_sup(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);

	void interpolate16_cen(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);
	void interpolate16_inf(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);
	void interpolate16_sup(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);

	void interpolate32_cen(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);
	void interpolate32_inf(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);
	void interpolate32_sup(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);

	void interpolate64_cen(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);
	void interpolate64_inf(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);
	void interpolate64_sup(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ = false);

private:
#ifdef USE_SSE4_1
    IntHalfbandFilterEO1<INTERPOLATORS_HB_FILTER_ORDER_FIRST> m_interpolator2;  // 1st stages
    IntHalfbandFilterEO1<INTERPOLATORS_HB_FILTER_ORDER_SECOND> m_interpolator4;  // 2nd stages
    IntHalfbandFilterEO1<INTERPOLATORS_HB_FILTER_ORDER_NEXT> m_interpolator8;  // 3rd stages
    IntHalfbandFilterEO1<INTERPOLATORS_HB_FILTER_ORDER_NEXT> m_interpolator16; // 4th stages
    IntHalfbandFilterEO1<INTERPOLATORS_HB_FILTER_ORDER_NEXT> m_interpolator32; // 5th stages
    IntHalfbandFilterEO1<INTERPOLATORS_HB_FILTER_ORDER_NEXT> m_interpolator64; // 6th stages
#else
	IntHalfbandFilterDB<qint32, INTERPOLATORS_HB_FILTER_ORDER_FIRST> m_interpolator2;  // 1st stages
	IntHalfbandFilterDB<qint32, INTERPOLATORS_HB_FILTER_ORDER_SECOND> m_interpolator4;  // 2nd stages
	IntHalfbandFilterDB<qint32, INTERPOLATORS_HB_FILTER_ORDER_NEXT> m_interpolator8;  // 3rd stages
	IntHalfbandFilterDB<qint32, INTERPOLATORS_HB_FILTER_ORDER_NEXT> m_interpolator16; // 4th stages
	IntHalfbandFilterDB<qint32, INTERPOLATORS_HB_FILTER_ORDER_NEXT> m_interpolator32; // 5th stages
	IntHalfbandFilterDB<qint32, INTERPOLATORS_HB_FILTER_ORDER_NEXT> m_interpolator64; // 6th stages
#endif
};

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate1(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
{
    if (invertIQ)
    {
        for (int pos = 0; pos < len - 1; pos += 2)
        {
            buf[pos+1] = (float) ((**it).m_real >> interpolation_shifts<OutBits, InBits>::post1);
            buf[pos+0] = (float) ((**it).m_imag >> interpolation_shifts<OutBits, InBits>::post1);
            ++(*it);
        }
    }
    else
    {
        for (int pos = 0; pos < len - 1; pos += 2)
        {
            buf[pos+0] = (float) ((**it).m_real >> interpolation_shifts<OutBits, InBits>::post1);
            buf[pos+1] = (float) ((**it).m_imag >> interpolation_shifts<OutBits, InBits>::post1);
            ++(*it);
        }
    }
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate2_cen(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
        *bufI = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre2;
        *bufQ = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre2;
//        intbuf[2] = 0;
//        intbuf[3] = 0;

        m_interpolator2.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3]);

        buf[pos+0] = (float) (intbuf[0] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+1] = (float) (intbuf[1] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+2] = (float) (intbuf[2] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+3] = (float) (intbuf[3] >> interpolation_shifts<OutBits, InBits>::post2);

        ++(*it);
    }
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate2_inf(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
        memset(intbuf, 0, 8*sizeof(qint32));

        *bufI0 = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre2;
        *bufQ0 = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre2;
        ++(*it);
        *bufI1 = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre2;
        *bufQ1 = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre2;
        ++(*it);

        m_interpolator2.myInterpolateInf(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3], &intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);

        buf[pos+0] = (float) (intbuf[0] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+1] = (float) (intbuf[1] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+2] = (float) (intbuf[2] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+3] = (float) (intbuf[3] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+4] = (float) (intbuf[4] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+5] = (float) (intbuf[5] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+6] = (float) (intbuf[6] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+7] = (float) (intbuf[7] >> interpolation_shifts<OutBits, InBits>::post2);
    }
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate2_sup(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
        memset(intbuf, 0, 8*sizeof(qint32));

        *bufI0 = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre2;
        *bufQ0 = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre2;
        ++(*it);
        *bufI1 = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre2;
        *bufQ1 = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre2;
        ++(*it);

        m_interpolator2.myInterpolateSup(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3], &intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);

        buf[pos+0] = (float) (intbuf[0] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+1] = (float) (intbuf[1] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+2] = (float) (intbuf[2] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+3] = (float) (intbuf[3] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+4] = (float) (intbuf[4] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+5] = (float) (intbuf[5] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+6] = (float) (intbuf[6] >> interpolation_shifts<OutBits, InBits>::post2);
        buf[pos+7] = (float) (intbuf[7] >> interpolation_shifts<OutBits, InBits>::post2);
    }
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate4_cen(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
        memset(intbuf, 0, 8*sizeof(qint32));
		*bufI  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre4;
		*bufQ  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre4;

        m_interpolator2.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[4], &intbuf[5]);

        m_interpolator4.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3]);
        m_interpolator4.myInterpolate(&intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);

        buf[pos+0] = (float) (intbuf[0] >> interpolation_shifts<OutBits, InBits>::post4);
        buf[pos+1] = (float) (intbuf[1] >> interpolation_shifts<OutBits, InBits>::post4);
        buf[pos+2] = (float) (intbuf[2] >> interpolation_shifts<OutBits, InBits>::post4);
        buf[pos+3] = (float) (intbuf[3] >> interpolation_shifts<OutBits, InBits>::post4);
        buf[pos+4] = (float) (intbuf[4] >> interpolation_shifts<OutBits, InBits>::post4);
        buf[pos+5] = (float) (intbuf[5] >> interpolation_shifts<OutBits, InBits>::post4);
        buf[pos+6] = (float) (intbuf[6] >> interpolation_shifts<OutBits, InBits>::post4);
        buf[pos+7] = (float) (intbuf[7] >> interpolation_shifts<OutBits, InBits>::post4);

		++(*it);
	}
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate4_inf(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
        memset(intbuf, 0, 16*sizeof(qint32));
		*bufI0  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre4;
		*bufQ0  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre4;
        ++(*it);
		*bufI1  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre4;
		*bufQ1  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre4;
        ++(*it);

        m_interpolator2.myInterpolateInf(&intbuf[0], &intbuf[1], &intbuf[4], &intbuf[5], &intbuf[8], &intbuf[9], &intbuf[12], &intbuf[13]);

        m_interpolator4.myInterpolateInf(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3], &intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);
        m_interpolator4.myInterpolateInf(&intbuf[8], &intbuf[9], &intbuf[10], &intbuf[11], &intbuf[12], &intbuf[13], &intbuf[14], &intbuf[15]);

        for (int i = 0; i < 16; i++) {
            buf[pos+i] = (float) (intbuf[i] >> interpolation_shifts<OutBits, InBits>::post4);
        }
    }
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate4_sup(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
        memset(intbuf, 0, 16*sizeof(qint32));
		*bufI0  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre4;
		*bufQ0  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre4;
        ++(*it);
		*bufI1  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre4;
		*bufQ1  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre4;
        ++(*it);

        m_interpolator2.myInterpolateSup(&intbuf[0], &intbuf[1], &intbuf[4], &intbuf[5], &intbuf[8], &intbuf[9], &intbuf[12], &intbuf[13]);

        m_interpolator4.myInterpolateSup(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3], &intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);
        m_interpolator4.myInterpolateSup(&intbuf[8], &intbuf[9], &intbuf[10], &intbuf[11], &intbuf[12], &intbuf[13], &intbuf[14], &intbuf[15]);

        for (int i = 0; i < 16; i++) {
            buf[pos+i] = (float) (intbuf[i] >> interpolation_shifts<OutBits, InBits>::post4);
        }
    }
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate8_cen(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
        memset(intbuf, 0, 16*sizeof(qint32));
        *bufI  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre8;
        *bufQ  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre8;

        m_interpolator2.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[8], &intbuf[9]);

        m_interpolator4.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[4], &intbuf[5]);
        m_interpolator4.myInterpolate(&intbuf[8], &intbuf[9], &intbuf[12], &intbuf[13]);

        m_interpolator8.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3]);
        m_interpolator8.myInterpolate(&intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);
        m_interpolator8.myInterpolate(&intbuf[8], &intbuf[9], &intbuf[10], &intbuf[11]);
        m_interpolator8.myInterpolate(&intbuf[12], &intbuf[13], &intbuf[14], &intbuf[15]);

        buf[pos+0]  = (float) (intbuf[0]  >> interpolation_shifts<OutBits, InBits>::post8);
        buf[pos+1]  = (float) (intbuf[1]  >> interpolation_shifts<OutBits, InBits>::post8);
        buf[pos+2]  = (float) (intbuf[2]  >> interpolation_shifts<OutBits, InBits>::post8);
        buf[pos+3]  = (float) (intbuf[3]  >> interpolation_shifts<OutBits, InBits>::post8);
        buf[pos+4]  = (float) (intbuf[4]  >> interpolation_shifts<OutBits, InBits>::post8);
        buf[pos+5]  = (float) (intbuf[5]  >> interpolation_shifts<OutBits, InBits>::post8);
        buf[pos+6]  = (float) (intbuf[6]  >> interpolation_shifts<OutBits, InBits>::post8);
        buf[pos+7]  = (float) (intbuf[7]  >> interpolation_shifts<OutBits, InBits>::post8);
        buf[pos+8]  = (float) (intbuf[8]  >> interpolation_shifts<OutBits, InBits>::post8);
        buf[pos+9]  = (float) (intbuf[9]  >> interpolation_shifts<OutBits, InBits>::post8);
        buf[pos+10] = (float) (intbuf[10] >> interpolation_shifts<OutBits, InBits>::post8);
        buf[pos+11] = (float) (intbuf[11] >> interpolation_shifts<OutBits, InBits>::post8);
        buf[pos+12] = (float) (intbuf[12] >> interpolation_shifts<OutBits, InBits>::post8);
        buf[pos+13] = (float) (intbuf[13] >> interpolation_shifts<OutBits, InBits>::post8);
        buf[pos+14] = (float) (intbuf[14] >> interpolation_shifts<OutBits, InBits>::post8);
        buf[pos+15] = (float) (intbuf[15] >> interpolation_shifts<OutBits, InBits>::post8);

        ++(*it);
	}
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate8_inf(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
        memset(intbuf, 0, 32*sizeof(qint32));
		*bufI0   = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre8;
		*bufQ0   = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre8;
        ++(*it);
		*bufI1  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre8;
		*bufQ1  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre8;
        ++(*it);

        m_interpolator2.myInterpolateSup(&intbuf[0], &intbuf[1], &intbuf[8], &intbuf[9], &intbuf[16], &intbuf[17], &intbuf[24], &intbuf[25]);

        m_interpolator4.myInterpolateInf(&intbuf[0], &intbuf[1], &intbuf[4], &intbuf[5], &intbuf[8], &intbuf[9], &intbuf[12], &intbuf[13]);
        m_interpolator4.myInterpolateInf(&intbuf[16], &intbuf[17], &intbuf[20], &intbuf[21], &intbuf[24], &intbuf[25], &intbuf[28], &intbuf[29]);

        m_interpolator8.myInterpolateInf(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3], &intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);
        m_interpolator8.myInterpolateInf(&intbuf[8], &intbuf[9], &intbuf[10], &intbuf[11], &intbuf[12], &intbuf[13], &intbuf[14], &intbuf[15]);
        m_interpolator8.myInterpolateInf(&intbuf[16], &intbuf[17], &intbuf[18], &intbuf[19], &intbuf[20], &intbuf[21], &intbuf[22], &intbuf[23]);
        m_interpolator8.myInterpolateInf(&intbuf[24], &intbuf[25], &intbuf[26], &intbuf[27], &intbuf[28], &intbuf[29], &intbuf[30], &intbuf[31]);

        for (int i = 0; i < 32; i++) {
            buf[pos+i] = (float) (intbuf[i] >> interpolation_shifts<OutBits, InBits>::post8);
        }
    }
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate8_sup(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
        memset(intbuf, 0, 32*sizeof(qint32));
		*bufI0   = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre8;
		*bufQ0   = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre8;
        ++(*it);
		*bufI1  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre8;
		*bufQ1  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre8;
        ++(*it);

        m_interpolator2.myInterpolateInf(&intbuf[0], &intbuf[1], &intbuf[8], &intbuf[9], &intbuf[16], &intbuf[17], &intbuf[24], &intbuf[25]);

        m_interpolator4.myInterpolateSup(&intbuf[0], &intbuf[1], &intbuf[4], &intbuf[5], &intbuf[8], &intbuf[9], &intbuf[12], &intbuf[13]);
        m_interpolator4.myInterpolateSup(&intbuf[16], &intbuf[17], &intbuf[20], &intbuf[21], &intbuf[24], &intbuf[25], &intbuf[28], &intbuf[29]);

        m_interpolator8.myInterpolateSup(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3], &intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);
        m_interpolator8.myInterpolateSup(&intbuf[8], &intbuf[9], &intbuf[10], &intbuf[11], &intbuf[12], &intbuf[13], &intbuf[14], &intbuf[15]);
        m_interpolator8.myInterpolateSup(&intbuf[16], &intbuf[17], &intbuf[18], &intbuf[19], &intbuf[20], &intbuf[21], &intbuf[22], &intbuf[23]);
        m_interpolator8.myInterpolateSup(&intbuf[24], &intbuf[25], &intbuf[26], &intbuf[27], &intbuf[28], &intbuf[29], &intbuf[30], &intbuf[31]);

        for (int i = 0; i < 32; i++) {
            buf[pos+i] = (float) (intbuf[i] >> interpolation_shifts<OutBits, InBits>::post8);
        }
    }
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate16_cen(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
        memset(intbuf, 0, 32*sizeof(qint32));
        *bufI  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre16;
        *bufQ  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre16;

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

        buf[pos+0]  = (float) (intbuf[0]  >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+1]  = (float) (intbuf[1]  >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+2]  = (float) (intbuf[2]  >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+3]  = (float) (intbuf[3]  >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+4]  = (float) (intbuf[4]  >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+5]  = (float) (intbuf[5]  >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+6]  = (float) (intbuf[6]  >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+7]  = (float) (intbuf[7]  >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+8]  = (float) (intbuf[8]  >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+9]  = (float) (intbuf[9]  >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+10] = (float) (intbuf[10] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+11] = (float) (intbuf[11] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+12] = (float) (intbuf[12] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+13] = (float) (intbuf[13] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+14] = (float) (intbuf[14] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+15] = (float) (intbuf[15] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+16] = (float) (intbuf[16] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+17] = (float) (intbuf[17] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+18] = (float) (intbuf[18] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+19] = (float) (intbuf[19] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+20] = (float) (intbuf[20] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+21] = (float) (intbuf[21] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+22] = (float) (intbuf[22] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+23] = (float) (intbuf[23] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+24] = (float) (intbuf[24] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+25] = (float) (intbuf[25] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+26] = (float) (intbuf[26] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+27] = (float) (intbuf[27] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+28] = (float) (intbuf[28] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+29] = (float) (intbuf[29] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+30] = (float) (intbuf[30] >> interpolation_shifts<OutBits, InBits>::post16);
        buf[pos+31] = (float) (intbuf[31] >> interpolation_shifts<OutBits, InBits>::post16);

		++(*it);
	}
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate16_inf(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
        memset(intbuf, 0, 64*sizeof(qint32));
		*bufI0   = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre16;
		*bufQ0   = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre16;
        ++(*it);
		*bufI1  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre16;
		*bufQ1  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre16;
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
            buf[pos+i] = (float) (intbuf[i] >> interpolation_shifts<OutBits, InBits>::post16);
        }
    }
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate16_sup(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
        memset(intbuf, 0, 64*sizeof(qint32));
		*bufI0   = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre16;
		*bufQ0   = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre16;
        ++(*it);
		*bufI1  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre16;
		*bufQ1  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre16;
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
            buf[pos+i] = (float) (intbuf[i] >> interpolation_shifts<OutBits, InBits>::post16);
        }
    }
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate32_cen(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
	    memset(intbuf, 0, 64*sizeof(qint32));
        *bufI  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre32;
        *bufQ  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre32;
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

        buf[pos+0]  = (float) (intbuf[0]  >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+1]  = (float) (intbuf[1]  >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+2]  = (float) (intbuf[2]  >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+3]  = (float) (intbuf[3]  >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+4]  = (float) (intbuf[4]  >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+5]  = (float) (intbuf[5]  >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+6]  = (float) (intbuf[6]  >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+7]  = (float) (intbuf[7]  >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+8]  = (float) (intbuf[8]  >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+9]  = (float) (intbuf[9]  >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+10] = (float) (intbuf[10] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+11] = (float) (intbuf[11] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+12] = (float) (intbuf[12] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+13] = (float) (intbuf[13] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+14] = (float) (intbuf[14] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+15] = (float) (intbuf[15] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+16] = (float) (intbuf[16] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+17] = (float) (intbuf[17] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+18] = (float) (intbuf[18] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+19] = (float) (intbuf[19] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+20] = (float) (intbuf[20] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+21] = (float) (intbuf[21] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+22] = (float) (intbuf[22] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+23] = (float) (intbuf[23] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+24] = (float) (intbuf[24] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+25] = (float) (intbuf[25] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+26] = (float) (intbuf[26] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+27] = (float) (intbuf[27] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+28] = (float) (intbuf[28] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+29] = (float) (intbuf[29] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+30] = (float) (intbuf[30] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+31] = (float) (intbuf[31] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+32] = (float) (intbuf[32] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+33] = (float) (intbuf[33] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+34] = (float) (intbuf[34] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+35] = (float) (intbuf[35] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+36] = (float) (intbuf[36] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+37] = (float) (intbuf[37] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+38] = (float) (intbuf[38] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+39] = (float) (intbuf[39] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+40] = (float) (intbuf[40] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+41] = (float) (intbuf[41] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+42] = (float) (intbuf[42] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+43] = (float) (intbuf[43] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+44] = (float) (intbuf[44] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+45] = (float) (intbuf[45] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+46] = (float) (intbuf[46] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+47] = (float) (intbuf[47] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+48] = (float) (intbuf[48] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+49] = (float) (intbuf[49] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+50] = (float) (intbuf[50] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+51] = (float) (intbuf[51] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+52] = (float) (intbuf[52] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+53] = (float) (intbuf[53] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+54] = (float) (intbuf[54] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+55] = (float) (intbuf[55] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+56] = (float) (intbuf[56] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+57] = (float) (intbuf[57] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+58] = (float) (intbuf[58] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+59] = (float) (intbuf[59] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+60] = (float) (intbuf[60] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+61] = (float) (intbuf[61] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+62] = (float) (intbuf[62] >> interpolation_shifts<OutBits, InBits>::post32);
        buf[pos+63] = (float) (intbuf[63] >> interpolation_shifts<OutBits, InBits>::post32);

        ++(*it);
	}
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate32_inf(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
	    memset(intbuf, 0, 128*sizeof(qint32));
        *bufI0  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre32;
        *bufQ0  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre32;
        ++(*it);
        *bufI1  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre32;
        *bufQ1  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre32;
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
            buf[pos+i] = (float) (intbuf[i] >> interpolation_shifts<OutBits, InBits>::post32);
        }
    }
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate32_sup(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
	    memset(intbuf, 0, 128*sizeof(qint32));
        *bufI0  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre32;
        *bufQ0  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre32;
        ++(*it);
        *bufI1  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre32;
        *bufQ1  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre32;
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
            buf[pos+i] = (float) (intbuf[i] >> interpolation_shifts<OutBits, InBits>::post32);
        }
    }
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate64_cen(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
        memset(intbuf, 0, 128*sizeof(qint32));
        *bufI  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre64;
        *bufQ  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre64;
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

        buf[pos+0]   = (float) (intbuf[0]   >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+1]   = (float) (intbuf[1]   >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+2]   = (float) (intbuf[2]   >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+3]   = (float) (intbuf[3]   >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+4]   = (float) (intbuf[4]   >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+5]   = (float) (intbuf[5]   >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+6]   = (float) (intbuf[6]   >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+7]   = (float) (intbuf[7]   >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+8]   = (float) (intbuf[8]   >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+9]   = (float) (intbuf[9]   >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+10]  = (float) (intbuf[10]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+11]  = (float) (intbuf[11]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+12]  = (float) (intbuf[12]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+13]  = (float) (intbuf[13]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+14]  = (float) (intbuf[14]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+15]  = (float) (intbuf[15]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+16]  = (float) (intbuf[16]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+17]  = (float) (intbuf[17]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+18]  = (float) (intbuf[18]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+19]  = (float) (intbuf[19]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+20]  = (float) (intbuf[20]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+21]  = (float) (intbuf[21]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+22]  = (float) (intbuf[22]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+23]  = (float) (intbuf[23]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+24]  = (float) (intbuf[24]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+25]  = (float) (intbuf[25]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+26]  = (float) (intbuf[26]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+27]  = (float) (intbuf[27]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+28]  = (float) (intbuf[28]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+29]  = (float) (intbuf[29]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+30]  = (float) (intbuf[30]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+31]  = (float) (intbuf[31]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+32]  = (float) (intbuf[32]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+33]  = (float) (intbuf[33]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+34]  = (float) (intbuf[34]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+35]  = (float) (intbuf[35]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+36]  = (float) (intbuf[36]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+37]  = (float) (intbuf[37]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+38]  = (float) (intbuf[38]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+39]  = (float) (intbuf[39]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+40]  = (float) (intbuf[40]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+41]  = (float) (intbuf[41]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+42]  = (float) (intbuf[42]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+43]  = (float) (intbuf[43]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+44]  = (float) (intbuf[44]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+45]  = (float) (intbuf[45]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+46]  = (float) (intbuf[46]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+47]  = (float) (intbuf[47]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+48]  = (float) (intbuf[48]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+49]  = (float) (intbuf[49]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+50]  = (float) (intbuf[50]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+51]  = (float) (intbuf[51]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+52]  = (float) (intbuf[52]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+53]  = (float) (intbuf[53]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+54]  = (float) (intbuf[54]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+55]  = (float) (intbuf[55]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+56]  = (float) (intbuf[56]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+57]  = (float) (intbuf[57]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+58]  = (float) (intbuf[58]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+59]  = (float) (intbuf[59]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+60]  = (float) (intbuf[60]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+61]  = (float) (intbuf[61]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+62]  = (float) (intbuf[62]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+63]  = (float) (intbuf[63]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+64]  = (float) (intbuf[64]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+65]  = (float) (intbuf[65]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+66]  = (float) (intbuf[66]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+67]  = (float) (intbuf[67]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+68]  = (float) (intbuf[68]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+69]  = (float) (intbuf[69]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+70]  = (float) (intbuf[70]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+71]  = (float) (intbuf[71]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+72]  = (float) (intbuf[72]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+73]  = (float) (intbuf[73]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+74]  = (float) (intbuf[74]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+75]  = (float) (intbuf[75]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+76]  = (float) (intbuf[76]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+77]  = (float) (intbuf[77]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+78]  = (float) (intbuf[78]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+79]  = (float) (intbuf[79]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+80]  = (float) (intbuf[80]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+81]  = (float) (intbuf[81]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+82]  = (float) (intbuf[82]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+83]  = (float) (intbuf[83]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+84]  = (float) (intbuf[84]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+85]  = (float) (intbuf[85]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+86]  = (float) (intbuf[86]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+87]  = (float) (intbuf[87]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+88]  = (float) (intbuf[88]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+89]  = (float) (intbuf[89]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+90]  = (float) (intbuf[90]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+91]  = (float) (intbuf[91]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+92]  = (float) (intbuf[92]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+93]  = (float) (intbuf[93]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+94]  = (float) (intbuf[94]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+95]  = (float) (intbuf[95]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+96]  = (float) (intbuf[96]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+97]  = (float) (intbuf[97]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+98]  = (float) (intbuf[98]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+99]  = (float) (intbuf[99]  >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+100] = (float) (intbuf[100] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+101] = (float) (intbuf[101] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+102] = (float) (intbuf[102] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+103] = (float) (intbuf[103] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+104] = (float) (intbuf[104] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+105] = (float) (intbuf[105] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+106] = (float) (intbuf[106] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+107] = (float) (intbuf[107] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+108] = (float) (intbuf[108] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+109] = (float) (intbuf[109] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+110] = (float) (intbuf[110] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+111] = (float) (intbuf[111] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+112] = (float) (intbuf[112] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+113] = (float) (intbuf[113] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+114] = (float) (intbuf[114] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+115] = (float) (intbuf[115] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+116] = (float) (intbuf[116] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+117] = (float) (intbuf[117] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+118] = (float) (intbuf[118] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+119] = (float) (intbuf[119] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+120] = (float) (intbuf[120] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+121] = (float) (intbuf[121] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+122] = (float) (intbuf[122] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+123] = (float) (intbuf[123] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+124] = (float) (intbuf[124] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+125] = (float) (intbuf[125] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+126] = (float) (intbuf[126] >> interpolation_shifts<OutBits, InBits>::post64);
        buf[pos+127] = (float) (intbuf[127] >> interpolation_shifts<OutBits, InBits>::post64);

        ++(*it);
	}
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate64_inf(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
	    memset(intbuf, 0, 256*sizeof(qint32));
        *bufI0  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre64;
        *bufQ0  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre64;
        ++(*it);
        *bufI1  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre64;
        *bufQ1  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre64;
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
            buf[pos+i] = (float) (intbuf[i] >> interpolation_shifts<OutBits, InBits>::post64);
        }
    }
}

template<uint OutBits, uint InBits>
void InterpolatorsIF<OutBits, InBits>::interpolate64_sup(SampleVector::iterator* it, float* buf, qint32 len, bool invertIQ)
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
	    memset(intbuf, 0, 256*sizeof(qint32));
        *bufI0  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre64;
        *bufQ0  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre64;
        ++(*it);
        *bufI1  = (**it).m_real << interpolation_shifts<OutBits, InBits>::pre64;
        *bufQ1  = (**it).m_imag << interpolation_shifts<OutBits, InBits>::pre64;
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
            buf[pos+i] = (float) (intbuf[i] >> interpolation_shifts<OutBits, InBits>::post64);
        }
    }
}

#endif // INCLUDE_SDRBASE_DSP_INTERPOLATORSIF_H_