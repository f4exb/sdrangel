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

#include "comm.hpp"
#include "amd.hpp"
#include "anf.hpp"
#include "emnr.hpp"
#include "anr.hpp"
#include "snba.hpp"
#include "RXA.hpp"

namespace WDSP {

AMD* AMD::create_amd
(
    int run,
    int buff_size,
    float *in_buff,
    float *out_buff,
    int mode,
    int levelfade,
    int sbmode,
    int sample_rate,
    double fmin,
    double fmax,
    double zeta,
    double omegaN,
    double tauR,
    double tauI
)
{
    AMD *a = new AMD();
    a->run = run;
    a->buff_size = buff_size;
    a->in_buff = in_buff;
    a->out_buff = out_buff;
    a->mode = mode;
    a->levelfade = levelfade;
    a->sbmode = sbmode;
    a->sample_rate = (float)sample_rate;
    a->fmin = fmin;
    a->fmax = fmax;
    a->zeta = zeta;
    a->omegaN = omegaN;
    a->tauR = tauR;
    a->tauI = tauI;

    init_amd(a);
    return a;
}

void AMD::destroy_amd(AMD *a)
{
    delete a;
}

void AMD::init_amd(AMD *a)
{
    //pll
    a->omega_min = 2 * M_PI * a->fmin / a->sample_rate;
    a->omega_max = 2 * M_PI * a->fmax / a->sample_rate;
    a->g1 = 1.0 - std::exp(-2.0 * a->omegaN * a->zeta / a->sample_rate);
    a->g2 = -a->g1 + 2.0 * (1 - exp(-a->omegaN * a->zeta / a->sample_rate) * cos(a->omegaN / a->sample_rate * sqrt(1.0 - a->zeta * a->zeta)));
    a->phs = 0.0;
    a->fil_out = 0.0;
    a->omega = 0.0;

    //fade leveler
    a->dc = 0.0;
    a->dc_insert = 0.0;
    a->mtauR = exp(-1.0 / (a->sample_rate * a->tauR));
    a->onem_mtauR = 1.0 - a->mtauR;
    a->mtauI = exp(-1.0 / (a->sample_rate * a->tauI));
    a->onem_mtauI = 1.0 - a->mtauI;

    //sideband separation
    a->c0[0] = -0.328201924180698;
    a->c0[1] = -0.744171491539427;
    a->c0[2] = -0.923022915444215;
    a->c0[3] = -0.978490468768238;
    a->c0[4] = -0.994128272402075;
    a->c0[5] = -0.998458978159551;
    a->c0[6] = -0.999790306259206;

    a->c1[0] = -0.0991227952747244;
    a->c1[1] = -0.565619728761389;
    a->c1[2] = -0.857467122550052;
    a->c1[3] = -0.959123933111275;
    a->c1[4] = -0.988739372718090;
    a->c1[5] = -0.996959189310611;
    a->c1[6] = -0.999282492800792;
}

void AMD::flush_amd (AMD *a)
{
    a->dc = 0.0;
    a->dc_insert = 0.0;
}

void AMD::xamd (AMD *a)
{
    int i;
    double audio;
    double vco[2];
    double corr[2];
    double det;
    double del_out;
    double ai, bi, aq, bq;
    double ai_ps, bi_ps, aq_ps, bq_ps;
    int j, k;

    if (a->run)
    {
        switch (a->mode)
        {

            case 0:     //AM Demodulator
                {
                    for (i = 0; i < a->buff_size; i++)
                    {
                        double xr = a->in_buff[2 * i + 0];
                        double xi = a->in_buff[2 * i + 1];
                        audio = sqrt(xr*xr + xi*xi);

                        if (a->levelfade)
                        {
                            a->dc = a->mtauR * a->dc + a->onem_mtauR * audio;
                            a->dc_insert = a->mtauI * a->dc_insert + a->onem_mtauI * audio;
                            audio += a->dc_insert - a->dc;
                        }

                        a->out_buff[2 * i + 0] = audio;
                        a->out_buff[2 * i + 1] = audio;
                    }

                    break;
                }

            case 1:     //Synchronous AM Demodulator with Sideband Separation
                {
                    for (i = 0; i < a->buff_size; i++)
                    {
                        vco[0] = cos(a->phs);
                        vco[1] = sin(a->phs);

                        ai = a->in_buff[2 * i + 0] * vco[0];
                        bi = a->in_buff[2 * i + 0] * vco[1];
                        aq = a->in_buff[2 * i + 1] * vco[0];
                        bq = a->in_buff[2 * i + 1] * vco[1];

                        if (a->sbmode != 0)
                        {
                            a->a[0] = a->dsI;
                            a->b[0] = bi;
                            a->c[0] = a->dsQ;
                            a->d[0] = aq;
                            a->dsI = ai;
                            a->dsQ = bq;

                            for (j = 0; j < STAGES; j++)
                            {
                                k = 3 * j;
                                a->a[k + 3] = a->c0[j] * (a->a[k] - a->a[k + 5]) + a->a[k + 2];
                                a->b[k + 3] = a->c1[j] * (a->b[k] - a->b[k + 5]) + a->b[k + 2];
                                a->c[k + 3] = a->c0[j] * (a->c[k] - a->c[k + 5]) + a->c[k + 2];
                                a->d[k + 3] = a->c1[j] * (a->d[k] - a->d[k + 5]) + a->d[k + 2];
                            }

                            ai_ps = a->a[OUT_IDX];
                            bi_ps = a->b[OUT_IDX];
                            bq_ps = a->c[OUT_IDX];
                            aq_ps = a->d[OUT_IDX];

                            for (j = OUT_IDX + 2; j > 0; j--)
                            {
                                a->a[j] = a->a[j - 1];
                                a->b[j] = a->b[j - 1];
                                a->c[j] = a->c[j - 1];
                                a->d[j] = a->d[j - 1];
                            }
                        }

                        corr[0] = +ai + bq;
                        corr[1] = -bi + aq;

                        switch(a->sbmode)
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
                        }

                        if (a->levelfade)
                        {
                            a->dc = a->mtauR * a->dc + a->onem_mtauR * audio;
                            a->dc_insert = a->mtauI * a->dc_insert + a->onem_mtauI * corr[0];
                            audio += a->dc_insert - a->dc;
                        }

                        a->out_buff[2 * i + 0] = audio;
                        a->out_buff[2 * i + 1] = audio;

                        if ((corr[0] == 0.0) && (corr[1] == 0.0))
                            corr[0] = 1.0;

                        det = atan2(corr[1], corr[0]);
                        del_out = a->fil_out;
                        a->omega += a->g2 * det;

                        if (a->omega < a->omega_min)
                            a->omega = a->omega_min;

                        if (a->omega > a->omega_max)
                            a->omega = a->omega_max;

                        a->fil_out = a->g1 * det + a->omega;
                        a->phs += del_out;

                        while (a->phs >= 2 * M_PI)
                            a->phs -= 2 * M_PI;

                        while (a->phs < 0.0)
                            a->phs += 2 * M_PI;
                    }

                    break;
                }
        }
    }
    else if (a->in_buff != a->out_buff)
    {
        std::copy (a->in_buff, a->in_buff + a->buff_size * 2, a->out_buff);
    }
}

void AMD::setBuffers_amd (AMD *a, float* in, float* out)
{
    a->in_buff = in;
    a->out_buff = out;
}

void AMD::setSamplerate_amd (AMD *a, int rate)
{
    a->sample_rate = rate;
    init_amd(a);
}

void AMD::setSize_amd (AMD *a, int size)
{
    a->buff_size = size;
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void AMD::SetAMDRun(RXA& rxa, int run)
{
    AMD *a = rxa.amd.p;

    if (a->run != run)
    {
        RXA::bp1Check (
            rxa,
            run,
            rxa.snba.p->run,
            rxa.emnr.p->run,
            rxa.anf.p->run,
            rxa.anr.p->run
        );

        a->run = run;
        RXA::bp1Set (rxa);
    }
}

void AMD::SetAMDSBMode(RXA& rxa, int sbmode)
{
    rxa.amd.p->sbmode = sbmode;
}

void AMD::SetAMDFadeLevel(RXA& rxa, int levelfade)
{
    rxa.amd.p->levelfade = levelfade;
}

} // namesoace WDSP
