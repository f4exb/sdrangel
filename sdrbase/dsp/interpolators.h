///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2017 Edouard Griffiths, F4EXB                              //
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

#ifndef INCLUDE_GPL_DSP_INTERPOLATORS_H_
#define INCLUDE_GPL_DSP_INTERPOLATORS_H_

#include "dsp/dsptypes.h"
#ifdef USE_SSE4_1
#include "dsp/inthalfbandfiltereo1.h"
#else
#include "dsp/inthalfbandfilterdb.h"
#endif

#define INTERPOLATORS_HB_FILTER_ORDER_FIRST  64
#define INTERPOLATORS_HB_FILTER_ORDER_SECOND 32
#define INTERPOLATORS_HB_FILTER_ORDER_NEXT   16

template<uint SdrBits, uint OutputBits>
struct interpolation_shifts
{
    static const uint post1  = 0;
    static const uint pre2   = 0;
    static const uint post2  = 0;
    static const uint pre4   = 0;
    static const uint post4  = 0;
    static const uint pre8   = 0;
    static const uint post8  = 0;
    static const uint pre16  = 0;
    static const uint post16 = 0;
    static const uint pre32  = 0;
    static const uint post32 = 0;
    static const uint pre64  = 0;
    static const uint post64 = 0;
};

template<>
struct interpolation_shifts<16, 16>
{
    static const uint post1  = 0;
    static const uint pre2   = 1;
    static const uint post2  = 1;
    static const uint pre4   = 2;
    static const uint post4  = 2;
    static const uint pre8   = 3;
    static const uint post8  = 3;
    static const uint pre16  = 3;
    static const uint post16 = 3;
    static const uint pre32  = 3;
    static const uint post32 = 3;
    static const uint pre64  = 3;
    static const uint post64 = 3;
};

template<>
struct interpolation_shifts<16, 12>
{
    static const uint post1  = 4;
    static const uint pre2   = 1;
    static const uint post2  = 5;
    static const uint pre4   = 2;
    static const uint post4  = 6;
    static const uint pre8   = 3;
    static const uint post8  = 7;
    static const uint pre16  = 3;
    static const uint post16 = 7;
    static const uint pre32  = 3;
    static const uint post32 = 7;
    static const uint pre64  = 3;
    static const uint post64 = 7;
};

template<>
struct interpolation_shifts<16, 8>
{
    static const uint post1  = 8;
    static const uint pre2   = 1;
    static const uint post2  = 9;
    static const uint pre4   = 2;
    static const uint post4  = 10;
    static const uint pre8   = 3;
    static const uint post8  = 11;
    static const uint pre16  = 3;
    static const uint post16 = 11;
    static const uint pre32  = 3;
    static const uint post32 = 11;
    static const uint pre64  = 3;
    static const uint post64 = 11;
};

template<typename T, uint SdrBits, uint OutputBits>
class Interpolators
{
public:
    // interleaved I/Q input buffer
	void interpolate1(SampleVector::iterator* it, T* buf, qint32 len);
	void interpolate2_cen(SampleVector::iterator* it, T* buf, qint32 len);
	void interpolate4_cen(SampleVector::iterator* it, T* buf, qint32 len);
	void interpolate8_cen(SampleVector::iterator* it, T* buf, qint32 len);
	void interpolate16_cen(SampleVector::iterator* it, T* buf, qint32 len);
	void interpolate32_cen(SampleVector::iterator* it, T* buf, qint32 len);
	void interpolate64_cen(SampleVector::iterator* it, T* buf, qint32 len);

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

template<typename T, uint SdrBits, uint OutputBits>
void Interpolators<T, SdrBits, OutputBits>::interpolate1(SampleVector::iterator* it, T* buf, qint32 len)
{
	for (int pos = 0; pos < len - 1; pos += 2)
	{
	    buf[pos+0] = (**it).m_real >> interpolation_shifts<SdrBits, OutputBits>::post1;
	    buf[pos+1] = (**it).m_imag >> interpolation_shifts<SdrBits, OutputBits>::post1;
		++(*it);
	}
}

template<typename T, uint SdrBits, uint OutputBits>
void Interpolators<T, SdrBits, OutputBits>::interpolate2_cen(SampleVector::iterator* it, T* buf, qint32 len)
{
	qint32 intbuf[4];

    for (int pos = 0; pos < len - 3; pos += 4)
    {
        intbuf[0] = (**it).m_real << interpolation_shifts<SdrBits, OutputBits>::pre2;
        intbuf[1] = (**it).m_imag << interpolation_shifts<SdrBits, OutputBits>::pre2;
//        intbuf[2] = 0;
//        intbuf[3] = 0;

        m_interpolator2.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3]);

        buf[pos+0] = intbuf[0] >> interpolation_shifts<SdrBits, OutputBits>::post2;
        buf[pos+1] = intbuf[1] >> interpolation_shifts<SdrBits, OutputBits>::post2;
        buf[pos+2] = intbuf[2] >> interpolation_shifts<SdrBits, OutputBits>::post2;
        buf[pos+3] = intbuf[3] >> interpolation_shifts<SdrBits, OutputBits>::post2;

        ++(*it);
    }
}

template<typename T, uint SdrBits, uint OutputBits>
void Interpolators<T, SdrBits, OutputBits>::interpolate4_cen(SampleVector::iterator* it, T* buf, qint32 len)
{
	qint32 intbuf[8];

	for (int pos = 0; pos < len - 7; pos += 8)
	{
        memset(intbuf, 0, 8*sizeof(qint32));
		intbuf[0]  = (**it).m_real << interpolation_shifts<SdrBits, OutputBits>::pre4;
		intbuf[1]  = (**it).m_imag << interpolation_shifts<SdrBits, OutputBits>::pre4;

        m_interpolator2.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[4], &intbuf[5]);

        m_interpolator4.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3]);
        m_interpolator4.myInterpolate(&intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);

        buf[pos+0] = intbuf[0] >> interpolation_shifts<SdrBits, OutputBits>::post4;
        buf[pos+1] = intbuf[1] >> interpolation_shifts<SdrBits, OutputBits>::post4;
        buf[pos+2] = intbuf[2] >> interpolation_shifts<SdrBits, OutputBits>::post4;
        buf[pos+3] = intbuf[3] >> interpolation_shifts<SdrBits, OutputBits>::post4;
        buf[pos+4] = intbuf[4] >> interpolation_shifts<SdrBits, OutputBits>::post4;
        buf[pos+5] = intbuf[5] >> interpolation_shifts<SdrBits, OutputBits>::post4;
        buf[pos+6] = intbuf[6] >> interpolation_shifts<SdrBits, OutputBits>::post4;
        buf[pos+7] = intbuf[7] >> interpolation_shifts<SdrBits, OutputBits>::post4;

		++(*it);
	}
}

template<typename T, uint SdrBits, uint OutputBits>
void Interpolators<T, SdrBits, OutputBits>::interpolate8_cen(SampleVector::iterator* it, T* buf, qint32 len)
{
	qint32 intbuf[16];

	for (int pos = 0; pos < len - 15; pos += 16)
	{
        memset(intbuf, 0, 16*sizeof(qint32));
        intbuf[0]  = (**it).m_real << interpolation_shifts<SdrBits, OutputBits>::pre8;
        intbuf[1]  = (**it).m_imag << interpolation_shifts<SdrBits, OutputBits>::pre8;

        m_interpolator2.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[8], &intbuf[9]);

        m_interpolator4.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[4], &intbuf[5]);
        m_interpolator4.myInterpolate(&intbuf[8], &intbuf[9], &intbuf[12], &intbuf[13]);

        m_interpolator8.myInterpolate(&intbuf[0], &intbuf[1], &intbuf[2], &intbuf[3]);
        m_interpolator8.myInterpolate(&intbuf[4], &intbuf[5], &intbuf[6], &intbuf[7]);
        m_interpolator8.myInterpolate(&intbuf[8], &intbuf[9], &intbuf[10], &intbuf[11]);
        m_interpolator8.myInterpolate(&intbuf[12], &intbuf[13], &intbuf[14], &intbuf[15]);

        buf[pos+0]  = intbuf[0]  >> interpolation_shifts<SdrBits, OutputBits>::post8;
        buf[pos+1]  = intbuf[1]  >> interpolation_shifts<SdrBits, OutputBits>::post8;
        buf[pos+2]  = intbuf[2]  >> interpolation_shifts<SdrBits, OutputBits>::post8;
        buf[pos+3]  = intbuf[3]  >> interpolation_shifts<SdrBits, OutputBits>::post8;
        buf[pos+4]  = intbuf[4]  >> interpolation_shifts<SdrBits, OutputBits>::post8;
        buf[pos+5]  = intbuf[5]  >> interpolation_shifts<SdrBits, OutputBits>::post8;
        buf[pos+6]  = intbuf[6]  >> interpolation_shifts<SdrBits, OutputBits>::post8;
        buf[pos+7]  = intbuf[7]  >> interpolation_shifts<SdrBits, OutputBits>::post8;
        buf[pos+8]  = intbuf[8]  >> interpolation_shifts<SdrBits, OutputBits>::post8;
        buf[pos+9]  = intbuf[9]  >> interpolation_shifts<SdrBits, OutputBits>::post8;
        buf[pos+10] = intbuf[10] >> interpolation_shifts<SdrBits, OutputBits>::post8;
        buf[pos+11] = intbuf[11] >> interpolation_shifts<SdrBits, OutputBits>::post8;
        buf[pos+12] = intbuf[12] >> interpolation_shifts<SdrBits, OutputBits>::post8;
        buf[pos+13] = intbuf[13] >> interpolation_shifts<SdrBits, OutputBits>::post8;
        buf[pos+14] = intbuf[14] >> interpolation_shifts<SdrBits, OutputBits>::post8;
        buf[pos+15] = intbuf[15] >> interpolation_shifts<SdrBits, OutputBits>::post8;

        ++(*it);
	}
}

template<typename T, uint SdrBits, uint OutputBits>
void Interpolators<T, SdrBits, OutputBits>::interpolate16_cen(SampleVector::iterator* it, T* buf, qint32 len)
{
	qint32 intbuf[32];

	for (int pos = 0; pos < len - 31; pos += 32)
	{
        memset(intbuf, 0, 32*sizeof(qint32));
        intbuf[0]  = (**it).m_real << interpolation_shifts<SdrBits, OutputBits>::pre16;
        intbuf[1]  = (**it).m_imag << interpolation_shifts<SdrBits, OutputBits>::pre16;

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

        buf[pos+0]  = intbuf[0]  >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+1]  = intbuf[1]  >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+2]  = intbuf[2]  >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+3]  = intbuf[3]  >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+4]  = intbuf[4]  >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+5]  = intbuf[5]  >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+6]  = intbuf[6]  >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+7]  = intbuf[7]  >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+8]  = intbuf[8]  >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+9]  = intbuf[9]  >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+10] = intbuf[10] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+11] = intbuf[11] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+12] = intbuf[12] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+13] = intbuf[13] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+14] = intbuf[14] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+15] = intbuf[15] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+16] = intbuf[16] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+17] = intbuf[17] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+18] = intbuf[18] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+19] = intbuf[19] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+20] = intbuf[20] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+21] = intbuf[21] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+22] = intbuf[22] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+23] = intbuf[23] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+24] = intbuf[24] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+25] = intbuf[25] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+26] = intbuf[26] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+27] = intbuf[27] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+28] = intbuf[28] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+29] = intbuf[29] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+30] = intbuf[30] >> interpolation_shifts<SdrBits, OutputBits>::post16;
        buf[pos+31] = intbuf[31] >> interpolation_shifts<SdrBits, OutputBits>::post16;

		++(*it);
	}
}

template<typename T, uint SdrBits, uint OutputBits>
void Interpolators<T, SdrBits, OutputBits>::interpolate32_cen(SampleVector::iterator* it, T* buf, qint32 len)
{
	qint32 intbuf[64];

	for (int pos = 0; pos < len - 63; pos += 64)
	{
	    memset(intbuf, 0, 64*sizeof(qint32));
        intbuf[0]  = (**it).m_real << interpolation_shifts<SdrBits, OutputBits>::pre32;
        intbuf[1]  = (**it).m_imag << interpolation_shifts<SdrBits, OutputBits>::pre32;
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

        buf[pos+0]  = intbuf[0]  >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+1]  = intbuf[1]  >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+2]  = intbuf[2]  >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+3]  = intbuf[3]  >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+4]  = intbuf[4]  >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+5]  = intbuf[5]  >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+6]  = intbuf[6]  >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+7]  = intbuf[7]  >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+8]  = intbuf[8]  >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+9]  = intbuf[9]  >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+10] = intbuf[10] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+11] = intbuf[11] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+12] = intbuf[12] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+13] = intbuf[13] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+14] = intbuf[14] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+15] = intbuf[15] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+16] = intbuf[16] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+17] = intbuf[17] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+18] = intbuf[18] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+19] = intbuf[19] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+20] = intbuf[20] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+21] = intbuf[21] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+22] = intbuf[22] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+23] = intbuf[23] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+24] = intbuf[24] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+25] = intbuf[25] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+26] = intbuf[26] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+27] = intbuf[27] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+28] = intbuf[28] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+29] = intbuf[29] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+30] = intbuf[30] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+31] = intbuf[31] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+32] = intbuf[32] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+33] = intbuf[33] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+34] = intbuf[34] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+35] = intbuf[35] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+36] = intbuf[36] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+37] = intbuf[37] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+38] = intbuf[38] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+39] = intbuf[39] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+40] = intbuf[40] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+41] = intbuf[41] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+42] = intbuf[42] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+43] = intbuf[43] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+44] = intbuf[44] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+45] = intbuf[45] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+46] = intbuf[46] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+47] = intbuf[47] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+48] = intbuf[48] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+49] = intbuf[49] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+50] = intbuf[50] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+51] = intbuf[51] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+52] = intbuf[52] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+53] = intbuf[53] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+54] = intbuf[54] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+55] = intbuf[55] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+56] = intbuf[56] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+57] = intbuf[57] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+58] = intbuf[58] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+59] = intbuf[59] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+60] = intbuf[60] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+61] = intbuf[61] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+62] = intbuf[62] >> interpolation_shifts<SdrBits, OutputBits>::post32;
        buf[pos+63] = intbuf[63] >> interpolation_shifts<SdrBits, OutputBits>::post32;

        ++(*it);
	}
}

template<typename T, uint SdrBits, uint OutputBits>
void Interpolators<T, SdrBits, OutputBits>::interpolate64_cen(SampleVector::iterator* it, T* buf, qint32 len)
{
	qint32 intbuf[128];

	for (int pos = 0; pos < len - 127; pos += 128)
	{
        memset(intbuf, 0, 128*sizeof(qint32));
        intbuf[0]  = (**it).m_real << interpolation_shifts<SdrBits, OutputBits>::pre64;
        intbuf[1]  = (**it).m_imag << interpolation_shifts<SdrBits, OutputBits>::pre64;
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

        buf[pos+0]   = intbuf[0]   >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+1]   = intbuf[1]   >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+2]   = intbuf[2]   >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+3]   = intbuf[3]   >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+4]   = intbuf[4]   >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+5]   = intbuf[5]   >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+6]   = intbuf[6]   >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+7]   = intbuf[7]   >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+8]   = intbuf[8]   >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+9]   = intbuf[9]   >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+10]  = intbuf[10]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+11]  = intbuf[11]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+12]  = intbuf[12]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+13]  = intbuf[13]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+14]  = intbuf[14]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+15]  = intbuf[15]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+16]  = intbuf[16]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+17]  = intbuf[17]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+18]  = intbuf[18]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+19]  = intbuf[19]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+20]  = intbuf[20]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+21]  = intbuf[21]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+22]  = intbuf[22]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+23]  = intbuf[23]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+24]  = intbuf[24]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+25]  = intbuf[25]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+26]  = intbuf[26]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+27]  = intbuf[27]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+28]  = intbuf[28]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+29]  = intbuf[29]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+30]  = intbuf[30]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+31]  = intbuf[31]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+32]  = intbuf[32]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+33]  = intbuf[33]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+34]  = intbuf[34]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+35]  = intbuf[35]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+36]  = intbuf[36]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+37]  = intbuf[37]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+38]  = intbuf[38]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+39]  = intbuf[39]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+40]  = intbuf[40]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+41]  = intbuf[41]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+42]  = intbuf[42]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+43]  = intbuf[43]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+44]  = intbuf[44]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+45]  = intbuf[45]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+46]  = intbuf[46]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+47]  = intbuf[47]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+48]  = intbuf[48]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+49]  = intbuf[49]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+50]  = intbuf[50]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+51]  = intbuf[51]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+52]  = intbuf[52]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+53]  = intbuf[53]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+54]  = intbuf[54]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+55]  = intbuf[55]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+56]  = intbuf[56]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+57]  = intbuf[57]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+58]  = intbuf[58]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+59]  = intbuf[59]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+60]  = intbuf[60]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+61]  = intbuf[61]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+62]  = intbuf[62]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+63]  = intbuf[63]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+64]  = intbuf[64]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+65]  = intbuf[65]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+66]  = intbuf[66]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+67]  = intbuf[67]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+68]  = intbuf[68]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+69]  = intbuf[69]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+70]  = intbuf[70]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+71]  = intbuf[71]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+72]  = intbuf[72]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+73]  = intbuf[73]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+74]  = intbuf[74]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+75]  = intbuf[75]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+76]  = intbuf[76]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+77]  = intbuf[77]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+78]  = intbuf[78]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+79]  = intbuf[79]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+80]  = intbuf[80]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+81]  = intbuf[81]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+82]  = intbuf[82]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+83]  = intbuf[83]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+84]  = intbuf[84]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+85]  = intbuf[85]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+86]  = intbuf[86]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+87]  = intbuf[87]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+88]  = intbuf[88]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+89]  = intbuf[89]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+90]  = intbuf[90]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+91]  = intbuf[91]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+92]  = intbuf[92]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+93]  = intbuf[93]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+94]  = intbuf[94]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+95]  = intbuf[95]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+96]  = intbuf[96]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+97]  = intbuf[97]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+98]  = intbuf[98]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+99]  = intbuf[99]  >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+100] = intbuf[100] >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+101] = intbuf[101] >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+102] = intbuf[102] >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+103] = intbuf[103] >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+104] = intbuf[104] >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+105] = intbuf[105] >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+106] = intbuf[106] >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+107] = intbuf[107] >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+108] = intbuf[108] >> interpolation_shifts<SdrBits, OutputBits>::post64;
        buf[pos+109] = intbuf[109] >> interpolation_shifts<SdrBits, OutputBits>::post64;

        ++(*it);
	}
}

#endif /* INCLUDE_GPL_DSP_INTERPOLATORS_H_ */
