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

template<uint SdrBits, uint InputBits> 
struct decimation_shifts
{
    static const uint pre1   = 0; 
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
struct decimation_shifts<16, 16>
{
    static const uint pre1   = 0;
    static const uint pre2   = 0;
    static const uint post2  = 1;
    static const uint pre4   = 0;
    static const uint post4  = 2;
    static const uint pre8   = 0;
    static const uint post8  = 3;
    static const uint pre16  = 0;
    static const uint post16 = 4;
    static const uint pre32  = 0;
    static const uint post32 = 5;
    static const uint pre64  = 0;
    static const uint post64 = 6;
};

template<>
struct decimation_shifts<16, 12>
{
    static const uint pre1   = 4; 
    static const uint pre2   = 3; 
    static const uint post2  = 0; 
    static const uint pre4   = 2; 
    static const uint post4  = 0; 
    static const uint pre8   = 1; 
    static const uint post8  = 0; 
    static const uint pre16  = 0; 
    static const uint post16 = 0; 
    static const uint pre32  = 0; 
    static const uint post32 = 1; 
    static const uint pre64  = 0;
    static const uint post64 = 2;
};

template<>
struct decimation_shifts<16, 8>
{
    static const uint pre1   = 6;
    static const uint pre2   = 5;
    static const uint post2  = 0; 
    static const uint pre4   = 4;
    static const uint post4  = 0; 
    static const uint pre8   = 3;
    static const uint post8  = 0; 
    static const uint pre16  = 2;
    static const uint post16 = 0; 
    static const uint pre32  = 1;
    static const uint post32 = 0; 
    static const uint pre64  = 0;
    static const uint post64 = 0;
};

template<typename T, uint SdrBits, uint InputBits>
class Decimators
{
public:
	void decimate1(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate2_u(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate2_inf(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate2_sup(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate2_cen(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate4_inf(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate4_sup(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate4_cen(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate8_inf(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate8_sup(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate8_cen(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate16_inf(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate16_sup(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate16_cen(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate32_inf(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate32_sup(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate32_cen(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate64_inf(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate64_sup(SampleVector::iterator* it, const T* buf, qint32 len);
	void decimate64_cen(SampleVector::iterator* it, const T* buf, qint32 len);

private:
	IntHalfbandFilter m_decimator2;  // 1st stages
	IntHalfbandFilter m_decimator4;  // 2nd stages
	IntHalfbandFilter m_decimator8;  // 3rd stages
	IntHalfbandFilter m_decimator16; // 4th stages
	IntHalfbandFilter m_decimator32; // 5th stages
	IntHalfbandFilter m_decimator64; // 6th stages
};

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate1(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint32 xreal, yimag;

	for (int pos = 0; pos < len; pos += 2)
	{
		xreal = buf[pos+0];
		yimag = buf[pos+1];
		(**it).setReal(xreal << decimation_shifts<SdrBits, InputBits>::pre1); // Valgrind optim (2 - comment not repeated)
		(**it).setImag(yimag << decimation_shifts<SdrBits, InputBits>::pre1);
		++(*it); // Valgrind optim (comment not repeated)
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate2_u(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint32 xreal, yimag;

	for (int pos = 0; pos < len - 7; pos += 8)
	{
		xreal = (buf[pos+0] - buf[pos+3]) << decimation_shifts<SdrBits, InputBits>::pre2;
		yimag = (buf[pos+1] + buf[pos+2] - 255) << decimation_shifts<SdrBits, InputBits>::pre2;
		(**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post2);
		(**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post2);
		++(*it);

		xreal = (buf[pos+7] - buf[pos+4]) << decimation_shifts<SdrBits, InputBits>::pre2;
		yimag = (255 - buf[pos+5] - buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre2;
		(**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post2);
		(**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post2);
		++(*it);
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate2_inf(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint32 xreal, yimag;

	for (int pos = 0; pos < len - 7; pos += 8)
	{
		xreal = (buf[pos+0] - buf[pos+3]) << decimation_shifts<SdrBits, InputBits>::pre2;
		yimag = (buf[pos+1] + buf[pos+2]) << decimation_shifts<SdrBits, InputBits>::pre2;
		(**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post2);
		(**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post2);
		++(*it);

		xreal = (buf[pos+7] - buf[pos+4]) << decimation_shifts<SdrBits, InputBits>::pre2;
		yimag = (- buf[pos+5] - buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre2;
		(**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post2);
		(**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post2);
		++(*it);
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate2_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint32 xreal, yimag;

	for (int pos = 0; pos < len - 7; pos += 8)
	{
		xreal = (buf[pos+1] - buf[pos+2]) << decimation_shifts<SdrBits, InputBits>::pre2;
		yimag = (- buf[pos+0] - buf[pos+3]) << decimation_shifts<SdrBits, InputBits>::pre2;
		(**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post2);
		(**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post2);
		++(*it);

		xreal = (buf[pos+6] - buf[pos+5]) << decimation_shifts<SdrBits, InputBits>::pre2;
		yimag = (buf[pos+4] + buf[pos+7]) << decimation_shifts<SdrBits, InputBits>::pre2;
		(**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post2);
		(**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post2);
		++(*it);
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate2_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	int pos = 0;

	while (pos < len - 3)
	{
		qint32 x0 = buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre2;
		qint32 y0 = buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre2;
		pos += 2;

		if (m_decimator2.workDecimateCenter(&x0, &y0))
		{
			(**it).setReal(x0 >> decimation_shifts<SdrBits, InputBits>::post2);
			(**it).setImag(y0 >> decimation_shifts<SdrBits, InputBits>::post2);
			++(*it);
		}
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate4_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	int pos = 0;

	while (pos < len)
	{
		qint32 x0 = buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre4;
		qint32 y0 = buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre4;
		pos += 2;

		if (m_decimator2.workDecimateCenter(&x0, &y0))
		{
			qint32 x1 = x0;
			qint32 y1 = y0;

			if (m_decimator4.workDecimateCenter(&x1, &y1))
			{
				(**it).setReal(x0 >> decimation_shifts<SdrBits, InputBits>::post4);
				(**it).setImag(y0 >> decimation_shifts<SdrBits, InputBits>::post4);
				++(*it);
			}
		}
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate8_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	int pos = 0;

	while (pos < len)
	{
		qint32 x0 = buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre8;
		qint32 y0 = buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre8;
		pos += 2;

		if (m_decimator2.workDecimateCenter(&x0, &y0))
		{
			qint32 x1 = x0;
			qint32 y1 = y0;

			if (m_decimator4.workDecimateCenter(&x1, &y1))
			{
				qint32 x2 = x1;
				qint32 y2 = y1;

				if (m_decimator8.workDecimateCenter(&x2, &y2))
				{
					(**it).setReal(x2 >> decimation_shifts<SdrBits, InputBits>::post8);
					(**it).setImag(y2 >> decimation_shifts<SdrBits, InputBits>::post8);
					++(*it);
				}
			}
		}
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate16_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	int pos = 0;

	while (pos < len)
	{
		qint32 x0 = buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre16;
		qint32 y0 = buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre16;
		pos += 2;

		if (m_decimator2.workDecimateCenter(&x0, &y0))
		{
			qint32 x1 = x0;
			qint32 y1 = y0;

			if (m_decimator4.workDecimateCenter(&x1, &y1))
			{
				qint32 x2 = x1;
				qint32 y2 = y1;

				if (m_decimator8.workDecimateCenter(&x2, &y2))
				{
					qint32 x3 = x2;
					qint32 y3 = y2;

					if (m_decimator16.workDecimateCenter(&x3, &y3))
					{
						(**it).setReal(x3 >> decimation_shifts<SdrBits, InputBits>::post16);
						(**it).setImag(y3 >> decimation_shifts<SdrBits, InputBits>::post16);
						++(*it);
					}
				}
			}
		}
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate32_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	int pos = 0;

	while (pos < len)
	{
		qint32 x0 = buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre32;
		qint32 y0 = buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre32;
		pos += 2;

		if (m_decimator2.workDecimateCenter(&x0, &y0))
		{
			qint32 x1 = x0;
			qint32 y1 = y0;

			if (m_decimator4.workDecimateCenter(&x1, &y1))
			{
				qint32 x2 = x1;
				qint32 y2 = y1;

				if (m_decimator8.workDecimateCenter(&x2, &y2))
				{
					qint32 x3 = x2;
					qint32 y3 = y2;

					if (m_decimator16.workDecimateCenter(&x3, &y3))
					{
						qint32 x4 = x3;
						qint32 y4 = y3;

						if (m_decimator32.workDecimateCenter(&x4, &y4))
						{
							(**it).setReal(x4 >> decimation_shifts<SdrBits, InputBits>::post32);
							(**it).setImag(y4 >> decimation_shifts<SdrBits, InputBits>::post32);
							++(*it);
						}
					}
				}
			}
		}
	}
}


template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate64_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	int pos = 0;

	while (pos < len)
	{
		qint32 x0 = buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre64;
		qint32 y0 = buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre64;
		pos += 2;

		if (m_decimator2.workDecimateCenter(&x0, &y0))
		{
			qint32 x1 = x0;
			qint32 y1 = y0;

			if (m_decimator4.workDecimateCenter(&x1, &y1))
			{
				qint32 x2 = x1;
				qint32 y2 = y1;

				if (m_decimator8.workDecimateCenter(&x2, &y2))
				{
					qint32 x3 = x2;
					qint32 y3 = y2;

					if (m_decimator16.workDecimateCenter(&x3, &y3))
					{
						qint32 x4 = x3;
						qint32 y4 = y3;

						if (m_decimator32.workDecimateCenter(&x4, &y4))
						{
							qint32 x5 = x4;
							qint32 y5 = y4;

							if (m_decimator64.workDecimateCenter(&x4, &y4))
							{
								(**it).setReal(x5 >> decimation_shifts<SdrBits, InputBits>::post64);
								(**it).setImag(y5 >> decimation_shifts<SdrBits, InputBits>::post64);
								++(*it);
							}
						}
					}
				}
			}
		}
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate4_inf(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint32 xreal, yimag;

	for (int pos = 0; pos < len - 7; pos += 8)
	{
		xreal = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) << decimation_shifts<SdrBits, InputBits>::pre4;
		yimag = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre4;

		(**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post4);
		(**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post4);

		++(*it);
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate4_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
	// Sup (USB):
	//            x  y   x  y   x   y  x   y  / x -> 1,-2,-5,6 / y -> -0,-3,4,7
	// [ rotate:  1, 0, -2, 3, -5, -4, 6, -7]
	// Inf (LSB):
	//            x  y   x  y   x   y  x   y  / x -> 0,-3,-4,7 / y -> 1,2,-5,-6
	// [ rotate:  0, 1, -3, 2, -4, -5, 7, -6]
	qint32 xreal, yimag;

	for (int pos = 0; pos < len - 7; pos += 8)
	{
		xreal = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre4;
		yimag = (- buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7]) << decimation_shifts<SdrBits, InputBits>::pre4;

		(**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post4);
		(**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post4);

		++(*it);
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate8_inf(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint32 xreal[2], yimag[2];

	for (int pos = 0; pos < len - 15; pos += 8)
	{
		xreal[0] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) << decimation_shifts<SdrBits, InputBits>::pre8;
		yimag[0] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre8;
		pos += 8;

		xreal[1] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) << decimation_shifts<SdrBits, InputBits>::pre8;
		yimag[1] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre8;

		m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);

		(**it).setReal(xreal[1] >> decimation_shifts<SdrBits, InputBits>::post8);
		(**it).setImag(yimag[1] >> decimation_shifts<SdrBits, InputBits>::post8);

		++(*it);
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate8_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint32 xreal[2], yimag[2];

	for (int pos = 0; pos < len - 15; pos += 8)
	{
		xreal[0] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre8;
		yimag[0] = (- buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7]) << decimation_shifts<SdrBits, InputBits>::pre8;
		pos += 8;

		xreal[1] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre8;
		yimag[1] = (- buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7]) << decimation_shifts<SdrBits, InputBits>::pre8;

		m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);

		(**it).setReal(xreal[1] >> decimation_shifts<SdrBits, InputBits>::post8);
		(**it).setImag(yimag[1] >> decimation_shifts<SdrBits, InputBits>::post8);

		++(*it);
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate16_inf(SampleVector::iterator* it, const T* buf, qint32 len)
{
	// Offset tuning: 4x downsample and rotate, then
	// downsample 4x more. [ rotate:  0, 1, -3, 2, -4, -5, 7, -6]
	qint32 xreal[4], yimag[4];

	for (int pos = 0; pos < len - 31; )
	{
		for (int i = 0; i < 4; i++)
		{
			xreal[i] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) << decimation_shifts<SdrBits, InputBits>::pre16;
			yimag[i] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre16;
			pos += 8;
		}

		m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
		m_decimator2.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);
        
		m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);

		(**it).setReal(xreal[3] >> decimation_shifts<SdrBits, InputBits>::post16);
		(**it).setImag(yimag[3] >> decimation_shifts<SdrBits, InputBits>::post16);

		++(*it);
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate16_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
	// Offset tuning: 4x downsample and rotate, then
	// downsample 4x more. [ rotate:  1, 0, -2, 3, -5, -4, 6, -7]
	qint32 xreal[4], yimag[4];

	for (int pos = 0; pos < len - 31; )
	{
		for (int i = 0; i < 4; i++)
		{
			xreal[i] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre16;
			yimag[i] = (buf[pos+4] + buf[pos+7] - buf[pos+0] - buf[pos+3]) << decimation_shifts<SdrBits, InputBits>::pre16;
			pos += 8;
		}

		m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
		m_decimator2.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);

		m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);

		(**it).setReal(xreal[3] >> decimation_shifts<SdrBits, InputBits>::post16);
		(**it).setImag(yimag[3] >> decimation_shifts<SdrBits, InputBits>::post16);

		++(*it);
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate32_inf(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint32 xreal[8], yimag[8];

	for (int pos = 0; pos < len - 63; )
	{
		for (int i = 0; i < 8; i++)
		{
			xreal[i] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) << decimation_shifts<SdrBits, InputBits>::pre32;
			yimag[i] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre32;
			pos += 8;
		}

		m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
		m_decimator2.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);
		m_decimator2.myDecimate(xreal[4], yimag[4], &xreal[5], &yimag[3]);
		m_decimator2.myDecimate(xreal[6], yimag[6], &xreal[7], &yimag[7]);

		m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);
		m_decimator4.myDecimate(xreal[5], yimag[5], &xreal[7], &yimag[7]);

		m_decimator8.myDecimate(xreal[3], yimag[3], &xreal[7], &yimag[7]);

		(**it).setReal(xreal[7] >> decimation_shifts<SdrBits, InputBits>::post32);
		(**it).setImag(yimag[7] >> decimation_shifts<SdrBits, InputBits>::post32);

		++(*it);
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate32_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint32 xreal[8], yimag[8];

	for (int pos = 0; pos < len - 63; )
	{
		for (int i = 0; i < 8; i++)
		{
			xreal[i] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre32;
			yimag[i] = (buf[pos+4] + buf[pos+7] - buf[pos+0] - buf[pos+3]) << decimation_shifts<SdrBits, InputBits>::pre32;
			pos += 8;
		}

		m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
		m_decimator2.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);
		m_decimator2.myDecimate(xreal[4], yimag[4], &xreal[5], &yimag[3]);
		m_decimator2.myDecimate(xreal[6], yimag[6], &xreal[7], &yimag[7]);

		m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);
		m_decimator4.myDecimate(xreal[5], yimag[5], &xreal[7], &yimag[7]);

		m_decimator8.myDecimate(xreal[3], yimag[3], &xreal[7], &yimag[7]);

		(**it).setReal(xreal[7] >> decimation_shifts<SdrBits, InputBits>::post32);
		(**it).setImag(yimag[7] >> decimation_shifts<SdrBits, InputBits>::post32);

		++(*it);
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate64_inf(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint32 xreal[16], yimag[16];

	for (int pos = 0; pos < len - 127; )
	{
		for (int i = 0; i < 16; i++)
		{
			xreal[i] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) << decimation_shifts<SdrBits, InputBits>::pre64;
			yimag[i] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre64;
			pos += 16;
		}

		m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
		m_decimator2.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);
		m_decimator2.myDecimate(xreal[4], yimag[4], &xreal[5], &yimag[3]);
		m_decimator2.myDecimate(xreal[6], yimag[6], &xreal[7], &yimag[7]);
		m_decimator2.myDecimate(xreal[8], yimag[8], &xreal[9], &yimag[9]);
		m_decimator2.myDecimate(xreal[10], yimag[10], &xreal[11], &yimag[11]);
		m_decimator2.myDecimate(xreal[12], yimag[12], &xreal[13], &yimag[13]);
		m_decimator2.myDecimate(xreal[14], yimag[14], &xreal[15], &yimag[15]);

		m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);
		m_decimator4.myDecimate(xreal[5], yimag[5], &xreal[7], &yimag[7]);
		m_decimator4.myDecimate(xreal[9], yimag[9], &xreal[11], &yimag[11]);
		m_decimator4.myDecimate(xreal[13], yimag[13], &xreal[15], &yimag[15]);

		m_decimator8.myDecimate(xreal[3], yimag[3], &xreal[7], &yimag[7]);
		m_decimator8.myDecimate(xreal[11], yimag[11], &xreal[15], &yimag[15]);

		m_decimator16.myDecimate(xreal[7], yimag[7], &xreal[15], &yimag[15]);

		(**it).setReal(xreal[15] >> decimation_shifts<SdrBits, InputBits>::post64);
		(**it).setImag(yimag[15] >> decimation_shifts<SdrBits, InputBits>::post64);

		++(*it);
	}
}

template<typename T, uint SdrBits, uint InputBits>
void Decimators<T, SdrBits, InputBits>::decimate64_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint32 xreal[16], yimag[16];

	for (int pos = 0; pos < len - 127; )
	{
		for (int i = 0; i < 16; i++)
		{
			xreal[i] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre32;
			yimag[i] = (buf[pos+4] + buf[pos+7] - buf[pos+0] - buf[pos+3]) << decimation_shifts<SdrBits, InputBits>::pre32;
			pos += 16;
		}

		m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
		m_decimator2.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);
		m_decimator2.myDecimate(xreal[4], yimag[4], &xreal[5], &yimag[3]);
		m_decimator2.myDecimate(xreal[6], yimag[6], &xreal[7], &yimag[7]);
		m_decimator2.myDecimate(xreal[8], yimag[8], &xreal[9], &yimag[9]);
		m_decimator2.myDecimate(xreal[10], yimag[10], &xreal[11], &yimag[11]);
		m_decimator2.myDecimate(xreal[12], yimag[12], &xreal[13], &yimag[13]);
		m_decimator2.myDecimate(xreal[14], yimag[14], &xreal[15], &yimag[15]);

		m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);
		m_decimator4.myDecimate(xreal[5], yimag[5], &xreal[7], &yimag[7]);
		m_decimator4.myDecimate(xreal[9], yimag[9], &xreal[11], &yimag[11]);
		m_decimator4.myDecimate(xreal[13], yimag[13], &xreal[15], &yimag[15]);

		m_decimator8.myDecimate(xreal[3], yimag[3], &xreal[7], &yimag[7]);
		m_decimator8.myDecimate(xreal[11], yimag[11], &xreal[15], &yimag[15]);

		m_decimator16.myDecimate(xreal[7], yimag[7], &xreal[15], &yimag[15]);

		(**it).setReal(xreal[15] >> decimation_shifts<SdrBits, InputBits>::post64);
		(**it).setImag(yimag[15] >> decimation_shifts<SdrBits, InputBits>::post64);

		++(*it);
	}
}

#endif /* INCLUDE_GPL_DSP_DECIMATORS_H_ */
