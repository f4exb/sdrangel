/*  amsq.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013 Warren Pratt, NR0V
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
#include "amsq.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

void AMSQ::compute_slews(AMSQ *a)
{
    int i;
    double delta, theta;
    delta = PI / (double)a->ntup;
    theta = 0.0;
    for (i = 0; i <= a->ntup; i++)
    {
        a->cup[i] = a->muted_gain + (1.0 - a->muted_gain) * 0.5 * (1.0 - cos (theta));
        theta += delta;
    }
    delta = PI / (double)a->ntdown;
    theta = 0.0;
    for (i = 0; i <= a->ntdown; i++)
    {
        a->cdown[i] = a->muted_gain + (1.0 - a->muted_gain) * 0.5 * (1.0 + cos (theta));
        theta += delta;
    }
}

void AMSQ::calc_amsq(AMSQ *a)
{
    // signal averaging
    a->trigsig = new float[a->size * 2];
    a->avm = exp(-1.0 / (a->rate * a->avtau));
    a->onem_avm = 1.0 - a->avm;
    a->avsig = 0.0;
    // level change
    a->ntup = (int)(a->tup * a->rate);
    a->ntdown = (int)(a->tdown * a->rate);
    a->cup = new float[(a->ntup + 1) * 2]; // (float *)malloc0((a->ntup + 1) * sizeof(float));
    a->cdown = new float[(a->ntdown + 1) * 2]; // (float *)malloc0((a->ntdown + 1) * sizeof(float));
    compute_slews(a);
    // control
    a->state = 0;
}

void AMSQ::decalc_amsq (AMSQ *a)
{
    delete[] a->cdown;
    delete[] a->cup;
    delete[] a->trigsig;
}

AMSQ* AMSQ::create_amsq (
    int run,
    int size,
    float* in,
    float* out,
    float* trigger,
    int rate,
    double avtau,
    double tup,
    double tdown,
    double tail_thresh,
    double unmute_thresh,
    double min_tail,
    double max_tail,
    double muted_gain
)
{
    AMSQ *a = new AMSQ;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = (float)rate;
    a->muted_gain = muted_gain;
    a->trigger = trigger;
    a->avtau = avtau;
    a->tup = tup;
    a->tdown = tdown;
    a->tail_thresh = tail_thresh;
    a->unmute_thresh = unmute_thresh;
    a->min_tail = min_tail;
    a->max_tail = max_tail;
    calc_amsq (a);
    return a;
}

void AMSQ::destroy_amsq (AMSQ *a)
{
    decalc_amsq (a);
    delete a;
}

void AMSQ::flush_amsq (AMSQ*a)
{
    std::fill(a->trigsig, a->trigsig + a->size * 2, 0);
    a->avsig = 0.0;
    a->state = 0;
}

enum _amsqstate
{
    MUTED,
    INCREASE,
    UNMUTED,
    TAIL,
    DECREASE
};

void AMSQ::xamsq (AMSQ *a)
{
    if (a->run)
    {
        int i;
        double sig, siglimit;
        for (i = 0; i < a->size; i++)
        {
            sig = sqrt (a->trigsig[2 * i + 0] * a->trigsig[2 * i + 0] + a->trigsig[2 * i + 1] * a->trigsig[2 * i + 1]);
            a->avsig = a->avm * a->avsig + a->onem_avm * sig;
            switch (a->state)
            {
            case MUTED:
                if (a->avsig > a->unmute_thresh)
                {
                    a->state = INCREASE;
                    a->count = a->ntup;
                }
                a->out[2 * i + 0] = a->muted_gain * a->in[2 * i + 0];
                a->out[2 * i + 1] = a->muted_gain * a->in[2 * i + 1];
                break;
            case INCREASE:
                a->out[2 * i + 0] = a->in[2 * i + 0] * a->cup[a->ntup - a->count];
                a->out[2 * i + 1] = a->in[2 * i + 1] * a->cup[a->ntup - a->count];
                if (a->count-- == 0)
                    a->state = UNMUTED;
                break;
            case UNMUTED:
                if (a->avsig < a->tail_thresh)
                {
                    a->state = TAIL;
                    if ((siglimit = a->avsig) > 1.0) siglimit = 1.0;
                    a->count = (int)((a->min_tail + (a->max_tail - a->min_tail) * (1.0 - siglimit)) * a->rate);
                }
                a->out[2 * i + 0] = a->in[2 * i + 0];
                a->out[2 * i + 1] = a->in[2 * i + 1];
                break;
            case TAIL:
                a->out[2 * i + 0] = a->in[2 * i + 0];
                a->out[2 * i + 1] = a->in[2 * i + 1];
                if (a->avsig > a->unmute_thresh)
                    a->state = UNMUTED;
                else if (a->count-- == 0)
                {
                    a->state = DECREASE;
                    a->count = a->ntdown;
                }
                break;
            case DECREASE:
                a->out[2 * i + 0] = a->in[2 * i + 0] * a->cdown[a->ntdown - a->count];
                a->out[2 * i + 1] = a->in[2 * i + 1] * a->cdown[a->ntdown - a->count];
                if (a->count-- == 0)
                    a->state = MUTED;
                break;
            }
        }
    }
    else if (a->in != a->out)
        std::copy( a->in,  a->in + a->size * 2, a->out);
}

void AMSQ::xamsqcap (AMSQ *a)
{
    std::copy(a->trigger, a->trigger + a->size * 2, a->trigsig);
}

void AMSQ::setBuffers_amsq (AMSQ *a, float* in, float* out, float* trigger)
{
    a->in = in;
    a->out = out;
    a->trigger = trigger;
}

void AMSQ::setSamplerate_amsq (AMSQ *a, int rate)
{
    decalc_amsq (a);
    a->rate = rate;
    calc_amsq (a);
}

void AMSQ::setSize_amsq (AMSQ *a, int size)
{
    decalc_amsq (a);
    a->size = size;
    calc_amsq (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void AMSQ::SetAMSQRun (RXA& rxa, int run)
{
    rxa.amsq.p->run = run;
}

void AMSQ::SetAMSQThreshold (RXA& rxa, double threshold)
{
    double thresh = pow (10.0, threshold / 20.0);
    rxa.amsq.p->tail_thresh = 0.9 * thresh;
    rxa.amsq.p->unmute_thresh =  thresh;
}

void AMSQ::SetAMSQMaxTail (RXA& rxa, double tail)
{
    AMSQ *a;
    a = rxa.amsq.p;
    if (tail < a->min_tail) tail = a->min_tail;
    a->max_tail = tail;
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void AMSQ::SetAMSQRun (TXA& txa, int run)
{
    txa.amsq.p->run = run;
}

void AMSQ::SetAMSQMutedGain (TXA& txa, double dBlevel)
{   // dBlevel is negative
    AMSQ *a;
    a = txa.amsq.p;
    a->muted_gain = pow (10.0, dBlevel / 20.0);
    compute_slews(a);
}

void AMSQ::SetAMSQThreshold (TXA& txa, double threshold)
{
    double thresh = pow (10.0, threshold / 20.0);
    txa.amsq.p->tail_thresh = 0.9 * thresh;
    txa.amsq.p->unmute_thresh =  thresh;
}

} // namespace WDSP
