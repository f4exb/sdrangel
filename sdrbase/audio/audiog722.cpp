///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <algorithm>
#include "audiog722.h"

#define G722_SAMPLE_RATE_8000 1
#define G722_PACKED 2

const int AudioG722::q6[32] =
{
       0,   35,   72,  110,  150,  190,  233,  276,
     323,  370,  422,  473,  530,  587,  650,  714,
     786,  858,  940, 1023, 1121, 1219, 1339, 1458,
    1612, 1765, 1980, 2195, 2557, 2919,    0,    0
};
const int AudioG722::iln[32] =
{
     0, 63, 62, 31, 30, 29, 28, 27,
    26, 25, 24, 23, 22, 21, 20, 19,
    18, 17, 16, 15, 14, 13, 12, 11,
    10,  9,  8,  7,  6,  5,  4,  0
};
const int AudioG722::ilp[32] =
{
     0, 61, 60, 59, 58, 57, 56, 55,
    54, 53, 52, 51, 50, 49, 48, 47,
    46, 45, 44, 43, 42, 41, 40, 39,
    38, 37, 36, 35, 34, 33, 32,  0
};
const int AudioG722::wl[8] =
{
    -60, -30, 58, 172, 334, 538, 1198, 3042
};
const int AudioG722::rl42[16] =
{
    0, 7, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 0
};
const int AudioG722::ilb[32] =
{
    2048, 2093, 2139, 2186, 2233, 2282, 2332,
    2383, 2435, 2489, 2543, 2599, 2656, 2714,
    2774, 2834, 2896, 2960, 3025, 3091, 3158,
    3228, 3298, 3371, 3444, 3520, 3597, 3676,
    3756, 3838, 3922, 4008
};
const int AudioG722::qm4[16] =
{
         0, -20456, -12896, -8968,
     -6288,  -4240,  -2584, -1200,
     20456,  12896,   8968,  6288,
      4240,   2584,   1200,     0
};
const int AudioG722::qm2[4] =
{
    -7408,  -1616,   7408,   1616
};
const int AudioG722::qmf_coeffs[12] =
{
       3,  -11,   12,   32, -210,  951, 3876, -805,  362, -156,   53,  -11,
};
const int AudioG722::ihn[3] = {0, 1, 0};
const int AudioG722::ihp[3] = {0, 3, 2};
const int AudioG722::wh[3] = {0, -214, 798};
const int AudioG722::rh2[4] = {2, 1, 2, 1};


AudioG722::g722_encode_state::g722_encode_state()
{
    init(64000, 0);
}

void AudioG722::g722_encode_state::init(int rate, int options)
{
    itu_test_mode = 0;
    std::fill(x, x+24, 0);
    in_buffer = 0;
    in_bits = 0;
    out_buffer = 0;
    out_bits = 0;

    char *pBand = (char *) band;
    std::fill(pBand, pBand + sizeof(band), 0);

    if (rate == 48000) {
        bits_per_sample = 6;
    } else if (rate == 56000) {
        bits_per_sample = 7;
    } else {
        bits_per_sample = 8;
    }


    if ((options & G722_SAMPLE_RATE_8000)) {
        eight_k = 1;
    } else {
        eight_k = 0;
    }

    if ((options & G722_PACKED)  &&  bits_per_sample != 8) {
        packed = 1;
    } else {
        packed = 0;
    }

    band[0].det = 32;
    band[1].det = 8;
}

AudioG722::AudioG722()
{}

AudioG722::~AudioG722()
{}

void AudioG722::init(int rate, int options)
{
    state.init(rate, options);
}

void AudioG722::block4(int band, int d)
{
    int wd1;
    int wd2;
    int wd3;
    int i;

    /* Block 4, RECONS */
    state.band[band].d[0] = d;
    state.band[band].r[0] = saturate(state.band[band].s + d);

    /* Block 4, PARREC */
    state.band[band].p[0] = saturate(state.band[band].sz + d);

    /* Block 4, UPPOL2 */
    for (i = 0;  i < 3;  i++) {
        state.band[band].sg[i] = state.band[band].p[i] >> 15;
    }

    wd1 = saturate(state.band[band].a[1] << 2);
    wd2 = (state.band[band].sg[0] == state.band[band].sg[1])  ?  -wd1  :  wd1;

    if (wd2 > 32767) {
        wd2 = 32767;
    }

    wd3 = (wd2 >> 7) + ((state.band[band].sg[0] == state.band[band].sg[2])  ?  128  :  -128);
    wd3 += (state.band[band].a[2]*32512) >> 15;

    if (wd3 > 12288) {
        wd3 = 12288;
    } else if (wd3 < -12288) {
        wd3 = -12288;
    }

    state.band[band].ap[2] = wd3;

    /* Block 4, UPPOL1 */
    state.band[band].sg[0] = state.band[band].p[0] >> 15;
    state.band[band].sg[1] = state.band[band].p[1] >> 15;
    wd1 = (state.band[band].sg[0] == state.band[band].sg[1])  ?  192  :  -192;
    wd2 = (state.band[band].a[1]*32640) >> 15;

    state.band[band].ap[1] = saturate(wd1 + wd2);
    wd3 = saturate(15360 - state.band[band].ap[2]);

    if (state.band[band].ap[1] > wd3) {
        state.band[band].ap[1] = wd3;
    } else if (state.band[band].ap[1] < -wd3) {
        state.band[band].ap[1] = -wd3;
    }

    /* Block 4, UPZERO */
    wd1 = (d == 0)  ?  0  :  128;
    state.band[band].sg[0] = d >> 15;

    for (i = 1;  i < 7;  i++)
    {
        state.band[band].sg[i] = state.band[band].d[i] >> 15;
        wd2 = (state.band[band].sg[i] == state.band[band].sg[0])  ?  wd1  :  -wd1;
        wd3 = (state.band[band].b[i]*32640) >> 15;
        state.band[band].bp[i] = saturate(wd2 + wd3);
    }

    /* Block 4, DELAYA */
    for (i = 6;  i > 0;  i--)
    {
        state.band[band].d[i] = state.band[band].d[i - 1];
        state.band[band].b[i] = state.band[band].bp[i];
    }

    for (i = 2;  i > 0;  i--)
    {
        state.band[band].r[i] = state.band[band].r[i - 1];
        state.band[band].p[i] = state.band[band].p[i - 1];
        state.band[band].a[i] = state.band[band].ap[i];
    }

    /* Block 4, FILTEP */
    wd1 = saturate(state.band[band].r[1] + state.band[band].r[1]);
    wd1 = (state.band[band].a[1]*wd1) >> 15;
    wd2 = saturate(state.band[band].r[2] + state.band[band].r[2]);
    wd2 = (state.band[band].a[2]*wd2) >> 15;
    state.band[band].sp = saturate(wd1 + wd2);

    /* Block 4, FILTEZ */
    state.band[band].sz = 0;

    for (i = 6;  i > 0;  i--)
    {
        wd1 = saturate(state.band[band].d[i] + state.band[band].d[i]);
        state.band[band].sz += (state.band[band].b[i]*wd1) >> 15;
    }

    state.band[band].sz = saturate(state.band[band].sz);

    /* Block 4, PREDIC */
    state.band[band].s = saturate(state.band[band].sp + state.band[band].sz);
}

int AudioG722::encode(uint8_t g722_data[], const int16_t amp[], int len)
{
    int dlow;
    int dhigh;
    int el;
    int wd;
    int wd1;
    int ril;
    int wd2;
    int il4;
    int ih2;
    int wd3;
    int eh;
    int mih;
    int i;
    int j;
    /* Low and high band PCM from the QMF */
    int xlow;
    int xhigh;
    int g722_bytes;
    /* Even and odd tap accumulators */
    int sumeven;
    int sumodd;
    int ihigh;
    int ilow;
    int code;

    g722_bytes = 0;
    xhigh = 0;

    for (j = 0;  j < len;  )
    {
        if (state.itu_test_mode)
        {
            xhigh = amp[j++] >> 1;
            xlow = xhigh;
        }
        else
        {
            if (state.eight_k)
            {
                xlow = amp[j++] >> 1;
            }
            else
            {
                /* Apply the transmit QMF */
                /* Shuffle the buffer down */
                for (i = 0;  i < 22;  i++) {
                    state.x[i] = state.x[i + 2];
                }

                state.x[22] = amp[j++];
                state.x[23] = amp[j++];

                /* Discard every other QMF output */
                sumeven = 0;
                sumodd = 0;

                for (i = 0;  i < 12;  i++)
                {
                    sumodd += state.x[2*i]*qmf_coeffs[i];
                    sumeven += state.x[2*i + 1]*qmf_coeffs[11 - i];
                }

                xlow = (sumeven + sumodd) >> 14;
                xhigh = (sumeven - sumodd) >> 14;
            }
        }
        /* Block 1L, SUBTRA */
        el = saturate(xlow - state.band[0].s);

        /* Block 1L, QUANTL */
        wd = (el >= 0)  ?  el  :  -(el + 1);

        for (i = 1;  i < 30;  i++)
        {
            wd1 = (q6[i]*state.band[0].det) >> 12;

            if (wd < wd1) {
                break;
            }
        }

        ilow = (el < 0)  ?  iln[i]  :  ilp[i];

        /* Block 2L, INVQAL */
        ril = ilow >> 2;
        wd2 = qm4[ril];
        dlow = (state.band[0].det*wd2) >> 15;

        /* Block 3L, LOGSCL */
        il4 = rl42[ril];
        wd = (state.band[0].nb*127) >> 7;
        state.band[0].nb = wd + wl[il4];

        if (state.band[0].nb < 0) {
            state.band[0].nb = 0;
        } else if (state.band[0].nb > 18432) {
            state.band[0].nb = 18432;
        }

        /* Block 3L, SCALEL */
        wd1 = (state.band[0].nb >> 6) & 31;
        wd2 = 8 - (state.band[0].nb >> 11);
        wd3 = (wd2 < 0)  ?  (ilb[wd1] << -wd2)  :  (ilb[wd1] >> wd2);
        state.band[0].det = wd3 << 2;

        block4(0, dlow);

        if (state.eight_k)
        {
            /* Just leave the high bits as zero */
            code = (0xC0 | ilow) >> (8 - state.bits_per_sample);
        }
        else
        {
            /* Block 1H, SUBTRA */
            eh = saturate(xhigh - state.band[1].s);

            /* Block 1H, QUANTH */
            wd = (eh >= 0)  ?  eh  :  -(eh + 1);
            wd1 = (564*state.band[1].det) >> 12;
            mih = (wd >= wd1)  ?  2  :  1;
            ihigh = (eh < 0)  ?  ihn[mih]  :  ihp[mih];

            /* Block 2H, INVQAH */
            wd2 = qm2[ihigh];
            dhigh = (state.band[1].det*wd2) >> 15;

            /* Block 3H, LOGSCH */
            ih2 = rh2[ihigh];
            wd = (state.band[1].nb*127) >> 7;
            state.band[1].nb = wd + wh[ih2];

            if (state.band[1].nb < 0) {
                state.band[1].nb = 0;
            } else if (state.band[1].nb > 22528) {
                state.band[1].nb = 22528;
            }

            /* Block 3H, SCALEH */
            wd1 = (state.band[1].nb >> 6) & 31;
            wd2 = 10 - (state.band[1].nb >> 11);
            wd3 = (wd2 < 0)  ?  (ilb[wd1] << -wd2)  :  (ilb[wd1] >> wd2);
            state.band[1].det = wd3 << 2;

            block4(1, dhigh);
            code = ((ihigh << 6) | ilow) >> (8 - state.bits_per_sample);
        }

        if (state.packed)
        {
            /* Pack the code bits */
            state.out_buffer |= (code << state.out_bits);
            state.out_bits += state.bits_per_sample;

            if (state.out_bits >= 8)
            {
                g722_data[g722_bytes++] = (uint8_t) (state.out_buffer & 0xFF);
                state.out_bits -= 8;
                state.out_buffer >>= 8;
            }
        }
        else
        {
            g722_data[g722_bytes++] = (uint8_t) code;
        }
    }

    return g722_bytes;
}
