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
#include "dsp/inthalfbandfiltereo.h"

#define DECIMATORS_HB_FILTER_ORDER 64

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
struct decimation_shifts<16, 24>
{
    static const uint pre1   = 0;
    static const uint pre2   = 0;
    static const uint post2  = 9;
    static const uint pre4   = 0;
    static const uint post4  = 10;
    static const uint pre8   = 0;
    static const uint post8  = 11;
    static const uint pre16  = 0;
    static const uint post16 = 12;
    static const uint pre32  = 0;
    static const uint post32 = 13;
    static const uint pre64  = 0;
    static const uint post64 = 14;
};

template<>
struct decimation_shifts<24, 24>
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
struct decimation_shifts<24, 16>
{
    static const uint pre1   = 8;
    static const uint pre2   = 7;
    static const uint post2  = 0;
    static const uint pre4   = 6;
    static const uint post4  = 0;
    static const uint pre8   = 5;
    static const uint post8  = 0;
    static const uint pre16  = 4;
    static const uint post16 = 0;
    static const uint pre32  = 3;
    static const uint post32 = 0;
    static const uint pre64  = 2;
    static const uint post64 = 0;
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
struct decimation_shifts<24, 12>
{
    static const uint pre1   = 12;
    static const uint pre2   = 11;
    static const uint post2  = 0;
    static const uint pre4   = 10;
    static const uint post4  = 0;
    static const uint pre8   = 9;
    static const uint post8  = 0;
    static const uint pre16  = 8;
    static const uint post16 = 0;
    static const uint pre32  = 7;
    static const uint post32 = 0;
    static const uint pre64  = 6;
    static const uint post64 = 0;
};

template<>
struct decimation_shifts<16, 8>
{
    static const uint pre1   = 8;
    static const uint pre2   = 7;
    static const uint post2  = 0;
    static const uint pre4   = 6;
    static const uint post4  = 0;
    static const uint pre8   = 5;
    static const uint post8  = 0;
    static const uint pre16  = 4;
    static const uint post16 = 0;
    static const uint pre32  = 3;
    static const uint post32 = 0;
    static const uint pre64  = 2;
    static const uint post64 = 0;
};

template<>
struct decimation_shifts<24, 8>
{
    static const uint pre1   = 16;
    static const uint pre2   = 15;
    static const uint post2  = 0;
    static const uint pre4   = 14;
    static const uint post4  = 0;
    static const uint pre8   = 13;
    static const uint post8  = 0;
    static const uint pre16  = 12;
    static const uint post16 = 0;
    static const uint pre32  = 11;
    static const uint post32 = 0;
    static const uint pre64  = 10;
    static const uint post64 = 0;
};

template<typename T>
struct TripleByteLE
{
    uint8_t b0;
    uint8_t b1;
    uint8_t b2;

    typedef union {
        struct {
            int32_t i;
        } __attribute__((__packed__));
        struct {
            uint8_t i0;
            uint8_t i1;
            uint8_t i2;
            uint8_t i3;
        } __attribute__((__packed__));
    } isample;

    operator T() const {
        isample s;
        s.i0 = 0;
        s.i1 = b0;
        s.i2 = b1;
        s.i3 = b2;
        return s.i;
    }
} __attribute__((__packed__));

template<>
struct TripleByteLE<qint32>
{
    uint8_t b0;
    uint8_t b1;
    uint8_t b2;

    typedef union {
        struct {
            qint32 i;
        } __attribute__((__packed__));
        struct {
            uint8_t i0;
            uint8_t i1;
            uint8_t i2;
            uint8_t i3;
        } __attribute__((__packed__));
    } isample;

    operator qint32() const {
        isample s;
        s.i0 = 0;
        s.i1 = b0;
        s.i2 = b1;
        s.i3 = b2;
        return s.i >> 8;
    }
} __attribute__((__packed__));

template<>
struct TripleByteLE<qint64>
{
    uint8_t b0;
    uint8_t b1;
    uint8_t b2;

    typedef union {
        struct {
            qint64 i;
        } __attribute__((__packed__));
        struct {
            uint32_t ia;
            uint8_t i0;
            uint8_t i1;
            uint8_t i2;
            uint8_t i3;
        } __attribute__((__packed__));
    } isample;

    operator qint64() const {
        isample s;
        s.ia = 0;
        s.i0 = 0;
        s.i1 = b0;
        s.i2 = b1;
        s.i3 = b2;
        return s.i >> 40;
    }
} __attribute__((__packed__));


/** Decimators with integer input and integer output */
template<typename StorageType, typename T, uint SdrBits, uint InputBits>
class Decimators
{
public:
    // interleaved I/Q input buffer
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
	// separate I and Q input buffers
    void decimate1(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate2_u(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate2_inf(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate2_sup(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate2_cen(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate4_inf(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate4_sup(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate4_cen(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate8_inf(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate8_sup(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate8_cen(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate16_inf(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate16_sup(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate16_cen(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate32_inf(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate32_sup(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate32_cen(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate64_inf(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate64_sup(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);
    void decimate64_cen(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len);

private:
#ifdef SDR_RX_SAMPLE_24BIT
    IntHalfbandFilterEO<qint64, qint64, DECIMATORS_HB_FILTER_ORDER> m_decimator2;  // 1st stages
    IntHalfbandFilterEO<qint64, qint64, DECIMATORS_HB_FILTER_ORDER> m_decimator4;  // 2nd stages
    IntHalfbandFilterEO<qint64, qint64, DECIMATORS_HB_FILTER_ORDER> m_decimator8;  // 3rd stages
    IntHalfbandFilterEO<qint64, qint64, DECIMATORS_HB_FILTER_ORDER> m_decimator16; // 4th stages
    IntHalfbandFilterEO<qint64, qint64, DECIMATORS_HB_FILTER_ORDER> m_decimator32; // 5th stages
    IntHalfbandFilterEO<qint64, qint64, DECIMATORS_HB_FILTER_ORDER> m_decimator64; // 6th stages
#else
    IntHalfbandFilterEO<qint32, qint32, DECIMATORS_HB_FILTER_ORDER> m_decimator2;  // 1st stages
    IntHalfbandFilterEO<qint32, qint32, DECIMATORS_HB_FILTER_ORDER> m_decimator4;  // 2nd stages
    IntHalfbandFilterEO<qint32, qint32, DECIMATORS_HB_FILTER_ORDER> m_decimator8;  // 3rd stages
    IntHalfbandFilterEO<qint32, qint32, DECIMATORS_HB_FILTER_ORDER> m_decimator16; // 4th stages
    IntHalfbandFilterEO<qint32, qint32, DECIMATORS_HB_FILTER_ORDER> m_decimator32; // 5th stages
    IntHalfbandFilterEO<qint32, qint32, DECIMATORS_HB_FILTER_ORDER> m_decimator64; // 6th stages
#endif
};

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate1(SampleVector::iterator* it, const T* buf, qint32 len)
{
	qint32 xreal, yimag;

	for (int pos = 0; pos < len - 1; pos += 2)
	{
		xreal = buf[pos+0];
		yimag = buf[pos+1];
		(**it).setReal(xreal << decimation_shifts<SdrBits, InputBits>::pre1); // Valgrind optim (2 - comment not repeated)
		(**it).setImag(yimag << decimation_shifts<SdrBits, InputBits>::pre1);
		++(*it); // Valgrind optim (comment not repeated)
	}
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate1(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len)
{
    qint32 xreal, yimag;

    for (int pos = 0; pos < len; pos += 1)
    {
        xreal = bufI[pos];
        yimag = bufQ[pos];
        (**it).setReal(xreal << decimation_shifts<SdrBits, InputBits>::pre1); // Valgrind optim (2 - comment not repeated)
        (**it).setImag(yimag << decimation_shifts<SdrBits, InputBits>::pre1);
        ++(*it); // Valgrind optim (comment not repeated)
    }
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate2_u(SampleVector::iterator* it, const T* buf, qint32 len)
{
	StorageType xreal, yimag;

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

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate2_u(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len)
{
    StorageType xreal, yimag;

    for (int pos = 0; pos < len - 3; pos += 4)
    {
        // 0: I[0] 1: Q[0] 2: I[1] 3: Q[1]
        xreal = (bufI[pos] - bufQ[pos+1]) << decimation_shifts<SdrBits, InputBits>::pre2;
        yimag = (bufQ[pos] + bufI[pos+1] - 255) << decimation_shifts<SdrBits, InputBits>::pre2;
        (**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post2);
        (**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post2);
        ++(*it);

        // 4: I[2] 5: Q[2] 6: I[3] 7: Q[3]
        xreal = (bufQ[pos+3] - bufI[pos+2]) << decimation_shifts<SdrBits, InputBits>::pre2;
        yimag = (255 - bufQ[pos+2] - bufI[pos+3]) << decimation_shifts<SdrBits, InputBits>::pre2;
        (**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post2);
        (**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post2);
        ++(*it);
    }
}

// No filtering: bad for Rx OK for signal tracking
//template<typename StorageType, typename T, uint SdrBits, uint InputBits>
//void Decimators<StorageType, T, SdrBits, InputBits>::decimate2_inf(SampleVector::iterator* it, const T* buf, qint32 len)
//{
//	StorageType xreal, yimag;
//
//	for (int pos = 0; pos < len - 7; pos += 8)
//	{
//		xreal = (buf[pos+0] - buf[pos+3]) << decimation_shifts<SdrBits, InputBits>::pre2;
//		yimag = (buf[pos+1] + buf[pos+2]) << decimation_shifts<SdrBits, InputBits>::pre2;
//		(**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post2);
//		(**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post2);
//		++(*it);
//
//		xreal = (buf[pos+7] - buf[pos+4]) << decimation_shifts<SdrBits, InputBits>::pre2;
//		yimag = (- buf[pos+5] - buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre2;
//		(**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post2);
//		(**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post2);
//		++(*it);
//	}
//}

//template<typename StorageType, typename T, uint SdrBits, uint InputBits>
//void Decimators<StorageType, T, SdrBits, InputBits>::decimate2_inf(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len)
//{
//    StorageType xreal, yimag;
//
//    for (int pos = 0; pos < len - 3; pos += 4)
//    {
//        // 0: I[0] 1: Q[0] 2: I[1] 3: Q[1]
//        xreal = (bufI[pos] - bufQ[pos+1]) << decimation_shifts<SdrBits, InputBits>::pre2;
//        yimag = (bufQ[pos] + bufI[pos+1]) << decimation_shifts<SdrBits, InputBits>::pre2;
//        (**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post2);
//        (**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post2);
//        ++(*it);
//
//        // 4: I[2] 5: Q[2] 6: I[3] 7: Q[3]
//        xreal = (bufQ[pos+3] - bufI[pos+2]) << decimation_shifts<SdrBits, InputBits>::pre2;
//        yimag = (- bufQ[pos+2] - bufI[pos+3]) << decimation_shifts<SdrBits, InputBits>::pre2;
//        (**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post2);
//        (**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post2);
//        ++(*it);
//    }
//}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate2_inf(SampleVector::iterator* it, const T* buf, qint32 len)
{
    StorageType buf2[4];

    for (int pos = 0; pos < len - 7; pos += 8)
    {
        m_decimator2.myDecimateInf(
                buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre2,
                &buf2[0]);

        (**it).setReal(buf2[0] >> decimation_shifts<SdrBits, InputBits>::post2);
        (**it).setImag(buf2[1] >> decimation_shifts<SdrBits, InputBits>::post2);
        ++(*it);

        (**it).setReal(buf2[2] >> decimation_shifts<SdrBits, InputBits>::post2);
        (**it).setImag(buf2[3] >> decimation_shifts<SdrBits, InputBits>::post2);
        ++(*it);
    }
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate2_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
    StorageType buf2[4];

    for (int pos = 0; pos < len - 7; pos += 8)
    {
        m_decimator2.myDecimateSup(
                buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre2,
                &buf2[0]);

        (**it).setReal(buf2[0] >> decimation_shifts<SdrBits, InputBits>::post2);
        (**it).setImag(buf2[1] >> decimation_shifts<SdrBits, InputBits>::post2);
        ++(*it);

        (**it).setReal(buf2[2] >> decimation_shifts<SdrBits, InputBits>::post2);
        (**it).setImag(buf2[3] >> decimation_shifts<SdrBits, InputBits>::post2);
        ++(*it);
    }
}

// No filtering: bad for Rx OK for signal tracking
//template<typename StorageType, typename T, uint SdrBits, uint InputBits>
//void Decimators<StorageType, T, SdrBits, InputBits>::decimate2_sup(SampleVector::iterator* it, const T* buf, qint32 len)
//{
//	StorageType xreal, yimag;
//
//	for (int pos = 0; pos < len - 7; pos += 8)
//	{
//		xreal = (buf[pos+1] - buf[pos+2]) << decimation_shifts<SdrBits, InputBits>::pre2;
//		yimag = (- buf[pos+0] - buf[pos+3]) << decimation_shifts<SdrBits, InputBits>::pre2;
//		(**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post2);
//		(**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post2);
//		++(*it);
//
//		xreal = (buf[pos+6] - buf[pos+5]) << decimation_shifts<SdrBits, InputBits>::pre2;
//		yimag = (buf[pos+4] + buf[pos+7]) << decimation_shifts<SdrBits, InputBits>::pre2;
//		(**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post2);
//		(**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post2);
//		++(*it);
//	}
//}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate4_inf(SampleVector::iterator* it, const T* buf, qint32 len)
{
    StorageType buf2[8], buf4[4];

    for (int pos = 0; pos < len - 15; pos += 16)
    {
        m_decimator2.myDecimateInf(
                buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre4,
                &buf2[0]);

        m_decimator2.myDecimateInf(
                buf[pos+8] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+9] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+10] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+11] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+12] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+13] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+14] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+15] << decimation_shifts<SdrBits, InputBits>::pre4,
                &buf2[4]);

        m_decimator4.myDecimateSup(
                buf2[0],
                buf2[1],
                buf2[2],
                buf2[3],
                buf2[4],
                buf2[5],
                buf2[6],
                buf2[7],
                &buf4[0]);

        (**it).setReal(buf4[0] >> decimation_shifts<SdrBits, InputBits>::post4);
        (**it).setImag(buf4[1] >> decimation_shifts<SdrBits, InputBits>::post4);
        ++(*it);

        (**it).setReal(buf4[2] >> decimation_shifts<SdrBits, InputBits>::post4);
        (**it).setImag(buf4[3] >> decimation_shifts<SdrBits, InputBits>::post4);
        ++(*it);
    }
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate4_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
    StorageType buf2[8], buf4[4];

    for (int pos = 0; pos < len - 15; pos += 16)
    {
        m_decimator2.myDecimateSup(
                buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre4,
                &buf2[0]);

        m_decimator2.myDecimateSup(
                buf[pos+8] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+9] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+10] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+11] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+12] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+13] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+14] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+15] << decimation_shifts<SdrBits, InputBits>::pre4,
                &buf2[4]);

        m_decimator4.myDecimateInf(
                buf2[0],
                buf2[1],
                buf2[2],
                buf2[3],
                buf2[4],
                buf2[5],
                buf2[6],
                buf2[7],
                &buf4[0]);

        (**it).setReal(buf4[0] >> decimation_shifts<SdrBits, InputBits>::post4);
        (**it).setImag(buf4[1] >> decimation_shifts<SdrBits, InputBits>::post4);
        ++(*it);

        (**it).setReal(buf4[2] >> decimation_shifts<SdrBits, InputBits>::post4);
        (**it).setImag(buf4[3] >> decimation_shifts<SdrBits, InputBits>::post4);
        ++(*it);
    }
}

// No filtering: bad for Rx OK for signal tracking
//template<typename StorageType, typename T, uint SdrBits, uint InputBits>
//void Decimators<StorageType, T, SdrBits, InputBits>::decimate4_inf(SampleVector::iterator* it, const T* buf, qint32 len)
//{
//	StorageType xreal, yimag;
//
//	for (int pos = 0; pos < len - 7; pos += 8)
//	{
//		xreal = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) << decimation_shifts<SdrBits, InputBits>::pre4;
//		yimag = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre4;
//
//		(**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post4);
//		(**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post4);
//
//		++(*it);
//	}
//}

// No filtering: bad for Rx OK for signal tracking
//template<typename StorageType, typename T, uint SdrBits, uint InputBits>
//void Decimators<StorageType, T, SdrBits, InputBits>::decimate4_sup(SampleVector::iterator* it, const T* buf, qint32 len)
//{
//	// Sup (USB):
//	//            x  y   x  y   x   y  x   y  / x -> 1,-2,-5,6 / y -> -0,-3,4,7
//	// [ rotate:  1, 0, -2, 3, -5, -4, 6, -7]
//	// Inf (LSB):
//	//            x  y   x  y   x   y  x   y  / x -> 0,-3,-4,7 / y -> 1,2,-5,-6
//	// [ rotate:  0, 1, -3, 2, -4, -5, 7, -6]
//	StorageType xreal, yimag;
//
//	for (int pos = 0; pos < len - 7; pos += 8)
//	{
//		xreal = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]) << decimation_shifts<SdrBits, InputBits>::pre4;
//		yimag = (- buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7]) << decimation_shifts<SdrBits, InputBits>::pre4;
//
//		(**it).setReal(xreal >> decimation_shifts<SdrBits, InputBits>::post4);
//		(**it).setImag(yimag >> decimation_shifts<SdrBits, InputBits>::post4);
//
//		++(*it);
//	}
//}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate8_inf(SampleVector::iterator* it, const T* buf, qint32 len)
{
    StorageType buf2[16], buf4[8], buf8[4];

	for (int pos = 0; pos < len - 31; pos += 32)
	{
        m_decimator2.myDecimateInf(
                buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre8,
                &buf2[0]);

        m_decimator2.myDecimateInf(
                buf[pos+8] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+9] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+10] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+11] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+12] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+13] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+14] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+15] << decimation_shifts<SdrBits, InputBits>::pre8,
                &buf2[4]);

        m_decimator2.myDecimateInf(
                buf[pos+16] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+17] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+18] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+19] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+20] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+21] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+22] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+23] << decimation_shifts<SdrBits, InputBits>::pre8,
                &buf2[8]);

        m_decimator2.myDecimateInf(
                buf[pos+24] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+25] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+26] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+27] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+28] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+29] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+30] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+31] << decimation_shifts<SdrBits, InputBits>::pre8,
                &buf2[12]);

        m_decimator4.myDecimateSup(
                &buf2[0],
                &buf4[0]);

        m_decimator4.myDecimateSup(
                &buf2[8],
                &buf4[4]);

        m_decimator8.myDecimateCen(
                &buf4[0],
                &buf8[0]);

        (**it).setReal(buf8[0] >> decimation_shifts<SdrBits, InputBits>::post8);
        (**it).setImag(buf8[1] >> decimation_shifts<SdrBits, InputBits>::post8);
        ++(*it);

        (**it).setReal(buf8[2] >> decimation_shifts<SdrBits, InputBits>::post8);
        (**it).setImag(buf8[3] >> decimation_shifts<SdrBits, InputBits>::post8);
        ++(*it);
	}
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate8_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
    StorageType buf2[16], buf4[8], buf8[4];

    for (int pos = 0; pos < len - 31; pos += 32)
    {
        m_decimator2.myDecimateSup(
                buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre8,
                &buf2[0]);

        m_decimator2.myDecimateSup(
                buf[pos+8] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+9] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+10] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+11] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+12] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+13] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+14] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+15] << decimation_shifts<SdrBits, InputBits>::pre8,
                &buf2[4]);

        m_decimator2.myDecimateSup(
                buf[pos+16] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+17] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+18] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+19] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+20] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+21] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+22] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+23] << decimation_shifts<SdrBits, InputBits>::pre8,
                &buf2[8]);

        m_decimator2.myDecimateSup(
                buf[pos+24] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+25] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+26] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+27] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+28] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+29] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+30] << decimation_shifts<SdrBits, InputBits>::pre8,
                buf[pos+31] << decimation_shifts<SdrBits, InputBits>::pre8,
                &buf2[12]);

        m_decimator4.myDecimateInf(
                &buf2[0],
                &buf4[0]);

        m_decimator4.myDecimateInf(
                &buf2[8],
                &buf4[4]);

        m_decimator8.myDecimateCen(
                &buf4[0],
                &buf8[0]);

        (**it).setReal(buf8[0] >> decimation_shifts<SdrBits, InputBits>::post8);
        (**it).setImag(buf8[1] >> decimation_shifts<SdrBits, InputBits>::post8);
        ++(*it);

        (**it).setReal(buf8[2] >> decimation_shifts<SdrBits, InputBits>::post8);
        (**it).setImag(buf8[3] >> decimation_shifts<SdrBits, InputBits>::post8);
        ++(*it);
    }
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate16_inf(SampleVector::iterator* it, const T* buf, qint32 len)
{
    StorageType buf2[32], buf4[16], buf8[8], buf16[4];

    for (int pos = 0; pos < len - 63; pos += 64)
    {
        m_decimator2.myDecimateInf(
                buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre16,
                &buf2[0]);

        m_decimator2.myDecimateInf(
                buf[pos+8] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+9] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+10] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+11] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+12] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+13] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+14] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+15] << decimation_shifts<SdrBits, InputBits>::pre16,
                &buf2[4]);

        m_decimator2.myDecimateInf(
                buf[pos+16] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+17] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+18] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+19] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+20] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+21] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+22] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+23] << decimation_shifts<SdrBits, InputBits>::pre16,
                &buf2[8]);

        m_decimator2.myDecimateInf(
                buf[pos+24] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+25] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+26] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+27] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+28] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+29] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+30] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+31] << decimation_shifts<SdrBits, InputBits>::pre16,
                &buf2[12]);

        m_decimator2.myDecimateInf(
                buf[pos+32] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+33] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+34] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+35] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+36] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+37] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+38] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+39] << decimation_shifts<SdrBits, InputBits>::pre16,
                &buf2[16]);

        m_decimator2.myDecimateInf(
                buf[pos+40] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+41] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+42] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+43] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+44] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+45] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+46] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+47] << decimation_shifts<SdrBits, InputBits>::pre16,
                &buf2[20]);

        m_decimator2.myDecimateInf(
                buf[pos+48] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+49] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+50] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+51] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+52] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+53] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+54] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+55] << decimation_shifts<SdrBits, InputBits>::pre16,
                &buf2[24]);

        m_decimator2.myDecimateInf(
                buf[pos+56] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+57] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+58] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+59] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+60] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+61] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+62] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+63] << decimation_shifts<SdrBits, InputBits>::pre16,
                &buf2[28]);

        m_decimator4.myDecimateSup(
                &buf2[0],
                &buf4[0]);

        m_decimator4.myDecimateSup(
                &buf2[8],
                &buf4[4]);

        m_decimator4.myDecimateSup(
                &buf2[16],
                &buf4[8]);

        m_decimator4.myDecimateSup(
                &buf2[24],
                &buf4[12]);

        m_decimator8.myDecimateSup(
                &buf4[0],
                &buf8[0]);

        m_decimator8.myDecimateSup(
                &buf4[8],
                &buf8[4]);

        m_decimator16.myDecimateCen(
                &buf8[0],
                &buf16[0]);

        (**it).setReal(buf16[0] >> decimation_shifts<SdrBits, InputBits>::post16);
        (**it).setImag(buf16[1] >> decimation_shifts<SdrBits, InputBits>::post16);
        ++(*it);

        (**it).setReal(buf16[2] >> decimation_shifts<SdrBits, InputBits>::post16);
        (**it).setImag(buf16[3] >> decimation_shifts<SdrBits, InputBits>::post16);
        ++(*it);
    }
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate16_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
    StorageType buf2[32], buf4[16], buf8[8], buf16[4];

    for (int pos = 0; pos < len - 63; pos += 64)
    {
        m_decimator2.myDecimateSup(
                buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre16,
                &buf2[0]);

        m_decimator2.myDecimateSup(
                buf[pos+8] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+9] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+10] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+11] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+12] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+13] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+14] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+15] << decimation_shifts<SdrBits, InputBits>::pre16,
                &buf2[4]);

        m_decimator2.myDecimateSup(
                buf[pos+16] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+17] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+18] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+19] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+20] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+21] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+22] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+23] << decimation_shifts<SdrBits, InputBits>::pre16,
                &buf2[8]);

        m_decimator2.myDecimateSup(
                buf[pos+24] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+25] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+26] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+27] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+28] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+29] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+30] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+31] << decimation_shifts<SdrBits, InputBits>::pre16,
                &buf2[12]);

        m_decimator2.myDecimateSup(
                buf[pos+32] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+33] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+34] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+35] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+36] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+37] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+38] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+39] << decimation_shifts<SdrBits, InputBits>::pre16,
                &buf2[16]);

        m_decimator2.myDecimateSup(
                buf[pos+40] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+41] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+42] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+43] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+44] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+45] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+46] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+47] << decimation_shifts<SdrBits, InputBits>::pre16,
                &buf2[20]);

        m_decimator2.myDecimateSup(
                buf[pos+48] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+49] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+50] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+51] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+52] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+53] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+54] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+55] << decimation_shifts<SdrBits, InputBits>::pre16,
                &buf2[24]);

        m_decimator2.myDecimateSup(
                buf[pos+56] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+57] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+58] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+59] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+60] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+61] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+62] << decimation_shifts<SdrBits, InputBits>::pre16,
                buf[pos+63] << decimation_shifts<SdrBits, InputBits>::pre16,
                &buf2[28]);

        m_decimator4.myDecimateInf(
                &buf2[0],
                &buf4[0]);

        m_decimator4.myDecimateInf(
                &buf2[8],
                &buf4[4]);

        m_decimator4.myDecimateInf(
                &buf2[16],
                &buf4[8]);

        m_decimator4.myDecimateInf(
                &buf2[24],
                &buf4[12]);

        m_decimator8.myDecimateInf(
                &buf4[0],
                &buf8[0]);

        m_decimator8.myDecimateInf(
                &buf4[8],
                &buf8[4]);

        m_decimator16.myDecimateCen(
                &buf8[0],
                &buf16[0]);

        (**it).setReal(buf16[0] >> decimation_shifts<SdrBits, InputBits>::post16);
        (**it).setImag(buf16[1] >> decimation_shifts<SdrBits, InputBits>::post16);
        ++(*it);

        (**it).setReal(buf16[2] >> decimation_shifts<SdrBits, InputBits>::post16);
        (**it).setImag(buf16[3] >> decimation_shifts<SdrBits, InputBits>::post16);
        ++(*it);
    }
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate32_inf(SampleVector::iterator* it, const T* buf, qint32 len)
{
    StorageType buf2[64], buf4[32], buf8[16], buf16[8], buf32[4];

    for (int pos = 0; pos < len - 127; pos += 128)
    {
        m_decimator2.myDecimateInf(
                buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[0]);

        m_decimator2.myDecimateInf(
                buf[pos+8] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+9] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+10] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+11] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+12] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+13] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+14] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+15] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[4]);

        m_decimator2.myDecimateInf(
                buf[pos+16] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+17] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+18] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+19] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+20] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+21] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+22] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+23] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[8]);

        m_decimator2.myDecimateInf(
                buf[pos+24] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+25] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+26] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+27] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+28] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+29] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+30] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+31] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[12]);

        m_decimator2.myDecimateInf(
                buf[pos+32] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+33] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+34] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+35] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+36] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+37] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+38] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+39] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[16]);

        m_decimator2.myDecimateInf(
                buf[pos+40] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+41] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+42] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+43] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+44] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+45] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+46] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+47] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[20]);

        m_decimator2.myDecimateInf(
                buf[pos+48] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+49] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+50] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+51] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+52] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+53] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+54] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+55] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[24]);

        m_decimator2.myDecimateInf(
                buf[pos+56] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+57] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+58] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+59] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+60] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+61] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+62] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+63] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[28]);

        m_decimator2.myDecimateInf(
                buf[pos+64] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+65] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+66] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+67] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+68] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+69] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+70] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+71] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[32]);

        m_decimator2.myDecimateInf(
                buf[pos+72] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+73] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+74] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+75] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+76] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+77] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+78] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+79] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[36]);

        m_decimator2.myDecimateInf(
                buf[pos+80] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+81] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+82] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+83] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+84] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+85] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+86] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+87] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[40]);

        m_decimator2.myDecimateInf(
                buf[pos+88] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+89] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+90] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+91] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+92] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+93] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+94] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+95] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[44]);

        m_decimator2.myDecimateInf(
                buf[pos+96] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+97] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+98] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+99] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+100] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+101] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+102] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+103] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[48]);

        m_decimator2.myDecimateInf(
                buf[pos+104] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+105] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+106] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+107] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+108] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+109] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+110] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+111] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[52]);

        m_decimator2.myDecimateInf(
                buf[pos+112] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+113] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+114] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+115] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+116] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+117] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+118] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+119] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[56]);

        m_decimator2.myDecimateInf(
                buf[pos+120] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+121] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+122] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+123] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+124] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+125] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+126] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+127] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[60]);

        m_decimator4.myDecimateSup(
                &buf2[0],
                &buf4[0]);

        m_decimator4.myDecimateSup(
                &buf2[8],
                &buf4[4]);

        m_decimator4.myDecimateSup(
                &buf2[16],
                &buf4[8]);

        m_decimator4.myDecimateSup(
                &buf2[24],
                &buf4[12]);

        m_decimator4.myDecimateSup(
                &buf2[32],
                &buf4[16]);

        m_decimator4.myDecimateSup(
                &buf2[40],
                &buf4[20]);

        m_decimator4.myDecimateSup(
                &buf2[48],
                &buf4[24]);

        m_decimator4.myDecimateSup(
                &buf2[56],
                &buf4[28]);

        m_decimator8.myDecimateSup(
                &buf4[0],
                &buf8[0]);

        m_decimator8.myDecimateSup(
                &buf4[8],
                &buf8[4]);

        m_decimator8.myDecimateSup(
                &buf4[16],
                &buf8[8]);

        m_decimator8.myDecimateSup(
                &buf4[24],
                &buf8[12]);

        m_decimator16.myDecimateSup(
                &buf8[0],
                &buf16[0]);

        m_decimator16.myDecimateSup(
                &buf8[8],
                &buf16[4]);

        m_decimator32.myDecimateCen(
                &buf16[0],
                &buf32[0]);

        (**it).setReal(buf32[0] >> decimation_shifts<SdrBits, InputBits>::post32);
        (**it).setImag(buf32[1] >> decimation_shifts<SdrBits, InputBits>::post32);
        ++(*it);

        (**it).setReal(buf32[2] >> decimation_shifts<SdrBits, InputBits>::post32);
        (**it).setImag(buf32[3] >> decimation_shifts<SdrBits, InputBits>::post32);
        ++(*it);
    }
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate32_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
    StorageType buf2[64], buf4[32], buf8[16], buf16[8], buf32[4];

    for (int pos = 0; pos < len - 127; pos += 128)
    {
        m_decimator2.myDecimateSup(
                buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[0]);

        m_decimator2.myDecimateSup(
                buf[pos+8] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+9] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+10] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+11] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+12] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+13] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+14] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+15] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[4]);

        m_decimator2.myDecimateSup(
                buf[pos+16] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+17] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+18] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+19] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+20] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+21] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+22] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+23] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[8]);

        m_decimator2.myDecimateSup(
                buf[pos+24] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+25] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+26] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+27] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+28] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+29] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+30] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+31] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[12]);

        m_decimator2.myDecimateSup(
                buf[pos+32] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+33] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+34] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+35] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+36] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+37] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+38] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+39] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[16]);

        m_decimator2.myDecimateSup(
                buf[pos+40] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+41] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+42] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+43] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+44] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+45] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+46] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+47] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[20]);

        m_decimator2.myDecimateSup(
                buf[pos+48] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+49] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+50] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+51] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+52] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+53] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+54] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+55] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[24]);

        m_decimator2.myDecimateSup(
                buf[pos+56] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+57] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+58] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+59] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+60] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+61] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+62] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+63] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[28]);

        m_decimator2.myDecimateSup(
                buf[pos+64] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+65] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+66] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+67] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+68] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+69] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+70] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+71] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[32]);

        m_decimator2.myDecimateSup(
                buf[pos+72] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+73] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+74] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+75] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+76] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+77] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+78] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+79] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[36]);

        m_decimator2.myDecimateSup(
                buf[pos+80] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+81] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+82] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+83] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+84] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+85] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+86] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+87] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[40]);

        m_decimator2.myDecimateSup(
                buf[pos+88] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+89] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+90] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+91] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+92] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+93] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+94] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+95] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[44]);

        m_decimator2.myDecimateSup(
                buf[pos+96] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+97] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+98] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+99] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+100] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+101] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+102] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+103] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[48]);

        m_decimator2.myDecimateSup(
                buf[pos+104] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+105] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+106] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+107] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+108] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+109] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+110] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+111] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[52]);

        m_decimator2.myDecimateSup(
                buf[pos+112] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+113] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+114] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+115] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+116] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+117] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+118] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+119] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[56]);

        m_decimator2.myDecimateSup(
                buf[pos+120] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+121] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+122] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+123] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+124] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+125] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+126] << decimation_shifts<SdrBits, InputBits>::pre32,
                buf[pos+127] << decimation_shifts<SdrBits, InputBits>::pre32,
                &buf2[60]);

        m_decimator4.myDecimateInf(
                &buf2[0],
                &buf4[0]);

        m_decimator4.myDecimateInf(
                &buf2[8],
                &buf4[4]);

        m_decimator4.myDecimateInf(
                &buf2[16],
                &buf4[8]);

        m_decimator4.myDecimateInf(
                &buf2[24],
                &buf4[12]);

        m_decimator4.myDecimateInf(
                &buf2[32],
                &buf4[16]);

        m_decimator4.myDecimateInf(
                &buf2[40],
                &buf4[20]);

        m_decimator4.myDecimateInf(
                &buf2[48],
                &buf4[24]);

        m_decimator4.myDecimateInf(
                &buf2[56],
                &buf4[28]);

        m_decimator8.myDecimateInf(
                &buf4[0],
                &buf8[0]);

        m_decimator8.myDecimateInf(
                &buf4[8],
                &buf8[4]);

        m_decimator8.myDecimateInf(
                &buf4[16],
                &buf8[8]);

        m_decimator8.myDecimateInf(
                &buf4[24],
                &buf8[12]);

        m_decimator16.myDecimateInf(
                &buf8[0],
                &buf16[0]);

        m_decimator16.myDecimateInf(
                &buf8[8],
                &buf16[4]);

        m_decimator32.myDecimateCen(
                &buf16[0],
                &buf32[0]);

        (**it).setReal(buf32[0] >> decimation_shifts<SdrBits, InputBits>::post32);
        (**it).setImag(buf32[1] >> decimation_shifts<SdrBits, InputBits>::post32);
        ++(*it);

        (**it).setReal(buf32[2] >> decimation_shifts<SdrBits, InputBits>::post32);
        (**it).setImag(buf32[3] >> decimation_shifts<SdrBits, InputBits>::post32);
        ++(*it);
    }
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate64_inf(SampleVector::iterator* it, const T* buf, qint32 len)
{
    StorageType buf2[128], buf4[64], buf8[32], buf16[16], buf32[8], buf64[4];

    for (int pos = 0; pos < len - 255; pos += 256)
    {
        m_decimator2.myDecimateInf(
                buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[0]);

        m_decimator2.myDecimateInf(
                buf[pos+8] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+9] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+10] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+11] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+12] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+13] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+14] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+15] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[4]);

        m_decimator2.myDecimateInf(
                buf[pos+16] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+17] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+18] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+19] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+20] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+21] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+22] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+23] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[8]);

        m_decimator2.myDecimateInf(
                buf[pos+24] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+25] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+26] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+27] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+28] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+29] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+30] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+31] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[12]);

        m_decimator2.myDecimateInf(
                buf[pos+32] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+33] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+34] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+35] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+36] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+37] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+38] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+39] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[16]);

        m_decimator2.myDecimateInf(
                buf[pos+40] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+41] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+42] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+43] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+44] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+45] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+46] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+47] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[20]);

        m_decimator2.myDecimateInf(
                buf[pos+48] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+49] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+50] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+51] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+52] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+53] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+54] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+55] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[24]);

        m_decimator2.myDecimateInf(
                buf[pos+56] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+57] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+58] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+59] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+60] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+61] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+62] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+63] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[28]);

        m_decimator2.myDecimateInf(
                buf[pos+64] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+65] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+66] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+67] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+68] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+69] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+70] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+71] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[32]);

        m_decimator2.myDecimateInf(
                buf[pos+72] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+73] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+74] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+75] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+76] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+77] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+78] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+79] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[36]);

        m_decimator2.myDecimateInf(
                buf[pos+80] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+81] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+82] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+83] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+84] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+85] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+86] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+87] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[40]);

        m_decimator2.myDecimateInf(
                buf[pos+88] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+89] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+90] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+91] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+92] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+93] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+94] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+95] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[44]);

        m_decimator2.myDecimateInf(
                buf[pos+96] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+97] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+98] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+99] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+100] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+101] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+102] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+103] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[48]);

        m_decimator2.myDecimateInf(
                buf[pos+104] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+105] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+106] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+107] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+108] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+109] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+110] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+111] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[52]);

        m_decimator2.myDecimateInf(
                buf[pos+112] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+113] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+114] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+115] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+116] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+117] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+118] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+119] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[56]);

        m_decimator2.myDecimateInf(
                buf[pos+120] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+121] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+122] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+123] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+124] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+125] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+126] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+127] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[60]);

        m_decimator2.myDecimateInf(
                buf[pos+128] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+129] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+130] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+131] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+132] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+133] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+134] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+135] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[64]);

        m_decimator2.myDecimateInf(
                buf[pos+136] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+137] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+138] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+139] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+140] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+141] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+142] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+143] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[68]);

        m_decimator2.myDecimateInf(
                buf[pos+144] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+145] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+146] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+147] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+148] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+149] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+150] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+151] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[72]);

        m_decimator2.myDecimateInf(
                buf[pos+152] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+153] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+154] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+155] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+156] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+157] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+158] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+159] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[76]);

        m_decimator2.myDecimateInf(
                buf[pos+160] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+161] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+162] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+163] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+164] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+165] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+166] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+167] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[80]);

        m_decimator2.myDecimateInf(
                buf[pos+168] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+169] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+170] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+171] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+172] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+173] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+174] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+175] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[84]);

        m_decimator2.myDecimateInf(
                buf[pos+176] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+177] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+178] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+179] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+180] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+181] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+182] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+183] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[88]);

        m_decimator2.myDecimateInf(
                buf[pos+184] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+185] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+186] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+187] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+188] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+189] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+190] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+191] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[92]);

        m_decimator2.myDecimateInf(
                buf[pos+192] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+193] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+194] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+195] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+196] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+197] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+198] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+199] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[96]);

        m_decimator2.myDecimateInf(
                buf[pos+200] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+201] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+202] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+203] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+204] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+205] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+206] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+207] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[100]);

        m_decimator2.myDecimateInf(
                buf[pos+208] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+209] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+210] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+211] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+212] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+213] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+214] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+215] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[104]);

        m_decimator2.myDecimateInf(
                buf[pos+216] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+217] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+218] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+219] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+220] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+221] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+222] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+223] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[108]);

        m_decimator2.myDecimateInf(
                buf[pos+224] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+225] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+226] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+227] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+228] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+229] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+230] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+231] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[112]);

        m_decimator2.myDecimateInf(
                buf[pos+232] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+233] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+234] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+235] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+236] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+237] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+238] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+239] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[116]);

        m_decimator2.myDecimateInf(
                buf[pos+240] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+241] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+242] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+243] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+244] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+245] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+246] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+247] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[120]);

        m_decimator2.myDecimateInf(
                buf[pos+248] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+249] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+250] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+251] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+252] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+253] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+254] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+255] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[124]);

        m_decimator4.myDecimateSup(
                &buf2[0],
                &buf4[0]);

        m_decimator4.myDecimateSup(
                &buf2[8],
                &buf4[4]);

        m_decimator4.myDecimateSup(
                &buf2[16],
                &buf4[8]);

        m_decimator4.myDecimateSup(
                &buf2[24],
                &buf4[12]);

        m_decimator4.myDecimateSup(
                &buf2[32],
                &buf4[16]);

        m_decimator4.myDecimateSup(
                &buf2[40],
                &buf4[20]);

        m_decimator4.myDecimateSup(
                &buf2[48],
                &buf4[24]);

        m_decimator4.myDecimateSup(
                &buf2[56],
                &buf4[28]);

        m_decimator4.myDecimateSup(
                &buf2[64],
                &buf4[32]);

        m_decimator4.myDecimateSup(
                &buf2[72],
                &buf4[36]);

        m_decimator4.myDecimateSup(
                &buf2[80],
                &buf4[40]);

        m_decimator4.myDecimateSup(
                &buf2[88],
                &buf4[44]);

        m_decimator4.myDecimateSup(
                &buf2[96],
                &buf4[48]);

        m_decimator4.myDecimateSup(
                &buf2[104],
                &buf4[52]);

        m_decimator4.myDecimateSup(
                &buf2[112],
                &buf4[56]);

        m_decimator4.myDecimateSup(
                &buf2[120],
                &buf4[60]);

        m_decimator8.myDecimateSup(
                &buf4[0],
                &buf8[0]);

        m_decimator8.myDecimateSup(
                &buf4[8],
                &buf8[4]);

        m_decimator8.myDecimateSup(
                &buf4[16],
                &buf8[8]);

        m_decimator8.myDecimateSup(
                &buf4[24],
                &buf8[12]);

        m_decimator8.myDecimateSup(
                &buf4[32],
                &buf8[16]);

        m_decimator8.myDecimateSup(
                &buf4[40],
                &buf8[20]);

        m_decimator8.myDecimateSup(
                &buf4[48],
                &buf8[24]);

        m_decimator8.myDecimateSup(
                &buf4[56],
                &buf8[28]);

        m_decimator16.myDecimateSup(
                &buf8[0],
                &buf16[0]);

        m_decimator16.myDecimateSup(
                &buf8[8],
                &buf16[4]);

        m_decimator16.myDecimateSup(
                &buf8[16],
                &buf16[8]);

        m_decimator16.myDecimateSup(
                &buf8[24],
                &buf16[12]);

        m_decimator32.myDecimateSup(
                &buf16[0],
                &buf32[0]);

        m_decimator32.myDecimateSup(
                &buf16[8],
                &buf32[4]);

        m_decimator64.myDecimateCen(
                &buf32[0],
                &buf64[0]);

        (**it).setReal(buf64[0] >> decimation_shifts<SdrBits, InputBits>::post64);
        (**it).setImag(buf64[1] >> decimation_shifts<SdrBits, InputBits>::post64);
        ++(*it);

        (**it).setReal(buf64[2] >> decimation_shifts<SdrBits, InputBits>::post64);
        (**it).setImag(buf64[3] >> decimation_shifts<SdrBits, InputBits>::post64);
        ++(*it);
    }
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate64_sup(SampleVector::iterator* it, const T* buf, qint32 len)
{
    StorageType buf2[128], buf4[64], buf8[32], buf16[16], buf32[8], buf64[4];

    for (int pos = 0; pos < len - 255; pos += 256)
    {
        m_decimator2.myDecimateSup(
                buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[0]);

        m_decimator2.myDecimateSup(
                buf[pos+8] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+9] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+10] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+11] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+12] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+13] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+14] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+15] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[4]);

        m_decimator2.myDecimateSup(
                buf[pos+16] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+17] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+18] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+19] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+20] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+21] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+22] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+23] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[8]);

        m_decimator2.myDecimateSup(
                buf[pos+24] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+25] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+26] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+27] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+28] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+29] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+30] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+31] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[12]);

        m_decimator2.myDecimateSup(
                buf[pos+32] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+33] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+34] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+35] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+36] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+37] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+38] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+39] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[16]);

        m_decimator2.myDecimateSup(
                buf[pos+40] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+41] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+42] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+43] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+44] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+45] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+46] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+47] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[20]);

        m_decimator2.myDecimateSup(
                buf[pos+48] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+49] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+50] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+51] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+52] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+53] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+54] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+55] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[24]);

        m_decimator2.myDecimateSup(
                buf[pos+56] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+57] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+58] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+59] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+60] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+61] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+62] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+63] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[28]);

        m_decimator2.myDecimateSup(
                buf[pos+64] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+65] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+66] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+67] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+68] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+69] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+70] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+71] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[32]);

        m_decimator2.myDecimateSup(
                buf[pos+72] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+73] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+74] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+75] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+76] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+77] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+78] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+79] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[36]);

        m_decimator2.myDecimateSup(
                buf[pos+80] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+81] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+82] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+83] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+84] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+85] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+86] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+87] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[40]);

        m_decimator2.myDecimateSup(
                buf[pos+88] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+89] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+90] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+91] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+92] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+93] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+94] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+95] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[44]);

        m_decimator2.myDecimateSup(
                buf[pos+96] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+97] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+98] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+99] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+100] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+101] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+102] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+103] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[48]);

        m_decimator2.myDecimateSup(
                buf[pos+104] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+105] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+106] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+107] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+108] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+109] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+110] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+111] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[52]);

        m_decimator2.myDecimateSup(
                buf[pos+112] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+113] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+114] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+115] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+116] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+117] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+118] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+119] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[56]);

        m_decimator2.myDecimateSup(
                buf[pos+120] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+121] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+122] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+123] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+124] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+125] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+126] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+127] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[60]);

        m_decimator2.myDecimateSup(
                buf[pos+128] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+129] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+130] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+131] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+132] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+133] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+134] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+135] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[64]);

        m_decimator2.myDecimateSup(
                buf[pos+136] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+137] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+138] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+139] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+140] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+141] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+142] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+143] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[68]);

        m_decimator2.myDecimateSup(
                buf[pos+144] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+145] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+146] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+147] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+148] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+149] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+150] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+151] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[72]);

        m_decimator2.myDecimateSup(
                buf[pos+152] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+153] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+154] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+155] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+156] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+157] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+158] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+159] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[76]);

        m_decimator2.myDecimateSup(
                buf[pos+160] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+161] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+162] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+163] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+164] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+165] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+166] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+167] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[80]);

        m_decimator2.myDecimateSup(
                buf[pos+168] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+169] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+170] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+171] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+172] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+173] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+174] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+175] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[84]);

        m_decimator2.myDecimateSup(
                buf[pos+176] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+177] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+178] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+179] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+180] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+181] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+182] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+183] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[88]);

        m_decimator2.myDecimateSup(
                buf[pos+184] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+185] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+186] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+187] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+188] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+189] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+190] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+191] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[92]);

        m_decimator2.myDecimateSup(
                buf[pos+192] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+193] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+194] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+195] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+196] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+197] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+198] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+199] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[96]);

        m_decimator2.myDecimateSup(
                buf[pos+200] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+201] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+202] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+203] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+204] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+205] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+206] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+207] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[100]);

        m_decimator2.myDecimateSup(
                buf[pos+208] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+209] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+210] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+211] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+212] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+213] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+214] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+215] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[104]);

        m_decimator2.myDecimateSup(
                buf[pos+216] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+217] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+218] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+219] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+220] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+221] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+222] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+223] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[108]);

        m_decimator2.myDecimateSup(
                buf[pos+224] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+225] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+226] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+227] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+228] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+229] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+230] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+231] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[112]);

        m_decimator2.myDecimateSup(
                buf[pos+232] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+233] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+234] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+235] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+236] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+237] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+238] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+239] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[116]);

        m_decimator2.myDecimateSup(
                buf[pos+240] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+241] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+242] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+243] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+244] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+245] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+246] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+247] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[120]);

        m_decimator2.myDecimateSup(
                buf[pos+248] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+249] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+250] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+251] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+252] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+253] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+254] << decimation_shifts<SdrBits, InputBits>::pre64,
                buf[pos+255] << decimation_shifts<SdrBits, InputBits>::pre64,
                &buf2[124]);

        m_decimator4.myDecimateInf(
                &buf2[0],
                &buf4[0]);

        m_decimator4.myDecimateInf(
                &buf2[8],
                &buf4[4]);

        m_decimator4.myDecimateInf(
                &buf2[16],
                &buf4[8]);

        m_decimator4.myDecimateInf(
                &buf2[24],
                &buf4[12]);

        m_decimator4.myDecimateInf(
                &buf2[32],
                &buf4[16]);

        m_decimator4.myDecimateInf(
                &buf2[40],
                &buf4[20]);

        m_decimator4.myDecimateInf(
                &buf2[48],
                &buf4[24]);

        m_decimator4.myDecimateInf(
                &buf2[56],
                &buf4[28]);

        m_decimator4.myDecimateInf(
                &buf2[64],
                &buf4[32]);

        m_decimator4.myDecimateInf(
                &buf2[72],
                &buf4[36]);

        m_decimator4.myDecimateInf(
                &buf2[80],
                &buf4[40]);

        m_decimator4.myDecimateInf(
                &buf2[88],
                &buf4[44]);

        m_decimator4.myDecimateInf(
                &buf2[96],
                &buf4[48]);

        m_decimator4.myDecimateInf(
                &buf2[104],
                &buf4[52]);

        m_decimator4.myDecimateInf(
                &buf2[112],
                &buf4[56]);

        m_decimator4.myDecimateInf(
                &buf2[120],
                &buf4[60]);

        m_decimator8.myDecimateInf(
                &buf4[0],
                &buf8[0]);

        m_decimator8.myDecimateInf(
                &buf4[8],
                &buf8[4]);

        m_decimator8.myDecimateInf(
                &buf4[16],
                &buf8[8]);

        m_decimator8.myDecimateInf(
                &buf4[24],
                &buf8[12]);

        m_decimator8.myDecimateInf(
                &buf4[32],
                &buf8[16]);

        m_decimator8.myDecimateInf(
                &buf4[40],
                &buf8[20]);

        m_decimator8.myDecimateInf(
                &buf4[48],
                &buf8[24]);

        m_decimator8.myDecimateInf(
                &buf4[56],
                &buf8[28]);

        m_decimator16.myDecimateInf(
                &buf8[0],
                &buf16[0]);

        m_decimator16.myDecimateInf(
                &buf8[8],
                &buf16[4]);

        m_decimator16.myDecimateInf(
                &buf8[16],
                &buf16[8]);

        m_decimator16.myDecimateInf(
                &buf8[24],
                &buf16[12]);

        m_decimator32.myDecimateInf(
                &buf16[0],
                &buf32[0]);

        m_decimator32.myDecimateInf(
                &buf16[8],
                &buf32[4]);

        m_decimator64.myDecimateCen(
                &buf32[0],
                &buf64[0]);

        (**it).setReal(buf64[0] >> decimation_shifts<SdrBits, InputBits>::post64);
        (**it).setImag(buf64[1] >> decimation_shifts<SdrBits, InputBits>::post64);
        ++(*it);

        (**it).setReal(buf64[2] >> decimation_shifts<SdrBits, InputBits>::post64);
        (**it).setImag(buf64[3] >> decimation_shifts<SdrBits, InputBits>::post64);
        ++(*it);
    }
}

//template<typename StorageType, typename T, uint SdrBits, uint InputBits>
//void Decimators<StorageType, T, SdrBits, InputBits>::decimate2_cen(SampleVector::iterator* it, const T* buf, qint32 len)
//{
//	StorageType intbuf[2];
//
//	for (int pos = 0; pos < len - 3; pos += 4)
//	{
//		intbuf[0]  = buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre2;
//		intbuf[1]  = buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre2;
//
//		m_decimator2.myDecimate(
//				buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre2,
//				buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre2,
//				&intbuf[0],
//				&intbuf[1]);
//
//		(**it).setReal(intbuf[0] >> decimation_shifts<SdrBits, InputBits>::post2);
//		(**it).setImag(intbuf[1] >> decimation_shifts<SdrBits, InputBits>::post2);
//
//		++(*it);
//	}
//}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate2_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
    StorageType buf2[4];

    for (int pos = 0; pos < len - 7; pos += 8)
    {
        m_decimator2.myDecimateCen(
                buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre2,
                buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre2,
                &buf2[0]);

        (**it).setReal(buf2[0] >> decimation_shifts<SdrBits, InputBits>::post2);
        (**it).setImag(buf2[1] >> decimation_shifts<SdrBits, InputBits>::post2);
        ++(*it);

        (**it).setReal(buf2[2] >> decimation_shifts<SdrBits, InputBits>::post2);
        (**it).setImag(buf2[3] >> decimation_shifts<SdrBits, InputBits>::post2);
        ++(*it);
    }
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate2_cen(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len)
{
    StorageType intbuf[2];

    for (int pos = 0; pos < len - 1; pos += 2)
    {
        intbuf[0]  = bufI[pos+1] << decimation_shifts<SdrBits, InputBits>::pre2;
        intbuf[1]  = bufQ[pos+1] << decimation_shifts<SdrBits, InputBits>::pre2;

        m_decimator2.myDecimate(
                bufI[pos+0] << decimation_shifts<SdrBits, InputBits>::pre2,
                bufQ[pos+0] << decimation_shifts<SdrBits, InputBits>::pre2,
                &intbuf[0],
                &intbuf[1]);

        (**it).setReal(intbuf[0] >> decimation_shifts<SdrBits, InputBits>::post2);
        (**it).setImag(intbuf[1] >> decimation_shifts<SdrBits, InputBits>::post2);
        ++(*it);
    }
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate4_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	StorageType buf2[8], buf4[4];

	for (int pos = 0; pos < len - 15; pos += 16)
	{
        m_decimator2.myDecimateCen(
                buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre4,
                &buf2[0]);

        m_decimator2.myDecimateCen(
                buf[pos+8] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+9] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+10] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+11] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+12] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+13] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+14] << decimation_shifts<SdrBits, InputBits>::pre4,
                buf[pos+15] << decimation_shifts<SdrBits, InputBits>::pre4,
                &buf2[4]);

        m_decimator4.myDecimateCen(
                &buf2[0],
                &buf4[0]);

        (**it).setReal(buf4[0] >> decimation_shifts<SdrBits, InputBits>::post4);
        (**it).setImag(buf4[1] >> decimation_shifts<SdrBits, InputBits>::post4);
        ++(*it);

        (**it).setReal(buf4[2] >> decimation_shifts<SdrBits, InputBits>::post4);
        (**it).setImag(buf4[3] >> decimation_shifts<SdrBits, InputBits>::post4);
        ++(*it);
	}
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate4_cen(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len)
{
    StorageType intbuf[4];

    for (int pos = 0; pos < len - 3; pos += 4)
    {
        intbuf[0]  = bufI[pos+1] << decimation_shifts<SdrBits, InputBits>::pre4;
        intbuf[1]  = bufQ[pos+1] << decimation_shifts<SdrBits, InputBits>::pre4;
        intbuf[2]  = bufI[pos+3] << decimation_shifts<SdrBits, InputBits>::pre4;
        intbuf[3]  = bufQ[pos+3] << decimation_shifts<SdrBits, InputBits>::pre4;

        m_decimator2.myDecimate(
                bufI[pos+0] << decimation_shifts<SdrBits, InputBits>::pre4,
                bufQ[pos+0] << decimation_shifts<SdrBits, InputBits>::pre4,
                &intbuf[0],
                &intbuf[1]);
        m_decimator2.myDecimate(
                bufI[pos+2] << decimation_shifts<SdrBits, InputBits>::pre4,
                bufQ[pos+2] << decimation_shifts<SdrBits, InputBits>::pre4,
                &intbuf[2],
                &intbuf[3]);

        m_decimator4.myDecimate(
                intbuf[0],
                intbuf[1],
                &intbuf[2],
                &intbuf[3]);

        (**it).setReal(intbuf[2] >> decimation_shifts<SdrBits, InputBits>::post4);
        (**it).setImag(intbuf[3] >> decimation_shifts<SdrBits, InputBits>::post4);
        ++(*it);
    }
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate8_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	StorageType intbuf[8];

	for (int pos = 0; pos < len - 15; pos += 16)
	{
		intbuf[0]  = buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre8;
		intbuf[1]  = buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre8;
		intbuf[2]  = buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre8;
		intbuf[3]  = buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre8;
		intbuf[4]  = buf[pos+10] << decimation_shifts<SdrBits, InputBits>::pre8;
		intbuf[5]  = buf[pos+11] << decimation_shifts<SdrBits, InputBits>::pre8;
		intbuf[6]  = buf[pos+14] << decimation_shifts<SdrBits, InputBits>::pre8;
		intbuf[7]  = buf[pos+15] << decimation_shifts<SdrBits, InputBits>::pre8;

		m_decimator2.myDecimate(
				buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre8,
				buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre8,
				&intbuf[0],
				&intbuf[1]);
		m_decimator2.myDecimate(
				buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre8,
				buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre8,
				&intbuf[2],
				&intbuf[3]);
		m_decimator2.myDecimate(
				buf[pos+8] << decimation_shifts<SdrBits, InputBits>::pre8,
				buf[pos+9] << decimation_shifts<SdrBits, InputBits>::pre8,
				&intbuf[4],
				&intbuf[5]);
		m_decimator2.myDecimate(
				buf[pos+12] << decimation_shifts<SdrBits, InputBits>::pre8,
				buf[pos+13] << decimation_shifts<SdrBits, InputBits>::pre8,
				&intbuf[6],
				&intbuf[7]);

		m_decimator4.myDecimate(
				intbuf[0],
				intbuf[1],
				&intbuf[2],
				&intbuf[3]);
		m_decimator4.myDecimate(
				intbuf[4],
				intbuf[5],
				&intbuf[6],
				&intbuf[7]);

		m_decimator8.myDecimate(
				intbuf[2],
				intbuf[3],
				&intbuf[6],
				&intbuf[7]);

		(**it).setReal(intbuf[6] >> decimation_shifts<SdrBits, InputBits>::post8);
		(**it).setImag(intbuf[7] >> decimation_shifts<SdrBits, InputBits>::post8);
		++(*it);
	}
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate8_cen(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len)
{
    StorageType intbuf[8];

    for (int pos = 0; pos < len - 7; pos += 8)
    {
        intbuf[0]  = bufI[pos+1] << decimation_shifts<SdrBits, InputBits>::pre8;
        intbuf[1]  = bufQ[pos+1] << decimation_shifts<SdrBits, InputBits>::pre8;
        intbuf[2]  = bufI[pos+3] << decimation_shifts<SdrBits, InputBits>::pre8;
        intbuf[3]  = bufQ[pos+3] << decimation_shifts<SdrBits, InputBits>::pre8;
        intbuf[4]  = bufI[pos+5] << decimation_shifts<SdrBits, InputBits>::pre8;
        intbuf[5]  = bufQ[pos+5] << decimation_shifts<SdrBits, InputBits>::pre8;
        intbuf[6]  = bufI[pos+7] << decimation_shifts<SdrBits, InputBits>::pre8;
        intbuf[7]  = bufQ[pos+7] << decimation_shifts<SdrBits, InputBits>::pre8;

        m_decimator2.myDecimate(
                bufI[pos+0] << decimation_shifts<SdrBits, InputBits>::pre8,
                bufQ[pos+0] << decimation_shifts<SdrBits, InputBits>::pre8,
                &intbuf[0],
                &intbuf[1]);
        m_decimator2.myDecimate(
                bufI[pos+2] << decimation_shifts<SdrBits, InputBits>::pre8,
                bufQ[pos+2] << decimation_shifts<SdrBits, InputBits>::pre8,
                &intbuf[2],
                &intbuf[3]);
        m_decimator2.myDecimate(
                bufI[pos+4] << decimation_shifts<SdrBits, InputBits>::pre8,
                bufQ[pos+4] << decimation_shifts<SdrBits, InputBits>::pre8,
                &intbuf[4],
                &intbuf[5]);
        m_decimator2.myDecimate(
                bufI[pos+6] << decimation_shifts<SdrBits, InputBits>::pre8,
                bufQ[pos+6] << decimation_shifts<SdrBits, InputBits>::pre8,
                &intbuf[6],
                &intbuf[7]);

        m_decimator4.myDecimate(
                intbuf[0],
                intbuf[1],
                &intbuf[2],
                &intbuf[3]);
        m_decimator4.myDecimate(
                intbuf[4],
                intbuf[5],
                &intbuf[6],
                &intbuf[7]);

        m_decimator8.myDecimate(
                intbuf[2],
                intbuf[3],
                &intbuf[6],
                &intbuf[7]);

        (**it).setReal(intbuf[6] >> decimation_shifts<SdrBits, InputBits>::post8);
        (**it).setImag(intbuf[7] >> decimation_shifts<SdrBits, InputBits>::post8);
        ++(*it);
    }
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate16_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	StorageType intbuf[16];

	for (int pos = 0; pos < len - 31; pos += 32)
	{
		intbuf[0]  = buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre16;
		intbuf[1]  = buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre16;
		intbuf[2]  = buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre16;
		intbuf[3]  = buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre16;
		intbuf[4]  = buf[pos+10] << decimation_shifts<SdrBits, InputBits>::pre16;
		intbuf[5]  = buf[pos+11] << decimation_shifts<SdrBits, InputBits>::pre16;
		intbuf[6]  = buf[pos+14] << decimation_shifts<SdrBits, InputBits>::pre16;
		intbuf[7]  = buf[pos+15] << decimation_shifts<SdrBits, InputBits>::pre16;
		intbuf[8]  = buf[pos+18] << decimation_shifts<SdrBits, InputBits>::pre16;
		intbuf[9]  = buf[pos+19] << decimation_shifts<SdrBits, InputBits>::pre16;
		intbuf[10] = buf[pos+22] << decimation_shifts<SdrBits, InputBits>::pre16;
		intbuf[11] = buf[pos+23] << decimation_shifts<SdrBits, InputBits>::pre16;
		intbuf[12] = buf[pos+26] << decimation_shifts<SdrBits, InputBits>::pre16;
		intbuf[13] = buf[pos+27] << decimation_shifts<SdrBits, InputBits>::pre16;
		intbuf[14] = buf[pos+30] << decimation_shifts<SdrBits, InputBits>::pre16;
		intbuf[15] = buf[pos+31] << decimation_shifts<SdrBits, InputBits>::pre16;

		m_decimator2.myDecimate(
				buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre16,
				buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre16,
				&intbuf[0],
				&intbuf[1]);
		m_decimator2.myDecimate(
				buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre16,
				buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre16,
				&intbuf[2],
				&intbuf[3]);
		m_decimator2.myDecimate(
				buf[pos+8] << decimation_shifts<SdrBits, InputBits>::pre16,
				buf[pos+9] << decimation_shifts<SdrBits, InputBits>::pre16,
				&intbuf[4],
				&intbuf[5]);
		m_decimator2.myDecimate(
				buf[pos+12] << decimation_shifts<SdrBits, InputBits>::pre16,
				buf[pos+13] << decimation_shifts<SdrBits, InputBits>::pre16,
				&intbuf[6],
				&intbuf[7]);
		m_decimator2.myDecimate(
				buf[pos+16] << decimation_shifts<SdrBits, InputBits>::pre16,
				buf[pos+17] << decimation_shifts<SdrBits, InputBits>::pre16,
				&intbuf[8],
				&intbuf[9]);
		m_decimator2.myDecimate(
				buf[pos+20] << decimation_shifts<SdrBits, InputBits>::pre16,
				buf[pos+21] << decimation_shifts<SdrBits, InputBits>::pre16,
				&intbuf[10],
				&intbuf[11]);
		m_decimator2.myDecimate(
				buf[pos+24] << decimation_shifts<SdrBits, InputBits>::pre16,
				buf[pos+25] << decimation_shifts<SdrBits, InputBits>::pre16,
				&intbuf[12],
				&intbuf[13]);
		m_decimator2.myDecimate(
				buf[pos+28] << decimation_shifts<SdrBits, InputBits>::pre16,
				buf[pos+29] << decimation_shifts<SdrBits, InputBits>::pre16,
				&intbuf[14],
				&intbuf[15]);

		m_decimator4.myDecimate(
				intbuf[0],
				intbuf[1],
				&intbuf[2],
				&intbuf[3]);
		m_decimator4.myDecimate(
				intbuf[4],
				intbuf[5],
				&intbuf[6],
				&intbuf[7]);
		m_decimator4.myDecimate(
				intbuf[8],
				intbuf[9],
				&intbuf[10],
				&intbuf[11]);
		m_decimator4.myDecimate(
				intbuf[12],
				intbuf[13],
				&intbuf[14],
				&intbuf[15]);

		m_decimator8.myDecimate(
				intbuf[2],
				intbuf[3],
				&intbuf[6],
				&intbuf[7]);
		m_decimator8.myDecimate(
				intbuf[10],
				intbuf[11],
				&intbuf[14],
				&intbuf[15]);

		m_decimator16.myDecimate(
				intbuf[6],
				intbuf[7],
				&intbuf[14],
				&intbuf[15]);

		(**it).setReal(intbuf[14] >> decimation_shifts<SdrBits, InputBits>::post16);
		(**it).setImag(intbuf[15] >> decimation_shifts<SdrBits, InputBits>::post16);
		++(*it);
	}
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate16_cen(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len)
{
    StorageType intbuf[16];

    for (int pos = 0; pos < len - 15; pos += 16)
    {
        intbuf[0]  = bufI[pos+1] << decimation_shifts<SdrBits, InputBits>::pre16;
        intbuf[1]  = bufQ[pos+1] << decimation_shifts<SdrBits, InputBits>::pre16;
        intbuf[2]  = bufI[pos+3] << decimation_shifts<SdrBits, InputBits>::pre16;
        intbuf[3]  = bufQ[pos+3] << decimation_shifts<SdrBits, InputBits>::pre16;
        intbuf[4]  = bufI[pos+5] << decimation_shifts<SdrBits, InputBits>::pre16;
        intbuf[5]  = bufQ[pos+5] << decimation_shifts<SdrBits, InputBits>::pre16;
        intbuf[6]  = bufI[pos+7] << decimation_shifts<SdrBits, InputBits>::pre16;
        intbuf[7]  = bufQ[pos+7] << decimation_shifts<SdrBits, InputBits>::pre16;
        intbuf[8]  = bufI[pos+9] << decimation_shifts<SdrBits, InputBits>::pre16;
        intbuf[9]  = bufQ[pos+9] << decimation_shifts<SdrBits, InputBits>::pre16;
        intbuf[10] = bufI[pos+11] << decimation_shifts<SdrBits, InputBits>::pre16;
        intbuf[11] = bufQ[pos+11] << decimation_shifts<SdrBits, InputBits>::pre16;
        intbuf[12] = bufI[pos+13] << decimation_shifts<SdrBits, InputBits>::pre16;
        intbuf[13] = bufQ[pos+13] << decimation_shifts<SdrBits, InputBits>::pre16;
        intbuf[14] = bufI[pos+15] << decimation_shifts<SdrBits, InputBits>::pre16;
        intbuf[15] = bufQ[pos+15] << decimation_shifts<SdrBits, InputBits>::pre16;

        m_decimator2.myDecimate(
                bufI[pos+0] << decimation_shifts<SdrBits, InputBits>::pre16,
                bufQ[pos+0] << decimation_shifts<SdrBits, InputBits>::pre16,
                &intbuf[0],
                &intbuf[1]);
        m_decimator2.myDecimate(
                bufI[pos+2] << decimation_shifts<SdrBits, InputBits>::pre16,
                bufQ[pos+2] << decimation_shifts<SdrBits, InputBits>::pre16,
                &intbuf[2],
                &intbuf[3]);
        m_decimator2.myDecimate(
                bufI[pos+4] << decimation_shifts<SdrBits, InputBits>::pre16,
                bufQ[pos+4] << decimation_shifts<SdrBits, InputBits>::pre16,
                &intbuf[4],
                &intbuf[5]);
        m_decimator2.myDecimate(
                bufI[pos+6] << decimation_shifts<SdrBits, InputBits>::pre16,
                bufQ[pos+6] << decimation_shifts<SdrBits, InputBits>::pre16,
                &intbuf[6],
                &intbuf[7]);
        m_decimator2.myDecimate(
                bufI[pos+8] << decimation_shifts<SdrBits, InputBits>::pre16,
                bufQ[pos+8] << decimation_shifts<SdrBits, InputBits>::pre16,
                &intbuf[8],
                &intbuf[9]);
        m_decimator2.myDecimate(
                bufI[pos+10] << decimation_shifts<SdrBits, InputBits>::pre16,
                bufQ[pos+10] << decimation_shifts<SdrBits, InputBits>::pre16,
                &intbuf[10],
                &intbuf[11]);
        m_decimator2.myDecimate(
                bufI[pos+12] << decimation_shifts<SdrBits, InputBits>::pre16,
                bufQ[pos+12] << decimation_shifts<SdrBits, InputBits>::pre16,
                &intbuf[12],
                &intbuf[13]);
        m_decimator2.myDecimate(
                bufI[pos+14] << decimation_shifts<SdrBits, InputBits>::pre16,
                bufQ[pos+14] << decimation_shifts<SdrBits, InputBits>::pre16,
                &intbuf[14],
                &intbuf[15]);

        m_decimator4.myDecimate(
                intbuf[0],
                intbuf[1],
                &intbuf[2],
                &intbuf[3]);
        m_decimator4.myDecimate(
                intbuf[4],
                intbuf[5],
                &intbuf[6],
                &intbuf[7]);
        m_decimator4.myDecimate(
                intbuf[8],
                intbuf[9],
                &intbuf[10],
                &intbuf[11]);
        m_decimator4.myDecimate(
                intbuf[12],
                intbuf[13],
                &intbuf[14],
                &intbuf[15]);

        m_decimator8.myDecimate(
                intbuf[2],
                intbuf[3],
                &intbuf[6],
                &intbuf[7]);
        m_decimator8.myDecimate(
                intbuf[10],
                intbuf[11],
                &intbuf[14],
                &intbuf[15]);

        m_decimator16.myDecimate(
                intbuf[6],
                intbuf[7],
                &intbuf[14],
                &intbuf[15]);

        (**it).setReal(intbuf[14] >> decimation_shifts<SdrBits, InputBits>::post16);
        (**it).setImag(intbuf[15] >> decimation_shifts<SdrBits, InputBits>::post16);
        ++(*it);
    }
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate32_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	StorageType intbuf[32];

	for (int pos = 0; pos < len - 63; pos += 64)
	{
		intbuf[0]  = buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[1]  = buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[2]  = buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[3]  = buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[4]  = buf[pos+10] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[5]  = buf[pos+11] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[6]  = buf[pos+14] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[7]  = buf[pos+15] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[8]  = buf[pos+18] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[9]  = buf[pos+19] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[10] = buf[pos+22] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[11] = buf[pos+23] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[12] = buf[pos+26] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[13] = buf[pos+27] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[14] = buf[pos+30] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[15] = buf[pos+31] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[16] = buf[pos+34] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[17] = buf[pos+35] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[18] = buf[pos+38] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[19] = buf[pos+39] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[20] = buf[pos+42] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[21] = buf[pos+43] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[22] = buf[pos+46] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[23] = buf[pos+47] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[24] = buf[pos+50] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[25] = buf[pos+51] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[26] = buf[pos+54] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[27] = buf[pos+55] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[28] = buf[pos+58] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[29] = buf[pos+59] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[30] = buf[pos+62] << decimation_shifts<SdrBits, InputBits>::pre32;
		intbuf[31] = buf[pos+63] << decimation_shifts<SdrBits, InputBits>::pre32;

		m_decimator2.myDecimate(
				buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre32,
				buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre32,
				&intbuf[0],
				&intbuf[1]);
		m_decimator2.myDecimate(
				buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre32,
				buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre32,
				&intbuf[2],
				&intbuf[3]);
		m_decimator2.myDecimate(
				buf[pos+8] << decimation_shifts<SdrBits, InputBits>::pre32,
				buf[pos+9] << decimation_shifts<SdrBits, InputBits>::pre32,
				&intbuf[4],
				&intbuf[5]);
		m_decimator2.myDecimate(
				buf[pos+12] << decimation_shifts<SdrBits, InputBits>::pre32,
				buf[pos+13] << decimation_shifts<SdrBits, InputBits>::pre32,
				&intbuf[6],
				&intbuf[7]);
		m_decimator2.myDecimate(
				buf[pos+16] << decimation_shifts<SdrBits, InputBits>::pre32,
				buf[pos+17] << decimation_shifts<SdrBits, InputBits>::pre32,
				&intbuf[8],
				&intbuf[9]);
		m_decimator2.myDecimate(
				buf[pos+20] << decimation_shifts<SdrBits, InputBits>::pre32,
				buf[pos+21] << decimation_shifts<SdrBits, InputBits>::pre32,
				&intbuf[10],
				&intbuf[11]);
		m_decimator2.myDecimate(
				buf[pos+24] << decimation_shifts<SdrBits, InputBits>::pre32,
				buf[pos+25] << decimation_shifts<SdrBits, InputBits>::pre32,
				&intbuf[12],
				&intbuf[13]);
		m_decimator2.myDecimate(
				buf[pos+28] << decimation_shifts<SdrBits, InputBits>::pre32,
				buf[pos+29] << decimation_shifts<SdrBits, InputBits>::pre32,
				&intbuf[14],
				&intbuf[15]);
		m_decimator2.myDecimate(
				buf[pos+32] << decimation_shifts<SdrBits, InputBits>::pre32,
				buf[pos+33] << decimation_shifts<SdrBits, InputBits>::pre32,
				&intbuf[16],
				&intbuf[17]);
		m_decimator2.myDecimate(
				buf[pos+36] << decimation_shifts<SdrBits, InputBits>::pre32,
				buf[pos+37] << decimation_shifts<SdrBits, InputBits>::pre32,
				&intbuf[18],
				&intbuf[19]);
		m_decimator2.myDecimate(
				buf[pos+40] << decimation_shifts<SdrBits, InputBits>::pre32,
				buf[pos+41] << decimation_shifts<SdrBits, InputBits>::pre32,
				&intbuf[20],
				&intbuf[21]);
		m_decimator2.myDecimate(
				buf[pos+44] << decimation_shifts<SdrBits, InputBits>::pre32,
				buf[pos+45] << decimation_shifts<SdrBits, InputBits>::pre32,
				&intbuf[22],
				&intbuf[23]);
		m_decimator2.myDecimate(
				buf[pos+48] << decimation_shifts<SdrBits, InputBits>::pre32,
				buf[pos+49] << decimation_shifts<SdrBits, InputBits>::pre32,
				&intbuf[24],
				&intbuf[25]);
		m_decimator2.myDecimate(
				buf[pos+52] << decimation_shifts<SdrBits, InputBits>::pre32,
				buf[pos+53] << decimation_shifts<SdrBits, InputBits>::pre32,
				&intbuf[26],
				&intbuf[27]);
		m_decimator2.myDecimate(
				buf[pos+56] << decimation_shifts<SdrBits, InputBits>::pre32,
				buf[pos+57] << decimation_shifts<SdrBits, InputBits>::pre32,
				&intbuf[28],
				&intbuf[29]);
		m_decimator2.myDecimate(
				buf[pos+60] << decimation_shifts<SdrBits, InputBits>::pre32,
				buf[pos+61] << decimation_shifts<SdrBits, InputBits>::pre32,
				&intbuf[30],
				&intbuf[31]);

		m_decimator4.myDecimate(
				intbuf[0],
				intbuf[1],
				&intbuf[2],
				&intbuf[3]);
		m_decimator4.myDecimate(
				intbuf[4],
				intbuf[5],
				&intbuf[6],
				&intbuf[7]);
		m_decimator4.myDecimate(
				intbuf[8],
				intbuf[9],
				&intbuf[10],
				&intbuf[11]);
		m_decimator4.myDecimate(
				intbuf[12],
				intbuf[13],
				&intbuf[14],
				&intbuf[15]);
		m_decimator4.myDecimate(
				intbuf[16],
				intbuf[17],
				&intbuf[18],
				&intbuf[19]);
		m_decimator4.myDecimate(
				intbuf[20],
				intbuf[21],
				&intbuf[22],
				&intbuf[23]);
		m_decimator4.myDecimate(
				intbuf[24],
				intbuf[25],
				&intbuf[26],
				&intbuf[27]);
		m_decimator4.myDecimate(
				intbuf[28],
				intbuf[29],
				&intbuf[30],
				&intbuf[31]);

		m_decimator8.myDecimate(
				intbuf[2],
				intbuf[3],
				&intbuf[6],
				&intbuf[7]);
		m_decimator8.myDecimate(
				intbuf[10],
				intbuf[11],
				&intbuf[14],
				&intbuf[15]);
		m_decimator8.myDecimate(
				intbuf[18],
				intbuf[19],
				&intbuf[22],
				&intbuf[23]);
		m_decimator8.myDecimate(
				intbuf[26],
				intbuf[27],
				&intbuf[30],
				&intbuf[31]);

		m_decimator16.myDecimate(
				intbuf[6],
				intbuf[7],
				&intbuf[14],
				&intbuf[15]);
		m_decimator16.myDecimate(
				intbuf[22],
				intbuf[23],
				&intbuf[30],
				&intbuf[31]);

		m_decimator32.myDecimate(
				intbuf[14],
				intbuf[15],
				&intbuf[30],
				&intbuf[31]);

		(**it).setReal(intbuf[30] >> decimation_shifts<SdrBits, InputBits>::post32);
		(**it).setImag(intbuf[31] >> decimation_shifts<SdrBits, InputBits>::post32);
		++(*it);
	}
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate32_cen(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len)
{
    StorageType intbuf[32];

    for (int pos = 0; pos < len - 31; pos += 32)
    {
        intbuf[0]  = bufI[pos+1] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[1]  = bufQ[pos+1] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[2]  = bufI[pos+3] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[3]  = bufQ[pos+3] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[4]  = bufI[pos+5] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[5]  = bufQ[pos+5] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[6]  = bufI[pos+7] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[7]  = bufQ[pos+7] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[8]  = bufI[pos+9] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[9]  = bufQ[pos+9] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[10] = bufI[pos+11] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[11] = bufQ[pos+11] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[12] = bufI[pos+13] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[13] = bufQ[pos+13] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[14] = bufI[pos+15] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[15] = bufQ[pos+15] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[16] = bufI[pos+17] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[17] = bufQ[pos+17] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[18] = bufI[pos+19] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[19] = bufQ[pos+19] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[20] = bufI[pos+21] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[21] = bufQ[pos+21] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[22] = bufI[pos+23] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[23] = bufQ[pos+23] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[24] = bufI[pos+25] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[25] = bufQ[pos+25] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[26] = bufI[pos+27] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[27] = bufQ[pos+27] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[28] = bufI[pos+29] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[29] = bufQ[pos+29] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[30] = bufI[pos+31] << decimation_shifts<SdrBits, InputBits>::pre32;
        intbuf[31] = bufQ[pos+31] << decimation_shifts<SdrBits, InputBits>::pre32;

        m_decimator2.myDecimate(
                bufI[pos+0] << decimation_shifts<SdrBits, InputBits>::pre32,
                bufQ[pos+0] << decimation_shifts<SdrBits, InputBits>::pre32,
                &intbuf[0],
                &intbuf[1]);
        m_decimator2.myDecimate(
                bufI[pos+2] << decimation_shifts<SdrBits, InputBits>::pre32,
                bufQ[pos+2] << decimation_shifts<SdrBits, InputBits>::pre32,
                &intbuf[2],
                &intbuf[3]);
        m_decimator2.myDecimate(
                bufI[pos+4] << decimation_shifts<SdrBits, InputBits>::pre32,
                bufQ[pos+4] << decimation_shifts<SdrBits, InputBits>::pre32,
                &intbuf[4],
                &intbuf[5]);
        m_decimator2.myDecimate(
                bufI[pos+6] << decimation_shifts<SdrBits, InputBits>::pre32,
                bufQ[pos+6] << decimation_shifts<SdrBits, InputBits>::pre32,
                &intbuf[6],
                &intbuf[7]);
        m_decimator2.myDecimate(
                bufI[pos+8] << decimation_shifts<SdrBits, InputBits>::pre32,
                bufQ[pos+8] << decimation_shifts<SdrBits, InputBits>::pre32,
                &intbuf[8],
                &intbuf[9]);
        m_decimator2.myDecimate(
                bufI[pos+10] << decimation_shifts<SdrBits, InputBits>::pre32,
                bufQ[pos+10] << decimation_shifts<SdrBits, InputBits>::pre32,
                &intbuf[10],
                &intbuf[11]);
        m_decimator2.myDecimate(
                bufI[pos+12] << decimation_shifts<SdrBits, InputBits>::pre32,
                bufQ[pos+12] << decimation_shifts<SdrBits, InputBits>::pre32,
                &intbuf[12],
                &intbuf[13]);
        m_decimator2.myDecimate(
                bufI[pos+14] << decimation_shifts<SdrBits, InputBits>::pre32,
                bufQ[pos+14] << decimation_shifts<SdrBits, InputBits>::pre32,
                &intbuf[14],
                &intbuf[15]);
        m_decimator2.myDecimate(
                bufI[pos+16] << decimation_shifts<SdrBits, InputBits>::pre32,
                bufQ[pos+16] << decimation_shifts<SdrBits, InputBits>::pre32,
                &intbuf[16],
                &intbuf[17]);
        m_decimator2.myDecimate(
                bufI[pos+18] << decimation_shifts<SdrBits, InputBits>::pre32,
                bufQ[pos+18] << decimation_shifts<SdrBits, InputBits>::pre32,
                &intbuf[18],
                &intbuf[19]);
        m_decimator2.myDecimate(
                bufI[pos+20] << decimation_shifts<SdrBits, InputBits>::pre32,
                bufQ[pos+20] << decimation_shifts<SdrBits, InputBits>::pre32,
                &intbuf[20],
                &intbuf[21]);
        m_decimator2.myDecimate(
                bufI[pos+22] << decimation_shifts<SdrBits, InputBits>::pre32,
                bufQ[pos+22] << decimation_shifts<SdrBits, InputBits>::pre32,
                &intbuf[22],
                &intbuf[23]);
        m_decimator2.myDecimate(
                bufI[pos+24] << decimation_shifts<SdrBits, InputBits>::pre32,
                bufQ[pos+24] << decimation_shifts<SdrBits, InputBits>::pre32,
                &intbuf[24],
                &intbuf[25]);
        m_decimator2.myDecimate(
                bufI[pos+26] << decimation_shifts<SdrBits, InputBits>::pre32,
                bufQ[pos+26] << decimation_shifts<SdrBits, InputBits>::pre32,
                &intbuf[26],
                &intbuf[27]);
        m_decimator2.myDecimate(
                bufI[pos+28] << decimation_shifts<SdrBits, InputBits>::pre32,
                bufQ[pos+28] << decimation_shifts<SdrBits, InputBits>::pre32,
                &intbuf[28],
                &intbuf[29]);
        m_decimator2.myDecimate(
                bufI[pos+30] << decimation_shifts<SdrBits, InputBits>::pre32,
                bufQ[pos+30] << decimation_shifts<SdrBits, InputBits>::pre32,
                &intbuf[30],
                &intbuf[31]);

        m_decimator4.myDecimate(
                intbuf[0],
                intbuf[1],
                &intbuf[2],
                &intbuf[3]);
        m_decimator4.myDecimate(
                intbuf[4],
                intbuf[5],
                &intbuf[6],
                &intbuf[7]);
        m_decimator4.myDecimate(
                intbuf[8],
                intbuf[9],
                &intbuf[10],
                &intbuf[11]);
        m_decimator4.myDecimate(
                intbuf[12],
                intbuf[13],
                &intbuf[14],
                &intbuf[15]);
        m_decimator4.myDecimate(
                intbuf[16],
                intbuf[17],
                &intbuf[18],
                &intbuf[19]);
        m_decimator4.myDecimate(
                intbuf[20],
                intbuf[21],
                &intbuf[22],
                &intbuf[23]);
        m_decimator4.myDecimate(
                intbuf[24],
                intbuf[25],
                &intbuf[26],
                &intbuf[27]);
        m_decimator4.myDecimate(
                intbuf[28],
                intbuf[29],
                &intbuf[30],
                &intbuf[31]);

        m_decimator8.myDecimate(
                intbuf[2],
                intbuf[3],
                &intbuf[6],
                &intbuf[7]);
        m_decimator8.myDecimate(
                intbuf[10],
                intbuf[11],
                &intbuf[14],
                &intbuf[15]);
        m_decimator8.myDecimate(
                intbuf[18],
                intbuf[19],
                &intbuf[22],
                &intbuf[23]);
        m_decimator8.myDecimate(
                intbuf[26],
                intbuf[27],
                &intbuf[30],
                &intbuf[31]);

        m_decimator16.myDecimate(
                intbuf[6],
                intbuf[7],
                &intbuf[14],
                &intbuf[15]);
        m_decimator16.myDecimate(
                intbuf[22],
                intbuf[23],
                &intbuf[30],
                &intbuf[31]);

        m_decimator32.myDecimate(
                intbuf[14],
                intbuf[15],
                &intbuf[30],
                &intbuf[31]);

        (**it).setReal(intbuf[30] >> decimation_shifts<SdrBits, InputBits>::post32);
        (**it).setImag(intbuf[31] >> decimation_shifts<SdrBits, InputBits>::post32);
        ++(*it);
    }
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate64_cen(SampleVector::iterator* it, const T* buf, qint32 len)
{
	StorageType intbuf[64];

	for (int pos = 0; pos < len - 127; pos += 128)
	{
		intbuf[0]  = buf[pos+2] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[1]  = buf[pos+3] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[2]  = buf[pos+6] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[3]  = buf[pos+7] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[4]  = buf[pos+10] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[5]  = buf[pos+11] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[6]  = buf[pos+14] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[7]  = buf[pos+15] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[8]  = buf[pos+18] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[9]  = buf[pos+19] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[10] = buf[pos+22] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[11] = buf[pos+23] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[12] = buf[pos+26] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[13] = buf[pos+27] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[14] = buf[pos+30] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[15] = buf[pos+31] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[16] = buf[pos+34] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[17] = buf[pos+35] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[18] = buf[pos+38] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[19] = buf[pos+39] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[20] = buf[pos+42] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[21] = buf[pos+43] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[22] = buf[pos+46] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[23] = buf[pos+47] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[24] = buf[pos+50] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[25] = buf[pos+51] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[26] = buf[pos+54] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[27] = buf[pos+55] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[28] = buf[pos+58] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[29] = buf[pos+59] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[30] = buf[pos+62] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[31] = buf[pos+63] << decimation_shifts<SdrBits, InputBits>::pre64;

		intbuf[32] = buf[pos+66] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[33] = buf[pos+67] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[34] = buf[pos+70] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[35] = buf[pos+71] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[36] = buf[pos+74] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[37] = buf[pos+75] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[38] = buf[pos+78] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[39] = buf[pos+79] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[40] = buf[pos+82] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[41] = buf[pos+83] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[42] = buf[pos+86] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[43] = buf[pos+87] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[44] = buf[pos+90] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[45] = buf[pos+91] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[46] = buf[pos+94] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[47] = buf[pos+95] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[48] = buf[pos+98] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[49] = buf[pos+99] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[50] = buf[pos+102] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[51] = buf[pos+103] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[52] = buf[pos+106] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[53] = buf[pos+107] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[54] = buf[pos+110] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[55] = buf[pos+111] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[56] = buf[pos+114] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[57] = buf[pos+115] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[58] = buf[pos+118] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[59] = buf[pos+119] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[60] = buf[pos+122] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[61] = buf[pos+123] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[62] = buf[pos+126] << decimation_shifts<SdrBits, InputBits>::pre64;
		intbuf[63] = buf[pos+127] << decimation_shifts<SdrBits, InputBits>::pre64;

		m_decimator2.myDecimate(
				buf[pos+0] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+1] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[0],
				&intbuf[1]);
		m_decimator2.myDecimate(
				buf[pos+4] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+5] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[2],
				&intbuf[3]);
		m_decimator2.myDecimate(
				buf[pos+8] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+9] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[4],
				&intbuf[5]);
		m_decimator2.myDecimate(
				buf[pos+12] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+13] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[6],
				&intbuf[7]);
		m_decimator2.myDecimate(
				buf[pos+16] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+17] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[8],
				&intbuf[9]);
		m_decimator2.myDecimate(
				buf[pos+20] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+21] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[10],
				&intbuf[11]);
		m_decimator2.myDecimate(
				buf[pos+24] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+25] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[12],
				&intbuf[13]);
		m_decimator2.myDecimate(
				buf[pos+28] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+29] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[14],
				&intbuf[15]);
		m_decimator2.myDecimate(
				buf[pos+32] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+33] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[16],
				&intbuf[17]);
		m_decimator2.myDecimate(
				buf[pos+36] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+37] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[18],
				&intbuf[19]);
		m_decimator2.myDecimate(
				buf[pos+40] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+41] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[20],
				&intbuf[21]);
		m_decimator2.myDecimate(
				buf[pos+44] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+45] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[22],
				&intbuf[23]);
		m_decimator2.myDecimate(
				buf[pos+48] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+49] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[24],
				&intbuf[25]);
		m_decimator2.myDecimate(
				buf[pos+52] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+53] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[26],
				&intbuf[27]);
		m_decimator2.myDecimate(
				buf[pos+56] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+57] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[28],
				&intbuf[29]);
		m_decimator2.myDecimate(
				buf[pos+60] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+61] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[30],
				&intbuf[31]);
		m_decimator2.myDecimate(
				buf[pos+64] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+65] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[32],
				&intbuf[33]);
		m_decimator2.myDecimate(
				buf[pos+68] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+69] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[34],
				&intbuf[35]);
		m_decimator2.myDecimate(
				buf[pos+72] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+73] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[36],
				&intbuf[37]);
		m_decimator2.myDecimate(
				buf[pos+76] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+77] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[38],
				&intbuf[39]);
		m_decimator2.myDecimate(
				buf[pos+80] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+81] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[40],
				&intbuf[41]);
		m_decimator2.myDecimate(
				buf[pos+84] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+85] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[42],
				&intbuf[43]);
		m_decimator2.myDecimate(
				buf[pos+88] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+89] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[44],
				&intbuf[45]);
		m_decimator2.myDecimate(
				buf[pos+92] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+93] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[46],
				&intbuf[47]);
		m_decimator2.myDecimate(
				buf[pos+96] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+97] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[48],
				&intbuf[49]);
		m_decimator2.myDecimate(
				buf[pos+100] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+101] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[50],
				&intbuf[51]);
		m_decimator2.myDecimate(
				buf[pos+104] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+105] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[52],
				&intbuf[53]);
		m_decimator2.myDecimate(
				buf[pos+108] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+109] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[54],
				&intbuf[55]);
		m_decimator2.myDecimate(
				buf[pos+112] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+113] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[56],
				&intbuf[57]);
		m_decimator2.myDecimate(
				buf[pos+116] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+117] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[58],
				&intbuf[59]);
		m_decimator2.myDecimate(
				buf[pos+120] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+121] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[60],
				&intbuf[61]);
		m_decimator2.myDecimate(
				buf[pos+124] << decimation_shifts<SdrBits, InputBits>::pre64,
				buf[pos+125] << decimation_shifts<SdrBits, InputBits>::pre64,
				&intbuf[62],
				&intbuf[63]);

		m_decimator4.myDecimate(
				intbuf[0],
				intbuf[1],
				&intbuf[2],
				&intbuf[3]);
		m_decimator4.myDecimate(
				intbuf[4],
				intbuf[5],
				&intbuf[6],
				&intbuf[7]);
		m_decimator4.myDecimate(
				intbuf[8],
				intbuf[9],
				&intbuf[10],
				&intbuf[11]);
		m_decimator4.myDecimate(
				intbuf[12],
				intbuf[13],
				&intbuf[14],
				&intbuf[15]);
		m_decimator4.myDecimate(
				intbuf[16],
				intbuf[17],
				&intbuf[18],
				&intbuf[19]);
		m_decimator4.myDecimate(
				intbuf[20],
				intbuf[21],
				&intbuf[22],
				&intbuf[23]);
		m_decimator4.myDecimate(
				intbuf[24],
				intbuf[25],
				&intbuf[26],
				&intbuf[27]);
		m_decimator4.myDecimate(
				intbuf[28],
				intbuf[29],
				&intbuf[30],
				&intbuf[31]);
		m_decimator4.myDecimate(
				intbuf[32],
				intbuf[33],
				&intbuf[34],
				&intbuf[35]);
		m_decimator4.myDecimate(
				intbuf[36],
				intbuf[37],
				&intbuf[38],
				&intbuf[39]);
		m_decimator4.myDecimate(
				intbuf[40],
				intbuf[41],
				&intbuf[42],
				&intbuf[43]);
		m_decimator4.myDecimate(
				intbuf[44],
				intbuf[45],
				&intbuf[46],
				&intbuf[47]);
		m_decimator4.myDecimate(
				intbuf[48],
				intbuf[49],
				&intbuf[50],
				&intbuf[51]);
		m_decimator4.myDecimate(
				intbuf[52],
				intbuf[53],
				&intbuf[54],
				&intbuf[55]);
		m_decimator4.myDecimate(
				intbuf[56],
				intbuf[57],
				&intbuf[58],
				&intbuf[59]);
		m_decimator4.myDecimate(
				intbuf[60],
				intbuf[61],
				&intbuf[62],
				&intbuf[63]);

		m_decimator8.myDecimate(
				intbuf[2],
				intbuf[3],
				&intbuf[6],
				&intbuf[7]);
		m_decimator8.myDecimate(
				intbuf[10],
				intbuf[11],
				&intbuf[14],
				&intbuf[15]);
		m_decimator8.myDecimate(
				intbuf[18],
				intbuf[19],
				&intbuf[22],
				&intbuf[23]);
		m_decimator8.myDecimate(
				intbuf[26],
				intbuf[27],
				&intbuf[30],
				&intbuf[31]);
		m_decimator8.myDecimate(
				intbuf[34],
				intbuf[35],
				&intbuf[38],
				&intbuf[39]);
		m_decimator8.myDecimate(
				intbuf[42],
				intbuf[43],
				&intbuf[46],
				&intbuf[47]);
		m_decimator8.myDecimate(
				intbuf[50],
				intbuf[51],
				&intbuf[54],
				&intbuf[55]);
		m_decimator8.myDecimate(
				intbuf[58],
				intbuf[59],
				&intbuf[62],
				&intbuf[63]);

		m_decimator16.myDecimate(
				intbuf[6],
				intbuf[7],
				&intbuf[14],
				&intbuf[15]);
		m_decimator16.myDecimate(
				intbuf[22],
				intbuf[23],
				&intbuf[30],
				&intbuf[31]);
		m_decimator16.myDecimate(
				intbuf[38],
				intbuf[39],
				&intbuf[46],
				&intbuf[47]);
		m_decimator16.myDecimate(
				intbuf[54],
				intbuf[55],
				&intbuf[62],
				&intbuf[63]);

		m_decimator32.myDecimate(
				intbuf[14],
				intbuf[15],
				&intbuf[30],
				&intbuf[31]);
		m_decimator32.myDecimate(
				intbuf[46],
				intbuf[47],
				&intbuf[62],
				&intbuf[63]);

		m_decimator64.myDecimate(
				intbuf[30],
				intbuf[31],
				&intbuf[62],
				&intbuf[63]);

		(**it).setReal(intbuf[62] >> decimation_shifts<SdrBits, InputBits>::post64);
		(**it).setImag(intbuf[63] >> decimation_shifts<SdrBits, InputBits>::post64);
		++(*it);
	}
}

template<typename StorageType, typename T, uint SdrBits, uint InputBits>
void Decimators<StorageType, T, SdrBits, InputBits>::decimate64_cen(SampleVector::iterator* it, const T* bufI, const T* bufQ, qint32 len)
{
    StorageType intbuf[64];

    for (int pos = 0; pos < len - 63; pos += 64)
    {
        intbuf[0]  = bufI[pos+1] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[1]  = bufQ[pos+1] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[2]  = bufI[pos+3] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[3]  = bufQ[pos+3] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[4]  = bufI[pos+5] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[5]  = bufQ[pos+5] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[6]  = bufI[pos+7] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[7]  = bufQ[pos+7] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[8]  = bufI[pos+9] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[9]  = bufQ[pos+9] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[10] = bufI[pos+11] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[11] = bufQ[pos+11] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[12] = bufI[pos+13] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[13] = bufQ[pos+13] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[14] = bufI[pos+15] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[15] = bufQ[pos+15] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[16] = bufI[pos+17] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[17] = bufQ[pos+17] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[18] = bufI[pos+19] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[19] = bufQ[pos+19] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[20] = bufI[pos+21] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[21] = bufQ[pos+21] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[22] = bufI[pos+23] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[23] = bufQ[pos+23] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[24] = bufI[pos+25] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[25] = bufQ[pos+25] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[26] = bufI[pos+27] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[27] = bufQ[pos+27] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[28] = bufI[pos+29] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[29] = bufQ[pos+29] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[30] = bufI[pos+31] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[31] = bufQ[pos+31] << decimation_shifts<SdrBits, InputBits>::pre64;

        intbuf[32] = bufI[pos+33] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[33] = bufQ[pos+33] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[34] = bufI[pos+35] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[35] = bufQ[pos+35] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[36] = bufI[pos+37] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[37] = bufQ[pos+37] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[38] = bufI[pos+39] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[39] = bufQ[pos+39] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[40] = bufI[pos+41] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[41] = bufQ[pos+41] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[42] = bufI[pos+43] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[43] = bufQ[pos+43] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[44] = bufI[pos+45] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[45] = bufQ[pos+45] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[46] = bufI[pos+47] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[47] = bufQ[pos+47] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[48] = bufI[pos+49] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[49] = bufQ[pos+49] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[50] = bufI[pos+51] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[51] = bufQ[pos+51] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[52] = bufI[pos+53] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[53] = bufQ[pos+53] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[54] = bufI[pos+55] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[55] = bufQ[pos+55] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[56] = bufI[pos+57] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[57] = bufQ[pos+57] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[58] = bufI[pos+59] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[59] = bufQ[pos+59] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[60] = bufI[pos+61] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[61] = bufQ[pos+61] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[62] = bufI[pos+63] << decimation_shifts<SdrBits, InputBits>::pre64;
        intbuf[63] = bufQ[pos+63] << decimation_shifts<SdrBits, InputBits>::pre64;

        m_decimator2.myDecimate(
                bufI[pos+0] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+0] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[0],
                &intbuf[1]);
        m_decimator2.myDecimate(
                bufI[pos+2] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+2] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[2],
                &intbuf[3]);
        m_decimator2.myDecimate(
                bufI[pos+4] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+4] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[4],
                &intbuf[5]);
        m_decimator2.myDecimate(
                bufI[pos+6] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+6] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[6],
                &intbuf[7]);
        m_decimator2.myDecimate(
                bufI[pos+8] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+8] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[8],
                &intbuf[9]);
        m_decimator2.myDecimate(
                bufI[pos+10] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+10] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[10],
                &intbuf[11]);
        m_decimator2.myDecimate(
                bufI[pos+12] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+12] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[12],
                &intbuf[13]);
        m_decimator2.myDecimate(
                bufI[pos+14] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+14] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[14],
                &intbuf[15]);
        m_decimator2.myDecimate(
                bufI[pos+16] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+16] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[16],
                &intbuf[17]);
        m_decimator2.myDecimate(
                bufI[pos+18] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+18] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[18],
                &intbuf[19]);
        m_decimator2.myDecimate(
                bufI[pos+20] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+20] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[20],
                &intbuf[21]);
        m_decimator2.myDecimate(
                bufI[pos+22] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+22] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[22],
                &intbuf[23]);
        m_decimator2.myDecimate(
                bufI[pos+24] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+24] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[24],
                &intbuf[25]);
        m_decimator2.myDecimate(
                bufI[pos+26] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+26] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[26],
                &intbuf[27]);
        m_decimator2.myDecimate(
                bufI[pos+28] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+28] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[28],
                &intbuf[29]);
        m_decimator2.myDecimate(
                bufI[pos+30] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+30] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[30],
                &intbuf[31]);
        m_decimator2.myDecimate(
                bufI[pos+32] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+32] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[32],
                &intbuf[33]);
        m_decimator2.myDecimate(
                bufI[pos+34] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+34] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[34],
                &intbuf[35]);
        m_decimator2.myDecimate(
                bufI[pos+36] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+36] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[36],
                &intbuf[37]);
        m_decimator2.myDecimate(
                bufI[pos+38] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+38] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[38],
                &intbuf[39]);
        m_decimator2.myDecimate(
                bufI[pos+40] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+40] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[40],
                &intbuf[41]);
        m_decimator2.myDecimate(
                bufI[pos+42] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+42] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[42],
                &intbuf[43]);
        m_decimator2.myDecimate(
                bufI[pos+44] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+44] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[44],
                &intbuf[45]);
        m_decimator2.myDecimate(
                bufI[pos+46] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+46] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[46],
                &intbuf[47]);
        m_decimator2.myDecimate(
                bufI[pos+48] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+48] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[48],
                &intbuf[49]);
        m_decimator2.myDecimate(
                bufI[pos+50] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+50] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[50],
                &intbuf[51]);
        m_decimator2.myDecimate(
                bufI[pos+52] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+52] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[52],
                &intbuf[53]);
        m_decimator2.myDecimate(
                bufI[pos+54] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+54] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[54],
                &intbuf[55]);
        m_decimator2.myDecimate(
                bufI[pos+56] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+56] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[56],
                &intbuf[57]);
        m_decimator2.myDecimate(
                bufI[pos+58] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+58] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[58],
                &intbuf[59]);
        m_decimator2.myDecimate(
                bufI[pos+60] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+60] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[60],
                &intbuf[61]);
        m_decimator2.myDecimate(
                bufI[pos+62] << decimation_shifts<SdrBits, InputBits>::pre64,
                bufQ[pos+62] << decimation_shifts<SdrBits, InputBits>::pre64,
                &intbuf[62],
                &intbuf[63]);

        m_decimator4.myDecimate(
                intbuf[0],
                intbuf[1],
                &intbuf[2],
                &intbuf[3]);
        m_decimator4.myDecimate(
                intbuf[4],
                intbuf[5],
                &intbuf[6],
                &intbuf[7]);
        m_decimator4.myDecimate(
                intbuf[8],
                intbuf[9],
                &intbuf[10],
                &intbuf[11]);
        m_decimator4.myDecimate(
                intbuf[12],
                intbuf[13],
                &intbuf[14],
                &intbuf[15]);
        m_decimator4.myDecimate(
                intbuf[16],
                intbuf[17],
                &intbuf[18],
                &intbuf[19]);
        m_decimator4.myDecimate(
                intbuf[20],
                intbuf[21],
                &intbuf[22],
                &intbuf[23]);
        m_decimator4.myDecimate(
                intbuf[24],
                intbuf[25],
                &intbuf[26],
                &intbuf[27]);
        m_decimator4.myDecimate(
                intbuf[28],
                intbuf[29],
                &intbuf[30],
                &intbuf[31]);
        m_decimator4.myDecimate(
                intbuf[32],
                intbuf[33],
                &intbuf[34],
                &intbuf[35]);
        m_decimator4.myDecimate(
                intbuf[36],
                intbuf[37],
                &intbuf[38],
                &intbuf[39]);
        m_decimator4.myDecimate(
                intbuf[40],
                intbuf[41],
                &intbuf[42],
                &intbuf[43]);
        m_decimator4.myDecimate(
                intbuf[44],
                intbuf[45],
                &intbuf[46],
                &intbuf[47]);
        m_decimator4.myDecimate(
                intbuf[48],
                intbuf[49],
                &intbuf[50],
                &intbuf[51]);
        m_decimator4.myDecimate(
                intbuf[52],
                intbuf[53],
                &intbuf[54],
                &intbuf[55]);
        m_decimator4.myDecimate(
                intbuf[56],
                intbuf[57],
                &intbuf[58],
                &intbuf[59]);
        m_decimator4.myDecimate(
                intbuf[60],
                intbuf[61],
                &intbuf[62],
                &intbuf[63]);

        m_decimator8.myDecimate(
                intbuf[2],
                intbuf[3],
                &intbuf[6],
                &intbuf[7]);
        m_decimator8.myDecimate(
                intbuf[10],
                intbuf[11],
                &intbuf[14],
                &intbuf[15]);
        m_decimator8.myDecimate(
                intbuf[18],
                intbuf[19],
                &intbuf[22],
                &intbuf[23]);
        m_decimator8.myDecimate(
                intbuf[26],
                intbuf[27],
                &intbuf[30],
                &intbuf[31]);
        m_decimator8.myDecimate(
                intbuf[34],
                intbuf[35],
                &intbuf[38],
                &intbuf[39]);
        m_decimator8.myDecimate(
                intbuf[42],
                intbuf[43],
                &intbuf[46],
                &intbuf[47]);
        m_decimator8.myDecimate(
                intbuf[50],
                intbuf[51],
                &intbuf[54],
                &intbuf[55]);
        m_decimator8.myDecimate(
                intbuf[58],
                intbuf[59],
                &intbuf[62],
                &intbuf[63]);

        m_decimator16.myDecimate(
                intbuf[6],
                intbuf[7],
                &intbuf[14],
                &intbuf[15]);
        m_decimator16.myDecimate(
                intbuf[22],
                intbuf[23],
                &intbuf[30],
                &intbuf[31]);
        m_decimator16.myDecimate(
                intbuf[38],
                intbuf[39],
                &intbuf[46],
                &intbuf[47]);
        m_decimator16.myDecimate(
                intbuf[54],
                intbuf[55],
                &intbuf[62],
                &intbuf[63]);

        m_decimator32.myDecimate(
                intbuf[14],
                intbuf[15],
                &intbuf[30],
                &intbuf[31]);
        m_decimator32.myDecimate(
                intbuf[46],
                intbuf[47],
                &intbuf[62],
                &intbuf[63]);

        m_decimator64.myDecimate(
                intbuf[30],
                intbuf[31],
                &intbuf[62],
                &intbuf[63]);

        (**it).setReal(intbuf[62] >> decimation_shifts<SdrBits, InputBits>::post64);
        (**it).setImag(intbuf[63] >> decimation_shifts<SdrBits, InputBits>::post64);
        ++(*it);
    }
}

#endif /* INCLUDE_GPL_DSP_DECIMATORS_H_ */
