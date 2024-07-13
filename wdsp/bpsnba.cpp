/*  snb.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2015, 2016 Warren Pratt, NR0V
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
#include "resample.hpp"
#include "lmath.hpp"
#include "fircore.hpp"
#include "nbp.hpp"
#include "amd.hpp"
#include "anf.hpp"
#include "anr.hpp"
#include "emnr.hpp"
#include "bpsnba.hpp"
#include "RXA.hpp"

#define MAXIMP          256

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                       BPSNBA Bandpass Filter                                          *
*                                                                                                       *
********************************************************************************************************/

// This is a thin wrapper for a notched-bandpass filter (nbp).  The basic difference is that it provides
// for its input and output to happen at different points in the processing pipeline.  This means it must
// include a buffer, 'buff'.  Its input and output are done via functions xbpshbain() and xbpshbaout().

void BPSNBA::calc_bpsnba (BPSNBA *a)
{
    a->buff = new float[a->size * 2]; // (double *) malloc0 (a->size * sizeof (complex));
    a->bpsnba = NBP::create_nbp (
        1,                          // run, always runs (use bpsnba 'run')
        a->run_notches,             // run the notches
        0,                          // position variable for nbp (not for bpsnba), always 0
        a->size,                    // buffer size
        a->nc,                      // number of filter coefficients
        a->mp,                      // minimum phase flag
        a->buff,                    // pointer to input buffer
        a->out,                     // pointer to output buffer
        a->f_low,                   // lower filter frequency
        a->f_high,                  // upper filter frequency
        a->rate,                    // sample rate
        a->wintype,                 // wintype
        a->gain,                    // gain
        a->autoincr,                // auto-increase notch width if below min
        a->maxpb,                   // max number of passbands
        a->ptraddr);                // addr of database pointer
}

BPSNBA* BPSNBA::create_bpsnba (
    int run,
    int run_notches,
    int position,
    int size,
    int nc,
    int mp,
    float* in,
    float* out,
    int rate,
    double abs_low_freq,
    double abs_high_freq,
    double f_low,
    double f_high,
    int wintype,
    double gain,
    int autoincr,
    int maxpb,
    NOTCHDB* ptraddr
)
{
    BPSNBA *a = new BPSNBA;
    a->run = run;
    a->run_notches = run_notches;
    a->position = position;
    a->size = size;
    a->nc = nc;
    a->mp = mp;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->abs_low_freq = abs_low_freq;
    a->abs_high_freq = abs_high_freq;
    a->f_low = f_low;
    a->f_high = f_high;
    a->wintype = wintype;
    a->gain = gain;
    a->autoincr = autoincr;
    a->maxpb = maxpb;
    a->ptraddr = ptraddr;
    calc_bpsnba (a);
    return a;
}

void BPSNBA::decalc_bpsnba (BPSNBA *a)
{
    NBP::destroy_nbp (a->bpsnba);
    delete[] (a->buff);
}

void BPSNBA::destroy_bpsnba (BPSNBA *a)
{
    decalc_bpsnba (a);
    delete[] (a);
}

void BPSNBA::flush_bpsnba (BPSNBA *a)
{
    memset (a->buff, 0, a->size * sizeof (wcomplex));
    NBP::flush_nbp (a->bpsnba);
}

void BPSNBA::setBuffers_bpsnba (BPSNBA *a, float* in, float* out)
{
    decalc_bpsnba (a);
    a->in = in;
    a->out = out;
    calc_bpsnba (a);
}

void BPSNBA::setSamplerate_bpsnba (BPSNBA *a, int rate)
{
    decalc_bpsnba (a);
    a->rate = rate;
    calc_bpsnba (a);
}

void BPSNBA::setSize_bpsnba (BPSNBA *a, int size)
{
    decalc_bpsnba (a);
    a->size = size;
    calc_bpsnba (a);
}

void BPSNBA::xbpsnbain (BPSNBA *a, int position)
{
    if (a->run && a->position == position)
        memcpy (a->buff, a->in, a->size * sizeof (wcomplex));
}

void BPSNBA::xbpsnbaout (BPSNBA *a, int position)
{
    if (a->run && a->position == position)
        NBP::xnbp (a->bpsnba, 0);
}

void BPSNBA::recalc_bpsnba_filter (BPSNBA *a, int update)
{
    // Call anytime one of the parameters listed below has been changed in
    // the BPSNBA struct.
    NBP *b = a->bpsnba;
    b->fnfrun = a->run_notches;
    b->flow = a->f_low;
    b->fhigh = a->f_high;
    b->wintype = a->wintype;
    b->gain = a->gain;
    b->autoincr = a->autoincr;
    NBP::calc_nbp_impulse (b);
    FIRCORE::setImpulse_fircore (b->p, b->impulse, update);
    delete[] (b->impulse);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void BPSNBA::BPSNBASetNC (RXA& rxa, int nc)
{
    BPSNBA *a = rxa.bpsnba.p;
    rxa.csDSP.lock();

    if (a->nc != nc)
    {
        a->nc = nc;
        a->bpsnba->nc = a->nc;
        NBP::setNc_nbp (a->bpsnba);
    }

    rxa.csDSP.unlock();
}

void BPSNBA::BPSNBASetMP (RXA& rxa, int mp)
{
    BPSNBA *a = rxa.bpsnba.p;

    if (a->mp != mp)
    {
        a->mp = mp;
        a->bpsnba->mp = a->mp;
        NBP::setMp_nbp (a->bpsnba);
    }
}

} // namespace
