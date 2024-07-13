/*  cfcomp.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2017, 2021 Warren Pratt, NR0V
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
#include "cfcomp.hpp"
#include "meterlog10.hpp"
#include "TXA.hpp"

namespace WDSP {

void CFCOMP::calc_cfcwindow (CFCOMP *a)
{
    int i;
    float arg0, arg1, cgsum, igsum, coherent_gain, inherent_power_gain, wmult;
    switch (a->wintype)
    {
    case 0:
        arg0 = 2.0 * PI / (float)a->fsize;
        cgsum = 0.0;
        igsum = 0.0;
        for (i = 0; i < a->fsize; i++)
        {
            a->window[i] = sqrt (0.54 - 0.46 * cos((float)i * arg0));
            cgsum += a->window[i];
            igsum += a->window[i] * a->window[i];
        }
        coherent_gain = cgsum / (float)a->fsize;
        inherent_power_gain = igsum / (float)a->fsize;
        wmult = 1.0 / sqrt (inherent_power_gain);
        for (i = 0; i < a->fsize; i++)
            a->window[i] *= wmult;
        a->winfudge = sqrt (1.0 / coherent_gain);
        break;
    case 1:
        arg0 = 2.0 * PI / (float)a->fsize;
        cgsum = 0.0;
        igsum = 0.0;
        for (i = 0; i < a->fsize; i++)
        {
            arg1 = cos(arg0 * (float)i);
            a->window[i]  = sqrt   (+0.21747
                          + arg1 * (-0.45325
                          + arg1 * (+0.28256
                          + arg1 * (-0.04672))));
            cgsum += a->window[i];
            igsum += a->window[i] * a->window[i];
        }
        coherent_gain = cgsum / (float)a->fsize;
        inherent_power_gain = igsum / (float)a->fsize;
        wmult = 1.0 / sqrt (inherent_power_gain);
        for (i = 0; i < a->fsize; i++)
            a->window[i] *= wmult;
        a->winfudge = sqrt (1.0 / coherent_gain);
        break;
    }
}

int CFCOMP::fCOMPcompare (const void *a, const void *b)
{
    if (*(float*)a < *(float*)b)
        return -1;
    else if (*(float*)a == *(float*)b)
        return 0;
    else
        return 1;
}

void CFCOMP::calc_comp (CFCOMP *a)
{
    int i, j;
    float f, frac, fincr, fmax;
    float* sary;
    a->precomplin = pow (10.0, 0.05 * a->precomp);
    a->prepeqlin  = pow (10.0, 0.05 * a->prepeq);
    fmax = 0.5 * a->rate;
    for (i = 0; i < a->nfreqs; i++)
    {
        a->F[i] = std::max (a->F[i], 0.0f);
        a->F[i] = std::min (a->F[i], fmax);
        a->G[i] = std::max (a->G[i], 0.0f);
    }
    sary = new float[3 * a->nfreqs]; // (float *)malloc0 (3 * a->nfreqs * sizeof (float));
    for (i = 0; i < a->nfreqs; i++)
    {
        sary[3 * i + 0] = a->F[i];
        sary[3 * i + 1] = a->G[i];
        sary[3 * i + 2] = a->E[i];
    }
    qsort (sary, a->nfreqs, 3 * sizeof (float), fCOMPcompare);
    for (i = 0; i < a->nfreqs; i++)
    {
        a->F[i] = sary[3 * i + 0];
        a->G[i] = sary[3 * i + 1];
        a->E[i] = sary[3 * i + 2];
    }
    delete[] (sary);
    a->fp[0] = 0.0;
    a->fp[a->nfreqs + 1] = fmax;
    a->gp[0] = a->G[0];
    a->gp[a->nfreqs + 1] = a->G[a->nfreqs - 1];
    a->ep[0] = a->E[0];                             // cutoff?
    a->ep[a->nfreqs + 1] = a->E[a->nfreqs - 1];     // cutoff?
    for (i = 0, j = 1; i < a->nfreqs; i++, j++)
    {
        a->fp[j] = a->F[i];
        a->gp[j] = a->G[i];
        a->ep[j] = a->E[i];
    }
    fincr = a->rate / (float)a->fsize;
    j = 0;
    // print_impulse ("gp.txt", a->nfreqs+2, a->gp, 0, 0);
    for (i = 0; i < a->msize; i++)
    {
        f = fincr * (float)i;
        while (f >= a->fp[j + 1] && j < a->nfreqs) j++;
        frac = (f - a->fp[j]) / (a->fp[j + 1] - a->fp[j]);
        a->comp[i] = pow (10.0, 0.05 * (frac * a->gp[j + 1] + (1.0 - frac) * a->gp[j]));
        a->peq[i]  = pow (10.0, 0.05 * (frac * a->ep[j + 1] + (1.0 - frac) * a->ep[j]));
        a->cfc_gain[i] = a->precomplin * a->comp[i];
    }
    // print_impulse ("comp.txt", a->msize, a->comp, 0, 0);
    delete[] sary;
}

void CFCOMP::calc_cfcomp(CFCOMP *a)
{
    int i;
    a->incr = a->fsize / a->ovrlp;
    if (a->fsize > a->bsize)
        a->iasize = a->fsize;
    else
        a->iasize = a->bsize + a->fsize - a->incr;
    a->iainidx = 0;
    a->iaoutidx = 0;
    if (a->fsize > a->bsize)
    {
        if (a->bsize > a->incr)  a->oasize = a->bsize;
        else                     a->oasize = a->incr;
        a->oainidx = (a->fsize - a->bsize - a->incr) % a->oasize;
    }
    else
    {
        a->oasize = a->bsize;
        a->oainidx = a->fsize - a->incr;
    }
    a->init_oainidx = a->oainidx;
    a->oaoutidx = 0;
    a->msize = a->fsize / 2 + 1;
    a->window    = new float[a->fsize]; // (float *)malloc0 (a->fsize  * sizeof(float));
    a->inaccum   = new float[a->iasize]; // (float *)malloc0 (a->iasize * sizeof(float));
    a->forfftin  = new float[a->fsize]; // (float *)malloc0 (a->fsize  * sizeof(float));
    a->forfftout = new float[a->msize * 2]; // (float *)malloc0 (a->msize  * sizeof(complex));
    a->cmask     = new float[a->msize]; // (float *)malloc0 (a->msize  * sizeof(float));
    a->mask      = new float[a->msize]; // (float *)malloc0 (a->msize  * sizeof(float));
    a->cfc_gain  = new float[a->msize]; // (float *)malloc0 (a->msize  * sizeof(float));
    a->revfftin  = new float[a->msize * 2]; // (float *)malloc0 (a->msize  * sizeof(complex));
    a->revfftout = new float[a->fsize]; // (float *)malloc0 (a->fsize  * sizeof(float));
    a->save      = new float*[a->ovrlp]; // (float **)malloc0(a->ovrlp  * sizeof(float *));
    for (i = 0; i < a->ovrlp; i++)
        a->save[i] = new float[a->fsize]; // (float *)malloc0(a->fsize * sizeof(float));
    a->outaccum = new float[a->oasize]; // (float *)malloc0(a->oasize * sizeof(float));
    a->nsamps = 0;
    a->saveidx = 0;
    a->Rfor = fftwf_plan_dft_r2c_1d(a->fsize, a->forfftin, (fftwf_complex *)a->forfftout, FFTW_ESTIMATE);
    a->Rrev = fftwf_plan_dft_c2r_1d(a->fsize, (fftwf_complex *)a->revfftin, a->revfftout, FFTW_ESTIMATE);
    calc_cfcwindow(a);

    a->pregain  = (2.0 * a->winfudge) / (float)a->fsize;
    a->postgain = 0.5 / ((float)a->ovrlp * a->winfudge);

    a->fp = new float[a->nfreqs + 2]; // (float *) malloc0 ((a->nfreqs + 2) * sizeof (float));
    a->gp = new float[a->nfreqs + 2]; // (float *) malloc0 ((a->nfreqs + 2) * sizeof (float));
    a->ep = new float[a->nfreqs + 2]; // (float *) malloc0 ((a->nfreqs + 2) * sizeof (float));
    a->comp = new float[a->msize]; // (float *) malloc0 (a->msize * sizeof (float));
    a->peq  =  new float[a->msize]; // (float *) malloc0 (a->msize * sizeof (float));
    calc_comp (a);

    a->gain = 0.0;
    a->mmult = exp (-1.0 / (a->rate * a->ovrlp * a->mtau));
    a->dmult = exp (-(float)a->fsize / (a->rate * a->ovrlp * a->dtau));

    a->delta         =  new float[a->msize]; //  (float*)malloc0 (a->msize * sizeof(float));
    a->delta_copy    =  new float[a->msize]; //  (float*)malloc0 (a->msize * sizeof(float));
    a->cfc_gain_copy =  new float[a->msize]; //  (float*)malloc0 (a->msize * sizeof(float));
}

void CFCOMP::decalc_cfcomp(CFCOMP *a)
{
    int i;
    delete[] (a->cfc_gain_copy);
    delete[] (a->delta_copy);
    delete[] (a->delta);
    delete[] (a->peq);
    delete[] (a->comp);
    delete[] (a->ep);
    delete[] (a->gp);
    delete[] (a->fp);

    fftwf_destroy_plan(a->Rrev);
    fftwf_destroy_plan(a->Rfor);
    delete[](a->outaccum);
    for (i = 0; i < a->ovrlp; i++)
        delete[](a->save[i]);
    delete[](a->save);
    delete[](a->revfftout);
    delete[](a->revfftin);
    delete[](a->cfc_gain);
    delete[](a->mask);
    delete[](a->cmask);
    delete[](a->forfftout);
    delete[](a->forfftin);
    delete[](a->inaccum);
    delete[](a->window);
}

CFCOMP* CFCOMP::create_cfcomp (int run, int position, int peq_run, int size, float* in, float* out, int fsize, int ovrlp,
    int rate, int wintype, int comp_method, int nfreqs, float precomp, float prepeq, float* F, float* G, float* E, float mtau, float dtau)
{
    CFCOMP *a = new CFCOMP;
    a->run = run;
    a->position = position;
    a->peq_run = peq_run;
    a->bsize = size;
    a->in = in;
    a->out = out;
    a->fsize = fsize;
    a->ovrlp = ovrlp;
    a->rate = rate;
    a->wintype = wintype;
    a->comp_method = comp_method;
    a->nfreqs = nfreqs;
    a->precomp = precomp;
    a->prepeq = prepeq;
    a->mtau = mtau;                 // compression metering time constant
    a->dtau = dtau;                 // compression display time constant
    a->F = new float[a->nfreqs]; // (float *)malloc0 (a->nfreqs * sizeof (float));
    a->G = new float[a->nfreqs]; // (float *)malloc0 (a->nfreqs * sizeof (float));
    a->E = new float[a->nfreqs]; // (float *)malloc0 (a->nfreqs * sizeof (float));
    memcpy (a->F, F, a->nfreqs * sizeof (float));
    memcpy (a->G, G, a->nfreqs * sizeof (float));
    memcpy (a->E, E, a->nfreqs * sizeof (float));
    calc_cfcomp (a);
    return a;
}

void CFCOMP::flush_cfcomp (CFCOMP *a)
{
    int i;
    memset (a->inaccum, 0, a->iasize * sizeof (float));
    for (i = 0; i < a->ovrlp; i++)
        memset (a->save[i], 0, a->fsize * sizeof (float));
    memset (a->outaccum, 0, a->oasize * sizeof (float));
    a->nsamps   = 0;
    a->iainidx  = 0;
    a->iaoutidx = 0;
    a->oainidx  = a->init_oainidx;
    a->oaoutidx = 0;
    a->saveidx  = 0;
    a->gain = 0.0;
    memset(a->delta, 0, a->msize * sizeof(float));
}

void CFCOMP::destroy_cfcomp (CFCOMP *a)
{
    decalc_cfcomp (a);
    delete[] (a->E);
    delete[] (a->G);
    delete[] (a->F);
    delete (a);
}


void CFCOMP::calc_mask (CFCOMP *a)
{
    int i;
    float comp, mask, delta;
    switch (a->comp_method)
    {
    case 0:
        {
            float mag, test;
            for (i = 0; i < a->msize; i++)
            {
                mag = sqrt (a->forfftout[2 * i + 0] * a->forfftout[2 * i + 0]
                          + a->forfftout[2 * i + 1] * a->forfftout[2 * i + 1]);
                comp = a->cfc_gain[i];
                test = comp * mag;
                if (test > 1.0)
                    mask = 1.0 / mag;
                else
                    mask = comp;
                a->cmask[i] = mask;
                if (test > a->gain) a->gain = test;
                else a->gain = a->mmult * a->gain;

                delta = a->cfc_gain[i] - a->cmask[i];
                if (delta > a->delta[i]) a->delta[i] = delta;
                else a->delta[i] *= a->dmult;
            }
            break;
        }
    }
    if (a->peq_run)
    {
        for (i = 0; i < a->msize; i++)
        {
            a->mask[i] = a->cmask[i] * a->prepeqlin * a->peq[i];
        }
    }
    else
        memcpy (a->mask, a->cmask, a->msize * sizeof (float));
    // print_impulse ("mask.txt", a->msize, a->mask, 0, 0);
    a->mask_ready = 1;
}

void CFCOMP::xcfcomp (CFCOMP *a, int pos)
{
    if (a->run && pos == a->position)
    {
        int i, j, k, sbuff, sbegin;
        for (i = 0; i < 2 * a->bsize; i += 2)
        {
            a->inaccum[a->iainidx] = a->in[i];
            a->iainidx = (a->iainidx + 1) % a->iasize;
        }
        a->nsamps += a->bsize;
        while (a->nsamps >= a->fsize)
        {
            for (i = 0, j = a->iaoutidx; i < a->fsize; i++, j = (j + 1) % a->iasize)
                a->forfftin[i] = a->pregain * a->window[i] * a->inaccum[j];
            a->iaoutidx = (a->iaoutidx + a->incr) % a->iasize;
            a->nsamps -= a->incr;
            fftwf_execute (a->Rfor);
            calc_mask(a);
            for (i = 0; i < a->msize; i++)
            {
                a->revfftin[2 * i + 0] = a->mask[i] * a->forfftout[2 * i + 0];
                a->revfftin[2 * i + 1] = a->mask[i] * a->forfftout[2 * i + 1];
            }
            fftwf_execute (a->Rrev);
            for (i = 0; i < a->fsize; i++)
                a->save[a->saveidx][i] = a->postgain * a->window[i] * a->revfftout[i];
            for (i = a->ovrlp; i > 0; i--)
            {
                sbuff = (a->saveidx + i) % a->ovrlp;
                sbegin = a->incr * (a->ovrlp - i);
                for (j = sbegin, k = a->oainidx; j < a->incr + sbegin; j++, k = (k + 1) % a->oasize)
                {
                    if ( i == a->ovrlp)
                        a->outaccum[k]  = a->save[sbuff][j];
                    else
                        a->outaccum[k] += a->save[sbuff][j];
                }
            }
            a->saveidx = (a->saveidx + 1) % a->ovrlp;
            a->oainidx = (a->oainidx + a->incr) % a->oasize;
        }
        for (i = 0; i < a->bsize; i++)
        {
            a->out[2 * i + 0] = a->outaccum[a->oaoutidx];
            a->out[2 * i + 1] = 0.0;
            a->oaoutidx = (a->oaoutidx + 1) % a->oasize;
        }
    }
    else if (a->out != a->in)
        memcpy (a->out, a->in, a->bsize * sizeof (wcomplex));
}

void CFCOMP::setBuffers_cfcomp (CFCOMP *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
}

void CFCOMP::setSamplerate_cfcomp (CFCOMP *a, int rate)
{
    decalc_cfcomp (a);
    a->rate = rate;
    calc_cfcomp (a);
}

void CFCOMP::setSize_cfcomp (CFCOMP *a, int size)
{
    decalc_cfcomp (a);
    a->bsize = size;
    calc_cfcomp (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void CFCOMP::SetCFCOMPRun (TXA& txa, int run)
{
    CFCOMP *a = txa.cfcomp.p;

    if (a->run != run) {
        a->run = run;
    }
}

void CFCOMP::SetCFCOMPPosition (TXA& txa, int pos)
{
    CFCOMP *a = txa.cfcomp.p;

    if (a->position != pos) {
        a->position = pos;
    }
}

void CFCOMP::SetCFCOMPprofile (TXA& txa, int nfreqs, float* F, float* G, float *E)
{
    CFCOMP *a = txa.cfcomp.p;
    a->nfreqs = nfreqs;
    delete[] (a->E);
    delete[] (a->F);
    delete[] (a->G);
    a->F = new float[a->nfreqs]; // (float *)malloc0 (a->nfreqs * sizeof (float));
    a->G = new float[a->nfreqs]; // (float *)malloc0 (a->nfreqs * sizeof (float));
    a->E = new float[a->nfreqs]; // (float *)malloc0 (a->nfreqs * sizeof (float));
    memcpy (a->F, F, a->nfreqs * sizeof (float));
    memcpy (a->G, G, a->nfreqs * sizeof (float));
    memcpy (a->E, E, a->nfreqs * sizeof (float));
    delete[] (a->ep);
    delete[] (a->gp);
    delete[] (a->fp);
    a->fp = new float[a->nfreqs]; // (float *) malloc0 ((a->nfreqs + 2) * sizeof (float));
    a->gp = new float[a->nfreqs]; // (float *) malloc0 ((a->nfreqs + 2) * sizeof (float));
    a->ep = new float[a->nfreqs]; // (float *) malloc0 ((a->nfreqs + 2) * sizeof (float));
    calc_comp(a);
}

void CFCOMP::SetCFCOMPPrecomp (TXA& txa, float precomp)
{
    CFCOMP *a = txa.cfcomp.p;

    if (a->precomp != precomp)
    {
        a->precomp = precomp;
        a->precomplin = pow (10.0, 0.05 * a->precomp);

        for (int i = 0; i < a->msize; i++)
        {
            a->cfc_gain[i] = a->precomplin * a->comp[i];
        }
    }
}

void CFCOMP::SetCFCOMPPeqRun (TXA& txa, int run)
{
    CFCOMP *a = txa.cfcomp.p;

    if (a->peq_run != run) {
        a->peq_run = run;
    }
}

void CFCOMP::SetCFCOMPPrePeq (TXA& txa, float prepeq)
{
    CFCOMP *a = txa.cfcomp.p;
    a->prepeq = prepeq;
    a->prepeqlin = pow (10.0, 0.05 * a->prepeq);
}

void CFCOMP::GetCFCOMPDisplayCompression (TXA& txa, float* comp_values, int* ready)
{
    int i;
    CFCOMP *a = txa.cfcomp.p;

    if ((*ready = a->mask_ready))
    {
        memcpy(a->delta_copy, a->delta, a->msize * sizeof(float));
        memcpy(a->cfc_gain_copy, a->cfc_gain, a->msize * sizeof(float));
        a->mask_ready = 0;
    }

    if (*ready)
    {
        for (i = 0; i < a->msize; i++)
            comp_values[i] = 20.0 * MemLog::mlog10 (a->cfc_gain_copy[i] / (a->cfc_gain_copy[i] - a->delta_copy[i]));
    }
}

} // namespace WDSP

