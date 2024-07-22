/*  nbp.c

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
#include "fir.hpp"
#include "fircore.hpp"
#include "bpsnba.hpp"
#include "nbp.hpp"
#include "RXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                           Notch Database                                              *
*                                                                                                       *
********************************************************************************************************/

 NOTCHDB* NOTCHDB::create_notchdb (int master_run, int maxnotches)
{
    NOTCHDB *a = new NOTCHDB;
    a->master_run = master_run;
    a->maxnotches = maxnotches;
    a->nn = 0;
    a->fcenter = new double[a->maxnotches]; // (float *) malloc0 (a->maxnotches * sizeof (float));
    a->fwidth  = new double[a->maxnotches]; // (float *) malloc0 (a->maxnotches * sizeof (float));
    a->nlow    = new double[a->maxnotches]; // (float *) malloc0 (a->maxnotches * sizeof (float));
    a->nhigh   = new double[a->maxnotches]; // (float *) malloc0 (a->maxnotches * sizeof (float));
    a->active  = new int[a->maxnotches]; // (int    *) malloc0 (a->maxnotches * sizeof (int   ));
    return a;
}

void NOTCHDB::destroy_notchdb (NOTCHDB *b)
{
    delete[] (b->active);
    delete[] (b->nhigh);
    delete[] (b->nlow);
    delete[] (b->fwidth);
    delete[] (b->fcenter);
}

/********************************************************************************************************
*                                                                                                       *
*                                       Notched Bandpass Filter                                         *
*                                                                                                       *
********************************************************************************************************/

float* NBP::fir_mbandpass (int N, int nbp, double* flow, double* fhigh, double rate, double scale, int wintype)
{
    int i, k;
    float* impulse = new float[N * 2]; // (float *) malloc0 (N * sizeof (complex));
    float* imp;
    for (k = 0; k < nbp; k++)
    {
        imp = FIR::fir_bandpass (N, flow[k], fhigh[k], rate, wintype, 1, scale);
        for (i = 0; i < N; i++)
        {
            impulse[2 * i + 0] += imp[2 * i + 0];
            impulse[2 * i + 1] += imp[2 * i + 1];
        }
        delete[] (imp);
    }
    return impulse;
}

double NBP::min_notch_width (NBP *a)
{
    double min_width;
    switch (a->wintype)
    {
    case 0:
        min_width = 1600.0 / (a->nc / 256) * (a->rate / 48000);
        break;
    case 1:
        min_width = 2200.0 / (a->nc / 256) * (a->rate / 48000);
        break;
    }
    return min_width;
}

int NBP::make_nbp (
    int nn,
    int* active,
    double* center,
    double* width,
    double* nlow,
    double* nhigh,
    double minwidth,
    int autoincr,
    double flow,
    double fhigh,
    double* bplow,
    double* bphigh,
    int* havnotch
)
{
    int nbp;
    int nnbp, adds;
    int i, j, k;
    double nl, nh;
    int* del = new int[1024]; // (int *) malloc0 (1024 * sizeof (int));
    if (fhigh > flow)
    {
        bplow[0]  = flow;
        bphigh[0] = fhigh;
        nbp = 1;
    }
    else
    {
        nbp = 0;
        delete[] del;
        return nbp;
    }
    *havnotch = 0;
    for (k = 0; k < nn; k++)
    {
        if (autoincr && width[k] < minwidth)
        {
            nl = center[k] - 0.5 * minwidth;
            nh = center[k] + 0.5 * minwidth;
        }
        else
        {
            nl = nlow[k];
            nh = nhigh[k];
        }
        if (active[k] && (nh > flow && nl < fhigh))
        {
            *havnotch = 1;
            adds = 0;
            for (i = 0; i < nbp; i++)
            {
                if (nh > bplow[i] && nl < bphigh[i])
                {
                    if (nl <= bplow[i] && nh >= bphigh[i])
                    {
                        del[i] = 1;
                    }
                    else if (nl > bplow[i] && nh < bphigh[i])
                    {

                        bplow[nbp + adds] = nh;
                        bphigh[nbp + adds] = bphigh[i];
                        bphigh[i] = nl;
                        adds++;
                    }
                    else if (nl <= bplow[i] && nh > bplow[i])
                    {
                        bplow[i] = nh;
                    }
                    else if (nl < bphigh[i] && nh >= bphigh[i])
                    {
                        bphigh[i] = nl;
                    }
                }
            }
            nbp += adds;
            nnbp = nbp;
            for (i = 0; i < nbp; i++)
            {
                if (del[i] == 1)
                {
                    nnbp--;
                    for (j = i; j < nnbp; j++)
                    {
                        bplow[j] = bplow[j + 1];
                        bphigh[j] = bphigh[j + 1];
                    }
                    del[i] = 0;
                }
            }
            nbp = nnbp;
        }
    }
    delete[] (del);
    return nbp;
}

void NBP::calc_nbp_lightweight (NBP *a)
{   // calculate and set new impulse response; used when changing tune freq or shift freq
    int i;
    double fl, fh;
    double offset;
    NOTCHDB *b = a->ptraddr;
    if (a->fnfrun)
    {
        offset = b->tunefreq + b->shift;
        fl = a->flow  + offset;
        fh = a->fhigh + offset;
        a->numpb = make_nbp (
            b->nn,
            b->active,
            b->fcenter,
            b->fwidth,
            b->nlow,
            b->nhigh,
            min_notch_width (a),
            a->autoincr,
            fl,
            fh,
            a->bplow,
            a->bphigh,
            &a->havnotch
        );
        // when tuning, no need to recalc filter if there were not and are not any notches in passband
        if (a->hadnotch || a->havnotch)
        {
            for (i = 0; i < a->numpb; i++)
            {
                a->bplow[i]  -= offset;
                a->bphigh[i] -= offset;
            }
            a->impulse = fir_mbandpass (a->nc, a->numpb, a->bplow, a->bphigh,
                a->rate, a->gain / (float)(2 * a->size), a->wintype);
            FIRCORE::setImpulse_fircore (a->p, a->impulse, 1);
            // print_impulse ("nbp.txt", a->size + 1, impulse, 1, 0);
            delete[](a->impulse);
        }
        a->hadnotch = a->havnotch;
    }
    else
        a->hadnotch = 1;
}

void NBP::calc_nbp_impulse (NBP *a)
{   // calculates impulse response; for create_fircore() and parameter changes
    int i;
    float fl, fh;
    double offset;
    NOTCHDB *b = a->ptraddr;
    if (a->fnfrun)
    {
        offset = b->tunefreq + b->shift;
        fl = a->flow  + offset;
        fh = a->fhigh + offset;
        a->numpb = make_nbp (
            b->nn,
            b->active,
            b->fcenter,
            b->fwidth,
            b->nlow,
            b->nhigh,
            min_notch_width (a),
            a->autoincr,
            fl,
            fh,
            a->bplow,
            a->bphigh,
            &a->havnotch
        );
        for (i = 0; i < a->numpb; i++)
        {
            a->bplow[i]  -= offset;
            a->bphigh[i] -= offset;
        }
        a->impulse = fir_mbandpass (
            a->nc,
            a->numpb,
            a->bplow,
            a->bphigh,
            a->rate,
            a->gain / (float)(2 * a->size),
            a->wintype
        );
    }
    else
    {
        a->impulse = FIR::fir_bandpass(
            a->nc,
            a->flow,
            a->fhigh,
            a->rate,
            a->wintype,
            1,
            a->gain / (float)(2 * a->size)
        );
    }
}

NBP* NBP::create_nbp(
    int run,
    int fnfrun,
    int position,
    int size,
    int nc,
    int mp,
    float* in,
    float* out,
    double flow,
    double fhigh,
    int rate,
    int wintype,
    double gain,
    int autoincr,
    int maxpb,
    NOTCHDB* ptraddr
)
{
    NBP *a = new NBP;
    a->run = run;
    a->fnfrun = fnfrun;
    a->position = position;
    a->size = size;
    a->nc = nc;
    a->mp = mp;
    a->rate = (double) rate;
    a->wintype = wintype;
    a->gain = gain;
    a->in = in;
    a->out = out;
    a->autoincr = autoincr;
    a->flow = flow;
    a->fhigh = fhigh;
    a->maxpb = maxpb;
    a->ptraddr = ptraddr;
    a->bplow   = new double[a->maxpb]; // (float *) malloc0 (a->maxpb * sizeof (float));
    a->bphigh  = new double[a->maxpb]; // (float *) malloc0 (a->maxpb * sizeof (float));
    calc_nbp_impulse (a);
    a->p = FIRCORE::create_fircore (a->size, a->in, a->out, a->nc, a->mp, a->impulse);
    // print_impulse ("nbp.txt", a->size + 1, impulse, 1, 0);
    delete[](a->impulse);
    return a;
}

void NBP::destroy_nbp (NBP *a)
{
    FIRCORE::destroy_fircore (a->p);
    delete[] (a->bphigh);
    delete[] (a->bplow);
    delete (a);
}

void NBP::flush_nbp (NBP *a)
{
    FIRCORE::flush_fircore (a->p);
}

void NBP::xnbp (NBP *a, int pos)
{
    if (a->run && pos == a->position)
        FIRCORE::xfircore (a->p);
    else if (a->in != a->out)
        std::copy( a->in,  a->in + a->size * 2, a->out);
}

void NBP::setBuffers_nbp (NBP *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
    FIRCORE::setBuffers_fircore (a->p, a->in, a->out);
}

void NBP::setSamplerate_nbp (NBP *a, int rate)
{
    a->rate = rate;
    calc_nbp_impulse (a);
    FIRCORE::setImpulse_fircore (a->p, a->impulse, 1);
    delete[] (a->impulse);
}

void NBP::setSize_nbp (NBP *a, int size)
{
    // NOTE:  'size' must be <= 'nc'
    a->size = size;
    FIRCORE::setSize_fircore (a->p, a->size);
    calc_nbp_impulse (a);
    FIRCORE::setImpulse_fircore (a->p, a->impulse, 1);
    delete[] (a->impulse);
}

void NBP::setNc_nbp (NBP *a)
{
    calc_nbp_impulse (a);
    FIRCORE::setNc_fircore (a->p, a->nc, a->impulse);
    delete[] (a->impulse);
}

void NBP::setMp_nbp (NBP *a)
{
    FIRCORE::setMp_fircore (a->p, a->mp);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

// DATABASE PROPERTIES

void NBP::UpdateNBPFiltersLightWeight (RXA& rxa)
{   // called when setting tune freq or shift freq
    calc_nbp_lightweight (rxa.nbp0);
    calc_nbp_lightweight (rxa.bpsnba->bpsnba);
}

void NBP::UpdateNBPFilters(RXA& rxa)
{
    NBP *a = rxa.nbp0;
    BPSNBA *b = rxa.bpsnba;
    if (a->fnfrun)
    {
        calc_nbp_impulse (a);
        FIRCORE::setImpulse_fircore (a->p, a->impulse, 1);
        delete[] (a->impulse);
    }
    if (b->bpsnba->fnfrun)
    {
        BPSNBA::recalc_bpsnba_filter (b, 1);
    }
}

int NBP::NBPAddNotch (RXA& rxa, int notch, double fcenter, double fwidth, int active)
{
    NOTCHDB *b;
    int i, j;
    int rval;
    b = rxa.ndb;
    if (notch <= b->nn && b->nn < b->maxnotches)
    {
        b->nn++;
        for (i = b->nn - 2, j = b->nn - 1; i >= notch; i--, j--)
        {
            b->fcenter[j] = b->fcenter[i];
            b->fwidth[j] = b->fwidth[i];
            b->nlow[j] = b->nlow[i];
            b->nhigh[j] = b->nhigh[i];
            b->active[j] = b->active[i];
        }
        b->fcenter[notch] = fcenter;
        b->fwidth[notch] = fwidth;
        b->nlow[notch] = fcenter - 0.5 * fwidth;
        b->nhigh[notch] = fcenter + 0.5 * fwidth;
        b->active[notch] = active;
        UpdateNBPFilters (rxa);
        rval = 0;
    }
    else
        rval = -1;
    return rval;
}

int NBP::NBPGetNotch (RXA& rxa, int notch, double* fcenter, double* fwidth, int* active)
{
    NOTCHDB *a;
    int rval;
    a = rxa.ndb;

    if (notch < a->nn)
    {
        *fcenter = a->fcenter[notch];
        *fwidth = a->fwidth[notch];
        *active = a->active[notch];
        rval = 0;
    }
    else
    {
        *fcenter = -1.0;
        *fwidth = 0.0;
        *active = -1;
        rval = -1;
    }

    return rval;
}

int NBP::NBPDeleteNotch (RXA& rxa, int notch)
{
    int i, j;
    int rval;
    NOTCHDB *a;
    a = rxa.ndb;
    if (notch < a->nn)
    {
        a->nn--;
        for (i = notch, j = notch + 1; i < a->nn; i++, j++)
        {
            a->fcenter[i] = a->fcenter[j];
            a->fwidth[i] = a->fwidth[j];
            a->nlow[i] = a->nlow[j];
            a->nhigh[i] = a->nhigh[j];
            a->active[i] = a->active[j];
        }
        UpdateNBPFilters (rxa);
        rval = 0;
    }
    else
        rval = -1;
    return rval;
}

int NBP::NBPEditNotch (RXA& rxa, int notch, double fcenter, double fwidth, int active)
{
    NOTCHDB *a;
    int rval;
    a = rxa.ndb;
    if (notch < a->nn)
    {
        a->fcenter[notch] = fcenter;
        a->fwidth[notch] = fwidth;
        a->nlow[notch] = fcenter - 0.5 * fwidth;
        a->nhigh[notch] = fcenter + 0.5 * fwidth;
        a->active[notch] = active;
        UpdateNBPFilters (rxa);
        rval = 0;
    }
    else
        rval = -1;
    return rval;
}

void NBP::NBPGetNumNotches (RXA& rxa, int* nnotches)
{
    NOTCHDB *a;
    a = rxa.ndb;
    *nnotches = a->nn;
}

void NBP::NBPSetTuneFrequency (RXA& rxa, double tunefreq)
{
    NOTCHDB *a;
    a = rxa.ndb;

    if (tunefreq != a->tunefreq)
    {
        a->tunefreq = tunefreq;
        UpdateNBPFiltersLightWeight (rxa);
    }
}

void NBP::NBPSetShiftFrequency (RXA& rxa, double shift)
{
    NOTCHDB *a;
    a = rxa.ndb;
    if (shift != a->shift)
    {
        a->shift = shift;
        UpdateNBPFiltersLightWeight (rxa);
    }
}

void NBP::NBPSetNotchesRun (RXA& rxa, int run)
{
    NOTCHDB *a = rxa.ndb;
    NBP *b = rxa.nbp0;

    if ( run != a->master_run)
    {
        a->master_run = run;                            // update variables
        b->fnfrun = a->master_run;
        RXA::bpsnbaCheck (rxa, rxa.mode, run);
        calc_nbp_impulse (b);                           // recalc nbp impulse response
        FIRCORE::setImpulse_fircore (b->p, b->impulse, 0);       // calculate new filter masks
        delete[] (b->impulse);
        RXA::bpsnbaSet (rxa);
        FIRCORE::setUpdate_fircore (b->p);                       // apply new filter masks
    }
}

// FILTER PROPERTIES

void NBP::NBPSetRun (RXA& rxa, int run)
{
    NBP *a;
    a = rxa.nbp0;
    a->run = run;
}

void NBP::NBPSetFreqs (RXA& rxa, double flow, double fhigh)
{
    NBP *a;
    a = rxa.nbp0;

    if ((flow != a->flow) || (fhigh != a->fhigh))
    {
        a->flow = flow;
        a->fhigh = fhigh;
        calc_nbp_impulse (a);
        FIRCORE::setImpulse_fircore (a->p, a->impulse, 1);
        delete[] (a->impulse);
    }
}

void NBP::NBPSetWindow (RXA& rxa, int wintype)
{
    NBP *a;
    BPSNBA *b;
    a = rxa.nbp0;
    b = rxa.bpsnba;

    if ((a->wintype != wintype))
    {
        a->wintype = wintype;
        calc_nbp_impulse (a);
        FIRCORE::setImpulse_fircore (a->p, a->impulse, 1);
        delete[] (a->impulse);
    }

    if ((b->wintype != wintype))
    {
        b->wintype = wintype;
        BPSNBA::recalc_bpsnba_filter (b, 1);
    }
}

void NBP::NBPSetNC (RXA& rxa, int nc)
{
    // NOTE:  'nc' must be >= 'size'
    NBP *a;
    a = rxa.nbp0;

    if (a->nc != nc)
    {
        a->nc = nc;
        setNc_nbp (a);
    }
}

void NBP::NBPSetMP (RXA& rxa, int mp)
{
    NBP *a;
    a = rxa.nbp0;

    if (a->mp != mp)
    {
        a->mp = mp;
        setMp_nbp (a);
    }
}

void NBP::NBPGetMinNotchWidth (RXA& rxa, double* minwidth)
{
    NBP *a;
    a = rxa.nbp0;
    *minwidth = min_notch_width (a);
}

void NBP::NBPSetAutoIncrease (RXA& rxa, int autoincr)
{
    NBP *a;
    BPSNBA *b;
    a = rxa.nbp0;
    b = rxa.bpsnba;

    if ((a->autoincr != autoincr))
    {
        a->autoincr = autoincr;
        calc_nbp_impulse (a);
        FIRCORE::setImpulse_fircore (a->p, a->impulse, 1);
        delete[] (a->impulse);
    }

    if ((b->autoincr != autoincr))
    {
        b->autoincr = autoincr;
        BPSNBA::recalc_bpsnba_filter (b, 1);
    }
}

} // namespace WDSP
