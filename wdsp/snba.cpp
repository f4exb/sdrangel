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

#define MAXIMP          256

namespace WDSP {

void SNBA::calc()
{
    if (inrate >= internalrate)
        isize = bsize / (inrate / internalrate);
    else
        isize = bsize * (internalrate / inrate);

    inbuff  = new float[isize * 2]; // (double *) malloc0 (isize * sizeof (complex));
    outbuff = new float[isize * 2]; // (double *) malloc0 (isize * sizeof (complex));

    if (inrate != internalrate)
        resamprun = 1;
    else
        resamprun = 0;

    inresamp  = new RESAMPLE(
        resamprun,
        bsize,
        in,
        inbuff,
        inrate,
        internalrate,
        0.0,
        0,
        2.0
    );
    inresamp->setFCLow(250.0);
    outresamp = new RESAMPLE(
        resamprun,
        isize,
        outbuff,
        out,
        internalrate,
        inrate,
        0.0,
        0,
        2.0
    );
    outresamp->setFCLow(200.0);
    incr = xsize / ovrlp;

    if (incr > isize)
        iasize = incr;
    else
        iasize = isize;

    iainidx = 0;
    iaoutidx = 0;
    inaccum = new double[iasize * 2]; // (double *) malloc0 (iasize * sizeof (double));
    nsamps = 0;

    if (incr > isize)
    {
        oasize = incr;
        oainidx = 0;
        oaoutidx = isize;
    }
    else
    {
        oasize = isize;
        oainidx = 0;
        oaoutidx = 0;
    }

    init_oaoutidx = oaoutidx;
    outaccum = new double[oasize * 2]; // (double *) malloc0 (oasize * sizeof (double));
}

SNBA::SNBA(
    int _run,
    float* _in,
    float* _out,
    int _inrate,
    int _internalrate,
    int _bsize,
    int _ovrlp,
    int _xsize,
    int _asize,
    int _npasses,
    double _k1,
    double _k2,
    int _b,
    int _pre,
    int _post,
    double _pmultmin,
    double _out_low_cut,
    double _out_high_cut
) :
    run(_run),
    in(_in),
    out(_out),
    inrate(_inrate),
    internalrate(_internalrate),
    bsize(_bsize),
    xsize(_xsize),
    ovrlp(_ovrlp),
    incr(0),
    iasize(0),
    iainidx(0),
    iaoutidx(0),
    inaccum(nullptr),
    xbase(nullptr),
    xaux(nullptr),
    nsamps(0),
    oasize(0),
    oainidx(0),
    oaoutidx(0),
    init_oaoutidx(0),
    outaccum(nullptr),
    resamprun(0),
    isize(0),
    inresamp(nullptr),
    outresamp(nullptr),
    inbuff(nullptr),
    outbuff(nullptr),
    out_low_cut(_out_low_cut),
    out_high_cut(_out_high_cut)
{
    exec.asize = _asize;
    exec.npasses = _npasses;
    sdet.k1 = _k1;
    sdet.k2 = _k2;
    sdet.b = _b;
    sdet.pre = _pre;
    sdet.post = _post;
    scan.pmultmin = _pmultmin;

    calc();

    xbase        = new double[2 * xsize]; // (double *) malloc0 (2 * xsize * sizeof (double));
    xaux         = xbase + xsize;
    exec.a       = new double[xsize]; //(double *) malloc0 (xsize * sizeof (double));
    exec.v       = new double[xsize]; //(double *) malloc0 (xsize * sizeof (double));
    exec.detout  = new int[xsize]; //(int    *) malloc0 (xsize * sizeof (int));
    exec.savex   = new double[xsize]; //(double *) malloc0 (xsize * sizeof (double));
    exec.xHout   = new double[xsize]; //(double *) malloc0 (xsize * sizeof (double));
    exec.unfixed = new int[xsize]; //(int    *) malloc0 (xsize * sizeof (int));
    sdet.vp      = new double[xsize]; //(double *) malloc0 (xsize * sizeof (double));
    sdet.vpwr    = new double[xsize]; //(double *) malloc0 (xsize * sizeof (double));

    wrk.xHat_a1rows_max = xsize + exec.asize;
    wrk.xHat_a2cols_max = xsize + 2 * exec.asize;
    wrk.xHat_r          = new double[xsize]; // (double *) malloc0 (xsize * sizeof(double));
    wrk.xHat_ATAI       = new double[xsize * xsize]; // (double *) malloc0 (xsize * xsize * sizeof(double));
    wrk.xHat_A1         = new double[wrk.xHat_a1rows_max * xsize]; // (double *) malloc0 (wrk.xHat_a1rows_max * xsize * sizeof(double));
    wrk.xHat_A2         = new double[wrk.xHat_a1rows_max * wrk.xHat_a2cols_max]; // (double *) malloc0 (wrk.xHat_a1rows_max * wrk.xHat_a2cols_max * sizeof(double));
    wrk.xHat_P1         = new double[xsize * wrk.xHat_a2cols_max]; // (double *) malloc0 (xsize * wrk.xHat_a2cols_max * sizeof(double));
    wrk.xHat_P2         = new double[xsize]; // (double *) malloc0 (xsize * sizeof(double));
    wrk.trI_y           = new double[xsize - 1]; // (double *) malloc0 ((xsize - 1) * sizeof(double));
    wrk.trI_v           = new double[xsize - 1]; // (double *) malloc0 ((xsize - 1) * sizeof(double));
    wrk.dR_z            = new double[xsize - 2]; // (double *) malloc0 ((xsize - 2) * sizeof(double));
    wrk.asolve_r        = new double[exec.asize + 1]; // (double *) malloc0 ((exec.asize + 1) * sizeof(double));
    wrk.asolve_z        = new double[exec.asize + 1]; // (double *) malloc0 ((exec.asize + 1) * sizeof(double));
}

void SNBA::decalc()
{
    delete (outresamp);
    delete (inresamp);
    delete[] (outbuff);
    delete[] (inbuff);
    delete[] (outaccum);
    delete[] (inaccum);
}

SNBA::~SNBA()
{
    delete[] (wrk.xHat_r);
    delete[] (wrk.xHat_ATAI);
    delete[] (wrk.xHat_A1);
    delete[] (wrk.xHat_A2);
    delete[] (wrk.xHat_P1);
    delete[] (wrk.xHat_P2);
    delete[] (wrk.trI_y);
    delete[] (wrk.trI_v);
    delete[] (wrk.dR_z);
    delete[] (wrk.asolve_r);
    delete[] (wrk.asolve_z);

    delete[] (sdet.vpwr);
    delete[] (sdet.vp);
    delete[] (exec.unfixed);
    delete[] (exec.xHout);
    delete[] (exec.savex);
    delete[] (exec.detout);
    delete[] (exec.v);
    delete[] (exec.a);

    delete[] (xbase);

    decalc();
}

void SNBA::flush()
{
    iainidx = 0;
    iaoutidx = 0;
    nsamps = 0;
    oainidx = 0;
    oaoutidx = init_oaoutidx;

    memset (inaccum,      0, iasize * sizeof (double));
    memset (outaccum,     0, oasize * sizeof (double));
    memset (xaux,         0, xsize  * sizeof (double));
    memset (exec.a,       0, xsize  * sizeof (double));
    memset (exec.v,       0, xsize  * sizeof (double));
    memset (exec.detout,  0, xsize  * sizeof (int));
    memset (exec.savex,   0, xsize  * sizeof (double));
    memset (exec.xHout,   0, xsize  * sizeof (double));
    memset (exec.unfixed, 0, xsize  * sizeof (int));
    memset (sdet.vp,      0, xsize  * sizeof (double));
    memset (sdet.vpwr,    0, xsize  * sizeof (double));

    std::fill(inbuff,  inbuff + isize  * 2,  0);
    std::fill(outbuff, outbuff + isize  * 2, 0);

    inresamp->flush();
    outresamp->flush();
}

void SNBA::setBuffers(float* _in, float* _out)
{
    decalc();
    in = _in;
    out = _out;
    calc();
}

void SNBA::setSamplerate(int rate)
{
    decalc();
    inrate = rate;
    calc();
}

void SNBA::setSize(int size)
{
    decalc();
    bsize = size;
    calc();
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

void SNBA::det(int asize, double* v, int* detout)
{
    int i, j;
    double medpwr;
    double t1, t2;
    int bstate, bcount, bsamp;

    for (i = asize, j = 0; i < xsize; i++, j++)
    {
        sdet.vpwr[i] = v[i] * v[i];
        sdet.vp[j] = sdet.vpwr[i];
    }

    LMathd::median(xsize - asize, sdet.vp, &medpwr);
    t1 = sdet.k1 * medpwr;
    t2 = 0.0;

    for (i = asize; i < xsize; i++)
    {
        if (sdet.vpwr[i] <= t1)
            t2 += sdet.vpwr[i];
        else if (sdet.vpwr[i] <= 2.0 * t1)
            t2 += 2.0 * t1 - sdet.vpwr[i];
    }
    t2 *= sdet.k2 / (double) (xsize - asize);

    for (i = asize; i < xsize; i++)
    {
        if (sdet.vpwr[i] > t2)
            detout[i] = 1;
        else
            detout[i] = 0;
    }

    bstate = 0;
    bcount = 0;
    bsamp = 0;

    for (i = asize; i < xsize; i++)
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

                if (bcount > sdet.b)
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

    for (i = asize; i < xsize; i++)
    {
        if (detout[i] == 1)
        {
            for (j = i - 1; j > i - 1 - sdet.pre; j--)
            {
                if (j >= asize)
                    detout[j] = 1;
            }
        }
    }

    for (i = xsize - 1; i >= asize; i--)
    {
        if (detout[i] == 1)
        {
            for (j = i + 1; j < i + 1 + sdet.post; j++)
            {
                if (j < xsize)
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

void SNBA::execFrame(double* x)
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
    memcpy (exec.savex, x, xsize * sizeof (double));
    LMathd::asolve(xsize, exec.asize, x, exec.a, wrk.asolve_r, wrk.asolve_z);
    invf(xsize, exec.asize, exec.a, x, exec.v);
    det(exec.asize, exec.v, exec.detout);

    for (i = 0; i < xsize; i++)
    {
        if (exec.detout[i] != 0)
            x[i] = 0.0;
    }

    nimp = scanFrame(xsize, exec.asize, scan.pmultmin, exec.detout, bimp, limp, befimp, aftimp, p_opt, &next);

    for (pass = 0; pass < exec.npasses; pass++)
    {
        memcpy (exec.unfixed, exec.detout, xsize * sizeof (int));

        for (k = 0; k < nimp; k++)
        {
            if (k > 0)
                scanFrame(xsize, exec.asize, scan.pmultmin, exec.unfixed, bimp, limp, befimp, aftimp, p_opt, &next);

            if ((p = p_opt[next]) > 0)
            {
                LMathd::asolve(xsize, p, x, exec.a, wrk.asolve_r, wrk.asolve_z);
                xHat(
                    limp[next],
                    p,
                    &x[bimp[next] - p],
                    exec.a,
                    exec.xHout,
                    wrk.xHat_r,
                    wrk.xHat_ATAI,
                    wrk.xHat_A1,
                    wrk.xHat_A2,
                    wrk.xHat_P1,
                    wrk.xHat_P2,
                    wrk.trI_y,
                    wrk.trI_v,
                    wrk.dR_z
                );
                memcpy (&x[bimp[next]], exec.xHout, limp[next] * sizeof (double));
                memset (&exec.unfixed[bimp[next]], 0, limp[next] * sizeof (int));
            }
            else
            {
                memcpy (&x[bimp[next]], &exec.savex[bimp[next]], limp[next] * sizeof (double));
            }
        }
    }
}

void SNBA::execute()
{
    if (run)
    {
        int i;
        inresamp->execute();

        for (i = 0; i < 2 * isize; i += 2)
        {
            inaccum[iainidx] = inbuff[i];
            iainidx = (iainidx + 1) % iasize;
        }

        nsamps += isize;

        while (nsamps >= incr)
        {
            memcpy (&xaux[xsize - incr], &inaccum[iaoutidx], incr * sizeof (double));
            execFrame (xaux);
            iaoutidx = (iaoutidx + incr) % iasize;
            nsamps -= incr;
            memcpy (&outaccum[oainidx], xaux, incr * sizeof (double));
            oainidx = (oainidx + incr) % oasize;
            memmove (xbase, &xbase[incr], (2 * xsize - incr) * sizeof (double));
        }

        for (i = 0; i < isize; i++)
        {
            outbuff[2 * i + 0] = outaccum[oaoutidx];
            outbuff[2 * i + 1] = 0.0;
            oaoutidx = (oaoutidx + 1) % oasize;
        }

        outresamp->execute();
    }
    else if (out != in)
    {
        std::copy(in, in + bsize * 2, out);
    }
}

/********************************************************************************************************
*                                                                                                       *
*                                        Public Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void SNBA::setOvrlp(int _ovrlp)
{
    decalc();
    ovrlp = _ovrlp;
    calc();
}

void SNBA::setAsize(int size)
{
    exec.asize = size;
}

void SNBA::setNpasses(int npasses)
{
    exec.npasses = npasses;
}

void SNBA::setK1(double k1)
{
    sdet.k1 = k1;
}

void SNBA::setK2(double k2)
{
    sdet.k2 = k2;
}

void SNBA::setBridge(int bridge)
{
    sdet.b = bridge;
}

void SNBA::setPresamps(int presamps)
{
    sdet.pre = presamps;
}

void SNBA::setPostsamps(int postsamps)
{
    sdet.post = postsamps;
}

void SNBA::setPmultmin(double pmultmin)
{
    scan.pmultmin = pmultmin;
}

void SNBA::setOutputBandwidth(double flow, double fhigh)
{
    double f_low, f_high;

    if (flow >= 0 && fhigh >= 0)
    {
        if (fhigh <  out_low_cut)
            fhigh =  out_low_cut;

        if (flow  > out_high_cut)
            flow  = out_high_cut;

        f_low  = std::max ( out_low_cut, flow);
        f_high = std::min (out_high_cut, fhigh);
    }
    else if (flow <= 0 && fhigh <= 0)
    {
        if (flow  >  -out_low_cut)
            flow  =  -out_low_cut;

        if (fhigh < -out_high_cut)
            fhigh = -out_high_cut;

        f_low  = std::max ( out_low_cut, -fhigh);
        f_high = std::min (out_high_cut, -flow);
    }
    else if (flow < 0 && fhigh > 0)
    {
        double absmax = std::max (-flow, fhigh);

        if (absmax <  out_low_cut)
            absmax =  out_low_cut;

        f_low = out_low_cut;
        f_high = std::min (out_high_cut, absmax);
    }

    outresamp->setBandwidth(f_low, f_high);
}

} // namespace
