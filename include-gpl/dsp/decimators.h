///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_GPL_DSP_DECIMATORS_H_
#define INCLUDE_GPL_DSP_DECIMATORS_H_

#include "dsp/dsptypes.h"
#include "dsp/inthalfbandfilter.h"

template<typename T>
class Decimators
{
public:
	void decimate1(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate2_u(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate2(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate2_sup(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate2_cen(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate4(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate4_sup(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate4_cen(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate8(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate8_sup(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate8_cen(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate16(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate16_sup(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate16_cen(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate32(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate32_sup(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate32_cen(SampleVector::iterator* it, const T* buf, qint32 len);

private:
	IntHalfbandFilter m_decimator2;  // 1st stages
	IntHalfbandFilter m_decimator4;  // 2nd stages
	IntHalfbandFilter m_decimator8;  // 3rd stages
	IntHalfbandFilter m_decimator16; // 4th stages
	IntHalfbandFilter m_decimator32; // 5th stages
};

template<typename T>
void Decimators<T>::decimate1(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal, yimag;

	for (int pos = 0; pos < len; pos += 2) {
		xreal = buf[pos+0];
		yimag = buf[pos+1];
		Sample s( xreal * 16, yimag * 16 ); // shift by 4 bit positions (signed)
		**it = s;
		(*it)++;
	}
}

template<typename T>
void Decimators<T>::decimate2_u(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal, yimag;
	for (int pos = 0; pos < len - 7; pos += 8) {
		xreal = buf[pos+0] - buf[pos+3];
		yimag = buf[pos+1] + buf[pos+2] - 255;
		Sample s( xreal << 3, yimag << 3 );
		**it = s;
		(*it)++;
		xreal = buf[pos+7] - buf[pos+4];
		yimag = 255 - buf[pos+5] - buf[pos+6];
		Sample t( xreal << 3, yimag << 3 );
		**it = t;
		(*it)++;
	}
}

template<typename T>
void Decimators<T>::decimate2(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal, yimag;
	for (int pos = 0; pos < len - 7; pos += 8) {
		xreal = buf[pos+0] - buf[pos+3];
		yimag = buf[pos+1] + buf[pos+2];
		Sample s( xreal << 3, yimag << 3 );
		**it = s;
		(*it)++;
		xreal = buf[pos+7] - buf[pos+4];
		yimag = - buf[pos+5] - buf[pos+6];
		Sample t( xreal << 3, yimag << 3 );
		**it = t;
		(*it)++;
	}
}

template<typename T>
void Decimators<T>::decimate2_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal, yimag;
	for (int pos = 0; pos < len - 7; pos += 8) {
		xreal = buf[pos+1] - buf[pos+2];
		yimag = - buf[pos+0] - buf[pos+3];
		Sample s( xreal << 3, yimag << 3 );
		**it = s;
		(*it)++;
		xreal = buf[pos+6] - buf[pos+5];
		yimag = buf[pos+4] + buf[pos+7];
		Sample t( xreal << 3, yimag << 3 );
		**it = t;
		(*it)++;
	}
}

template<typename T>
void Decimators<T>::decimate2_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	for (int pos = 0; pos < len - 3; pos += 4) {
		Sample s1(buf[pos+0] << 3, buf[pos+1] << 3);
		Sample s2(buf[pos+2] << 3, buf[pos+3] << 3);
		m_decimator2.myDecimate(&s1, &s2);
		**it = s2;
		(*it)++;
	}
}

template<typename T>
void Decimators<T>::decimate4_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	for (int pos = 0; pos < len - 7; pos += 8) {
		Sample s1(buf[pos+0] << 4, buf[pos+1] << 4);
		Sample s2(buf[pos+2] << 4, buf[pos+3] << 4);
		Sample s3(buf[pos+4] << 4, buf[pos+5] << 4);
		Sample s4(buf[pos+6] << 4, buf[pos+7] << 4);
		m_decimator2.myDecimate(&s1, &s2);
		m_decimator2.myDecimate(&s3, &s4);
		m_decimator4.myDecimate(&s2, &s4);
		**it = s4;
		(*it)++;
	}
}

template<typename T>
void Decimators<T>::decimate8_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	for (int pos = 0; pos < len - 15; pos += 16) {
		Sample s1(buf[pos+0] << 4, buf[pos+1] << 4);
		Sample s2(buf[pos+2] << 4, buf[pos+3] << 4);
		Sample s3(buf[pos+4] << 4, buf[pos+5] << 4);
		Sample s4(buf[pos+6] << 4, buf[pos+7] << 4);
		Sample s5(buf[pos+8] << 4, buf[pos+9] << 4);
		Sample s6(buf[pos+10] << 4, buf[pos+11] << 4);
		Sample s7(buf[pos+12] << 4, buf[pos+13] << 4);
		Sample s8(buf[pos+14] << 4, buf[pos+15] << 4);
		m_decimator2.myDecimate(&s1, &s2);
		m_decimator2.myDecimate(&s3, &s4);
		m_decimator2.myDecimate(&s5, &s6);
		m_decimator2.myDecimate(&s7, &s8);
		m_decimator4.myDecimate(&s2, &s4);
		m_decimator4.myDecimate(&s6, &s8);
		m_decimator8.myDecimate(&s4, &s8);
		**it = s8;
		(*it)++;
	}
}

template<typename T>
void Decimators<T>::decimate16_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	for (int pos = 0; pos < len - 31; pos += 32) {
		Sample s1(buf[pos+0] << 4, buf[pos+1] << 4);
		Sample s2(buf[pos+2] << 4, buf[pos+3] << 4);
		Sample s3(buf[pos+4] << 4, buf[pos+5] << 4);
		Sample s4(buf[pos+6] << 4, buf[pos+7] << 4);
		Sample s5(buf[pos+8] << 4, buf[pos+9] << 4);
		Sample s6(buf[pos+10] << 4, buf[pos+11] << 4);
		Sample s7(buf[pos+12] << 4, buf[pos+13] << 4);
		Sample s8(buf[pos+14] << 4, buf[pos+15] << 4);
		Sample s9(buf[pos+16] << 4, buf[pos+17] << 4);
		Sample s10(buf[pos+18] << 4, buf[pos+19] << 4);
		Sample s11(buf[pos+20] << 4, buf[pos+21] << 4);
		Sample s12(buf[pos+22] << 4, buf[pos+23] << 4);
		Sample s13(buf[pos+24] << 4, buf[pos+25] << 4);
		Sample s14(buf[pos+26] << 4, buf[pos+27] << 4);
		Sample s15(buf[pos+28] << 4, buf[pos+29] << 4);
		Sample s16(buf[pos+30] << 4, buf[pos+31] << 4);

		m_decimator2.myDecimate(&s1, &s2);
		m_decimator2.myDecimate(&s3, &s4);
		m_decimator2.myDecimate(&s5, &s6);
		m_decimator2.myDecimate(&s7, &s8);
		m_decimator2.myDecimate(&s9, &s10);
		m_decimator2.myDecimate(&s11, &s12);
		m_decimator2.myDecimate(&s13, &s14);
		m_decimator2.myDecimate(&s15, &s16);

		m_decimator4.myDecimate(&s2, &s4);
		m_decimator4.myDecimate(&s6, &s8);
		m_decimator4.myDecimate(&s10, &s12);
		m_decimator4.myDecimate(&s14, &s16);

		m_decimator8.myDecimate(&s4, &s8);
		m_decimator8.myDecimate(&s12, &s16);

		m_decimator16.myDecimate(&s8, &s16);

		**it = s16;
		(*it)++;
	}
}

template<typename T>
void Decimators<T>::decimate32_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	for (int pos = 0; pos < len - 63; pos += 64) {
		Sample s1(buf[pos+0] << 4, buf[pos+1] << 4);
		Sample s2(buf[pos+2] << 4, buf[pos+3] << 4);
		Sample s3(buf[pos+4] << 4, buf[pos+5] << 4);
		Sample s4(buf[pos+6] << 4, buf[pos+7] << 4);
		Sample s5(buf[pos+8] << 4, buf[pos+9] << 4);
		Sample s6(buf[pos+10] << 4, buf[pos+11] << 4);
		Sample s7(buf[pos+12] << 4, buf[pos+13] << 4);
		Sample s8(buf[pos+14] << 4, buf[pos+15] << 4);
		Sample s9(buf[pos+16] << 4, buf[pos+17] << 4);
		Sample s10(buf[pos+18] << 4, buf[pos+19] << 4);
		Sample s11(buf[pos+20] << 4, buf[pos+21] << 4);
		Sample s12(buf[pos+22] << 4, buf[pos+23] << 4);
		Sample s13(buf[pos+24] << 4, buf[pos+25] << 4);
		Sample s14(buf[pos+26] << 4, buf[pos+27] << 4);
		Sample s15(buf[pos+28] << 4, buf[pos+29] << 4);
		Sample s16(buf[pos+30] << 4, buf[pos+31] << 4);
		Sample s17(buf[pos+32] << 4, buf[pos+33] << 4);
		Sample s18(buf[pos+34] << 4, buf[pos+35] << 4);
		Sample s19(buf[pos+36] << 4, buf[pos+37] << 4);
		Sample s20(buf[pos+38] << 4, buf[pos+39] << 4);
		Sample s21(buf[pos+40] << 4, buf[pos+41] << 4);
		Sample s22(buf[pos+42] << 4, buf[pos+43] << 4);
		Sample s23(buf[pos+44] << 4, buf[pos+45] << 4);
		Sample s24(buf[pos+46] << 4, buf[pos+47] << 4);
		Sample s25(buf[pos+48] << 4, buf[pos+49] << 4);
		Sample s26(buf[pos+50] << 4, buf[pos+51] << 4);
		Sample s27(buf[pos+52] << 4, buf[pos+53] << 4);
		Sample s28(buf[pos+54] << 4, buf[pos+55] << 4);
		Sample s29(buf[pos+56] << 4, buf[pos+57] << 4);
		Sample s30(buf[pos+58] << 4, buf[pos+59] << 4);
		Sample s31(buf[pos+60] << 4, buf[pos+61] << 4);
		Sample s32(buf[pos+62] << 4, buf[pos+63] << 4);

		m_decimator2.myDecimate(&s1, &s2);
		m_decimator2.myDecimate(&s3, &s4);
		m_decimator2.myDecimate(&s5, &s6);
		m_decimator2.myDecimate(&s7, &s8);
		m_decimator2.myDecimate(&s9, &s10);
		m_decimator2.myDecimate(&s11, &s12);
		m_decimator2.myDecimate(&s13, &s14);
		m_decimator2.myDecimate(&s15, &s16);
		m_decimator2.myDecimate(&s17, &s18);
		m_decimator2.myDecimate(&s19, &s20);
		m_decimator2.myDecimate(&s21, &s22);
		m_decimator2.myDecimate(&s23, &s24);
		m_decimator2.myDecimate(&s25, &s26);
		m_decimator2.myDecimate(&s27, &s28);
		m_decimator2.myDecimate(&s29, &s30);
		m_decimator2.myDecimate(&s31, &s32);

		m_decimator4.myDecimate(&s2, &s4);
		m_decimator4.myDecimate(&s6, &s8);
		m_decimator4.myDecimate(&s10, &s12);
		m_decimator4.myDecimate(&s14, &s16);
		m_decimator4.myDecimate(&s18, &s20);
		m_decimator4.myDecimate(&s22, &s24);
		m_decimator4.myDecimate(&s26, &s28);
		m_decimator4.myDecimate(&s30, &s32);

		m_decimator8.myDecimate(&s4, &s8);
		m_decimator8.myDecimate(&s12, &s16);
		m_decimator8.myDecimate(&s20, &s24);
		m_decimator8.myDecimate(&s28, &s32);

		m_decimator16.myDecimate(&s8, &s16);
		m_decimator16.myDecimate(&s24, &s32);

		m_decimator32.myDecimate(&s16, &s32);

		**it = s32;
		(*it)++;
	}
}

template<typename T>
void Decimators<T>::decimate4(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal, yimag;
	for (int pos = 0; pos < len - 7; pos += 8) {
		xreal = buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4];
		yimag = buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6];
		Sample s( xreal << 2, yimag << 2 ); // was shift 3
		**it = s;
		(*it)++;
	}
}

template<typename T>
void Decimators<T>::decimate4_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
	// Sup (USB):
	//            x  y   x  y   x   y  x   y  / x -> 1,-2,-5,6 / y -> -0,-3,4,7
	// [ rotate:  1, 0, -2, 3, -5, -4, 6, -7]
	// Inf (LSB):
	//            x  y   x  y   x   y  x   y  / x -> 0,-3,-4,7 / y -> 1,2,-5,-6
	// [ rotate:  0, 1, -3, 2, -4, -5, 7, -6]
	qint16 xreal, yimag;
	for (int pos = 0; pos < len - 7; pos += 8) {
		xreal = buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6];
		yimag = - buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7];
		//xreal = buf[pos+0] - buf[pos+3] - buf[pos+4] + buf[pos+7];
		//yimag = buf[pos+1] + buf[pos+2] - buf[pos+5] - buf[pos+6];
		Sample s( xreal << 2, yimag << 2 ); // was shift 3
		**it = s;
		(*it)++;
	}
}

template<typename T>
void Decimators<T>::decimate8(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal, yimag;
	for (int pos = 0; pos < len - 15; pos += 8) {
		xreal = buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4];
		yimag = buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6];
		Sample s1( xreal << 2, yimag << 2 ); // was shift 3
		pos += 8;
		xreal = buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4];
		yimag = buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6];
		Sample s2( xreal << 2, yimag << 2 ); // was shift 3

		m_decimator2.myDecimate(&s1, &s2);
		**it = s2;
		(*it)++;
	}
}

template<typename T>
void Decimators<T>::decimate8_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal, yimag;
	for (int pos = 0; pos < len - 15; pos += 8) {
		xreal = buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6];
		yimag = - buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7];
		Sample s1( xreal << 2, yimag << 2 ); // was shift 3
		pos += 8;
		xreal = buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6];
		yimag = - buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7];
		Sample s2( xreal << 2, yimag << 2 ); // was shift 3

		m_decimator2.myDecimate(&s1, &s2);
		**it = s2;
		(*it)++;
	}
}

template<typename T>
void Decimators<T>::decimate16(SampleVector::iterator* it, const T* buf, qint32 len)
{
	// Offset tuning: 4x downsample and rotate, then
	// downsample 4x more. [ rotate:  0, 1, -3, 2, -4, -5, 7, -6]
	qint16 xreal[4], yimag[4];

	for (int pos = 0; pos < len - 31; ) {
		for (int i = 0; i < 4; i++) {
			xreal[i] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) << 2; // was shift 4
			yimag[i] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) << 2; // was shift 4
			pos += 8;
		}

		Sample s1( xreal[0], yimag[0] );
		Sample s2( xreal[1], yimag[1] );
		Sample s3( xreal[2], yimag[2] );
		Sample s4( xreal[3], yimag[3] );

		m_decimator2.myDecimate(&s1, &s2);
		m_decimator2.myDecimate(&s3, &s4);

		m_decimator4.myDecimate(&s2, &s4);

		**it = s4;
		(*it)++;
	}
}

template<typename T>
void Decimators<T>::decimate16_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
	// Offset tuning: 4x downsample and rotate, then
	// downsample 4x more. [ rotate:  1, 0, -2, 3, -5, -4, 6, -7]
	qint16 xreal[4], yimag[4];

	for (int pos = 0; pos < len - 31; ) {
		for (int i = 0; i < 4; i++) {
			xreal[i] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]) << 2; // was shift 4
			yimag[i] = (buf[pos+4] + buf[pos+7] - buf[pos+0] - buf[pos+3]) << 2; // was shift 4
			pos += 8;
		}

		Sample s1( xreal[0], yimag[0] );
		Sample s2( xreal[1], yimag[1] );
		Sample s3( xreal[2], yimag[2] );
		Sample s4( xreal[3], yimag[3] );

		m_decimator2.myDecimate(&s1, &s2);
		m_decimator2.myDecimate(&s3, &s4);

		m_decimator4.myDecimate(&s2, &s4);

		**it = s4;
		(*it)++;
	}
}

template<typename T>
void Decimators<T>::decimate32(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal[8], yimag[8];

	for (int pos = 0; pos < len - 63; ) {
		for (int i = 0; i < 8; i++) {
			xreal[i] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) << 2;
			yimag[i] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) << 2;
			pos += 8;
		}

		Sample s1( xreal[0], yimag[0] );
		Sample s2( xreal[1], yimag[1] );
		Sample s3( xreal[2], yimag[2] );
		Sample s4( xreal[3], yimag[3] );
		Sample s5( xreal[4], yimag[4] );
		Sample s6( xreal[5], yimag[5] );
		Sample s7( xreal[6], yimag[6] );
		Sample s8( xreal[7], yimag[7] );

		m_decimator2.myDecimate(&s1, &s2);
		m_decimator2.myDecimate(&s3, &s4);
		m_decimator2.myDecimate(&s5, &s6);
		m_decimator2.myDecimate(&s7, &s8);

		m_decimator4.myDecimate(&s2, &s4);
		m_decimator4.myDecimate(&s6, &s8);

		m_decimator8.myDecimate(&s4, &s8);

		**it = s8;
		(*it)++;
	}
}

template<typename T>
void Decimators<T>::decimate32_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal[8], yimag[8];

	for (int pos = 0; pos < len - 63; ) {
		for (int i = 0; i < 8; i++) {
			xreal[i] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]) << 2;
			yimag[i] = (buf[pos+4] + buf[pos+7] - buf[pos+0] - buf[pos+3]) << 2;
			pos += 8;
		}

		Sample s1( xreal[0], yimag[0] );
		Sample s2( xreal[1], yimag[1] );
		Sample s3( xreal[2], yimag[2] );
		Sample s4( xreal[3], yimag[3] );
		Sample s5( xreal[4], yimag[4] );
		Sample s6( xreal[5], yimag[5] );
		Sample s7( xreal[6], yimag[6] );
		Sample s8( xreal[7], yimag[7] );

		m_decimator2.myDecimate(&s1, &s2);
		m_decimator2.myDecimate(&s3, &s4);
		m_decimator2.myDecimate(&s5, &s6);
		m_decimator2.myDecimate(&s7, &s8);

		m_decimator4.myDecimate(&s2, &s4);
		m_decimator4.myDecimate(&s6, &s8);

		m_decimator8.myDecimate(&s4, &s8);

		**it = s8;
		(*it)++;
	}
}

#endif /* INCLUDE_GPL_DSP_DECIMATORS_H_ */
