///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_DSP_DECIMATORSIF_H_
#define SDRBASE_DSP_DECIMATORSIF_H_

#include "dsp/dsptypes.h"
#include "dsp/inthalfbandfiltereof.h"

#define DECIMATORS_IF_FILTER_ORDER 64

template<uint InputBits>
struct decimation_scale
{
    static const float scaleIn;
};

template<uint InputBits>
const float decimation_scale<InputBits>::scaleIn = 1.0;

template<>
struct decimation_scale<8>
{
    static const float scaleIn;
};

template<>
struct decimation_scale<12>
{
    static const float scaleIn;
};

template<>
struct decimation_scale<16>
{
    static const float scaleIn;
};


template<typename T, uint InputBits>
class DecimatorsIF {
public:
    // interleaved I/Q input buffer
    void decimate1(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate2_inf(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate2_sup(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate2_cen(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate4_inf(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate4_sup(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate4_cen(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate8_inf(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate8_sup(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate8_cen(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate16_inf(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate16_sup(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate16_cen(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate32_inf(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate32_sup(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate32_cen(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate64_inf(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate64_sup(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);
    void decimate64_cen(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ);

    IntHalfbandFilterEOF<DECIMATORS_IF_FILTER_ORDER> m_decimator2;  // 1st stages
    IntHalfbandFilterEOF<DECIMATORS_IF_FILTER_ORDER> m_decimator4;  // 2nd stages
    IntHalfbandFilterEOF<DECIMATORS_IF_FILTER_ORDER> m_decimator8;  // 3rd stages
    IntHalfbandFilterEOF<DECIMATORS_IF_FILTER_ORDER> m_decimator16; // 4th stages
    IntHalfbandFilterEOF<DECIMATORS_IF_FILTER_ORDER> m_decimator32; // 5th stages
    IntHalfbandFilterEOF<DECIMATORS_IF_FILTER_ORDER> m_decimator64; // 6th stages
};

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate1(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float xreal, yimag;

    for (int pos = 0; pos < nbIAndQ - 1; pos += 2)
    {
        xreal = buf[pos+0] * decimation_scale<InputBits>::scaleIn;
        yimag = buf[pos+1] * decimation_scale<InputBits>::scaleIn;
        (**it).setReal(xreal);
        (**it).setImag(yimag);
        ++(*it); // Valgrind optim (comment not repeated)
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate2_inf(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float xreal, yimag;

    for (int pos = 0; pos < nbIAndQ - 7; pos += 8)
    {
        xreal = (buf[pos+0] - buf[pos+3]) * decimation_scale<InputBits>::scaleIn;
        yimag = (buf[pos+1] + buf[pos+2]) * decimation_scale<InputBits>::scaleIn;
        (**it).setReal(xreal);
        (**it).setImag(yimag);
        ++(*it);

        xreal = (buf[pos+7] - buf[pos+4]) * decimation_scale<InputBits>::scaleIn;
        yimag = (- buf[pos+5] - buf[pos+6]) * decimation_scale<InputBits>::scaleIn;
        (**it).setReal(xreal);
        (**it).setImag(yimag);
        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate2_sup(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float xreal, yimag;

    for (int pos = 0; pos < nbIAndQ - 7; pos += 8)
    {
        xreal = (buf[pos+1] - buf[pos+2]) * decimation_scale<InputBits>::scaleIn;
        yimag = (- buf[pos+0] - buf[pos+3]) * decimation_scale<InputBits>::scaleIn;
        (**it).setReal(xreal);
        (**it).setImag(yimag);
        ++(*it);

        xreal = (buf[pos+6] - buf[pos+5]) * decimation_scale<InputBits>::scaleIn;
        yimag = (buf[pos+4] + buf[pos+7]) * decimation_scale<InputBits>::scaleIn;
        (**it).setReal(xreal);
        (**it).setImag(yimag);
        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate2_cen(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float intbuf[2];

    for (int pos = 0; pos < nbIAndQ - 3; pos += 4)
    {
        intbuf[0]  = buf[pos+2];
        intbuf[1]  = buf[pos+3];

        m_decimator2.myDecimate(
                buf[pos+0],
                buf[pos+1],
                &intbuf[0],
                &intbuf[1]);

        (**it).setReal(intbuf[0] * decimation_scale<InputBits>::scaleIn);
        (**it).setImag(intbuf[1] * decimation_scale<InputBits>::scaleIn);

        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate4_inf(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float xreal, yimag;

    for (int pos = 0; pos < nbIAndQ - 7; pos += 8)
    {
        xreal = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) * decimation_scale<InputBits>::scaleIn;
        yimag = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) * decimation_scale<InputBits>::scaleIn;

        (**it).setReal(xreal);
        (**it).setImag(yimag);

        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate4_sup(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float xreal, yimag;

    for (int pos = 0; pos < nbIAndQ - 7; pos += 8)
    {
        xreal = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]) * decimation_scale<InputBits>::scaleIn;
        yimag = (- buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7]) * decimation_scale<InputBits>::scaleIn;

        (**it).setReal(xreal);
        (**it).setImag(yimag);

        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate4_cen(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float intbuf[4];

    for (int pos = 0; pos < nbIAndQ - 7; pos += 8)
    {
        intbuf[0]  = buf[pos+2];
        intbuf[1]  = buf[pos+3];
        intbuf[2]  = buf[pos+6];
        intbuf[3]  = buf[pos+7];

        m_decimator2.myDecimate(
                buf[pos+0],
                buf[pos+1],
                &intbuf[0],
                &intbuf[1]);
        m_decimator2.myDecimate(
                buf[pos+4],
                buf[pos+5],
                &intbuf[2],
                &intbuf[3]);

        m_decimator4.myDecimate(
                intbuf[0],
                intbuf[1],
                &intbuf[2],
                &intbuf[3]);

        (**it).setReal(intbuf[2] * decimation_scale<InputBits>::scaleIn);
        (**it).setImag(intbuf[3] * decimation_scale<InputBits>::scaleIn);
        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate8_inf(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float xreal[2], yimag[2];

    for (int pos = 0; pos < nbIAndQ - 15; pos += 8)
    {
        xreal[0] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]);
        yimag[0] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]);
        pos += 8;

        xreal[1] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]);
        yimag[1] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]);

        m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);

        (**it).setReal(xreal[1] * decimation_scale<InputBits>::scaleIn);
        (**it).setImag(yimag[1] * decimation_scale<InputBits>::scaleIn);

        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate8_sup(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float xreal[2], yimag[2];

    for (int pos = 0; pos < nbIAndQ - 15; pos += 8)
    {
        xreal[0] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]);
        yimag[0] = (- buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7]);
        pos += 8;

        xreal[1] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]);
        yimag[1] = (- buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7]);

        m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);

        (**it).setReal(xreal[1] * decimation_scale<InputBits>::scaleIn);
        (**it).setImag(yimag[1] * decimation_scale<InputBits>::scaleIn);

        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate8_cen(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float intbuf[8];

    for (int pos = 0; pos < nbIAndQ - 15; pos += 16)
    {
        intbuf[0]  = buf[pos+2];
        intbuf[1]  = buf[pos+3];
        intbuf[2]  = buf[pos+6];
        intbuf[3]  = buf[pos+7];
        intbuf[4]  = buf[pos+10];
        intbuf[5]  = buf[pos+11];
        intbuf[6]  = buf[pos+14];
        intbuf[7]  = buf[pos+15];

        m_decimator2.myDecimate(
                buf[pos+0],
                buf[pos+1],
                &intbuf[0],
                &intbuf[1]);
        m_decimator2.myDecimate(
                buf[pos+4],
                buf[pos+5],
                &intbuf[2],
                &intbuf[3]);
        m_decimator2.myDecimate(
                buf[pos+8],
                buf[pos+9],
                &intbuf[4],
                &intbuf[5]);
        m_decimator2.myDecimate(
                buf[pos+12],
                buf[pos+13],
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

        (**it).setReal(intbuf[6] * decimation_scale<InputBits>::scaleIn);
        (**it).setImag(intbuf[7] * decimation_scale<InputBits>::scaleIn);
        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate16_inf(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float xreal[4], yimag[4];

    for (int pos = 0; pos < nbIAndQ - 31; )
    {
        for (int i = 0; i < 4; i++)
        {
            xreal[i] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]);
            yimag[i] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]);
            pos += 8;
        }

        m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
        m_decimator2.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);

        m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);

        (**it).setReal(xreal[3] * decimation_scale<InputBits>::scaleIn);
        (**it).setImag(yimag[3] * decimation_scale<InputBits>::scaleIn);

        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate16_sup(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float xreal[4], yimag[4];

    for (int pos = 0; pos < nbIAndQ - 31; )
    {
        for (int i = 0; i < 4; i++)
        {
            xreal[i] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]);
            yimag[i] = (buf[pos+4] + buf[pos+7] - buf[pos+0] - buf[pos+3]);
            pos += 8;
        }

        m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
        m_decimator2.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);

        m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);

        (**it).setReal(xreal[3] * decimation_scale<InputBits>::scaleIn);
        (**it).setImag(yimag[3] * decimation_scale<InputBits>::scaleIn);

        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate16_cen(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float intbuf[16];

    for (int pos = 0; pos < nbIAndQ - 31; pos += 32)
    {
        intbuf[0]  = buf[pos+2];
        intbuf[1]  = buf[pos+3];
        intbuf[2]  = buf[pos+6];
        intbuf[3]  = buf[pos+7];
        intbuf[4]  = buf[pos+10];
        intbuf[5]  = buf[pos+11];
        intbuf[6]  = buf[pos+14];
        intbuf[7]  = buf[pos+15];
        intbuf[8]  = buf[pos+18];
        intbuf[9]  = buf[pos+19];
        intbuf[10] = buf[pos+22];
        intbuf[11] = buf[pos+23];
        intbuf[12] = buf[pos+26];
        intbuf[13] = buf[pos+27];
        intbuf[14] = buf[pos+30];
        intbuf[15] = buf[pos+31];

        m_decimator2.myDecimate(
                buf[pos+0],
                buf[pos+1],
                &intbuf[0],
                &intbuf[1]);
        m_decimator2.myDecimate(
                buf[pos+4],
                buf[pos+5],
                &intbuf[2],
                &intbuf[3]);
        m_decimator2.myDecimate(
                buf[pos+8],
                buf[pos+9],
                &intbuf[4],
                &intbuf[5]);
        m_decimator2.myDecimate(
                buf[pos+12],
                buf[pos+13],
                &intbuf[6],
                &intbuf[7]);
        m_decimator2.myDecimate(
                buf[pos+16],
                buf[pos+17],
                &intbuf[8],
                &intbuf[9]);
        m_decimator2.myDecimate(
                buf[pos+20],
                buf[pos+21],
                &intbuf[10],
                &intbuf[11]);
        m_decimator2.myDecimate(
                buf[pos+24],
                buf[pos+25],
                &intbuf[12],
                &intbuf[13]);
        m_decimator2.myDecimate(
                buf[pos+28],
                buf[pos+29],
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

        (**it).setReal(intbuf[14] * decimation_scale<InputBits>::scaleIn);
        (**it).setImag(intbuf[15] * decimation_scale<InputBits>::scaleIn);
        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate32_inf(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float xreal[8], yimag[8];

    for (int pos = 0; pos < nbIAndQ - 63; )
    {
        for (int i = 0; i < 8; i++)
        {
            xreal[i] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]);
            yimag[i] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]);
            pos += 8;
        }

        m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
        m_decimator2.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);
        m_decimator2.myDecimate(xreal[4], yimag[4], &xreal[5], &yimag[5]);
        m_decimator2.myDecimate(xreal[6], yimag[6], &xreal[7], &yimag[7]);

        m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);
        m_decimator4.myDecimate(xreal[5], yimag[5], &xreal[7], &yimag[7]);

        m_decimator8.myDecimate(xreal[3], yimag[3], &xreal[7], &yimag[7]);

        (**it).setReal(xreal[7] * decimation_scale<InputBits>::scaleIn);
        (**it).setImag(yimag[7] * decimation_scale<InputBits>::scaleIn);

        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate32_sup(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float xreal[8], yimag[8];

    for (int pos = 0; pos < nbIAndQ - 63; )
    {
        for (int i = 0; i < 8; i++)
        {
            xreal[i] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]);
            yimag[i] = (buf[pos+4] + buf[pos+7] - buf[pos+0] - buf[pos+3]);
            pos += 8;
        }

        m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
        m_decimator2.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);
        m_decimator2.myDecimate(xreal[4], yimag[4], &xreal[5], &yimag[5]);
        m_decimator2.myDecimate(xreal[6], yimag[6], &xreal[7], &yimag[7]);

        m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);
        m_decimator4.myDecimate(xreal[5], yimag[5], &xreal[7], &yimag[7]);

        m_decimator8.myDecimate(xreal[3], yimag[3], &xreal[7], &yimag[7]);

        (**it).setReal(xreal[7] * decimation_scale<InputBits>::scaleIn);
        (**it).setImag(yimag[7] * decimation_scale<InputBits>::scaleIn);

        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate32_cen(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float intbuf[32];

    for (int pos = 0; pos < nbIAndQ - 63; pos += 64)
    {
        intbuf[0]  = buf[pos+2];
        intbuf[1]  = buf[pos+3];
        intbuf[2]  = buf[pos+6];
        intbuf[3]  = buf[pos+7];
        intbuf[4]  = buf[pos+10];
        intbuf[5]  = buf[pos+11];
        intbuf[6]  = buf[pos+14];
        intbuf[7]  = buf[pos+15];
        intbuf[8]  = buf[pos+18];
        intbuf[9]  = buf[pos+19];
        intbuf[10] = buf[pos+22];
        intbuf[11] = buf[pos+23];
        intbuf[12] = buf[pos+26];
        intbuf[13] = buf[pos+27];
        intbuf[14] = buf[pos+30];
        intbuf[15] = buf[pos+31];
        intbuf[16] = buf[pos+34];
        intbuf[17] = buf[pos+35];
        intbuf[18] = buf[pos+38];
        intbuf[19] = buf[pos+39];
        intbuf[20] = buf[pos+42];
        intbuf[21] = buf[pos+43];
        intbuf[22] = buf[pos+46];
        intbuf[23] = buf[pos+47];
        intbuf[24] = buf[pos+50];
        intbuf[25] = buf[pos+51];
        intbuf[26] = buf[pos+54];
        intbuf[27] = buf[pos+55];
        intbuf[28] = buf[pos+58];
        intbuf[29] = buf[pos+59];
        intbuf[30] = buf[pos+62];
        intbuf[31] = buf[pos+63];

        m_decimator2.myDecimate(
                buf[pos+0],
                buf[pos+1],
                &intbuf[0],
                &intbuf[1]);
        m_decimator2.myDecimate(
                buf[pos+4],
                buf[pos+5],
                &intbuf[2],
                &intbuf[3]);
        m_decimator2.myDecimate(
                buf[pos+8],
                buf[pos+9],
                &intbuf[4],
                &intbuf[5]);
        m_decimator2.myDecimate(
                buf[pos+12],
                buf[pos+13],
                &intbuf[6],
                &intbuf[7]);
        m_decimator2.myDecimate(
                buf[pos+16],
                buf[pos+17],
                &intbuf[8],
                &intbuf[9]);
        m_decimator2.myDecimate(
                buf[pos+20],
                buf[pos+21],
                &intbuf[10],
                &intbuf[11]);
        m_decimator2.myDecimate(
                buf[pos+24],
                buf[pos+25],
                &intbuf[12],
                &intbuf[13]);
        m_decimator2.myDecimate(
                buf[pos+28],
                buf[pos+29],
                &intbuf[14],
                &intbuf[15]);
        m_decimator2.myDecimate(
                buf[pos+32],
                buf[pos+33],
                &intbuf[16],
                &intbuf[17]);
        m_decimator2.myDecimate(
                buf[pos+36],
                buf[pos+37],
                &intbuf[18],
                &intbuf[19]);
        m_decimator2.myDecimate(
                buf[pos+40],
                buf[pos+41],
                &intbuf[20],
                &intbuf[21]);
        m_decimator2.myDecimate(
                buf[pos+44],
                buf[pos+45],
                &intbuf[22],
                &intbuf[23]);
        m_decimator2.myDecimate(
                buf[pos+48],
                buf[pos+49],
                &intbuf[24],
                &intbuf[25]);
        m_decimator2.myDecimate(
                buf[pos+52],
                buf[pos+53],
                &intbuf[26],
                &intbuf[27]);
        m_decimator2.myDecimate(
                buf[pos+56],
                buf[pos+57],
                &intbuf[28],
                &intbuf[29]);
        m_decimator2.myDecimate(
                buf[pos+60],
                buf[pos+61],
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

        (**it).setReal(intbuf[30] * decimation_scale<InputBits>::scaleIn);
        (**it).setImag(intbuf[31] * decimation_scale<InputBits>::scaleIn);
        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate64_inf(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float xreal[16], yimag[16];

    for (int pos = 0; pos < nbIAndQ - 127; )
    {
        for (int i = 0; i < 16; i++)
        {
            xreal[i] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]);
            yimag[i] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]);
            pos += 8;
        }

        m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
        m_decimator2.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);
        m_decimator2.myDecimate(xreal[4], yimag[4], &xreal[5], &yimag[5]);
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

        (**it).setReal(xreal[15] * decimation_scale<InputBits>::scaleIn);
        (**it).setImag(yimag[15] * decimation_scale<InputBits>::scaleIn);

        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate64_sup(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float xreal[16], yimag[16];

    for (int pos = 0; pos < nbIAndQ - 127; )
    {
        for (int i = 0; i < 16; i++)
        {
            xreal[i] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]);
            yimag[i] = (buf[pos+4] + buf[pos+7] - buf[pos+0] - buf[pos+3]);
            pos += 8;
        }

        m_decimator2.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
        m_decimator2.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);
        m_decimator2.myDecimate(xreal[4], yimag[4], &xreal[5], &yimag[5]);
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

        (**it).setReal(xreal[15] * decimation_scale<InputBits>::scaleIn);
        (**it).setImag(yimag[15] * decimation_scale<InputBits>::scaleIn);

        ++(*it);
    }
}

template<typename T, uint InputBits>
void DecimatorsIF<T, InputBits>::decimate64_cen(FSampleVector::iterator* it, const T* buf, qint32 nbIAndQ)
{
    float intbuf[64];

    for (int pos = 0; pos < nbIAndQ - 127; pos += 128)
    {
        intbuf[0]  = buf[pos+2];
        intbuf[1]  = buf[pos+3];
        intbuf[2]  = buf[pos+6];
        intbuf[3]  = buf[pos+7];
        intbuf[4]  = buf[pos+10];
        intbuf[5]  = buf[pos+11];
        intbuf[6]  = buf[pos+14];
        intbuf[7]  = buf[pos+15];
        intbuf[8]  = buf[pos+18];
        intbuf[9]  = buf[pos+19];
        intbuf[10] = buf[pos+22];
        intbuf[11] = buf[pos+23];
        intbuf[12] = buf[pos+26];
        intbuf[13] = buf[pos+27];
        intbuf[14] = buf[pos+30];
        intbuf[15] = buf[pos+31];
        intbuf[16] = buf[pos+34];
        intbuf[17] = buf[pos+35];
        intbuf[18] = buf[pos+38];
        intbuf[19] = buf[pos+39];
        intbuf[20] = buf[pos+42];
        intbuf[21] = buf[pos+43];
        intbuf[22] = buf[pos+46];
        intbuf[23] = buf[pos+47];
        intbuf[24] = buf[pos+50];
        intbuf[25] = buf[pos+51];
        intbuf[26] = buf[pos+54];
        intbuf[27] = buf[pos+55];
        intbuf[28] = buf[pos+58];
        intbuf[29] = buf[pos+59];
        intbuf[30] = buf[pos+62];
        intbuf[31] = buf[pos+63];

        intbuf[32] = buf[pos+66];
        intbuf[33] = buf[pos+67];
        intbuf[34] = buf[pos+70];
        intbuf[35] = buf[pos+71];
        intbuf[36] = buf[pos+74];
        intbuf[37] = buf[pos+75];
        intbuf[38] = buf[pos+78];
        intbuf[39] = buf[pos+79];
        intbuf[40] = buf[pos+82];
        intbuf[41] = buf[pos+83];
        intbuf[42] = buf[pos+86];
        intbuf[43] = buf[pos+87];
        intbuf[44] = buf[pos+90];
        intbuf[45] = buf[pos+91];
        intbuf[46] = buf[pos+94];
        intbuf[47] = buf[pos+95];
        intbuf[48] = buf[pos+98];
        intbuf[49] = buf[pos+99];
        intbuf[50] = buf[pos+102];
        intbuf[51] = buf[pos+103];
        intbuf[52] = buf[pos+106];
        intbuf[53] = buf[pos+107];
        intbuf[54] = buf[pos+110];
        intbuf[55] = buf[pos+111];
        intbuf[56] = buf[pos+114];
        intbuf[57] = buf[pos+115];
        intbuf[58] = buf[pos+118];
        intbuf[59] = buf[pos+119];
        intbuf[60] = buf[pos+122];
        intbuf[61] = buf[pos+123];
        intbuf[62] = buf[pos+126];
        intbuf[63] = buf[pos+127];

        m_decimator2.myDecimate(
                buf[pos+0],
                buf[pos+1],
                &intbuf[0],
                &intbuf[1]);
        m_decimator2.myDecimate(
                buf[pos+4],
                buf[pos+5],
                &intbuf[2],
                &intbuf[3]);
        m_decimator2.myDecimate(
                buf[pos+8],
                buf[pos+9],
                &intbuf[4],
                &intbuf[5]);
        m_decimator2.myDecimate(
                buf[pos+12],
                buf[pos+13],
                &intbuf[6],
                &intbuf[7]);
        m_decimator2.myDecimate(
                buf[pos+16],
                buf[pos+17],
                &intbuf[8],
                &intbuf[9]);
        m_decimator2.myDecimate(
                buf[pos+20],
                buf[pos+21],
                &intbuf[10],
                &intbuf[11]);
        m_decimator2.myDecimate(
                buf[pos+24],
                buf[pos+25],
                &intbuf[12],
                &intbuf[13]);
        m_decimator2.myDecimate(
                buf[pos+28],
                buf[pos+29],
                &intbuf[14],
                &intbuf[15]);
        m_decimator2.myDecimate(
                buf[pos+32],
                buf[pos+33],
                &intbuf[16],
                &intbuf[17]);
        m_decimator2.myDecimate(
                buf[pos+36],
                buf[pos+37],
                &intbuf[18],
                &intbuf[19]);
        m_decimator2.myDecimate(
                buf[pos+40],
                buf[pos+41],
                &intbuf[20],
                &intbuf[21]);
        m_decimator2.myDecimate(
                buf[pos+44],
                buf[pos+45],
                &intbuf[22],
                &intbuf[23]);
        m_decimator2.myDecimate(
                buf[pos+48],
                buf[pos+49],
                &intbuf[24],
                &intbuf[25]);
        m_decimator2.myDecimate(
                buf[pos+52],
                buf[pos+53],
                &intbuf[26],
                &intbuf[27]);
        m_decimator2.myDecimate(
                buf[pos+56],
                buf[pos+57],
                &intbuf[28],
                &intbuf[29]);
        m_decimator2.myDecimate(
                buf[pos+60],
                buf[pos+61],
                &intbuf[30],
                &intbuf[31]);
        m_decimator2.myDecimate(
                buf[pos+64],
                buf[pos+65],
                &intbuf[32],
                &intbuf[33]);
        m_decimator2.myDecimate(
                buf[pos+68],
                buf[pos+69],
                &intbuf[34],
                &intbuf[35]);
        m_decimator2.myDecimate(
                buf[pos+72],
                buf[pos+73],
                &intbuf[36],
                &intbuf[37]);
        m_decimator2.myDecimate(
                buf[pos+76],
                buf[pos+77],
                &intbuf[38],
                &intbuf[39]);
        m_decimator2.myDecimate(
                buf[pos+80],
                buf[pos+81],
                &intbuf[40],
                &intbuf[41]);
        m_decimator2.myDecimate(
                buf[pos+84],
                buf[pos+85],
                &intbuf[42],
                &intbuf[43]);
        m_decimator2.myDecimate(
                buf[pos+88],
                buf[pos+89],
                &intbuf[44],
                &intbuf[45]);
        m_decimator2.myDecimate(
                buf[pos+92],
                buf[pos+93],
                &intbuf[46],
                &intbuf[47]);
        m_decimator2.myDecimate(
                buf[pos+96],
                buf[pos+97],
                &intbuf[48],
                &intbuf[49]);
        m_decimator2.myDecimate(
                buf[pos+100],
                buf[pos+101],
                &intbuf[50],
                &intbuf[51]);
        m_decimator2.myDecimate(
                buf[pos+104],
                buf[pos+105],
                &intbuf[52],
                &intbuf[53]);
        m_decimator2.myDecimate(
                buf[pos+108],
                buf[pos+109],
                &intbuf[54],
                &intbuf[55]);
        m_decimator2.myDecimate(
                buf[pos+112],
                buf[pos+113],
                &intbuf[56],
                &intbuf[57]);
        m_decimator2.myDecimate(
                buf[pos+116],
                buf[pos+117],
                &intbuf[58],
                &intbuf[59]);
        m_decimator2.myDecimate(
                buf[pos+120],
                buf[pos+121],
                &intbuf[60],
                &intbuf[61]);
        m_decimator2.myDecimate(
                buf[pos+124],
                buf[pos+125],
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

        (**it).setReal(intbuf[62] * decimation_scale<InputBits>::scaleIn);
        (**it).setImag(intbuf[63] * decimation_scale<InputBits>::scaleIn);
        ++(*it);
    }
}

#endif /* SDRBASE_DSP_DECIMATORSIF_H_ */
