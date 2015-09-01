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
		(**it).setReal(xreal * 16); // Valgrind optim (2 - comment not repeated)
		(**it).setImag(yimag * 16);
		//Sample s( xreal * 16, yimag * 16 ); // shift by 4 bit positions (signed)
		//**it = s;
		++(*it); // Valgrind optim (comment not repeated)
	}
}

template<typename T>
void Decimators<T>::decimate2_u(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal, yimag;

	for (int pos = 0; pos < len - 7; pos += 8)
	{
		xreal = buf[pos+0] - buf[pos+3];
		yimag = buf[pos+1] + buf[pos+2] - 255;
		//Sample s( xreal << 3, yimag << 3 );
		//**it = s;
		(**it).setReal(xreal << 3);
		(**it).setImag(yimag << 3);
		++(*it);
		xreal = buf[pos+7] - buf[pos+4];
		yimag = 255 - buf[pos+5] - buf[pos+6];
		//Sample t( xreal << 3, yimag << 3 );
		//**it = t;
		(**it).setReal(xreal << 3);
		(**it).setImag(yimag << 3);
		++(*it);
	}
}

template<typename T>
void Decimators<T>::decimate2(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal, yimag;

	for (int pos = 0; pos < len - 7; pos += 8)
	{
		xreal = buf[pos+0] - buf[pos+3];
		yimag = buf[pos+1] + buf[pos+2];
		//Sample s( xreal << 3, yimag << 3 );
		//**it = s;
		(**it).setReal(xreal << 3);
		(**it).setImag(yimag << 3);
		++(*it);
		xreal = buf[pos+7] - buf[pos+4];
		yimag = - buf[pos+5] - buf[pos+6];
		//Sample t( xreal << 3, yimag << 3 );
		//**it = t;
		(**it).setReal(xreal << 3);
		(**it).setImag(yimag << 3);
		++(*it);
	}
}

template<typename T>
void Decimators<T>::decimate2_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal, yimag;

	for (int pos = 0; pos < len - 7; pos += 8)
	{
		xreal = buf[pos+1] - buf[pos+2];
		yimag = - buf[pos+0] - buf[pos+3];
		//Sample s( xreal << 3, yimag << 3 );
		//**it = s;
		(**it).setReal(xreal << 3);
		(**it).setImag(yimag << 3);
		++(*it);
		xreal = buf[pos+6] - buf[pos+5];
		yimag = buf[pos+4] + buf[pos+7];
		//Sample t( xreal << 3, yimag << 3 );
		//**it = t;
		(**it).setReal(xreal << 3);
		(**it).setImag(yimag << 3);
		++(*it);
	}
}

/*
template<typename T>
void Decimators<T>::decimate2_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	int pos = 0;

	while (pos < len - 3)
	{
		Sample s0(buf[pos+0] << 3, buf[pos+1] << 3);
		pos += 2;

		if (m_decimator2.workDecimateCenter(&s0))
		{
			**it = s0;
			++(*it);
		}
	}
}*/

template<typename T>
void Decimators<T>::decimate2_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	int pos = 0;

	while (pos < len - 3)
	{
		qint16 x0 = buf[pos+0] << 3;
		qint16 y0 = buf[pos+1] << 3;
		pos += 2;

		if (m_decimator2.workDecimateCenter(&x0, &y0))
		{
			(**it).setReal(x0);
			(**it).setImag(y0);
			++(*it);
		}
	}
}

/*
template<typename T>
void Decimators<T>::decimate4_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	int pos = 0;

	while (pos < len)
	{
		Sample s0(buf[pos+0] << 4, buf[pos+1] << 4);
		pos += 2;

		if (m_decimator2.workDecimateCenter(&s0))
		{
			Sample s1 = s0;

			if (m_decimator4.workDecimateCenter(&s1))
			{
				(**it) = s1;
				++(*it);
			}
		}
	}
}*/

template<typename T>
void Decimators<T>::decimate4_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	int pos = 0;

	while (pos < len)
	{
		qint16 x0 = buf[pos+0] << 3;
		qint16 y0 = buf[pos+1] << 3;
		pos += 2;

		if (m_decimator2.workDecimateCenter(&x0, &y0))
		{
			qint16 x1 = x0;
			qint16 y1 = y0;

			if (m_decimator4.workDecimateCenter(&x1, &y1))
			{
				(**it).setReal(x0);
				(**it).setImag(y0);
				++(*it);
			}
		}
	}
}

template<typename T>
void Decimators<T>::decimate8_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	int pos = 0;

	while (pos < len)
	{
		qint16 x0 = buf[pos+0] << 4;
		qint16 y0 = buf[pos+1] << 4;
		pos += 2;

		if (m_decimator2.workDecimateCenter(&x0, &y0))
		{
			qint16 x1 = x0;
			qint16 y1 = y0;

			if (m_decimator4.workDecimateCenter(&x1, &y1))
			{
				qint16 x2 = x1;
				qint16 y2 = y1;

				if (m_decimator8.workDecimateCenter(&x2, &y2))
				{
					(**it).setReal(x2);
					(**it).setImag(y2);
					++(*it);
				}
			}
		}
	}
}

/*
template<typename T>
void Decimators<T>::decimate8_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	int pos = 0;

	while (pos < len)
	{
		Sample s0(buf[pos+0] << 4, buf[pos+1] << 4);
		pos += 2;

		if (m_decimator2.workDecimateCenter(&s0))
		{
			Sample s1 = s0;

			if (m_decimator4.workDecimateCenter(&s1))
			{
				Sample s2 = s1;

				if (m_decimator8.workDecimateCenter(&s2))
				{
					(**it) = s2;
					++(*it);
				}
			}
		}
	}
}*/

template<typename T>
void Decimators<T>::decimate16_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	int pos = 0;

	while (pos < len)
	{
		qint16 x0 = buf[pos+0] << 4;
		qint16 y0 = buf[pos+1] << 4;
		pos += 2;

		if (m_decimator2.workDecimateCenter(&x0, &y0))
		{
			qint16 x1 = x0;
			qint16 y1 = y0;

			if (m_decimator4.workDecimateCenter(&x1, &y1))
			{
				qint16 x2 = x1;
				qint16 y2 = y1;

				if (m_decimator8.workDecimateCenter(&x2, &y2))
				{
					qint16 x3 = x2;
					qint16 y3 = y2;

					if (m_decimator16.workDecimateCenter(&x3, &y3))
					{
						(**it).setReal(x3);
						(**it).setImag(y3);
						++(*it);
					}
				}
			}
		}
	}
}

template<typename T>
void Decimators<T>::decimate32_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	int pos = 0;

	while (pos < len)
	{
		qint16 x0 = buf[pos+0] << 4;
		qint16 y0 = buf[pos+1] << 4;
		pos += 2;

		if (m_decimator2.workDecimateCenter(&x0, &y0))
		{
			qint16 x1 = x0;
			qint16 y1 = y0;

			if (m_decimator4.workDecimateCenter(&x1, &y1))
			{
				qint16 x2 = x1;
				qint16 y2 = y1;

				if (m_decimator8.workDecimateCenter(&x2, &y2))
				{
					qint16 x3 = x2;
					qint16 y3 = y2;

					if (m_decimator16.workDecimateCenter(&x3, &y3))
					{
						qint16 x4 = x3;
						qint16 y4 = y3;

						if (m_decimator32.workDecimateCenter(&x4, &y4))
						{
							(**it).setReal(x4);
							(**it).setImag(y4);
							++(*it);
						}
					}
				}
			}
		}
	}
}

template<typename T>
void Decimators<T>::decimate4(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal, yimag;

	for (int pos = 0; pos < len - 7; pos += 8)
	{
		xreal = buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4];
		yimag = buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6];

		(**it).setReal(xreal << 2);
		(**it).setImag(yimag << 2);
		/*
		Sample s( xreal << 2, yimag << 2 ); // was shift 3
		**it = s;*/
		++(*it);
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

	for (int pos = 0; pos < len - 7; pos += 8)
	{
		xreal = buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6];
		yimag = - buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7];
		//xreal = buf[pos+0] - buf[pos+3] - buf[pos+4] + buf[pos+7];
		//yimag = buf[pos+1] + buf[pos+2] - buf[pos+5] - buf[pos+6];

		(**it).setReal(xreal << 2);
		(**it).setImag(yimag << 2);
		/*
		Sample s( xreal << 2, yimag << 2 ); // was shift 3
		**it = s;*/
		++(*it);
	}
}

template<typename T>
void Decimators<T>::decimate8(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal[2], yimag[2];

	for (int pos = 0; pos < len - 15; pos += 8)
	{
		xreal[0] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) << 2;
		yimag[0] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) << 2;
		//Sample s1( xreal << 2, yimag << 2 ); // was shift 3
		pos += 8;
		xreal[1] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) << 2;
		yimag[1] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) << 2;
		//Sample s2( xreal << 2, yimag << 2 ); // was shift 3

		m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);

		(**it).setReal(xreal[1]);
		(**it).setImag(yimag[1]);
		/*
		m_decimator2.myDecimate(&s1, &s2);
		**it = s2;*/
		++(*it);
	}
}

template<typename T>
void Decimators<T>::decimate8_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal[2], yimag[2];

	for (int pos = 0; pos < len - 15; pos += 8)
	{
		xreal[0] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]) << 2;
		yimag[0] = (- buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7]) << 2;
		//Sample s1( xreal << 2, yimag << 2 ); // was shift 3
		pos += 8;
		xreal[1] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]) << 2;
		yimag[1] = (- buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7]) << 2;
		//Sample s2( xreal << 2, yimag << 2 ); // was shift 3

		m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);

		(**it).setReal(xreal[1]);
		(**it).setImag(yimag[1]);
		/*
		m_decimator2.myDecimate(&s1, &s2);
		**it = s2;*/
		++(*it);
	}
}

template<typename T>
void Decimators<T>::decimate16(SampleVector::iterator* it, const T* buf, qint32 len)
{
	// Offset tuning: 4x downsample and rotate, then
	// downsample 4x more. [ rotate:  0, 1, -3, 2, -4, -5, 7, -6]
	qint16 xreal[4], yimag[4];

	for (int pos = 0; pos < len - 31; )
	{
		for (int i = 0; i < 4; i++)
		{
			xreal[i] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) << 2; // was shift 4
			yimag[i] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) << 2; // was shift 4
			pos += 8;
		}

		m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
		m_decimator2.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);
		m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);

		(**it).setReal(xreal[3]);
		(**it).setImag(yimag[3]);

		/* Valgrind optim
		Sample s1( xreal[0], yimag[0] );
		Sample s2( xreal[1], yimag[1] );
		Sample s3( xreal[2], yimag[2] );
		Sample s4( xreal[3], yimag[3] );

		m_decimator2.myDecimate(&s1, &s2);
		m_decimator2.myDecimate(&s3, &s4);

		m_decimator4.myDecimate(&s2, &s4);

		**it = s4;*/
		++(*it);
	}
}

template<typename T>
void Decimators<T>::decimate16_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
	// Offset tuning: 4x downsample and rotate, then
	// downsample 4x more. [ rotate:  1, 0, -2, 3, -5, -4, 6, -7]
	qint16 xreal[4], yimag[4];

	for (int pos = 0; pos < len - 31; )
	{
		for (int i = 0; i < 4; i++)
		{
			xreal[i] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]) << 2; // was shift 4
			yimag[i] = (buf[pos+4] + buf[pos+7] - buf[pos+0] - buf[pos+3]) << 2; // was shift 4
			pos += 8;
		}

		m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
		m_decimator2.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);

		m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);

		(**it).setReal(xreal[3]);
		(**it).setImag(yimag[3]);
		/*
		Sample s1( xreal[0], yimag[0] );
		Sample s2( xreal[1], yimag[1] );
		Sample s3( xreal[2], yimag[2] );
		Sample s4( xreal[3], yimag[3] );

		m_decimator2.myDecimate(&s1, &s2);
		m_decimator2.myDecimate(&s3, &s4);

		m_decimator4.myDecimate(&s2, &s4);

		**it = s4;*/
		++(*it);
	}
}

template<typename T>
void Decimators<T>::decimate32(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal[8], yimag[8];

	for (int pos = 0; pos < len - 63; )
	{
		for (int i = 0; i < 8; i++)
		{
			xreal[i] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) << 2;
			yimag[i] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) << 2;
			pos += 8;
		}

		m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
		m_decimator2.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);
		m_decimator2.myDecimate(xreal[4], yimag[4], &xreal[5], &yimag[3]);
		m_decimator2.myDecimate(xreal[6], yimag[6], &xreal[7], &yimag[7]);

		m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);
		m_decimator4.myDecimate(xreal[5], yimag[5], &xreal[7], &yimag[7]);

		m_decimator8.myDecimate(xreal[3], yimag[3], &xreal[7], &yimag[7]);

		(**it).setReal(xreal[7]);
		(**it).setImag(yimag[7]);
		/*
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

		**it = s8;*/
		++(*it);
	}
}

template<typename T>
void Decimators<T>::decimate32_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint16 xreal[8], yimag[8];

	for (int pos = 0; pos < len - 63; )
	{
		for (int i = 0; i < 8; i++)
		{
			xreal[i] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]) << 2;
			yimag[i] = (buf[pos+4] + buf[pos+7] - buf[pos+0] - buf[pos+3]) << 2;
			pos += 8;
		}

		m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
		m_decimator2.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);
		m_decimator2.myDecimate(xreal[4], yimag[4], &xreal[5], &yimag[3]);
		m_decimator2.myDecimate(xreal[6], yimag[6], &xreal[7], &yimag[7]);

		m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);
		m_decimator4.myDecimate(xreal[5], yimag[5], &xreal[7], &yimag[7]);

		m_decimator8.myDecimate(xreal[3], yimag[3], &xreal[7], &yimag[7]);

		(**it).setReal(xreal[7]);
		(**it).setImag(yimag[7]);
		/*
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

		**it = s8;*/
		++(*it);
	}
}

#endif /* INCLUDE_GPL_DSP_DECIMATORS_H_ */
