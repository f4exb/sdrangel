/*  iqc.c

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

#include <thread>
#include <chrono>

#include "comm.hpp"
#include "iqc.hpp"
#include "TXA.hpp"

namespace WDSP {

void IQC::size_iqc (IQC *a)
{
    int i;
    a->t = new float[a->ints + 1]; // (float *) malloc0 ((a->ints + 1) * sizeof(float));
    for (i = 0; i <= a->ints; i++)
        a->t[i] = (float)i / (float)a->ints;
    for (i = 0; i < 2; i++)
    {
        a->cm[i] = new float[a->ints * 4]; // (float *) malloc0 (a->ints * 4 * sizeof(float));
        a->cc[i] = new float[a->ints * 4]; // (float *) malloc0 (a->ints * 4 * sizeof(float));
        a->cs[i] = new float[a->ints * 4]; // (float *) malloc0 (a->ints * 4 * sizeof(float));
    }
    a->dog.cpi = new int[a->ints]; // (int *) malloc0 (a->ints * sizeof (int));
    a->dog.count = 0;
    a->dog.full_ints = 0;
}

void IQC::desize_iqc (IQC *a)
{
    int i;
    delete[] (a->dog.cpi);
    for (i = 0; i < 2; i++)
    {
        delete[] (a->cm[i]);
        delete[] (a->cc[i]);
        delete[] (a->cs[i]);
    }
    delete[] (a->t);
}

void IQC::calc_iqc (IQC *a)
{
    int i;
    float delta, theta;
    a->cset = 0;
    a->count = 0;
    a->state = 0;
    a->busy = 0;
    a->ntup = (int)(a->tup * a->rate);
    a->cup = new float[a->ntup + 1]; // (float *) malloc0 ((a->ntup + 1) * sizeof (float));
    delta = PI / (float)a->ntup;
    theta = 0.0;
    for (i = 0; i <= a->ntup; i++)
    {
        a->cup[i] = 0.5 * (1.0 - cos (theta));
        theta += delta;
    }
    size_iqc (a);
}

void IQC::decalc_iqc (IQC *a)
{
    desize_iqc (a);
    delete[] (a->cup);
}

IQC* IQC::create_iqc (int run, int size, float* in, float* out, float rate, int ints, float tup, int spi)
{
    IQC *a = new IQC;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->ints = ints;
    a->tup = tup;
    a->dog.spi = spi;
    calc_iqc (a);
    return a;
}

void IQC::destroy_iqc (IQC *a)
{
    decalc_iqc (a);
    delete (a);
}

void IQC::flush_iqc (IQC *)
{

}

enum _iqcstate
{
    RUN = 0,
    BEGIN,
    SWAP,
    END,
    DONE
};

void IQC::xiqc (IQC *a)
{
    if (a->run == 1)
    {
        int i, k, cset, mset;
        float I, Q, env, dx, ym, yc, ys, PRE0, PRE1;
        for (i = 0; i < a->size; i++)
        {
            I = a->in[2 * i + 0];
            Q = a->in[2 * i + 1];
            env = sqrt (I * I + Q * Q);
            if ((k = (int)(env * a->ints)) > a->ints - 1) k = a->ints - 1;
            dx = env - a->t[k];
            cset = a->cset;
            ym = a->cm[cset][4 * k + 0] + dx * (a->cm[cset][4 * k + 1] + dx * (a->cm[cset][4 * k + 2] + dx * a->cm[cset][4 * k + 3]));
            yc = a->cc[cset][4 * k + 0] + dx * (a->cc[cset][4 * k + 1] + dx * (a->cc[cset][4 * k + 2] + dx * a->cc[cset][4 * k + 3]));
            ys = a->cs[cset][4 * k + 0] + dx * (a->cs[cset][4 * k + 1] + dx * (a->cs[cset][4 * k + 2] + dx * a->cs[cset][4 * k + 3]));
            PRE0 = ym * (I * yc - Q * ys);
            PRE1 = ym * (I * ys + Q * yc);

            switch (a->state)
            {
            case RUN:
                if (a->dog.cpi[k] != a->dog.spi)
                    if (++a->dog.cpi[k] == a->dog.spi)
                        a->dog.full_ints++;
                if (a->dog.full_ints == a->ints)
                {
                    ++a->dog.count;
                    a->dog.full_ints = 0;
                    memset (a->dog.cpi, 0, a->ints * sizeof (int));
                }
                break;
            case BEGIN:
                PRE0 = (1.0 - a->cup[a->count]) * I + a->cup[a->count] * PRE0;
                PRE1 = (1.0 - a->cup[a->count]) * Q + a->cup[a->count] * PRE1;
                if (a->count++ == a->ntup)
                {
                    a->state = RUN;
                    a->count = 0;
                    a->busy = 0; // InterlockedBitTestAndReset (&a->busy, 0);
                }
                break;
            case SWAP:
                mset = 1 - cset;
                ym = a->cm[mset][4 * k + 0] + dx * (a->cm[mset][4 * k + 1] + dx * (a->cm[mset][4 * k + 2] + dx * a->cm[mset][4 * k + 3]));
                yc = a->cc[mset][4 * k + 0] + dx * (a->cc[mset][4 * k + 1] + dx * (a->cc[mset][4 * k + 2] + dx * a->cc[mset][4 * k + 3]));
                ys = a->cs[mset][4 * k + 0] + dx * (a->cs[mset][4 * k + 1] + dx * (a->cs[mset][4 * k + 2] + dx * a->cs[mset][4 * k + 3]));
                PRE0 = (1.0 - a->cup[a->count]) * ym * (I * yc - Q * ys) + a->cup[a->count] * PRE0;
                PRE1 = (1.0 - a->cup[a->count]) * ym * (I * ys + Q * yc) + a->cup[a->count] * PRE1;
                if (a->count++ == a->ntup)
                {
                    a->state = RUN;
                    a->count = 0;
                    a->busy = 0; // InterlockedBitTestAndReset (&a->busy, 0);
                }
                break;
            case END:
                PRE0 = (1.0 - a->cup[a->count]) * PRE0 + a->cup[a->count] * I;
                PRE1 = (1.0 - a->cup[a->count]) * PRE1 + a->cup[a->count] * Q;
                if (a->count++ == a->ntup)
                {
                    a->state = DONE;
                    a->count = 0;
                    a->busy = 0; // InterlockedBitTestAndReset (&a->busy, 0);
                }
                break;
            case DONE:
                PRE0 = I;
                PRE1 = Q;
                break;
            }
            a->out[2 * i + 0] = PRE0;
            a->out[2 * i + 1] = PRE1;
            // print_iqc_values("iqc.txt", a->state, env, PRE0, PRE1, ym, yc, ys, 1.1);
        }
    }
    else if (a->out != a->in)
        std::copy( a->in,  a->in + a->size * 2, a->out);
}

void IQC::setBuffers_iqc (IQC *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
}

void IQC::setSamplerate_iqc (IQC *a, int rate)
{
    decalc_iqc (a);
    a->rate = rate;
    calc_iqc (a);
}

void IQC::setSize_iqc (IQC *a, int size)
{
    a->size = size;
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void IQC::GetiqcValues (TXA& txa, float* cm, float* cc, float* cs)
{
    IQC *a;
    a = txa.iqc.p0;
    memcpy (cm, a->cm[a->cset], a->ints * 4 * sizeof (float));
    memcpy (cc, a->cc[a->cset], a->ints * 4 * sizeof (float));
    memcpy (cs, a->cs[a->cset], a->ints * 4 * sizeof (float));
}

void IQC::SetiqcValues (TXA& txa, float* cm, float* cc, float* cs)
{
    IQC *a;
    a = txa.iqc.p0;
    a->cset = 1 - a->cset;
    memcpy (a->cm[a->cset], cm, a->ints * 4 * sizeof (float));
    memcpy (a->cc[a->cset], cc, a->ints * 4 * sizeof (float));
    memcpy (a->cs[a->cset], cs, a->ints * 4 * sizeof (float));
    a->state = RUN;
}

void IQC::SetiqcSwap (TXA& txa, float* cm, float* cc, float* cs)
{
    IQC *a = txa.iqc.p1;
    a->cset = 1 - a->cset;
    memcpy (a->cm[a->cset], cm, a->ints * 4 * sizeof (float));
    memcpy (a->cc[a->cset], cc, a->ints * 4 * sizeof (float));
    memcpy (a->cs[a->cset], cs, a->ints * 4 * sizeof (float));
    a->busy = 1; // InterlockedBitTestAndSet (&a->busy, 0);
    a->state = SWAP;
    a->count = 0;
    // while (_InterlockedAnd (&a->busy, 1)) Sleep(1);
    while (a->busy == 1) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void IQC::SetiqcStart (TXA& txa, float* cm, float* cc, float* cs)
{
    IQC *a = txa.iqc.p1;
    a->cset = 0;
    memcpy (a->cm[a->cset], cm, a->ints * 4 * sizeof (float));
    memcpy (a->cc[a->cset], cc, a->ints * 4 * sizeof (float));
    memcpy (a->cs[a->cset], cs, a->ints * 4 * sizeof (float));
    a->busy = 1; // InterlockedBitTestAndSet (&a->busy, 0);
    a->state = BEGIN;
    a->count = 0;
    txa.iqc.p1->run = 1; //InterlockedBitTestAndSet   (&txa.iqc.p1->run, 0);
    // while (_InterlockedAnd (&a->busy, 1)) Sleep(1);
    while (a->busy == 1) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void IQC::SetiqcEnd (TXA& txa)
{
    IQC *a = txa.iqc.p1;
    a->busy = 1; // InterlockedBitTestAndSet (&a->busy, 0);
    a->state = END;
    a->count = 0;
    // while (_InterlockedAnd (&a->busy, 1)) Sleep(1);
    while (a->busy == 1) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    txa.iqc.p1->run = 0; //InterlockedBitTestAndReset (&txa.iqc.p1->run, 0);
}

void IQC::GetiqcDogCount (TXA& txa, int* count)
{
    IQC *a = txa.iqc.p1;
    *count = a->dog.count;
}

void IQC::SetiqcDogCount (TXA& txa, int count)
{
    IQC *a = txa.iqc.p1;
    a->dog.count = count;
}

} // namespace WDSP
