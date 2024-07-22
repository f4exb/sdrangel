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
#include "firmin.hpp"
#include "nbp.hpp"
#include "amd.hpp"
#include "anf.hpp"
#include "anr.hpp"
#include "emnr.hpp"
#include "snba.hpp"
#include "RXA.hpp"

#define MAXIMP          256

namespace WDSP {

void SNBA::calc_snba (SNBA *d)
{
    if (d->inrate >= d->internalrate)
        d->isize = d->bsize / (d->inrate / d->internalrate);
    else
        d->isize = d->bsize * (d->internalrate / d->inrate);

    d->inbuff  = new float[d->isize * 2]; // (double *) malloc0 (d->isize * sizeof (complex));
    d->outbuff = new float[d->isize * 2]; // (double *) malloc0 (d->isize * sizeof (complex));

    if (d->inrate != d->internalrate)
        d->resamprun = 1;
    else
        d->resamprun = 0;

    d->inresamp  = RESAMPLE::create_resample (
        d->resamprun,
        d->bsize,
        d->in,
        d->inbuff,
        d->inrate,
        d->internalrate,
        0.0,
        0,
        2.0
    );
    RESAMPLE::setFCLow_resample (d->inresamp, 250.0);
    d->outresamp = RESAMPLE::create_resample (
        d->resamprun,
        d->isize,
        d->outbuff,
        d->out,
        d->internalrate,
        d->inrate,
        0.0,
        0,
        2.0
    );
    RESAMPLE::setFCLow_resample (d->outresamp, 200.0);
    d->incr = d->xsize / d->ovrlp;

    if (d->incr > d->isize)
        d->iasize = d->incr;
    else
        d->iasize = d->isize;

    d->iainidx = 0;
    d->iaoutidx = 0;
    d->inaccum = new double[d->iasize * 2]; // (double *) malloc0 (d->iasize * sizeof (double));
    d->nsamps = 0;

    if (d->incr > d->isize)
    {
        d->oasize = d->incr;
        d->oainidx = 0;
        d->oaoutidx = d->isize;
    }
    else
    {
        d->oasize = d->isize;
        d->oainidx = 0;
        d->oaoutidx = 0;
    }

    d->init_oaoutidx = d->oaoutidx;
    d->outaccum = new double[d->oasize * 2]; // (double *) malloc0 (d->oasize * sizeof (double));
}

SNBA* SNBA::create_snba (
    int run,
    float* in,
    float* out,
    int inrate,
    int internalrate,
    int bsize,
    int ovrlp,
    int xsize,
    int asize,
    int npasses,
    double k1,
    double k2,
    int b,
    int pre,
    int post,
    double pmultmin,
    double out_low_cut,
    double out_high_cut
)
{
    SNBA *d = new SNBA;
    d->run = run;
    d->in = in;
    d->out = out;
    d->inrate = inrate;
    d->internalrate = internalrate;
    d->bsize = bsize;
    d->ovrlp = ovrlp;
    d->xsize = xsize;
    d->exec.asize = asize;
    d->exec.npasses = npasses;
    d->sdet.k1 = k1;
    d->sdet.k2 = k2;
    d->sdet.b = b;
    d->sdet.pre = pre;
    d->sdet.post = post;
    d->scan.pmultmin = pmultmin;
    d->out_low_cut = out_low_cut;
    d->out_high_cut = out_high_cut;

    calc_snba (d);

    d->xbase        = new double[2 * d->xsize]; // (double *) malloc0 (2 * d->xsize * sizeof (double));
    d->xaux         = d->xbase + d->xsize;
    d->exec.a       = new double[d->xsize]; //(double *) malloc0 (d->xsize * sizeof (double));
    d->exec.v       = new double[d->xsize]; //(double *) malloc0 (d->xsize * sizeof (double));
    d->exec.detout  = new int[d->xsize]; //(int    *) malloc0 (d->xsize * sizeof (int));
    d->exec.savex   = new double[d->xsize]; //(double *) malloc0 (d->xsize * sizeof (double));
    d->exec.xHout   = new double[d->xsize]; //(double *) malloc0 (d->xsize * sizeof (double));
    d->exec.unfixed = new int[d->xsize]; //(int    *) malloc0 (d->xsize * sizeof (int));
    d->sdet.vp      = new double[d->xsize]; //(double *) malloc0 (d->xsize * sizeof (double));
    d->sdet.vpwr    = new double[d->xsize]; //(double *) malloc0 (d->xsize * sizeof (double));

    d->wrk.xHat_a1rows_max = d->xsize + d->exec.asize;
    d->wrk.xHat_a2cols_max = d->xsize + 2 * d->exec.asize;
    d->wrk.xHat_r          = new double[d->xsize]; // (double *) malloc0 (d->xsize * sizeof(double));
    d->wrk.xHat_ATAI       = new double[d->xsize * d->xsize]; // (double *) malloc0 (d->xsize * d->xsize * sizeof(double));
    d->wrk.xHat_A1         = new double[d->wrk.xHat_a1rows_max * d->xsize]; // (double *) malloc0 (d->wrk.xHat_a1rows_max * d->xsize * sizeof(double));
    d->wrk.xHat_A2         = new double[d->wrk.xHat_a1rows_max * d->wrk.xHat_a2cols_max]; // (double *) malloc0 (d->wrk.xHat_a1rows_max * d->wrk.xHat_a2cols_max * sizeof(double));
    d->wrk.xHat_P1         = new double[d->xsize * d->wrk.xHat_a2cols_max]; // (double *) malloc0 (d->xsize * d->wrk.xHat_a2cols_max * sizeof(double));
    d->wrk.xHat_P2         = new double[d->xsize]; // (double *) malloc0 (d->xsize * sizeof(double));
    d->wrk.trI_y           = new double[d->xsize - 1]; // (double *) malloc0 ((d->xsize - 1) * sizeof(double));
    d->wrk.trI_v           = new double[d->xsize - 1]; // (double *) malloc0 ((d->xsize - 1) * sizeof(double));
    d->wrk.dR_z            = new double[d->xsize - 2]; // (double *) malloc0 ((d->xsize - 2) * sizeof(double));
    d->wrk.asolve_r        = new double[d->exec.asize + 1]; // (double *) malloc0 ((d->exec.asize + 1) * sizeof(double));
    d->wrk.asolve_z        = new double[d->exec.asize + 1]; // (double *) malloc0 ((d->exec.asize + 1) * sizeof(double));

    return d;
}

void SNBA::decalc_snba (SNBA *d)
{
    RESAMPLE::destroy_resample (d->outresamp);
    RESAMPLE::destroy_resample (d->inresamp);
    delete[] (d->outbuff);
    delete[] (d->inbuff);
    delete[] (d->outaccum);
    delete[] (d->inaccum);
}

void SNBA::destroy_snba (SNBA *d)
{
    delete[] (d->wrk.xHat_r);
    delete[] (d->wrk.xHat_ATAI);
    delete[] (d->wrk.xHat_A1);
    delete[] (d->wrk.xHat_A2);
    delete[] (d->wrk.xHat_P1);
    delete[] (d->wrk.xHat_P2);
    delete[] (d->wrk.trI_y);
    delete[] (d->wrk.trI_v);
    delete[] (d->wrk.dR_z);
    delete[] (d->wrk.asolve_r);
    delete[] (d->wrk.asolve_z);

    delete[] (d->sdet.vpwr);
    delete[] (d->sdet.vp);
    delete[] (d->exec.unfixed);
    delete[] (d->exec.xHout);
    delete[] (d->exec.savex);
    delete[] (d->exec.detout);
    delete[] (d->exec.v);
    delete[] (d->exec.a);

    delete[] (d->xbase);

    decalc_snba (d);

    delete[] (d);
}

void SNBA::flush_snba (SNBA *d)
{
    d->iainidx = 0;
    d->iaoutidx = 0;
    d->nsamps = 0;
    d->oainidx = 0;
    d->oaoutidx = d->init_oaoutidx;

    memset (d->inaccum,      0, d->iasize * sizeof (double));
    memset (d->outaccum,     0, d->oasize * sizeof (double));
    memset (d->xaux,         0, d->xsize  * sizeof (double));
    memset (d->exec.a,       0, d->xsize  * sizeof (double));
    memset (d->exec.v,       0, d->xsize  * sizeof (double));
    memset (d->exec.detout,  0, d->xsize  * sizeof (int));
    memset (d->exec.savex,   0, d->xsize  * sizeof (double));
    memset (d->exec.xHout,   0, d->xsize  * sizeof (double));
    memset (d->exec.unfixed, 0, d->xsize  * sizeof (int));
    memset (d->sdet.vp,      0, d->xsize  * sizeof (double));
    memset (d->sdet.vpwr,    0, d->xsize  * sizeof (double));

    std::fill(d->inbuff,  d->inbuff + d->isize  * 2,  0);
    std::fill(d->outbuff, d->outbuff + d->isize  * 2, 0);

    RESAMPLE::flush_resample (d->inresamp);
    RESAMPLE::flush_resample (d->outresamp);
}

void SNBA::setBuffers_snba (SNBA *a, float* in, float* out)
{
    decalc_snba (a);
    a->in = in;
    a->out = out;
    calc_snba (a);
}

void SNBA::setSamplerate_snba (SNBA *a, int rate)
{
    decalc_snba (a);
    a->inrate = rate;
    calc_snba (a);
}

void SNBA::setSize_snba (SNBA *a, int size)
{
    decalc_snba (a);
    a->bsize = size;
    calc_snba (a);
}

void SNBA::ATAc0 (int n, int nr, double* A, double* r)
{
    int i, j;
    memset(r, 0, n * sizeof (double));

    for (i = 0; i < n; i++)
    {
        for (j = 0; j < nr; j++)
            r[i] += A[j * n + i] * A[j * n + 0];
    }
}

void SNBA::multA1TA2(double* a1, double* a2, int m, int n, int q, double* c)
{
    int i, j, k;
    int p = q - m;
    memset (c, 0, m * n * sizeof (double));

    for (i = 0; i < m; i++)
    {
        for (j = 0; j < n; j++)
        {
            if (j < p)
            {
                for (k = i; k <= std::min(i + p, j); k++)
                    c[i * n + j] += a1[k * m + i] * a2[k * n + j];
            }
            if (j >= n - p)
            {
                for (k = std::max(i, q - (n - j)); k <= i + p; k++)
                    c[i * n + j] += a1[k * m + i] * a2[k * n + j];
            }
        }
    }
}

void SNBA::multXKE(double* a, double* xk, int m, int q, int p, double* vout)
{
    int i, k;
    memset (vout, 0, m * sizeof (double));

    for (i = 0; i < m; i++)
    {
        for (k = i; k < p; k++)
            vout[i] += a[i * q + k] * xk[k];
        for (k = q - p; k <= q - m + i; k++)
            vout[i] += a[i * q + k] * xk[k];
    }
}

void SNBA::multAv(double* a, double* v, int m, int q, double* vout)
{
    int i, k;
    memset (vout, 0, m * sizeof (double));

    for (i = 0; i < m; i++)
    {
        for (k = 0; k < q; k++)
            vout[i] += a[i * q + k] * v[k];
    }
}

void SNBA::xHat(
    int xusize,
    int asize,
    double* xk,
    double* a,
    double* xout,
    double* r,
    double* ATAI,
    double* A1,
    double* A2,
    double* P1,
    double* P2,
    double* trI_y,
    double* trI_v,
    double* dR_z
)
{
    int i, j, k;
    int a1rows = xusize + asize;
    int a2cols = xusize + 2 * asize;
    memset (r,    0, xusize          * sizeof(double));     // work space
    memset (ATAI, 0, xusize * xusize * sizeof(double));     // work space
    memset (A1,   0, a1rows * xusize * sizeof(double));     // work space
    memset (A2,   0, a1rows * a2cols * sizeof(double));     // work space
    memset (P1,   0, xusize * a2cols * sizeof(double));     // work space
    memset (P2,   0, xusize          * sizeof(double));     // work space

    for (i = 0; i < xusize; i++)
    {
        A1[i * xusize + i] = 1.0;
        k = i + 1;
        for (j = k; j < k + asize; j++)
            A1[j * xusize + i] = - a[j - k];
    }

    for (i = 0; i < asize; i++)
        {
            for (k = asize - i - 1, j = 0; k < asize; k++, j++)
                A2[j * a2cols + i] = a[k];
        }
    for (i = asize + xusize; i < 2 * asize + xusize; i++)
        {
            A2[(i - asize) * a2cols + i] = - 1.0;
            for (j = i - asize + 1, k = 0; j < xusize + asize; j++, k++)
                A2[j * a2cols + i] = a[k];
        }

    ATAc0(xusize, xusize + asize, A1, r);
    LMathd::trI(xusize, r, ATAI, trI_y, trI_v, dR_z);
    multA1TA2(A1, A2, xusize, 2 * asize + xusize, xusize + asize, P1);
    multXKE(P1, xk, xusize, xusize + 2 * asize, asize, P2);
    multAv(ATAI, P2, xusize, xusize, xout);
}

void SNBA::invf(int xsize, int asize, double* a, double* x, double* v)
{
    int i, j;
    memset (v, 0, xsize * sizeof (double));

    for (i = asize; i < xsize - asize; i++)
    {
        for (j = 0; j < asize; j++)
            v[i] += a[j] * (x[i - 1 - j] + x[i + 1 + j]);
        v[i] = x[i] - 0.5 * v[i];
    }
    for (i = xsize - asize; i < xsize; i++)
    {
        for (j = 0; j < asize; j++)
            v[i] += a[j] * x[i - 1 - j];
        v[i] = x[i] - v[i];
    }
}

void SNBA::det(SNBA *d, int asize, double* v, int* detout)
{
    int i, j;
    double medpwr;
    double t1, t2;
    int bstate, bcount, bsamp;

    for (i = asize, j = 0; i < d->xsize; i++, j++)
    {
        d->sdet.vpwr[i] = v[i] * v[i];
        d->sdet.vp[j] = d->sdet.vpwr[i];
    }

    LMathd::median(d->xsize - asize, d->sdet.vp, &medpwr);
    t1 = d->sdet.k1 * medpwr;
    t2 = 0.0;

    for (i = asize; i < d->xsize; i++)
    {
        if (d->sdet.vpwr[i] <= t1)
            t2 += d->sdet.vpwr[i];
        else if (d->sdet.vpwr[i] <= 2.0 * t1)
            t2 += 2.0 * t1 - d->sdet.vpwr[i];
    }
    t2 *= d->sdet.k2 / (double) (d->xsize - asize);

    for (i = asize; i < d->xsize; i++)
    {
        if (d->sdet.vpwr[i] > t2)
            detout[i] = 1;
        else
            detout[i] = 0;
    }

    bstate = 0;
    bcount = 0;
    bsamp = 0;

    for (i = asize; i < d->xsize; i++)
    {
        switch (bstate)
        {
            case 0:
                if (detout[i] == 1)
                    bstate = 1;

                break;

            case 1:
                if (detout[i] == 0)
                {
                    bstate = 2;
                    bsamp = i;
                    bcount = 1;
                }

                break;

            case 2:
                ++bcount;

                if (bcount > d->sdet.b)
                {
                    if (detout[i] == 1)
                        bstate = 1;
                    else
                        bstate = 0;
                }
                else if (detout[i] == 1)
                {
                    for (j = bsamp; j < bsamp + bcount - 1; j++)
                        detout[j] = 1;
                    bstate = 1;
                }

                break;
        }
    }

    for (i = asize; i < d->xsize; i++)
    {
        if (detout[i] == 1)
        {
            for (j = i - 1; j > i - 1 - d->sdet.pre; j--)
            {
                if (j >= asize)
                    detout[j] = 1;
            }
        }
    }

    for (i = d->xsize - 1; i >= asize; i--)
    {
        if (detout[i] == 1)
        {
            for (j = i + 1; j < i + 1 + d->sdet.post; j++)
            {
                if (j < d->xsize)
                    detout[j] = 1;
            }
        }
    }
}

int SNBA::scanFrame(
    int xsize,
    int pval,
    double pmultmin,
    int* det,
    int* bimp,
    int* limp,
    int* befimp,
    int* aftimp,
    int* p_opt,
    int* next
)
{
    int inflag = 0;
    int i = 0, j = 0, k = 0;
    int nimp = 0;
    double td;
    int ti;
    double merit[MAXIMP] = { 0 };
    int nextlist[MAXIMP];
    memset (befimp, 0, MAXIMP * sizeof (int));
    memset (aftimp, 0, MAXIMP * sizeof (int));

    while (i < xsize && nimp < MAXIMP)
    {
        if (det[i] == 1 && inflag == 0)
        {
            inflag = 1;
            bimp[nimp] = i;
            limp[nimp] = 1;
            nimp++;
        }
        else if (det[i] == 1)
        {
            limp[nimp - 1]++;
        }
        else
        {
            inflag = 0;
            befimp[nimp]++;
            if (nimp > 0)
                aftimp[nimp - 1]++;
        }

        i++;
    }

    for (i = 0; i < nimp; i++)
    {
        if (befimp[i] < aftimp[i])
            p_opt[i] = befimp[i];
        else
            p_opt[i] = aftimp[i];

        if (p_opt[i] > pval)
            p_opt[i] = pval;

        if (p_opt[i] < (int)(pmultmin * limp[i]))
            p_opt[i] = -1;
    }

    for (i = 0; i < nimp; i++)
    {
        merit[i] = (double)p_opt[i] / (double)limp[i];
        nextlist[i] = i;
    }

    for (j = 0; j < nimp - 1; j++)
    {
        for (k = 0; k < nimp - j - 1; k++)
        {
            if (merit[k] < merit[k + 1])
            {
                td = merit[k];
                ti = nextlist[k];
                merit[k] = merit[k + 1];
                nextlist[k] = nextlist[k + 1];
                merit[k + 1] = td;
                nextlist[k + 1] = ti;
            }
        }
    }

    i = 1;

    if (nimp > 0)
    {
        while (merit[i] == merit[0] && i < nimp)
            i++;
    }

    for (j = 0; j < i - 1; j++)
    {
        for (k = 0; k < i - j - 1; k++)
        {
            if (limp[nextlist[k]] < limp[nextlist[k + 1]])
            {
                td = merit[k];
                ti = nextlist[k];
                merit[k] = merit[k + 1];
                nextlist[k] = nextlist[k + 1];
                merit[k + 1] = td;
                nextlist[k + 1] = ti;
            }
        }
    }

    *next = nextlist[0];
    return nimp;
}

void SNBA::execFrame(SNBA *d, double* x)
{
    int i, k;
    int pass;
    int nimp;
    int bimp[MAXIMP];
    int limp[MAXIMP];
    int befimp[MAXIMP];
    int aftimp[MAXIMP];
    int p_opt[MAXIMP];
    int next = 0;
    int p;
    memcpy (d->exec.savex, x, d->xsize * sizeof (double));
    LMathd::asolve(d->xsize, d->exec.asize, x, d->exec.a, d->wrk.asolve_r, d->wrk.asolve_z);
    invf(d->xsize, d->exec.asize, d->exec.a, x, d->exec.v);
    det(d, d->exec.asize, d->exec.v, d->exec.detout);

    for (i = 0; i < d->xsize; i++)
    {
        if (d->exec.detout[i] != 0)
            x[i] = 0.0;
    }

    nimp = scanFrame(d->xsize, d->exec.asize, d->scan.pmultmin, d->exec.detout, bimp, limp, befimp, aftimp, p_opt, &next);

    for (pass = 0; pass < d->exec.npasses; pass++)
    {
        memcpy (d->exec.unfixed, d->exec.detout, d->xsize * sizeof (int));

        for (k = 0; k < nimp; k++)
        {
            if (k > 0)
                scanFrame(d->xsize, d->exec.asize, d->scan.pmultmin, d->exec.unfixed, bimp, limp, befimp, aftimp, p_opt, &next);

            if ((p = p_opt[next]) > 0)
            {
                LMathd::asolve(d->xsize, p, x, d->exec.a, d->wrk.asolve_r, d->wrk.asolve_z);
                xHat(limp[next], p, &x[bimp[next] - p], d->exec.a, d->exec.xHout,
                    d->wrk.xHat_r, d->wrk.xHat_ATAI, d->wrk.xHat_A1, d->wrk.xHat_A2,
                    d->wrk.xHat_P1, d->wrk.xHat_P2, d->wrk.trI_y, d->wrk.trI_v, d->wrk.dR_z);
                memcpy (&x[bimp[next]], d->exec.xHout, limp[next] * sizeof (double));
                memset (&d->exec.unfixed[bimp[next]], 0, limp[next] * sizeof (int));
            }
            else
            {
                memcpy (&x[bimp[next]], &d->exec.savex[bimp[next]], limp[next] * sizeof (double));
            }
        }
    }
}

void SNBA::xsnba (SNBA *d)
{
    if (d->run)
    {
        int i;
        RESAMPLE::xresample (d->inresamp);

        for (i = 0; i < 2 * d->isize; i += 2)
        {
            d->inaccum[d->iainidx] = d->inbuff[i];
            d->iainidx = (d->iainidx + 1) % d->iasize;
        }

        d->nsamps += d->isize;

        while (d->nsamps >= d->incr)
        {
            memcpy (&d->xaux[d->xsize - d->incr], &d->inaccum[d->iaoutidx], d->incr * sizeof (double));
            execFrame (d, d->xaux);
            d->iaoutidx = (d->iaoutidx + d->incr) % d->iasize;
            d->nsamps -= d->incr;
            memcpy (&d->outaccum[d->oainidx], d->xaux, d->incr * sizeof (double));
            d->oainidx = (d->oainidx + d->incr) % d->oasize;
            memmove (d->xbase, &d->xbase[d->incr], (2 * d->xsize - d->incr) * sizeof (double));
        }

        for (i = 0; i < d->isize; i++)
        {
            d->outbuff[2 * i + 0] = d->outaccum[d->oaoutidx];
            d->outbuff[2 * i + 1] = 0.0;
            d->oaoutidx = (d->oaoutidx + 1) % d->oasize;
        }

        RESAMPLE::xresample (d->outresamp);
    }
    else if (d->out != d->in)
    {
        std::copy(d->in, d->in + d->bsize * 2, d->out);
    }
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void SNBA::SetSNBARun (RXA& rxa, int run)
{
    SNBA *a = rxa.snba;

    if (a->run != run)
    {
        RXA::bpsnbaCheck (rxa, rxa.mode, rxa.ndb->master_run);
        RXA::bp1Check (
            rxa,
            rxa.amd->run,
            run,
            rxa.emnr->run,
            rxa.anf->run,
            rxa.anr->run
        );
        a->run = run;
        RXA::bp1Set (rxa);
        RXA::bpsnbaSet (rxa);
    }
}

void SNBA::SetSNBAovrlp (RXA& rxa, int ovrlp)
{
    decalc_snba (rxa.snba);
    rxa.snba->ovrlp = ovrlp;
    calc_snba (rxa.snba);
}

void SNBA::SetSNBAasize (RXA& rxa, int size)
{
    rxa.snba->exec.asize = size;
}

void SNBA::SetSNBAnpasses (RXA& rxa, int npasses)
{
    rxa.snba->exec.npasses = npasses;
}

void SNBA::SetSNBAk1 (RXA& rxa, double k1)
{
    rxa.snba->sdet.k1 = k1;
}

void SNBA::SetSNBAk2 (RXA& rxa, double k2)
{
    rxa.snba->sdet.k2 = k2;
}

void SNBA::SetSNBAbridge (RXA& rxa, int bridge)
{
    rxa.snba->sdet.b = bridge;
}

void SNBA::SetSNBApresamps (RXA& rxa, int presamps)
{
    rxa.snba->sdet.pre = presamps;
}

void SNBA::SetSNBApostsamps (RXA& rxa, int postsamps)
{
    rxa.snba->sdet.post = postsamps;
}

void SNBA::SetSNBApmultmin (RXA& rxa, double pmultmin)
{
    rxa.snba->scan.pmultmin = pmultmin;
}

void SNBA::SetSNBAOutputBandwidth (RXA& rxa, double flow, double fhigh)
{
    SNBA *a = rxa.snba;
    RESAMPLE *d = a->outresamp;
    double f_low, f_high;

    if (flow >= 0 && fhigh >= 0)
    {
        if (fhigh <  a->out_low_cut)
            fhigh =  a->out_low_cut;

        if (flow  > a->out_high_cut)
            flow  = a->out_high_cut;

        f_low  = std::max ( a->out_low_cut, flow);
        f_high = std::min (a->out_high_cut, fhigh);
    }
    else if (flow <= 0 && fhigh <= 0)
    {
        if (flow  >  -a->out_low_cut)
            flow  =  -a->out_low_cut;

        if (fhigh < -a->out_high_cut)
            fhigh = -a->out_high_cut;

        f_low  = std::max ( a->out_low_cut, -fhigh);
        f_high = std::min (a->out_high_cut, -flow);
    }
    else if (flow < 0 && fhigh > 0)
    {
        double absmax = std::max (-flow, fhigh);

        if (absmax <  a->out_low_cut)
            absmax =  a->out_low_cut;

        f_low = a->out_low_cut;
        f_high = std::min (a->out_high_cut, absmax);
    }

    RESAMPLE::setBandwidth_resample (d, f_low, f_high);
}

} // namespace
