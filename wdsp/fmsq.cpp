/*  fmsq.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2016 Warren Pratt, NR0V
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

#include "comm.hpp"
#include "firmin.hpp"
#include "eq.hpp"
#include "fmsq.hpp"
#include "RXA.hpp"

namespace WDSP {

void FMSQ::calc_fmsq (FMSQ *a)
{
    double delta, theta;
    float* impulse;
    int i;
    // noise filter
    a->noise = new float[2 * a->size * 2]; // (float *)malloc0(2 * a->size * sizeof(complex));
    a->F[0] = 0.0;
    a->F[1] = a->fc;
    a->F[2] = *a->pllpole;
    a->F[3] = 20000.0;
    a->G[0] = 0.0;
    a->G[1] = 0.0;
    a->G[2] = 3.0;
    a->G[3] = +20.0 * log10(20000.0 / *a->pllpole);
    impulse = EQP::eq_impulse (a->nc, 3, a->F, a->G, a->rate, 1.0 / (2.0 * a->size), 0, 0);
    a->p = FIRCORE::create_fircore (a->size, a->trigger, a->noise, a->nc, a->mp, impulse);
    delete[]  (impulse);
    // noise averaging
    a->avm = exp(-1.0 / (a->rate * a->avtau));
    a->onem_avm = 1.0 - a->avm;
    a->avnoise = 100.0;
    a->longavm = exp(-1.0 / (a->rate * a->longtau));
    a->onem_longavm = 1.0 - a->longavm;
    a->longnoise = 1.0;
    // level change
    a->ntup   = (int)(a->tup   * a->rate);
    a->ntdown = (int)(a->tdown * a->rate);
    a->cup   = new float[a->ntup + 1]; // (float *)malloc0 ((a->ntup   + 1) * sizeof(float));
    a->cdown = new float[a->ntdown + 1]; //(float *)malloc0 ((a->ntdown + 1) * sizeof(float));
    delta = PI / (float)a->ntup;
    theta = 0.0;
    for (i = 0; i <= a->ntup; i++)
    {
        a->cup[i] = 0.5 * (1.0 - cos(theta));
        theta += delta;
    }
    delta = PI / (float)a->ntdown;
    theta = 0.0;
    for (i = 0; i <= a->ntdown; i++)
    {
        a->cdown[i] = 0.5 * (1 + cos(theta));
        theta += delta;
    }
    // control
    a->state = 0;
    a->ready = 0;
    a->ramp = 0.0;
    a->rstep = 1.0 / a->rate;
}

void FMSQ::decalc_fmsq (FMSQ *a)
{
    delete[] (a->cdown);
    delete[] (a->cup);
    FIRCORE::destroy_fircore (a->p);
    delete[] (a->noise);
}

FMSQ* FMSQ::create_fmsq (
    int run,
    int size,
    float* insig,
    float* outsig,
    float* trigger,
    int rate,
    double fc,
    double* pllpole,
    double tdelay,
    double avtau,
    double longtau,
    double tup,
    double tdown,
    double tail_thresh,
    double unmute_thresh,
    double min_tail,
    double max_tail,
    int nc,
    int mp
)
{
    FMSQ *a = new FMSQ;
    a->run = run;
    a->size = size;
    a->insig = insig;
    a->outsig = outsig;
    a->trigger = trigger;
    a->rate = (float)rate;
    a->fc = fc;
    a->pllpole = pllpole;
    a->tdelay = tdelay;
    a->avtau = avtau;
    a->longtau = longtau;
    a->tup = tup;
    a->tdown = tdown;
    a->tail_thresh = tail_thresh;
    a->unmute_thresh = unmute_thresh;
    a->min_tail = min_tail;
    a->max_tail = max_tail;
    a->nc = nc;
    a->mp = mp;
    calc_fmsq (a);
    return a;
}

void FMSQ::destroy_fmsq (FMSQ *a)
{
    decalc_fmsq (a);
    delete  (a);
}

void FMSQ::flush_fmsq (FMSQ *a)
{
    FIRCORE::flush_fircore (a->p);
    a->avnoise = 100.0;
    a->longnoise = 1.0;
    a->state = 0;
    a->ready = 0;
    a->ramp = 0.0;
}

enum _fmsqstate
{
    MUTED,
    INCREASE,
    UNMUTED,
    TAIL,
    DECREASE
};

void FMSQ::xfmsq (FMSQ *a)
{
    if (a->run)
    {
        int i;
        float noise, lnlimit;
        FIRCORE::xfircore (a->p);
        for (i = 0; i < a->size; i++)
        {
            noise = sqrt(a->noise[2 * i + 0] * a->noise[2 * i + 0] + a->noise[2 * i + 1] * a->noise[2 * i + 1]);
            a->avnoise = a->avm * a->avnoise + a->onem_avm * noise;
            a->longnoise = a->longavm * a->longnoise + a->onem_longavm * noise;
            if (!a->ready) a->ramp += a->rstep;
            if (a->ramp >= a->tdelay) a->ready = 1;

            switch (a->state)
            {
            case MUTED:
                if (a->avnoise < a->unmute_thresh && a->ready)
                {
                    a->state = INCREASE;
                    a->count = a->ntup;
                }
                a->outsig[2 * i + 0] = 0.0;
                a->outsig[2 * i + 1] = 0.0;
                break;
            case INCREASE:
                a->outsig[2 * i + 0] = a->insig[2 * i + 0] * a->cup[a->ntup - a->count];
                a->outsig[2 * i + 1] = a->insig[2 * i + 1] * a->cup[a->ntup - a->count];
                if (a->count-- == 0)
                    a->state = UNMUTED;
                break;
            case UNMUTED:
                if (a->avnoise > a->tail_thresh)
                {
                    a->state = TAIL;
                    if ((lnlimit = a->longnoise) > 1.0) lnlimit = 1.0;
                    a->count = (int)((a->min_tail + (a->max_tail - a->min_tail) * lnlimit) * a->rate);
                }
                a->outsig[2 * i + 0] = a->insig[2 * i + 0];
                a->outsig[2 * i + 1] = a->insig[2 * i + 1];
                break;
            case TAIL:
                a->outsig[2 * i + 0] = a->insig[2 * i + 0];
                a->outsig[2 * i + 1] = a->insig[2 * i + 1];
                if (a->avnoise < a->unmute_thresh)
                    a->state = UNMUTED;
                else if (a->count-- == 0)
                {
                    a->state = DECREASE;
                    a->count = a->ntdown;
                }
                break;
            case DECREASE:
                a->outsig[2 * i + 0] = a->insig[2 * i + 0] * a->cdown[a->ntdown - a->count];
                a->outsig[2 * i + 1] = a->insig[2 * i + 1] * a->cdown[a->ntdown - a->count];
                if (a->count-- == 0)
                    a->state = MUTED;
                break;
            }
        }
    }
    else if (a->insig != a->outsig)
        memcpy (a->outsig, a->insig, a->size * sizeof (wcomplex));
}

void FMSQ::setBuffers_fmsq (FMSQ *a, float* in, float* out, float* trig)
{
    a->insig = in;
    a->outsig = out;
    a->trigger = trig;
    FIRCORE::setBuffers_fircore (a->p, a->trigger, a->noise);
}

void FMSQ::setSamplerate_fmsq (FMSQ *a, int rate)
{
    decalc_fmsq (a);
    a->rate = rate;
    calc_fmsq (a);
}

void FMSQ::setSize_fmsq (FMSQ *a, int size)
{
    decalc_fmsq (a);
    a->size = size;
    calc_fmsq (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void FMSQ::SetFMSQRun (RXA& rxa, int run)
{
    rxa.csDSP.lock();
    rxa.fmsq.p->run = run;
    rxa.csDSP.unlock();
}

void FMSQ::SetFMSQThreshold (RXA& rxa, double threshold)
{
    rxa.csDSP.lock();
    rxa.fmsq.p->tail_thresh = threshold;
    rxa.fmsq.p->unmute_thresh = 0.9 * threshold;
    rxa.csDSP.unlock();
}

void FMSQ::SetFMSQNC (RXA& rxa, int nc)
{
    FMSQ *a;
    float* impulse;
    rxa.csDSP.lock();
    a = rxa.fmsq.p;
    if (a->nc != nc)
    {
        a->nc = nc;
        impulse = EQP::eq_impulse (a->nc, 3, a->F, a->G, a->rate, 1.0 / (2.0 * a->size), 0, 0);
        FIRCORE::setNc_fircore (a->p, a->nc, impulse);
        delete[]  (impulse);
    }
    rxa.csDSP.unlock();
}

void FMSQ::SetFMSQMP (RXA& rxa, int mp)
{
    FMSQ *a;
    a = rxa.fmsq.p;
    if (a->mp != mp)
    {
        a->mp = mp;
        FIRCORE::setMp_fircore (a->p, a->mp);
    }
}

} // namespace WDSP
