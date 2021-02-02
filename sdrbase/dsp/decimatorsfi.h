///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_DSP_DECIMATORSFI_H_
#define SDRBASE_DSP_DECIMATORSFI_H_

#include "dsp/inthalfbandfiltereof.h"
#include "export.h"

#define DECIMATORSFI_HB_FILTER_ORDER 64

/** Decimators with float input and integer output */
template<bool IQOrder>
class DecimatorsFI
{
public:
    SDRBASE_API void decimate1(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    SDRBASE_API void decimate2_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    SDRBASE_API void decimate2_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate2_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    SDRBASE_API void decimate4_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    SDRBASE_API void decimate4_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate4_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate8_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate8_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate8_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate16_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate16_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate16_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate32_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate32_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate32_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate64_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate64_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate64_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate128_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate256_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);

    IntHalfbandFilterEOF<DECIMATORSFI_HB_FILTER_ORDER, IQOrder> m_decimator2;  // 1st stages
    IntHalfbandFilterEOF<DECIMATORSFI_HB_FILTER_ORDER, true> m_decimator2s; // 1st stages - straight
    IntHalfbandFilterEOF<DECIMATORSFI_HB_FILTER_ORDER, true> m_decimator4;  // 2nd stages
    IntHalfbandFilterEOF<DECIMATORSFI_HB_FILTER_ORDER, true> m_decimator8;  // 3rd stages
    IntHalfbandFilterEOF<DECIMATORSFI_HB_FILTER_ORDER, true> m_decimator16; // 4th stages
    IntHalfbandFilterEOF<DECIMATORSFI_HB_FILTER_ORDER, true> m_decimator32; // 5th stages
    IntHalfbandFilterEOF<DECIMATORSFI_HB_FILTER_ORDER, true> m_decimator64; // 6th stages
    IntHalfbandFilterEOF<DECIMATORSFI_HB_FILTER_ORDER, true> m_decimator128; // 7th stages
    IntHalfbandFilterEOF<DECIMATORSFI_HB_FILTER_ORDER, true> m_decimator256; // 8th stages
};


template<bool IQOrder>
void DecimatorsFI<IQOrder>::decimate2_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
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

        (**it).setReal(intbuf[0] * SDR_RX_SCALED);
        (**it).setImag(intbuf[1] * SDR_RX_SCALED);

        ++(*it);
    }
}

template<bool IQOrder>
void DecimatorsFI<IQOrder>::decimate8_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
{
    float xreal[2], yimag[2];

    for (int pos = 0; pos < nbIAndQ - 15; pos += 8)
    {
        xreal[0] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]);
        yimag[0] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]);
        pos += 8;

        xreal[1] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]);
        yimag[1] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]);

        m_decimator2s.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);

        (**it).setReal((IQOrder ? xreal[1] : yimag[1]) * SDR_RX_SCALED);
        (**it).setImag((IQOrder ? yimag[1] : xreal[1]) * SDR_RX_SCALED);

        ++(*it);
    }
}

template<bool IQOrder>
void DecimatorsFI<IQOrder>::decimate8_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
{
    float xreal[2], yimag[2];

    for (int pos = 0; pos < nbIAndQ - 15; pos += 8)
    {
        xreal[0] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]);
        yimag[0] = (- buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7]);
        pos += 8;

        xreal[1] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]);
        yimag[1] = (- buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7]);

        m_decimator2s.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);

        (**it).setReal((IQOrder ? xreal[1] : yimag[1]) * SDR_RX_SCALED);
        (**it).setImag((IQOrder ? yimag[1] : xreal[1]) * SDR_RX_SCALED);

        ++(*it);
    }
}

template<bool IQOrder>
void DecimatorsFI<IQOrder>::decimate16_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
{
    // Offset tuning: 4x downsample and rotate, then
    // downsample 4x more. [ rotate:  0, 1, -3, 2, -4, -5, 7, -6]
    float xreal[4], yimag[4];

    for (int pos = 0; pos < nbIAndQ - 31; )
    {
        for (int i = 0; i < 4; i++)
        {
            xreal[i] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]);
            yimag[i] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]);
            pos += 8;
        }

        m_decimator2s.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
        m_decimator2s.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);

        m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);

        (**it).setReal((IQOrder ? xreal[3] : yimag[3]) * SDR_RX_SCALED);
        (**it).setImag((IQOrder ? yimag[3] : xreal[3]) * SDR_RX_SCALED);

        ++(*it);
    }
}

template<bool IQOrder>
void DecimatorsFI<IQOrder>::decimate16_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
{
    // Offset tuning: 4x downsample and rotate, then
    // downsample 4x more. [ rotate:  1, 0, -2, 3, -5, -4, 6, -7]
    float xreal[4], yimag[4];

    for (int pos = 0; pos < nbIAndQ - 31; )
    {
        for (int i = 0; i < 4; i++)
        {
            xreal[i] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]);
            yimag[i] = (buf[pos+4] + buf[pos+7] - buf[pos+0] - buf[pos+3]);
            pos += 8;
        }

        m_decimator2s.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
        m_decimator2s.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);

        m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);

        (**it).setReal((IQOrder ? xreal[3] : yimag[3]) * SDR_RX_SCALED);
        (**it).setImag((IQOrder ? yimag[3] : xreal[3]) * SDR_RX_SCALED);

        ++(*it);
    }
}

template<bool IQOrder>
void DecimatorsFI<IQOrder>::decimate32_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
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

        m_decimator2s.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
        m_decimator2s.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);
        m_decimator2s.myDecimate(xreal[4], yimag[4], &xreal[5], &yimag[5]);
        m_decimator2s.myDecimate(xreal[6], yimag[6], &xreal[7], &yimag[7]);

        m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);
        m_decimator4.myDecimate(xreal[5], yimag[5], &xreal[7], &yimag[7]);

        m_decimator8.myDecimate(xreal[3], yimag[3], &xreal[7], &yimag[7]);

        (**it).setReal((IQOrder ? xreal[7] : yimag[7]) * SDR_RX_SCALED);
        (**it).setImag((IQOrder ? yimag[7] : xreal[7]) * SDR_RX_SCALED);

        ++(*it);
    }
}

template<bool IQOrder>
void DecimatorsFI<IQOrder>::decimate32_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
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

        m_decimator2s.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
        m_decimator2s.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);
        m_decimator2s.myDecimate(xreal[4], yimag[4], &xreal[5], &yimag[5]);
        m_decimator2s.myDecimate(xreal[6], yimag[6], &xreal[7], &yimag[7]);

        m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);
        m_decimator4.myDecimate(xreal[5], yimag[5], &xreal[7], &yimag[7]);

        m_decimator8.myDecimate(xreal[3], yimag[3], &xreal[7], &yimag[7]);

        (**it).setReal((IQOrder ? xreal[7] : yimag[7]) * SDR_RX_SCALED);
        (**it).setImag((IQOrder ? yimag[7] : xreal[7]) * SDR_RX_SCALED);

        ++(*it);
    }
}

template<bool IQOrder>
void DecimatorsFI<IQOrder>::decimate64_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
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

        m_decimator2s.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
        m_decimator2s.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);
        m_decimator2s.myDecimate(xreal[4], yimag[4], &xreal[5], &yimag[5]);
        m_decimator2s.myDecimate(xreal[6], yimag[6], &xreal[7], &yimag[7]);
        m_decimator2s.myDecimate(xreal[8], yimag[8], &xreal[9], &yimag[9]);
        m_decimator2s.myDecimate(xreal[10], yimag[10], &xreal[11], &yimag[11]);
        m_decimator2s.myDecimate(xreal[12], yimag[12], &xreal[13], &yimag[13]);
        m_decimator2s.myDecimate(xreal[14], yimag[14], &xreal[15], &yimag[15]);

        m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);
        m_decimator4.myDecimate(xreal[5], yimag[5], &xreal[7], &yimag[7]);
        m_decimator4.myDecimate(xreal[9], yimag[9], &xreal[11], &yimag[11]);
        m_decimator4.myDecimate(xreal[13], yimag[13], &xreal[15], &yimag[15]);

        m_decimator8.myDecimate(xreal[3], yimag[3], &xreal[7], &yimag[7]);
        m_decimator8.myDecimate(xreal[11], yimag[11], &xreal[15], &yimag[15]);

        m_decimator16.myDecimate(xreal[7], yimag[7], &xreal[15], &yimag[15]);

        (**it).setReal((IQOrder ? xreal[15] : yimag[15]) * SDR_RX_SCALED);
        (**it).setImag((IQOrder ? yimag[15] : xreal[15]) * SDR_RX_SCALED);

        ++(*it);
    }
}

template<bool IQOrder>
void DecimatorsFI<IQOrder>::decimate64_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
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

        m_decimator2s.myDecimate(xreal[0], yimag[0], &xreal[1], &yimag[1]);
        m_decimator2s.myDecimate(xreal[2], yimag[2], &xreal[3], &yimag[3]);
        m_decimator2s.myDecimate(xreal[4], yimag[4], &xreal[5], &yimag[5]);
        m_decimator2s.myDecimate(xreal[6], yimag[6], &xreal[7], &yimag[7]);
        m_decimator2s.myDecimate(xreal[8], yimag[8], &xreal[9], &yimag[9]);
        m_decimator2s.myDecimate(xreal[10], yimag[10], &xreal[11], &yimag[11]);
        m_decimator2s.myDecimate(xreal[12], yimag[12], &xreal[13], &yimag[13]);
        m_decimator2s.myDecimate(xreal[14], yimag[14], &xreal[15], &yimag[15]);

        m_decimator4.myDecimate(xreal[1], yimag[1], &xreal[3], &yimag[3]);
        m_decimator4.myDecimate(xreal[5], yimag[5], &xreal[7], &yimag[7]);
        m_decimator4.myDecimate(xreal[9], yimag[9], &xreal[11], &yimag[11]);
        m_decimator4.myDecimate(xreal[13], yimag[13], &xreal[15], &yimag[15]);

        m_decimator8.myDecimate(xreal[3], yimag[3], &xreal[7], &yimag[7]);
        m_decimator8.myDecimate(xreal[11], yimag[11], &xreal[15], &yimag[15]);

        m_decimator16.myDecimate(xreal[7], yimag[7], &xreal[15], &yimag[15]);

        (**it).setReal((IQOrder ? xreal[15] : yimag[15]) * SDR_RX_SCALED);
        (**it).setImag((IQOrder ? yimag[15] : xreal[15]) * SDR_RX_SCALED);

        ++(*it);
    }
}

template<bool IQOrder>
void DecimatorsFI<IQOrder>::decimate4_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
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

        (**it).setReal(intbuf[2] * SDR_RX_SCALED);
        (**it).setImag(intbuf[3] * SDR_RX_SCALED);
        ++(*it);
    }
}

template<bool IQOrder>
void DecimatorsFI<IQOrder>::decimate8_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
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

        (**it).setReal(intbuf[6] * SDR_RX_SCALED);
        (**it).setImag(intbuf[7] * SDR_RX_SCALED);
        ++(*it);
    }
}

template<bool IQOrder>
void DecimatorsFI<IQOrder>::decimate16_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
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

        (**it).setReal(intbuf[14] * SDR_RX_SCALED);
        (**it).setImag(intbuf[15] * SDR_RX_SCALED);
        ++(*it);
    }
}

template<bool IQOrder>
void DecimatorsFI<IQOrder>::decimate32_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
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

        (**it).setReal(intbuf[30] * SDR_RX_SCALED);
        (**it).setImag(intbuf[31] * SDR_RX_SCALED);
        ++(*it);
    }
}

template<bool IQOrder>
void DecimatorsFI<IQOrder>::decimate64_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
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

        (**it).setReal(intbuf[62] * SDR_RX_SCALED);
        (**it).setImag(intbuf[63] * SDR_RX_SCALED);
        ++(*it);
    }
}

template<bool IQOrder>
void DecimatorsFI<IQOrder>::decimate128_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
{
    float intbuf[128];

    for (int pos = 0; pos < nbIAndQ - 255; pos += 256)
    {
        float *intbufp;
        int posi;

        intbufp = intbuf;
        posi = pos;
        for (int i = 0; i < 4; i++)
        {
            intbufp[0]  = buf[posi+2];
            intbufp[1]  = buf[posi+3];
            intbufp[2]  = buf[posi+6];
            intbufp[3]  = buf[posi+7];
            intbufp[4]  = buf[posi+10];
            intbufp[5]  = buf[posi+11];
            intbufp[6]  = buf[posi+14];
            intbufp[7]  = buf[posi+15];
            intbufp[8]  = buf[posi+18];
            intbufp[9]  = buf[posi+19];
            intbufp[10] = buf[posi+22];
            intbufp[11] = buf[posi+23];
            intbufp[12] = buf[posi+26];
            intbufp[13] = buf[posi+27];
            intbufp[14] = buf[posi+30];
            intbufp[15] = buf[posi+31];
            intbufp[16] = buf[posi+34];
            intbufp[17] = buf[posi+35];
            intbufp[18] = buf[posi+38];
            intbufp[19] = buf[posi+39];
            intbufp[20] = buf[posi+42];
            intbufp[21] = buf[posi+43];
            intbufp[22] = buf[posi+46];
            intbufp[23] = buf[posi+47];
            intbufp[24] = buf[posi+50];
            intbufp[25] = buf[posi+51];
            intbufp[26] = buf[posi+54];
            intbufp[27] = buf[posi+55];
            intbufp[28] = buf[posi+58];
            intbufp[29] = buf[posi+59];
            intbufp[30] = buf[posi+62];
            intbufp[31] = buf[posi+63];
            intbufp += 32;
            posi += 64;
        }

        intbufp = intbuf;
        posi = pos;
        for (int i = 0; i < 2; i++)
        {
            m_decimator2.myDecimate(
                    buf[posi+0],
                    buf[posi+1],
                    &intbufp[0],
                    &intbufp[1]);
            m_decimator2.myDecimate(
                    buf[posi+4],
                    buf[posi+5],
                    &intbufp[2],
                    &intbufp[3]);
            m_decimator2.myDecimate(
                    buf[posi+8],
                    buf[posi+9],
                    &intbufp[4],
                    &intbufp[5]);
            m_decimator2.myDecimate(
                    buf[posi+12],
                    buf[posi+13],
                    &intbufp[6],
                    &intbufp[7]);
            m_decimator2.myDecimate(
                    buf[posi+16],
                    buf[posi+17],
                    &intbufp[8],
                    &intbufp[9]);
            m_decimator2.myDecimate(
                    buf[posi+20],
                    buf[posi+21],
                    &intbufp[10],
                    &intbufp[11]);
            m_decimator2.myDecimate(
                    buf[posi+24],
                    buf[posi+25],
                    &intbufp[12],
                    &intbufp[13]);
            m_decimator2.myDecimate(
                    buf[posi+28],
                    buf[posi+29],
                    &intbufp[14],
                    &intbufp[15]);
            m_decimator2.myDecimate(
                    buf[posi+32],
                    buf[posi+33],
                    &intbufp[16],
                    &intbufp[17]);
            m_decimator2.myDecimate(
                    buf[posi+36],
                    buf[posi+37],
                    &intbufp[18],
                    &intbufp[19]);
            m_decimator2.myDecimate(
                    buf[posi+40],
                    buf[posi+41],
                    &intbufp[20],
                    &intbufp[21]);
            m_decimator2.myDecimate(
                    buf[posi+44],
                    buf[posi+45],
                    &intbufp[22],
                    &intbufp[23]);
            m_decimator2.myDecimate(
                    buf[posi+48],
                    buf[posi+49],
                    &intbufp[24],
                    &intbufp[25]);
            m_decimator2.myDecimate(
                    buf[posi+52],
                    buf[posi+53],
                    &intbufp[26],
                    &intbufp[27]);
            m_decimator2.myDecimate(
                    buf[posi+56],
                    buf[posi+57],
                    &intbufp[28],
                    &intbufp[29]);
            m_decimator2.myDecimate(
                    buf[posi+60],
                    buf[posi+61],
                    &intbufp[30],
                    &intbufp[31]);
            m_decimator2.myDecimate(
                    buf[posi+64],
                    buf[posi+65],
                    &intbufp[32],
                    &intbufp[33]);
            m_decimator2.myDecimate(
                    buf[posi+68],
                    buf[posi+69],
                    &intbufp[34],
                    &intbufp[35]);
            m_decimator2.myDecimate(
                    buf[posi+72],
                    buf[posi+73],
                    &intbufp[36],
                    &intbufp[37]);
            m_decimator2.myDecimate(
                    buf[posi+76],
                    buf[posi+77],
                    &intbufp[38],
                    &intbufp[39]);
            m_decimator2.myDecimate(
                    buf[posi+80],
                    buf[posi+81],
                    &intbufp[40],
                    &intbufp[41]);
            m_decimator2.myDecimate(
                    buf[posi+84],
                    buf[posi+85],
                    &intbufp[42],
                    &intbufp[43]);
            m_decimator2.myDecimate(
                    buf[posi+88],
                    buf[posi+89],
                    &intbufp[44],
                    &intbufp[45]);
            m_decimator2.myDecimate(
                    buf[posi+92],
                    buf[posi+93],
                    &intbufp[46],
                    &intbufp[47]);
            m_decimator2.myDecimate(
                    buf[posi+96],
                    buf[posi+97],
                    &intbufp[48],
                    &intbufp[49]);
            m_decimator2.myDecimate(
                    buf[posi+100],
                    buf[posi+101],
                    &intbufp[50],
                    &intbufp[51]);
            m_decimator2.myDecimate(
                    buf[posi+104],
                    buf[posi+105],
                    &intbufp[52],
                    &intbufp[53]);
            m_decimator2.myDecimate(
                    buf[posi+108],
                    buf[posi+109],
                    &intbufp[54],
                    &intbufp[55]);
            m_decimator2.myDecimate(
                    buf[posi+112],
                    buf[posi+113],
                    &intbufp[56],
                    &intbufp[57]);
            m_decimator2.myDecimate(
                    buf[posi+116],
                    buf[posi+117],
                    &intbufp[58],
                    &intbufp[59]);
            m_decimator2.myDecimate(
                    buf[posi+120],
                    buf[posi+121],
                    &intbufp[60],
                    &intbufp[61]);
            m_decimator2.myDecimate(
                    buf[posi+124],
                    buf[posi+125],
                    &intbufp[62],
                    &intbufp[63]);
            intbufp += 64;
            posi += 128;
        }

        intbufp = intbuf;
        for (int i = 0; i < 2; i++)
        {
            m_decimator4.myDecimate(
                    intbufp[0],
                    intbufp[1],
                    &intbufp[2],
                    &intbufp[3]);
            m_decimator4.myDecimate(
                    intbufp[4],
                    intbufp[5],
                    &intbufp[6],
                    &intbufp[7]);
            m_decimator4.myDecimate(
                    intbufp[8],
                    intbufp[9],
                    &intbufp[10],
                    &intbufp[11]);
            m_decimator4.myDecimate(
                    intbufp[12],
                    intbufp[13],
                    &intbufp[14],
                    &intbufp[15]);
            m_decimator4.myDecimate(
                    intbufp[16],
                    intbufp[17],
                    &intbufp[18],
                    &intbufp[19]);
            m_decimator4.myDecimate(
                    intbufp[20],
                    intbufp[21],
                    &intbufp[22],
                    &intbufp[23]);
            m_decimator4.myDecimate(
                    intbufp[24],
                    intbufp[25],
                    &intbufp[26],
                    &intbufp[27]);
            m_decimator4.myDecimate(
                    intbufp[28],
                    intbufp[29],
                    &intbufp[30],
                    &intbufp[31]);
            m_decimator4.myDecimate(
                    intbufp[32],
                    intbufp[33],
                    &intbufp[34],
                    &intbufp[35]);
            m_decimator4.myDecimate(
                    intbufp[36],
                    intbufp[37],
                    &intbufp[38],
                    &intbufp[39]);
            m_decimator4.myDecimate(
                    intbufp[40],
                    intbufp[41],
                    &intbufp[42],
                    &intbufp[43]);
            m_decimator4.myDecimate(
                    intbufp[44],
                    intbufp[45],
                    &intbufp[46],
                    &intbufp[47]);
            m_decimator4.myDecimate(
                    intbufp[48],
                    intbufp[49],
                    &intbufp[50],
                    &intbufp[51]);
            m_decimator4.myDecimate(
                    intbufp[52],
                    intbufp[53],
                    &intbufp[54],
                    &intbufp[55]);
            m_decimator4.myDecimate(
                    intbufp[56],
                    intbufp[57],
                    &intbufp[58],
                    &intbufp[59]);
            m_decimator4.myDecimate(
                    intbufp[60],
                    intbufp[61],
                    &intbufp[62],
                    &intbufp[63]);
            intbufp += 64;
        }

        intbufp = intbuf;
        for (int i = 0; i < 2; i++)
        {
            m_decimator8.myDecimate(
                    intbufp[2],
                    intbufp[3],
                    &intbufp[6],
                    &intbufp[7]);
            m_decimator8.myDecimate(
                    intbufp[10],
                    intbufp[11],
                    &intbufp[14],
                    &intbufp[15]);
            m_decimator8.myDecimate(
                    intbufp[18],
                    intbufp[19],
                    &intbufp[22],
                    &intbufp[23]);
            m_decimator8.myDecimate(
                    intbufp[26],
                    intbufp[27],
                    &intbufp[30],
                    &intbufp[31]);
            m_decimator8.myDecimate(
                    intbufp[34],
                    intbufp[35],
                    &intbufp[38],
                    &intbufp[39]);
            m_decimator8.myDecimate(
                    intbufp[42],
                    intbufp[43],
                    &intbufp[46],
                    &intbufp[47]);
            m_decimator8.myDecimate(
                    intbufp[50],
                    intbufp[51],
                    &intbufp[54],
                    &intbufp[55]);
            m_decimator8.myDecimate(
                    intbufp[58],
                    intbufp[59],
                    &intbufp[62],
                    &intbufp[63]);
            intbufp += 64;
        }

        intbufp = intbuf;
        for (int i = 0; i < 2; i++)
        {
            m_decimator16.myDecimate(
                    intbufp[6],
                    intbufp[7],
                    &intbufp[14],
                    &intbufp[15]);
            m_decimator16.myDecimate(
                    intbufp[22],
                    intbufp[23],
                    &intbufp[30],
                    &intbufp[31]);
            m_decimator16.myDecimate(
                    intbufp[38],
                    intbufp[39],
                    &intbufp[46],
                    &intbufp[47]);
            m_decimator16.myDecimate(
                    intbufp[54],
                    intbufp[55],
                    &intbufp[62],
                    &intbufp[63]);
            intbufp += 64;
        }

        intbufp = intbuf;
        for (int i = 0; i < 2; i++)
        {
            m_decimator32.myDecimate(
                    intbufp[14],
                    intbufp[15],
                    &intbufp[30],
                    &intbufp[31]);
            m_decimator32.myDecimate(
                    intbufp[46],
                    intbufp[47],
                    &intbufp[62],
                    &intbufp[63]);
            intbufp += 64;
        }

        intbufp = intbuf;
        for (int i = 0; i < 2; i++)
        {
            m_decimator64.myDecimate(
                    intbufp[30],
                    intbufp[31],
                    &intbufp[62],
                    &intbufp[63]);
            intbufp += 64;
        }

        m_decimator128.myDecimate(
                intbuf[62],
                intbuf[63],
                &intbuf[126],
                &intbuf[127]);

        (**it).setReal(intbuf[126] * SDR_RX_SCALED);
        (**it).setImag(intbuf[127] * SDR_RX_SCALED);
        ++(*it);
    }
}

template<bool IQOrder>
void DecimatorsFI<IQOrder>::decimate256_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
{
    float intbuf[256];

    for (int pos = 0; pos < nbIAndQ - 511; pos += 512)
    {
        float *intbufp;
        int posi;

        intbufp = intbuf;
        posi = pos;
        for (int i = 0; i < 8; i++)
        {
            intbufp[0]  = buf[posi+2];
            intbufp[1]  = buf[posi+3];
            intbufp[2]  = buf[posi+6];
            intbufp[3]  = buf[posi+7];
            intbufp[4]  = buf[posi+10];
            intbufp[5]  = buf[posi+11];
            intbufp[6]  = buf[posi+14];
            intbufp[7]  = buf[posi+15];
            intbufp[8]  = buf[posi+18];
            intbufp[9]  = buf[posi+19];
            intbufp[10] = buf[posi+22];
            intbufp[11] = buf[posi+23];
            intbufp[12] = buf[posi+26];
            intbufp[13] = buf[posi+27];
            intbufp[14] = buf[posi+30];
            intbufp[15] = buf[posi+31];
            intbufp[16] = buf[posi+34];
            intbufp[17] = buf[posi+35];
            intbufp[18] = buf[posi+38];
            intbufp[19] = buf[posi+39];
            intbufp[20] = buf[posi+42];
            intbufp[21] = buf[posi+43];
            intbufp[22] = buf[posi+46];
            intbufp[23] = buf[posi+47];
            intbufp[24] = buf[posi+50];
            intbufp[25] = buf[posi+51];
            intbufp[26] = buf[posi+54];
            intbufp[27] = buf[posi+55];
            intbufp[28] = buf[posi+58];
            intbufp[29] = buf[posi+59];
            intbufp[30] = buf[posi+62];
            intbufp[31] = buf[posi+63];
            intbufp += 32;
            posi += 64;
        }

        intbufp = intbuf;
        posi = pos;
        for (int i = 0; i < 4; i++)
        {
            m_decimator2.myDecimate(
                    buf[posi+0],
                    buf[posi+1],
                    &intbufp[0],
                    &intbufp[1]);
            m_decimator2.myDecimate(
                    buf[posi+4],
                    buf[posi+5],
                    &intbufp[2],
                    &intbufp[3]);
            m_decimator2.myDecimate(
                    buf[posi+8],
                    buf[posi+9],
                    &intbufp[4],
                    &intbufp[5]);
            m_decimator2.myDecimate(
                    buf[posi+12],
                    buf[posi+13],
                    &intbufp[6],
                    &intbufp[7]);
            m_decimator2.myDecimate(
                    buf[posi+16],
                    buf[posi+17],
                    &intbufp[8],
                    &intbufp[9]);
            m_decimator2.myDecimate(
                    buf[posi+20],
                    buf[posi+21],
                    &intbufp[10],
                    &intbufp[11]);
            m_decimator2.myDecimate(
                    buf[posi+24],
                    buf[posi+25],
                    &intbufp[12],
                    &intbufp[13]);
            m_decimator2.myDecimate(
                    buf[posi+28],
                    buf[posi+29],
                    &intbufp[14],
                    &intbufp[15]);
            m_decimator2.myDecimate(
                    buf[posi+32],
                    buf[posi+33],
                    &intbufp[16],
                    &intbufp[17]);
            m_decimator2.myDecimate(
                    buf[posi+36],
                    buf[posi+37],
                    &intbufp[18],
                    &intbufp[19]);
            m_decimator2.myDecimate(
                    buf[posi+40],
                    buf[posi+41],
                    &intbufp[20],
                    &intbufp[21]);
            m_decimator2.myDecimate(
                    buf[posi+44],
                    buf[posi+45],
                    &intbufp[22],
                    &intbufp[23]);
            m_decimator2.myDecimate(
                    buf[posi+48],
                    buf[posi+49],
                    &intbufp[24],
                    &intbufp[25]);
            m_decimator2.myDecimate(
                    buf[posi+52],
                    buf[posi+53],
                    &intbufp[26],
                    &intbufp[27]);
            m_decimator2.myDecimate(
                    buf[posi+56],
                    buf[posi+57],
                    &intbufp[28],
                    &intbufp[29]);
            m_decimator2.myDecimate(
                    buf[posi+60],
                    buf[posi+61],
                    &intbufp[30],
                    &intbufp[31]);
            m_decimator2.myDecimate(
                    buf[posi+64],
                    buf[posi+65],
                    &intbufp[32],
                    &intbufp[33]);
            m_decimator2.myDecimate(
                    buf[posi+68],
                    buf[posi+69],
                    &intbufp[34],
                    &intbufp[35]);
            m_decimator2.myDecimate(
                    buf[posi+72],
                    buf[posi+73],
                    &intbufp[36],
                    &intbufp[37]);
            m_decimator2.myDecimate(
                    buf[posi+76],
                    buf[posi+77],
                    &intbufp[38],
                    &intbufp[39]);
            m_decimator2.myDecimate(
                    buf[posi+80],
                    buf[posi+81],
                    &intbufp[40],
                    &intbufp[41]);
            m_decimator2.myDecimate(
                    buf[posi+84],
                    buf[posi+85],
                    &intbufp[42],
                    &intbufp[43]);
            m_decimator2.myDecimate(
                    buf[posi+88],
                    buf[posi+89],
                    &intbufp[44],
                    &intbufp[45]);
            m_decimator2.myDecimate(
                    buf[posi+92],
                    buf[posi+93],
                    &intbufp[46],
                    &intbufp[47]);
            m_decimator2.myDecimate(
                    buf[posi+96],
                    buf[posi+97],
                    &intbufp[48],
                    &intbufp[49]);
            m_decimator2.myDecimate(
                    buf[posi+100],
                    buf[posi+101],
                    &intbufp[50],
                    &intbufp[51]);
            m_decimator2.myDecimate(
                    buf[posi+104],
                    buf[posi+105],
                    &intbufp[52],
                    &intbufp[53]);
            m_decimator2.myDecimate(
                    buf[posi+108],
                    buf[posi+109],
                    &intbufp[54],
                    &intbufp[55]);
            m_decimator2.myDecimate(
                    buf[posi+112],
                    buf[posi+113],
                    &intbufp[56],
                    &intbufp[57]);
            m_decimator2.myDecimate(
                    buf[posi+116],
                    buf[posi+117],
                    &intbufp[58],
                    &intbufp[59]);
            m_decimator2.myDecimate(
                    buf[posi+120],
                    buf[posi+121],
                    &intbufp[60],
                    &intbufp[61]);
            m_decimator2.myDecimate(
                    buf[posi+124],
                    buf[posi+125],
                    &intbufp[62],
                    &intbufp[63]);
            intbufp += 64;
            posi += 128;
        }

        intbufp = intbuf;
        for (int i = 0; i < 4; i++)
        {
            m_decimator4.myDecimate(
                    intbufp[0],
                    intbufp[1],
                    &intbufp[2],
                    &intbufp[3]);
            m_decimator4.myDecimate(
                    intbufp[4],
                    intbufp[5],
                    &intbufp[6],
                    &intbufp[7]);
            m_decimator4.myDecimate(
                    intbufp[8],
                    intbufp[9],
                    &intbufp[10],
                    &intbufp[11]);
            m_decimator4.myDecimate(
                    intbufp[12],
                    intbufp[13],
                    &intbufp[14],
                    &intbufp[15]);
            m_decimator4.myDecimate(
                    intbufp[16],
                    intbufp[17],
                    &intbufp[18],
                    &intbufp[19]);
            m_decimator4.myDecimate(
                    intbufp[20],
                    intbufp[21],
                    &intbufp[22],
                    &intbufp[23]);
            m_decimator4.myDecimate(
                    intbufp[24],
                    intbufp[25],
                    &intbufp[26],
                    &intbufp[27]);
            m_decimator4.myDecimate(
                    intbufp[28],
                    intbufp[29],
                    &intbufp[30],
                    &intbufp[31]);
            m_decimator4.myDecimate(
                    intbufp[32],
                    intbufp[33],
                    &intbufp[34],
                    &intbufp[35]);
            m_decimator4.myDecimate(
                    intbufp[36],
                    intbufp[37],
                    &intbufp[38],
                    &intbufp[39]);
            m_decimator4.myDecimate(
                    intbufp[40],
                    intbufp[41],
                    &intbufp[42],
                    &intbufp[43]);
            m_decimator4.myDecimate(
                    intbufp[44],
                    intbufp[45],
                    &intbufp[46],
                    &intbufp[47]);
            m_decimator4.myDecimate(
                    intbufp[48],
                    intbufp[49],
                    &intbufp[50],
                    &intbufp[51]);
            m_decimator4.myDecimate(
                    intbufp[52],
                    intbufp[53],
                    &intbufp[54],
                    &intbufp[55]);
            m_decimator4.myDecimate(
                    intbufp[56],
                    intbufp[57],
                    &intbufp[58],
                    &intbufp[59]);
            m_decimator4.myDecimate(
                    intbufp[60],
                    intbufp[61],
                    &intbufp[62],
                    &intbufp[63]);
            intbufp += 64;
        }

        intbufp = intbuf;
        for (int i = 0; i < 4; i++)
        {
            m_decimator8.myDecimate(
                    intbufp[2],
                    intbufp[3],
                    &intbufp[6],
                    &intbufp[7]);
            m_decimator8.myDecimate(
                    intbufp[10],
                    intbufp[11],
                    &intbufp[14],
                    &intbufp[15]);
            m_decimator8.myDecimate(
                    intbufp[18],
                    intbufp[19],
                    &intbufp[22],
                    &intbufp[23]);
            m_decimator8.myDecimate(
                    intbufp[26],
                    intbufp[27],
                    &intbufp[30],
                    &intbufp[31]);
            m_decimator8.myDecimate(
                    intbufp[34],
                    intbufp[35],
                    &intbufp[38],
                    &intbufp[39]);
            m_decimator8.myDecimate(
                    intbufp[42],
                    intbufp[43],
                    &intbufp[46],
                    &intbufp[47]);
            m_decimator8.myDecimate(
                    intbufp[50],
                    intbufp[51],
                    &intbufp[54],
                    &intbufp[55]);
            m_decimator8.myDecimate(
                    intbufp[58],
                    intbufp[59],
                    &intbufp[62],
                    &intbufp[63]);
            intbufp += 64;
        }

        intbufp = intbuf;
        for (int i = 0; i < 4; i++)
        {
            m_decimator16.myDecimate(
                    intbufp[6],
                    intbufp[7],
                    &intbufp[14],
                    &intbufp[15]);
            m_decimator16.myDecimate(
                    intbufp[22],
                    intbufp[23],
                    &intbufp[30],
                    &intbufp[31]);
            m_decimator16.myDecimate(
                    intbufp[38],
                    intbufp[39],
                    &intbufp[46],
                    &intbufp[47]);
            m_decimator16.myDecimate(
                    intbufp[54],
                    intbufp[55],
                    &intbufp[62],
                    &intbufp[63]);
            intbufp += 64;
        }

        intbufp = intbuf;
        for (int i = 0; i < 4; i++)
        {
            m_decimator32.myDecimate(
                    intbufp[14],
                    intbufp[15],
                    &intbufp[30],
                    &intbufp[31]);
            m_decimator32.myDecimate(
                    intbufp[46],
                    intbufp[47],
                    &intbufp[62],
                    &intbufp[63]);
            intbufp += 64;
        }

        intbufp = intbuf;
        for (int i = 0; i < 4; i++)
        {
            m_decimator64.myDecimate(
                    intbufp[30],
                    intbufp[31],
                    &intbufp[62],
                    &intbufp[63]);
            intbufp += 64;
        }

        intbufp = intbuf;
        for (int i = 0; i < 2; i++)
        {
            m_decimator128.myDecimate(
                    intbufp[62],
                    intbufp[63],
                    &intbufp[126],
                    &intbufp[127]);
            intbufp += 128;
        }

        m_decimator256.myDecimate(
                intbuf[126],
                intbuf[127],
                &intbuf[254],
                &intbuf[255]);

        (**it).setReal(intbuf[254] * SDR_RX_SCALED);
        (**it).setImag(intbuf[255] * SDR_RX_SCALED);
        ++(*it);
    }
}

#endif /* SDRBASE_DSP_DECIMATORSFI_H_ */
