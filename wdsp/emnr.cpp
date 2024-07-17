/*  emnr.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2015 Warren Pratt, NR0V
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
#include <limits>

#include "comm.hpp"
#include "calculus.hpp"
#include "emnr.hpp"
#include "amd.hpp"
#include "anr.hpp"
#include "anf.hpp"
#include "snba.hpp"
#include "bandpass.hpp"
#include "RXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                           Special Functions                                           *
*                                                                                                       *
********************************************************************************************************/

// MODIFIED BESSEL FUNCTIONS OF THE 0TH AND 1ST ORDERS, Polynomial Approximations
// M. Abramowitz and I. Stegun, Eds., "Handbook of Mathematical Functions."  Washington, DC:  National
//      Bureau of Standards, 1964.
// Shanjie Zhang and Jianming Jin, "Computation of Special Functions."  New York, NY, John Wiley and Sons,
//      Inc., 1996.  [Sample code given in FORTRAN]

double EMNR::bessI0 (double x)
{
    double res, p;
    if (x == 0.0)
        res = 1.0;
    else
    {
        if (x < 0.0) x = -x;
        if (x <= 3.75)
        {
            p = x / 3.75;
            p = p * p;
            res = ((((( 0.0045813  * p
                + 0.0360768) * p
                + 0.2659732) * p
                + 1.2067492) * p
                + 3.0899424) * p
                + 3.5156229) * p
                + 1.0;
        }
        else
        {
            p = 3.75 / x;
            res = exp (x) / sqrt (x)
                  * (((((((( + 0.00392377  * p
                    - 0.01647633) * p
                    + 0.02635537) * p
                    - 0.02057706) * p
                    + 0.00916281) * p
                    - 0.00157565) * p
                    + 0.00225319) * p
                    + 0.01328592) * p
                    + 0.39894228);
        }
    }
    return res;
}

double EMNR::bessI1 (double x)
{

    double res, p;
    if (x == 0.0)
        res = 0.0;
    else
    {
        if (x < 0.0) x = -x;
        if (x <= 3.75)
        {
            p = x / 3.75;
            p = p * p;
            res = x
                  * (((((( 0.00032411  * p
                    + 0.00301532) * p
                    + 0.02658733) * p
                    + 0.15084934) * p
                    + 0.51498869) * p
                    + 0.87890594) * p
                    + 0.5);
        }
        else
        {
            p = 3.75 / x;
            res = exp (x) / sqrt (x)
                  * (((((((( - 0.00420059  * p
                    + 0.01787654) * p
                    - 0.02895312) * p
                    + 0.02282967) * p
                    - 0.01031555) * p
                    + 0.00163801) * p
                    - 0.00362018) * p
                    - 0.03988024) * p
                    + 0.39894228);
        }
    }
    return res;
}

// EXPONENTIAL INTEGRAL, E1(x)
// M. Abramowitz and I. Stegun, Eds., "Handbook of Mathematical Functions."  Washington, DC:  National
//      Bureau of Standards, 1964.
// Shanjie Zhang and Jianming Jin, "Computation of Special Functions."  New York, NY, John Wiley and Sons,
//      Inc., 1996.  [Sample code given in FORTRAN]

double EMNR::e1xb (double x)
{
    double e1, ga, r, t, t0;
    int k, m;
    if (x == 0.0)
        e1 = 1.0e300;
    else if (x <= 1.0)
    {
        e1 = 1.0;
        r = 1.0;

        for (k = 1; k <= 25; k++)
        {
            r = -r * k * x / ((k + 1.0)*(k + 1.0));
            e1 = e1 + r;
            if ( fabs (r) <= fabs (e1) * 1.0e-15 )
                break;
        }

        ga = 0.5772156649015328;
        e1 = - ga - log (x) + x * e1;
    }
    else
    {
        m = 20 + (int)(80.0 / x);
        t0 = 0.0;
        for (k = m; k >= 1; k--)
            t0 = (float)k / (1.0 + k / (x + t0));
        t = 1.0 / (x + t0);
        e1 = exp (- x) * t;
    }
    return e1;
}

/********************************************************************************************************
*                                                                                                       *
*                                           Main Body of Code                                           *
*                                                                                                       *
********************************************************************************************************/

void EMNR::calc_window (EMNR *a)
{
    int i;
    float arg, sum, inv_coherent_gain;
    switch (a->wintype)
    {
    case 0:
        arg = 2.0 * PI / (float)a->fsize;
        sum = 0.0;
        for (i = 0; i < a->fsize; i++)
        {
            a->window[i] = sqrt (0.54 - 0.46 * cos((float)i * arg));
            sum += a->window[i];
        }
        inv_coherent_gain = (float)a->fsize / sum;
        for (i = 0; i < a->fsize; i++)
            a->window[i] *= inv_coherent_gain;
        break;
    }
}

void EMNR::interpM (double* res, double x, int nvals, double* xvals, double* yvals)
{
    if (x <= xvals[0])
        *res = yvals[0];
    else if (x >= xvals[nvals - 1])
        *res = yvals[nvals - 1];
    else
    {
        int idx = 0;
        double xllow, xlhigh, frac;
        while (x >= xvals[idx])  idx++;
        xllow = log10 (xvals[idx - 1]);
        xlhigh = log10(xvals[idx]);
        frac = (log10 (x) - xllow) / (xlhigh - xllow);
        *res = yvals[idx - 1] + frac * (yvals[idx] - yvals[idx - 1]);
    }
}

void EMNR::calc_emnr(EMNR *a)
{
    int i;
    double Dvals[18] = { 1.0, 2.0, 5.0, 8.0, 10.0, 15.0, 20.0, 30.0, 40.0,
        60.0, 80.0, 120.0, 140.0, 160.0, 180.0, 220.0, 260.0, 300.0 };
    double Mvals[18] = { 0.000, 0.260, 0.480, 0.580, 0.610, 0.668, 0.705, 0.762, 0.800,
        0.841, 0.865, 0.890, 0.900, 0.910, 0.920, 0.930, 0.935, 0.940 };
    // float Hvals[18] = { 0.000, 0.150, 0.480, 0.780, 0.980, 1.550, 2.000, 2.300, 2.520,
    //     3.100, 3.380, 4.150, 4.350, 4.250, 3.900, 4.100, 4.700, 5.000 };
    a->incr = a->fsize / a->ovrlp;
    a->gain = a->ogain / a->fsize / (float)a->ovrlp;
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
    a->window = new float[a->fsize]; // (float *)malloc0(a->fsize * sizeof(float));
    a->inaccum = new float[a->iasize]; // (float *)malloc0(a->iasize * sizeof(float));
    a->forfftin = new float[a->fsize]; // (float *)malloc0(a->fsize * sizeof(float));
    a->forfftout = new float[a->msize * 2]; // (float *)malloc0(a->msize * sizeof(complex));
    a->mask = new double[a->msize]; // (float *)malloc0(a->msize * sizeof(float));
    std::fill(a->mask, a->mask + a->msize, 1.0);
    a->revfftin = new float[a->msize * 2]; // (float *)malloc0(a->msize * sizeof(complex));
    a->revfftout = new float[a->fsize]; // (float *)malloc0(a->fsize * sizeof(float));
    a->save = new float*[a->ovrlp]; // (float **)malloc0(a->ovrlp * sizeof(float *));
    for (i = 0; i < a->ovrlp; i++)
        a->save[i] = new float[a->fsize]; // (float *)malloc0(a->fsize * sizeof(float));
    a->outaccum = new float[a->oasize]; // (float *)malloc0(a->oasize * sizeof(float));
    a->nsamps = 0;
    a->saveidx = 0;
    a->Rfor = fftwf_plan_dft_r2c_1d(a->fsize, a->forfftin, (fftwf_complex *)a->forfftout, FFTW_ESTIMATE);
    a->Rrev = fftwf_plan_dft_c2r_1d(a->fsize, (fftwf_complex *)a->revfftin, a->revfftout, FFTW_ESTIMATE);
    calc_window(a);

    a->g.msize = a->msize;
    a->g.mask = a->mask;
    a->g.y = a->forfftout;
    a->g.lambda_y = new double[a->msize]; // (float *)malloc0(a->msize * sizeof(float));
    a->g.lambda_d = new double[a->msize]; // (float *)malloc0(a->msize * sizeof(float));
    a->g.prev_gamma = new double[a->msize]; // (float *)malloc0(a->msize * sizeof(float));
    a->g.prev_mask = new double[a->msize]; // (float *)malloc0(a->msize * sizeof(float));

    a->g.gf1p5 = sqrt(PI) / 2.0;
    {
        float tau = -128.0 / 8000.0 / log(0.98);
        a->g.alpha = exp(-a->incr / a->rate / tau);
    }
    a->g.eps_floor = std::numeric_limits<double>::min();
    a->g.gamma_max = 1000.0;
    a->g.q = 0.2;
    for (i = 0; i < a->g.msize; i++)
    {
        a->g.prev_mask[i] = 1.0;
        a->g.prev_gamma[i] = 1.0;
    }
    a->g.gmax = 10000.0;
    //
    a->g.GG = new double[241 * 241]; // (float *)malloc0(241 * 241 * sizeof(float));
    a->g.GGS = new double[241 * 241]; // (float *)malloc0(241 * 241 * sizeof(float));
    if ((a->g.fileb = fopen("calculus", "rb")))
    {
        std::size_t lgg = fread(a->g.GG, sizeof(float), 241 * 241, a->g.fileb);
        if (lgg != 241 * 241) {
            fprintf(stderr, "GG file has an invalid size\n");
        }
        std::size_t lggs =fread(a->g.GGS, sizeof(float), 241 * 241, a->g.fileb);
        if (lggs != 241 * 241) {
            fprintf(stderr, "GGS file has an invalid size\n");
        }
        fclose(a->g.fileb);
    }
    else
    {
        std::copy(Calculus::GG, Calculus::GG + (241 * 241), a->g.GG);
        std::copy(Calculus::GGS, Calculus::GGS + (241 * 241), a->g.GGS);
    }
    //

    a->np.incr = a->incr;
    a->np.rate = a->rate;
    a->np.msize = a->msize;
    a->np.lambda_y = a->g.lambda_y;
    a->np.lambda_d = a->g.lambda_d;

    {
        float tau = -128.0 / 8000.0 / log(0.7);
        a->np.alphaCsmooth = exp(-a->np.incr / a->np.rate / tau);
    }
    {
        float tau = -128.0 / 8000.0 / log(0.96);
        a->np.alphaMax = exp(-a->np.incr / a->np.rate / tau);
    }
    {
        float tau = -128.0 / 8000.0 / log(0.7);
        a->np.alphaCmin = exp(-a->np.incr / a->np.rate / tau);
    }
    {
        float tau = -128.0 / 8000.0 / log(0.3);
        a->np.alphaMin_max_value = exp(-a->np.incr / a->np.rate / tau);
    }
    a->np.snrq = -a->np.incr / (0.064 * a->np.rate);
    {
        float tau = -128.0 / 8000.0 / log(0.8);
        a->np.betamax = exp(-a->np.incr / a->np.rate / tau);
    }
    a->np.invQeqMax = 0.5;
    a->np.av = 2.12;
    a->np.Dtime = 8.0 * 12.0 * 128.0 / 8000.0;
    a->np.U = 8;
    a->np.V = (int)(0.5 + (a->np.Dtime * a->np.rate / (a->np.U * a->np.incr)));
    if (a->np.V < 4) a->np.V = 4;
    if ((a->np.U = (int)(0.5 + (a->np.Dtime * a->np.rate / (a->np.V * a->np.incr)))) < 1) a->np.U = 1;
    a->np.D = a->np.U * a->np.V;
    interpM(&a->np.MofD, a->np.D, 18, Dvals, Mvals);
    interpM(&a->np.MofV, a->np.V, 18, Dvals, Mvals);
    a->np.invQbar_points[0] = 0.03;
    a->np.invQbar_points[1] = 0.05;
    a->np.invQbar_points[2] = 0.06;
    a->np.invQbar_points[3] = 1.0e300;
    {
        float db;
        db = 10.0 * log10(8.0) / (12.0 * 128 / 8000);
        a->np.nsmax[0] = pow(10.0, db / 10.0 * a->np.V * a->np.incr / a->np.rate);
        db = 10.0 * log10(4.0) / (12.0 * 128 / 8000);
        a->np.nsmax[1] = pow(10.0, db / 10.0 * a->np.V * a->np.incr / a->np.rate);
        db = 10.0 * log10(2.0) / (12.0 * 128 / 8000);
        a->np.nsmax[2] = pow(10.0, db / 10.0 * a->np.V * a->np.incr / a->np.rate);
        db = 10.0 * log10(1.2) / (12.0 * 128 / 8000);
        a->np.nsmax[3] = pow(10.0, db / 10.0 * a->np.V * a->np.incr / a->np.rate);
    }

    a->np.p = new double[a->np.msize]; // (float *)malloc0(a->np.msize * sizeof(float));
    a->np.alphaOptHat = new double[a->np.msize]; // (float *)malloc0(a->np.msize * sizeof(float));
    a->np.alphaHat = new double[a->np.msize]; // (float *)malloc0(a->np.msize * sizeof(float));
    a->np.sigma2N = new double[a->np.msize]; // (float *)malloc0(a->np.msize * sizeof(float));
    a->np.pbar = new double[a->np.msize]; // (float *)malloc0(a->np.msize * sizeof(float));
    a->np.p2bar = new double[a->np.msize]; // (float *)malloc0(a->np.msize * sizeof(float));
    a->np.Qeq = new double[a->np.msize]; // (float *)malloc0(a->np.msize * sizeof(float));
    a->np.bmin = new double[a->np.msize]; // (float *)malloc0(a->np.msize * sizeof(float));
    a->np.bmin_sub = new double[a->np.msize]; // (float *)malloc0(a->np.msize * sizeof(float));
    a->np.k_mod = new int[a->np.msize]; // (int *)malloc0(a->np.msize * sizeof(int));
    a->np.actmin = new double[a->np.msize]; // (float *)malloc0(a->np.msize * sizeof(float));
    a->np.actmin_sub =  new double[a->np.msize]; // (float *)malloc0(a->np.msize * sizeof(float));
    a->np.lmin_flag = new int[a->np.msize]; // (int *)malloc0(a->np.msize * sizeof(int));
    a->np.pmin_u = new double[a->np.msize]; // (float *)malloc0(a->np.msize * sizeof(float));
    a->np.actminbuff = new double*[a->np.U]; // (float**)malloc0(a->np.U     * sizeof(float*));
    for (i = 0; i < a->np.U; i++)
        a->np.actminbuff[i] = new double[a->np.msize]; // (float *)malloc0(a->np.msize * sizeof(float));

    {
        int k, ku;
        a->np.alphaC = 1.0;
        a->np.subwc = a->np.V;
        a->np.amb_idx = 0;
        for (k = 0; k < a->np.msize; k++) a->np.lambda_y[k] = 0.5;
        std::copy(a->np.lambda_y, a->np.lambda_y + a->np.msize, a->np.p);
        std::copy(a->np.lambda_y, a->np.lambda_y + a->np.msize, a->np.sigma2N);
        std::copy(a->np.lambda_y, a->np.lambda_y + a->np.msize, a->np.pbar);
        std::copy(a->np.lambda_y, a->np.lambda_y + a->np.msize, a->np.pmin_u);
        for (k = 0; k < a->np.msize; k++)
        {
            a->np.p2bar[k] = a->np.lambda_y[k] * a->np.lambda_y[k];
            a->np.actmin[k] = 1.0e300;
            a->np.actmin_sub[k] = 1.0e300;
            for (ku = 0; ku < a->np.U; ku++)
                a->np.actminbuff[ku][k] = 1.0e300;
        }
        std::fill(a->np.lmin_flag, a->np.lmin_flag + a->np.msize, 0);
    }

    a->nps.incr = a->incr;
    a->nps.rate = a->rate;
    a->nps.msize = a->msize;
    a->nps.lambda_y = a->g.lambda_y;
    a->nps.lambda_d = a->g.lambda_d;

    {
        float tau = -128.0 / 8000.0 / log(0.8);
        a->nps.alpha_pow = exp(-a->nps.incr / a->nps.rate / tau);
    }
    {
        float tau = -128.0 / 8000.0 / log(0.9);
        a->nps.alpha_Pbar = exp(-a->nps.incr / a->nps.rate / tau);
    }
    a->nps.epsH1 = pow(10.0, 15.0 / 10.0);
    a->nps.epsH1r = a->nps.epsH1 / (1.0 + a->nps.epsH1);

    a->nps.sigma2N = new double[a->nps.msize]; // (float *)malloc0(a->nps.msize * sizeof(float));
    a->nps.PH1y = new double[a->nps.msize]; // (float *)malloc0(a->nps.msize * sizeof(float));
    a->nps.Pbar = new double[a->nps.msize]; // (float *)malloc0(a->nps.msize * sizeof(float));
    a->nps.EN2y = new double[a->nps.msize]; // (float *)malloc0(a->nps.msize * sizeof(float));

    for (i = 0; i < a->nps.msize; i++)
    {
        a->nps.sigma2N[i] = 0.5;
        a->nps.Pbar[i] = 0.5;
    }

    a->ae.msize = a->msize;
    a->ae.lambda_y = a->g.lambda_y;

    a->ae.zetaThresh = 0.75;
    a->ae.psi = 10.0;

    a->ae.nmask = new double[a->ae.msize]; // (float *)malloc0(a->ae.msize * sizeof(float));
}

void EMNR::decalc_emnr(EMNR *a)
{
    int i;
    delete[] (a->ae.nmask);

    delete[] (a->nps.EN2y);
    delete[] (a->nps.Pbar);
    delete[] (a->nps.PH1y);
    delete[] (a->nps.sigma2N);

    for (i = 0; i < a->np.U; i++)
        delete[] (a->np.actminbuff[i]);
    delete[] (a->np.actminbuff);
    delete[] (a->np.pmin_u);
    delete[] (a->np.lmin_flag);
    delete[] (a->np.actmin_sub);
    delete[] (a->np.actmin);
    delete[] (a->np.k_mod);
    delete[] (a->np.bmin_sub);
    delete[] (a->np.bmin);
    delete[] (a->np.Qeq);
    delete[] (a->np.p2bar);
    delete[] (a->np.pbar);
    delete[] (a->np.sigma2N);
    delete[] (a->np.alphaHat);
    delete[] (a->np.alphaOptHat);
    delete[] (a->np.p);

    delete[] (a->g.GGS);
    delete[] (a->g.GG);
    delete[] (a->g.prev_mask);
    delete[] (a->g.prev_gamma);
    delete[] (a->g.lambda_d);
    delete[] (a->g.lambda_y);

    fftwf_destroy_plan(a->Rrev);
    fftwf_destroy_plan(a->Rfor);
    delete[] (a->outaccum);
    for (i = 0; i < a->ovrlp; i++)
        delete[] (a->save[i]);
    delete[] (a->save);
    delete[] (a->revfftout);
    delete[] (a->revfftin);
    delete[] (a->mask);
    delete[] (a->forfftout);
    delete[] (a->forfftin);
    delete[] (a->inaccum);
    delete[] (a->window);
}

EMNR* EMNR::create_emnr (
    int run,
    int position,
    int size,
    float* in,
    float* out,
    int fsize,
    int ovrlp,
    int rate,
    int wintype,
    float gain,
    int gain_method,
    int npe_method,
    int ae_run
)
{
    EMNR *a = new EMNR;

    a->run = run;
    a->position = position;
    a->bsize = size;
    a->in = in;
    a->out = out;
    a->fsize = fsize;
    a->ovrlp = ovrlp;
    a->rate = rate;
    a->wintype = wintype;
    a->ogain = gain;
    a->g.gain_method = gain_method;
    a->g.npe_method = npe_method;
    a->g.ae_run = ae_run;
    calc_emnr (a);
    return a;
}

void EMNR::flush_emnr (EMNR *a)
{
    int i;
    std::fill(a->inaccum, a->inaccum + a->iasize, 0);
    for (i = 0; i < a->ovrlp; i++)
        std::fill(a->save[i], a->save[i] + a->fsize, 0);
    std::fill(a->outaccum, a->outaccum + a->oasize, 0);
    a->nsamps   = 0;
    a->iainidx  = 0;
    a->iaoutidx = 0;
    a->oainidx  = a->init_oainidx;
    a->oaoutidx = 0;
    a->saveidx  = 0;
}

void EMNR::destroy_emnr (EMNR *a)
{
    decalc_emnr (a);
    delete[]  (a);
}

void EMNR::LambdaD(EMNR *a)
{
    int k;
    double f0, f1, f2, f3;
    double sum_prev_p;
    double sum_lambda_y;
    double alphaCtilda;
    double sum_prev_sigma2N;
    double alphaMin, SNR;
    double beta, varHat, invQeq;
    double invQbar;
    double bc;
    double QeqTilda, QeqTildaSub;
    double noise_slope_max;

    sum_prev_p = 0.0;
    sum_lambda_y = 0.0;
    sum_prev_sigma2N = 0.0;
    for (k = 0; k < a->np.msize; k++)
    {
        sum_prev_p += a->np.p[k];
        sum_lambda_y += a->np.lambda_y[k];
        sum_prev_sigma2N += a->np.sigma2N[k];
    }
    for (k = 0; k < a->np.msize; k++)
    {
        f0 = a->np.p[k] / a->np.sigma2N[k] - 1.0;
        a->np.alphaOptHat[k] = 1.0 / (1.0 + f0 * f0);
    }
    SNR = sum_prev_p / sum_prev_sigma2N;
    alphaMin = std::min (a->np.alphaMin_max_value, pow (SNR, a->np.snrq));
    for (k = 0; k < a->np.msize; k++)
        if (a->np.alphaOptHat[k] < alphaMin) a->np.alphaOptHat[k] = alphaMin;
    f1 = sum_prev_p / sum_lambda_y - 1.0;
    alphaCtilda = 1.0 / (1.0 + f1 * f1);
    a->np.alphaC = a->np.alphaCsmooth * a->np.alphaC + (1.0 - a->np.alphaCsmooth) * std::max (alphaCtilda, a->np.alphaCmin);
    f2 = a->np.alphaMax * a->np.alphaC;
    for (k = 0; k < a->np.msize; k++)
        a->np.alphaHat[k] = f2 * a->np.alphaOptHat[k];
    for (k = 0; k < a->np.msize; k++)
        a->np.p[k] = a->np.alphaHat[k] * a->np.p[k] + (1.0 - a->np.alphaHat[k]) * a->np.lambda_y[k];
    invQbar = 0.0;
    for (k = 0; k < a->np.msize; k++)
    {
        beta = std::min (a->np.betamax, a->np.alphaHat[k] * a->np.alphaHat[k]);
        a->np.pbar[k] = beta * a->np.pbar[k] + (1.0 - beta) * a->np.p[k];
        a->np.p2bar[k] = beta * a->np.p2bar[k] + (1.0 - beta) * a->np.p[k] * a->np.p[k];
        varHat = a->np.p2bar[k] - a->np.pbar[k] * a->np.pbar[k];
        invQeq = varHat / (2.0 * a->np.sigma2N[k] * a->np.sigma2N[k]);
        if (invQeq > a->np.invQeqMax) invQeq = a->np.invQeqMax;
        a->np.Qeq[k] = 1.0 / invQeq;
        invQbar += invQeq;
    }
    invQbar /= (float)a->np.msize;
    bc = 1.0 + a->np.av * sqrt (invQbar);
    for (k = 0; k < a->np.msize; k++)
    {
        QeqTilda    = (a->np.Qeq[k] - 2.0 * a->np.MofD) / (1.0 - a->np.MofD);
        QeqTildaSub = (a->np.Qeq[k] - 2.0 * a->np.MofV) / (1.0 - a->np.MofV);
        a->np.bmin[k]     = 1.0 + 2.0 * (a->np.D - 1.0) / QeqTilda;
        a->np.bmin_sub[k] = 1.0 + 2.0 * (a->np.V - 1.0) / QeqTildaSub;
    }
    std::fill(a->np.k_mod, a->np.k_mod + a->np.msize, 0);
    for (k = 0; k < a->np.msize; k++)
    {
        f3 = a->np.p[k] * a->np.bmin[k] * bc;
        if (f3 < a->np.actmin[k])
        {
            a->np.actmin[k] = f3;
            a->np.actmin_sub[k] = a->np.p[k] * a->np.bmin_sub[k] * bc;
            a->np.k_mod[k] = 1;
        }
    }
    if (a->np.subwc == a->np.V)
    {
        if      (invQbar < a->np.invQbar_points[0]) noise_slope_max = a->np.nsmax[0];
        else if (invQbar < a->np.invQbar_points[1]) noise_slope_max = a->np.nsmax[1];
        else if (invQbar < a->np.invQbar_points[2]) noise_slope_max = a->np.nsmax[2];
        else                                        noise_slope_max = a->np.nsmax[3];

        for (k = 0; k < a->np.msize; k++)
        {
            int ku;
            float min;
            if (a->np.k_mod[k])
                a->np.lmin_flag[k] = 0;
            a->np.actminbuff[a->np.amb_idx][k] = a->np.actmin[k];
            min = 1.0e300;
            for (ku = 0; ku < a->np.U; ku++)
                if (a->np.actminbuff[ku][k] < min) min = a->np.actminbuff[ku][k];
            a->np.pmin_u[k] = min;
            if ((a->np.lmin_flag[k] == 1)
                && (a->np.actmin_sub[k] < noise_slope_max * a->np.pmin_u[k])
                && (a->np.actmin_sub[k] >                   a->np.pmin_u[k]))
            {
                a->np.pmin_u[k] = a->np.actmin_sub[k];
                for (ku = 0; ku < a->np.U; ku++)
                    a->np.actminbuff[ku][k] = a->np.actmin_sub[k];
            }
            a->np.lmin_flag[k] = 0;
            a->np.actmin[k] = 1.0e300;
            a->np.actmin_sub[k] = 1.0e300;
        }
        if (++a->np.amb_idx == a->np.U) a->np.amb_idx = 0;
        a->np.subwc = 1;
    }
    else
    {
        if (a->np.subwc > 1)
        {
            for (k = 0; k < a->np.msize; k++)
            {
                if (a->np.k_mod[k])
                {
                    a->np.lmin_flag[k] = 1;
                    a->np.sigma2N[k] = std::min (a->np.actmin_sub[k], a->np.pmin_u[k]);
                    a->np.pmin_u[k] = a->np.sigma2N[k];
                }
            }
        }
        ++a->np.subwc;
    }
    std::copy(a->np.sigma2N, a->np.sigma2N + a->np.msize, a->np.lambda_d);
}

void EMNR::LambdaDs (EMNR *a)
{
    int k;
    for (k = 0; k < a->nps.msize; k++)
    {
        a->nps.PH1y[k] = 1.0 / (1.0 + (1.0 + a->nps.epsH1) * exp (- a->nps.epsH1r * a->nps.lambda_y[k] / a->nps.sigma2N[k]));
        a->nps.Pbar[k] = a->nps.alpha_Pbar * a->nps.Pbar[k] + (1.0 - a->nps.alpha_Pbar) * a->nps.PH1y[k];
        if (a->nps.Pbar[k] > 0.99)
            a->nps.PH1y[k] = std::min (a->nps.PH1y[k], 0.99);
        a->nps.EN2y[k] = (1.0 - a->nps.PH1y[k]) * a->nps.lambda_y[k] + a->nps.PH1y[k] * a->nps.sigma2N[k];
        a->nps.sigma2N[k] = a->nps.alpha_pow * a->nps.sigma2N[k] + (1.0 - a->nps.alpha_pow) * a->nps.EN2y[k];
    }
    std::copy(a->nps.sigma2N, a->nps.sigma2N + a->nps.msize, a->nps.lambda_d);
}

void EMNR::aepf(EMNR *a)
{
    int k, m;
    int N, n;
    float sumPre, sumPost, zeta, zetaT;
    sumPre = 0.0;
    sumPost = 0.0;
    for (k = 0; k < a->ae.msize; k++)
    {
        sumPre += a->ae.lambda_y[k];
        sumPost += a->mask[k] * a->mask[k] * a->ae.lambda_y[k];
    }
    zeta = sumPost / sumPre;
    if (zeta >= a->ae.zetaThresh)
        zetaT = 1.0;
    else
        zetaT = zeta;
    if (zetaT == 1.0)
        N = 1;
    else
        N = 1 + 2 * (int)(0.5 + a->ae.psi * (1.0 - zetaT / a->ae.zetaThresh));
    n = N / 2;
    for (k = n; k < (a->ae.msize - n); k++)
    {
        a->ae.nmask[k] = 0.0;
        for (m = k - n; m <= (k + n); m++)
            a->ae.nmask[k] += a->mask[m];
        a->ae.nmask[k] /= (float)N;
    }
    std::copy(a->ae.nmask, a->ae.nmask + (a->ae.msize - 2 * n), a->mask + n);
}

double EMNR::getKey(double* type, double gamma, double xi)
{
    int ngamma1, ngamma2, nxi1 = 0, nxi2 = 0;
    double tg, tx, dg, dx;
    const double dmin = 0.001;
    const double dmax = 1000.0;
    if (gamma <= dmin)
    {
        ngamma1 = ngamma2 = 0;
        tg = 0.0;
    }
    else if (gamma >= dmax)
    {
        ngamma1 = ngamma2 = 240;
        tg = 60.0;
    }
    else
    {
        tg = 10.0 * log10(gamma / dmin);
        ngamma1 = (int)(4.0 * tg);
        ngamma2 = ngamma1 + 1;
    }
    if (xi <= dmin)
    {
        nxi1 = nxi2 = 0;
        tx = 0.0;
    }
    else if (xi >= dmax)
    {
        nxi1 = nxi2 = 240;
        tx = 60.0;
    }
    else
    {
        tx = 10.0 * log10(xi / dmin);
        nxi1 = (int)(4.0 * tx);
        nxi2 = nxi1 + 1;
    }
    dg = (tg - 0.25 * ngamma1) / 0.25;
    dx = (tx - 0.25 * nxi1) / 0.25;
    return (1.0 - dg)  * (1.0 - dx) * type[241 * nxi1 + ngamma1]
        +  (1.0 - dg)  *        dx  * type[241 * nxi2 + ngamma1]
        +         dg   * (1.0 - dx) * type[241 * nxi1 + ngamma2]
        +         dg   *        dx  * type[241 * nxi2 + ngamma2];
}

void EMNR::calc_gain (EMNR *a)
{
    int k;
    for (k = 0; k < a->g.msize; k++)
    {
        double y0 = a->g.y[2 * k + 0];
        double y1 = a->g.y[2 * k + 1];
        a->g.lambda_y[k] = y0 * y0 + y1 * y1;
    }
    switch (a->g.npe_method)
    {
    case 0:
        LambdaD(a);
        break;
    case 1:
        LambdaDs(a);
        break;
    }
    switch (a->g.gain_method)
    {
    case 0:
        {
            double gamma, eps_hat, v;
            for (k = 0; k < a->msize; k++)
            {
                gamma = std::min (a->g.lambda_y[k] / a->g.lambda_d[k], a->g.gamma_max);
                eps_hat = a->g.alpha * a->g.prev_mask[k] * a->g.prev_mask[k] * a->g.prev_gamma[k]
                    + (1.0 - a->g.alpha) * std::max (gamma - 1.0f, a->g.eps_floor);
                v = (eps_hat / (1.0 + eps_hat)) * gamma;
                a->g.mask[k] = a->g.gf1p5 * sqrt (v) / gamma * exp (- 0.5 * v)
                    * ((1.0 + v) * bessI0 (0.5 * v) + v * bessI1 (0.5 * v));
                {
                    double v2 = std::min (v, 700.0);
                    double eta = a->g.mask[k] * a->g.mask[k] * a->g.lambda_y[k] / a->g.lambda_d[k];
                    double eps = eta / (1.0 - a->g.q);
                    double witchHat = (1.0 - a->g.q) / a->g.q * exp (v2) / (1.0 + eps);
                    a->g.mask[k] *= witchHat / (1.0 + witchHat);
                }
                if (a->g.mask[k] > a->g.gmax) a->g.mask[k] = a->g.gmax;
                if (a->g.mask[k] != a->g.mask[k]) a->g.mask[k] = 0.01;
                a->g.prev_gamma[k] = gamma;
                a->g.prev_mask[k] = a->g.mask[k];
            }
            break;
        }
    case 1:
        {
            double gamma, eps_hat, v, ehr;
            for (k = 0; k < a->g.msize; k++)
            {
                gamma = std::min (a->g.lambda_y[k] / a->g.lambda_d[k], a->g.gamma_max);
                eps_hat = a->g.alpha * a->g.prev_mask[k] * a->g.prev_mask[k] * a->g.prev_gamma[k]
                    + (1.0 - a->g.alpha) * std::max (gamma - 1.0f, a->g.eps_floor);
                ehr = eps_hat / (1.0 + eps_hat);
                v = ehr * gamma;
                if((a->g.mask[k] = ehr * exp (std::min (700.0, 0.5 * e1xb(v)))) > a->g.gmax) a->g.mask[k] = a->g.gmax;
                if (a->g.mask[k] != a->g.mask[k])a->g.mask[k] = 0.01;
                a->g.prev_gamma[k] = gamma;
                a->g.prev_mask[k] = a->g.mask[k];
            }
            break;
        }
    case 2:
        {
            double gamma, eps_hat, eps_p;
            for (k = 0; k < a->msize; k++)
            {
                gamma = std::min(a->g.lambda_y[k] / a->g.lambda_d[k], a->g.gamma_max);
                eps_hat = a->g.alpha * a->g.prev_mask[k] * a->g.prev_mask[k] * a->g.prev_gamma[k]
                    + (1.0 - a->g.alpha) * std::max(gamma - 1.0f, a->g.eps_floor);
                eps_p = eps_hat / (1.0 - a->g.q);
                a->g.mask[k] = getKey(a->g.GG, gamma, eps_hat) * getKey(a->g.GGS, gamma, eps_p);
                a->g.prev_gamma[k] = gamma;
                a->g.prev_mask[k] = a->g.mask[k];
            }
            break;
        }
    }
    if (a->g.ae_run) aepf(a);
}

void EMNR::xemnr (EMNR *a, int pos)
{
    if (a->run && pos == a->position)
    {
        int i, j, k, sbuff, sbegin;
        float g1;
        for (i = 0; i < 2 * a->bsize; i += 2)
        {
            a->inaccum[a->iainidx] = a->in[i];
            a->iainidx = (a->iainidx + 1) % a->iasize;
        }
        a->nsamps += a->bsize;
        while (a->nsamps >= a->fsize)
        {
            for (i = 0, j = a->iaoutidx; i < a->fsize; i++, j = (j + 1) % a->iasize)
                a->forfftin[i] = a->window[i] * a->inaccum[j];
            a->iaoutidx = (a->iaoutidx + a->incr) % a->iasize;
            a->nsamps -= a->incr;
            fftwf_execute (a->Rfor);
            calc_gain(a);
            for (i = 0; i < a->msize; i++)
            {
                g1 = a->gain * a->mask[i];
                a->revfftin[2 * i + 0] = g1 * a->forfftout[2 * i + 0];
                a->revfftin[2 * i + 1] = g1 * a->forfftout[2 * i + 1];
            }
            fftwf_execute (a->Rrev);
            for (i = 0; i < a->fsize; i++)
                a->save[a->saveidx][i] = a->window[i] * a->revfftout[i];
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
        std::copy(a->in, a->in + a->bsize * 2, a->out);
}

void EMNR::setBuffers_emnr (EMNR *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
}

void EMNR::setSamplerate_emnr (EMNR *a, int rate)
{
    decalc_emnr (a);
    a->rate = rate;
    calc_emnr (a);
}

void EMNR::setSize_emnr (EMNR *a, int size)
{
    decalc_emnr (a);
    a->bsize = size;
    calc_emnr (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void EMNR::SetEMNRRun (RXA& rxa, int run)
{
    EMNR *a = rxa.emnr.p;

    if (a->run != run)
    {
        RXA::bp1Check (
            rxa,
            rxa.amd.p->run,
            rxa.snba.p->run,
            run,
            rxa.anf.p->run,
            rxa.anr.p->run
        );
        a->run = run;
        RXA::bp1Set (rxa);
    }
}

void EMNR::SetEMNRgainMethod (RXA& rxa, int method)
{
    rxa.emnr.p->g.gain_method = method;
}

void EMNR::SetEMNRnpeMethod (RXA& rxa, int method)
{
    rxa.emnr.p->g.npe_method = method;
}

void EMNR::SetEMNRaeRun (RXA& rxa, int run)
{
    rxa.emnr.p->g.ae_run = run;
}

void EMNR::SetEMNRPosition (RXA& rxa, int position)
{
    rxa.emnr.p->position = position;
    rxa.bp1.p->position  = position;
}

void EMNR::SetEMNRaeZetaThresh (RXA& rxa, double zetathresh)
{
    rxa.emnr.p->ae.zetaThresh = zetathresh;
}

void EMNR::SetEMNRaePsi (RXA& rxa, double psi)
{
    rxa.emnr.p->ae.psi = psi;
}

} // namespace WDSP
