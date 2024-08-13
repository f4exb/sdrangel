/*  amd.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2012, 2013 Warren Pratt, NR0V
Copyright (C) 2024 Edouard Griffiths, F4EXB Adapted to SDRangel

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

warren@wpratt.com

*/

#include <cmath>
#include <array>

#include "comm.hpp"
#include "amd.hpp"
#include "anf.hpp"
#include "emnr.hpp"
#include "anr.hpp"
#include "snba.hpp"

namespace WDSP {

AMD::AMD
(
    int _run,
    int _buff_size,
    float *_in_buff,
    float *_out_buff,
    int _mode,
    int _levelfade,
    int _sbmode,
    int _sample_rate,
    double _fmin,
    double _fmax,
    double _zeta,
    double _omegaN,
    double _tauR,
    double _tauI
) :
    run(_run),
    buff_size(_buff_size),
    in_buff(_in_buff),
    out_buff(_out_buff),
    mode(_mode),
    sample_rate((double) _sample_rate),
    fmin(_fmin),
    fmax(_fmax),
    zeta(_zeta),
    omegaN(_omegaN),
    tauR(_tauR),
    tauI(_tauI),
    sbmode(_sbmode),
    levelfade(_levelfade)
{
    init();
}

void AMD::init()
{
    //pll
    omega_min = 2 * M_PI * fmin / sample_rate;
    omega_max = 2 * M_PI * fmax / sample_rate;
    g1 = 1.0 - std::exp(-2.0 * omegaN * zeta / sample_rate);
    g2 = -g1 + 2.0 * (1 - exp(-omegaN * zeta / sample_rate) * cos(omegaN / sample_rate * sqrt(1.0 - zeta * zeta)));
    phs = 0.0;
    fil_out = 0.0;
    omega = 0.0;

    //fade leveler
    dc = 0.0;
    dc_insert = 0.0;
    mtauR = exp(-1.0 / (sample_rate * tauR));
    onem_mtauR = 1.0 - mtauR;
    mtauI = exp(-1.0 / (sample_rate * tauI));
    onem_mtauI = 1.0 - mtauI;

    //sideband separation
    c0[0] = -0.328201924180698;
    c0[1] = -0.744171491539427;
    c0[2] = -0.923022915444215;
    c0[3] = -0.978490468768238;
    c0[4] = -0.994128272402075;
    c0[5] = -0.998458978159551;
    c0[6] = -0.999790306259206;

    c1[0] = -0.0991227952747244;
    c1[1] = -0.565619728761389;
    c1[2] = -0.857467122550052;
    c1[3] = -0.959123933111275;
    c1[4] = -0.988739372718090;
    c1[5] = -0.996959189310611;
    c1[6] = -0.999282492800792;
}

void AMD::flush()
{
    dc = 0.0;
    dc_insert = 0.0;
}

void AMD::execute()
{
    double audio;
    std::array<double, 2> vco;
    std::array<double, 2> corr;
    double det;
    double del_out;
    double ai;
    double bi;
    double aq;
    double bq;
    double ai_ps;
    double bi_ps;
    double aq_ps;
    double bq_ps;

    if (run)
    {
        switch (mode)
        {

            case 0:     //AM Demodulator
                {
                    for (int i = 0; i < buff_size; i++)
                    {
                        double xr = in_buff[2 * i + 0];
                        double xi = in_buff[2 * i + 1];
                        audio = sqrt(xr*xr + xi*xi);

                        if (levelfade)
                        {
                            dc = mtauR * dc + onem_mtauR * audio;
                            dc_insert = mtauI * dc_insert + onem_mtauI * audio;
                            audio += dc_insert - dc;
                        }

                        out_buff[2 * i + 0] = (float) audio;
                        out_buff[2 * i + 1] = (float) audio;
                    }

                    break;
                }

            case 1:     //Synchronous AM Demodulator with Sideband Separation
                {
                    for (int i = 0; i < buff_size; i++)
                    {
                        vco[0] = cos(phs);
                        vco[1] = sin(phs);

                        ai = in_buff[2 * i + 0] * vco[0];
                        bi = in_buff[2 * i + 0] * vco[1];
                        aq = in_buff[2 * i + 1] * vco[0];
                        bq = in_buff[2 * i + 1] * vco[1];

                        if (sbmode != 0)
                        {
                            a[0] = dsI;
                            b[0] = bi;
                            c[0] = dsQ;
                            d[0] = aq;
                            dsI = ai;
                            dsQ = bq;

                            for (int j = 0; j < STAGES; j++)
                            {
                                int k = 3 * j;
                                a[k + 3] = c0[j] * (a[k] - a[k + 5]) + a[k + 2];
                                b[k + 3] = c1[j] * (b[k] - b[k + 5]) + b[k + 2];
                                c[k + 3] = c0[j] * (c[k] - c[k + 5]) + c[k + 2];
                                d[k + 3] = c1[j] * (d[k] - d[k + 5]) + d[k + 2];
                            }

                            ai_ps = a[OUT_IDX];
                            bi_ps = b[OUT_IDX];
                            bq_ps = c[OUT_IDX];
                            aq_ps = d[OUT_IDX];

                            for (int j = OUT_IDX + 2; j > 0; j--)
                            {
                                a[j] = a[j - 1];
                                b[j] = b[j - 1];
                                c[j] = c[j - 1];
                                d[j] = d[j - 1];
                            }
                        }

                        corr[0] = +ai + bq;
                        corr[1] = -bi + aq;

                        switch(sbmode)
                        {
                        case 0: //both sidebands
                            {
                                audio = corr[0];
                                break;
                            }
                        case 1: //LSB
                            {
                                audio = (ai_ps - bi_ps) + (aq_ps + bq_ps);
                                break;
                            }
                        case 2: //USB
                            {
                                audio = (ai_ps + bi_ps) - (aq_ps - bq_ps);
                                break;
                            }
                        default:
                            break;
                        }

                        if (levelfade)
                        {
                            dc = mtauR * dc + onem_mtauR * audio;
                            dc_insert = mtauI * dc_insert + onem_mtauI * corr[0];
                            audio += dc_insert - dc;
                        }

                        out_buff[2 * i + 0] = (float) audio;
                        out_buff[2 * i + 1] = (float) audio;

                        if ((corr[0] == 0.0) && (corr[1] == 0.0))
                            corr[0] = 1.0;

                        det = atan2(corr[1], corr[0]);
                        del_out = fil_out;
                        omega += g2 * det;

                        if (omega < omega_min)
                            omega = omega_min;

                        if (omega > omega_max)
                            omega = omega_max;

                        fil_out = g1 * det + omega;
                        phs += del_out;

                        while (phs >= 2 * M_PI)
                            phs -= 2 * M_PI;

                        while (phs < 0.0)
                            phs += 2 * M_PI;
                    }

                    break;
                }
            default:
                break;
        }
    }
    else if (in_buff != out_buff)
    {
        std::copy (in_buff, in_buff + buff_size * 2, out_buff);
    }
}

void AMD::setBuffers(float* in, float* out)
{
    in_buff = in;
    out_buff = out;
}

void AMD::setSamplerate(int rate)
{
    sample_rate = rate;
    init();
}

void AMD::setSize(int size)
{
    buff_size = size;
}

/********************************************************************************************************
*                                                                                                       *
*                                           Public Properties                                           *
*                                                                                                       *
********************************************************************************************************/

void AMD::setSBMode(int _sbmode)
{
    sbmode = _sbmode;
}

void AMD::setFadeLevel(int _levelfade)
{
    levelfade = _levelfade;
}

} // namesoace WDSP
