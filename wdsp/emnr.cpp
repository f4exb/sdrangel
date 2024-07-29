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

EMNR::AE::AE(
    int _msize,
    double* _lambda_y,
    double _zetaThresh,
    double _psi
) :
    msize(_msize),
    lambda_y(_lambda_y),
    zetaThresh(_zetaThresh),
    psi(_psi)
{
    nmask = new double[msize];
}

EMNR::AE::~AE()
{
    delete[] nmask;
}

EMNR::NPS::NPS(
    int _incr,
    double _rate,
    int _msize,
    double* _lambda_y,
    double* _lambda_d,

    double _alpha_pow,
    double _alpha_Pbar,
    double _epsH1
) :
    incr(_incr),
    rate(_rate),
    msize(_msize),
    lambda_y(_lambda_y),
    lambda_d(_lambda_d),
    alpha_pow(_alpha_pow),
    alpha_Pbar(_alpha_Pbar),
    epsH1(_epsH1)
{
    epsH1r = epsH1 / (1.0 + epsH1);
    sigma2N = new double[msize]; // (float *)malloc0(nps.msize * sizeof(float));
    PH1y = new double[msize]; // (float *)malloc0(nps.msize * sizeof(float));
    Pbar = new double[msize]; // (float *)malloc0(nps.msize * sizeof(float));
    EN2y = new double[msize]; // (float *)malloc0(nps.msize * sizeof(float));

    for (int i = 0; i < msize; i++)
    {
        sigma2N[i] = 0.5;
        Pbar[i] = 0.5;
    }
}

EMNR::NPS::~NPS()
{
    delete[] sigma2N;
    delete[] PH1y;
    delete[] Pbar;
    delete[] EN2y;
}

void EMNR::NPS::LambdaDs()
{
    int k;

    for (k = 0; k < msize; k++)
    {
        PH1y[k] = 1.0 / (1.0 + (1.0 + epsH1) * exp (- epsH1r * lambda_y[k] / sigma2N[k]));
        Pbar[k] = alpha_Pbar * Pbar[k] + (1.0 - alpha_Pbar) * PH1y[k];

        if (Pbar[k] > 0.99)
            PH1y[k] = std::min (PH1y[k], 0.99);

        EN2y[k] = (1.0 - PH1y[k]) * lambda_y[k] + PH1y[k] * sigma2N[k];
        sigma2N[k] = alpha_pow * sigma2N[k] + (1.0 - alpha_pow) * EN2y[k];
    }

    std::copy(sigma2N, sigma2N + msize, lambda_d);
}

const std::array<double, 18> EMNR::NP::DVals = { 1.0, 2.0, 5.0, 8.0, 10.0, 15.0, 20.0, 30.0, 40.0,
    60.0, 80.0, 120.0, 140.0, 160.0, 180.0, 220.0, 260.0, 300.0 };
const std::array<double, 18> EMNR::NP::MVals = { 0.000, 0.260, 0.480, 0.580, 0.610, 0.668, 0.705, 0.762, 0.800,
    0.841, 0.865, 0.890, 0.900, 0.910, 0.920, 0.930, 0.935, 0.940 };

EMNR::NP::NP(
    int _incr,
    double _rate,
    int _msize,
    double* _lambda_y,
    double* _lambda_d
)
{
    incr = _incr;
    rate = _rate;
    msize = _msize;
    lambda_y = _lambda_y;
    lambda_d = _lambda_d;

    {
        double tau = -128.0 / 8000.0 / log(0.7);
        alphaCsmooth = exp(-incr / rate / tau);
    }

    {
        double tau = -128.0 / 8000.0 / log(0.96);
        alphaMax = exp(-incr / rate / tau);
    }

    {
        double tau = -128.0 / 8000.0 / log(0.7);
        alphaCmin = exp(-incr / rate / tau);
    }

    {
        double tau = -128.0 / 8000.0 / log(0.3);
        alphaMin_max_value = exp(-incr / rate / tau);
    }

    snrq = -incr / (0.064 * rate);

    double tau4 = -128.0 / 8000.0 / log(0.8);
    betamax = exp(-incr / rate / tau4);

    invQeqMax = 0.5;
    av = 2.12;
    Dtime = 8.0 * 12.0 * 128.0 / 8000.0;
    U = 8;
    V = (int)(0.5 + (Dtime * rate / (U * incr)));

    if (V < 4)
        V = 4;

    if ((U = (int)(0.5 + (Dtime * rate / (V * incr)))) < 1)
        U = 1;

    D = U * V;
    interpM(&MofD, D, 18, DVals, MVals);
    interpM(&MofV, V, 18, DVals, MVals);

    invQbar_points[0] = 0.03;
    invQbar_points[1] = 0.05;
    invQbar_points[2] = 0.06;
    invQbar_points[3] = std::numeric_limits<double>::max();;

    {
        double db;
        db = 10.0 * log10(8.0) / (12.0 * 128 / 8000);
        nsmax[0] = pow(10.0, db / 10.0 * V * incr / rate);
        db = 10.0 * log10(4.0) / (12.0 * 128 / 8000);
        nsmax[1] = pow(10.0, db / 10.0 * V * incr / rate);
        db = 10.0 * log10(2.0) / (12.0 * 128 / 8000);
        nsmax[2] = pow(10.0, db / 10.0 * V * incr / rate);
        db = 10.0 * log10(1.2) / (12.0 * 128 / 8000);
        nsmax[3] = pow(10.0, db / 10.0 * V * incr / rate);
    }

    p = new double[msize]; // (float *)malloc0(np.msize * sizeof(float));
    alphaOptHat = new double[msize]; // (float *)malloc0(np.msize * sizeof(float));
    alphaHat = new double[msize]; // (float *)malloc0(np.msize * sizeof(float));
    sigma2N = new double[msize]; // (float *)malloc0(np.msize * sizeof(float));
    pbar = new double[msize]; // (float *)malloc0(np.msize * sizeof(float));
    p2bar = new double[msize]; // (float *)malloc0(np.msize * sizeof(float));
    Qeq = new double[msize]; // (float *)malloc0(np.msize * sizeof(float));
    bmin = new double[msize]; // (float *)malloc0(np.msize * sizeof(float));
    bmin_sub = new double[msize]; // (float *)malloc0(np.msize * sizeof(float));
    k_mod = new int[msize]; // (int *)malloc0(np.msize * sizeof(int));
    actmin = new double[msize]; // (float *)malloc0(np.msize * sizeof(float));
    actmin_sub =  new double[msize]; // (float *)malloc0(np.msize * sizeof(float));
    lmin_flag = new int[msize]; // (int *)malloc0(np.msize * sizeof(int));
    pmin_u = new double[msize]; // (float *)malloc0(np.msize * sizeof(float));
    actminbuff = new double*[U]; // (float**)malloc0(np.U     * sizeof(float*));

    for (int i = 0; i < U; i++) {
        actminbuff[i] = new double[msize]; // (float *)malloc0(np.msize * sizeof(float));
    }

    {
        int k, ku;
        alphaC = 1.0;
        subwc = V;
        amb_idx = 0;

        for (k = 0; k < msize; k++) {
            lambda_y[k] = 0.5;
        }

        std::copy(lambda_y, lambda_y + msize, p);
        std::copy(lambda_y, lambda_y + msize, sigma2N);
        std::copy(lambda_y, lambda_y + msize, pbar);
        std::copy(lambda_y, lambda_y + msize, pmin_u);

        for (k = 0; k < msize; k++)
        {
            p2bar[k] = lambda_y[k] * lambda_y[k];
            actmin[k] = std::numeric_limits<double>::max();
            actmin_sub[k] = std::numeric_limits<double>::max();;

            for (ku = 0; ku < U; ku++) {
                actminbuff[ku][k] = std::numeric_limits<double>::max();;
            }
        }

        std::fill(lmin_flag, lmin_flag + msize, 0);
    }
}

EMNR::NP::~NP()
{
    for (int i = 0; i < U; i++)
        delete[] (actminbuff[i]);

    delete[] (actminbuff);
    delete[] (pmin_u);
    delete[] (lmin_flag);
    delete[] (actmin_sub);
    delete[] (actmin);
    delete[] (k_mod);
    delete[] (bmin_sub);
    delete[] (bmin);
    delete[] (Qeq);
    delete[] (p2bar);
    delete[] (pbar);
    delete[] (sigma2N);
    delete[] (alphaHat);
    delete[] (alphaOptHat);
    delete[] (p);
}

void EMNR::NP::interpM (
    double* res,
    double x,
    int nvals,
    const std::array<double, 18>& xvals,
    const std::array<double, 18>& yvals
)
{
    if (x <= xvals[0])
    {
        *res = yvals[0];
    }
    else if (x >= xvals[nvals - 1])
    {
        *res = yvals[nvals - 1];
    }
    else
    {
        int idx = 1;
        double xllow, xlhigh, frac;

        while ((x >= xvals[idx]) && (idx < nvals - 1))
            idx++;

        xllow = log10 (xvals[idx - 1]);
        xlhigh = log10(xvals[idx]);
        frac = (log10 (x) - xllow) / (xlhigh - xllow);
        *res = yvals[idx - 1] + frac * (yvals[idx] - yvals[idx - 1]);
    }
}

void EMNR::NP::LambdaD()
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

    for (k = 0; k < msize; k++)
    {
        sum_prev_p += p[k];
        sum_lambda_y += lambda_y[k];
        sum_prev_sigma2N += sigma2N[k];
    }

    for (k = 0; k < msize; k++)
    {
        f0 = p[k] / sigma2N[k] - 1.0;
        alphaOptHat[k] = 1.0 / (1.0 + f0 * f0);
    }

    SNR = sum_prev_p / sum_prev_sigma2N;
    alphaMin = std::min (alphaMin_max_value, pow (SNR, snrq));

    for (k = 0; k < msize; k++)
        if (alphaOptHat[k] < alphaMin) alphaOptHat[k] = alphaMin;

    f1 = sum_prev_p / sum_lambda_y - 1.0;
    alphaCtilda = 1.0 / (1.0 + f1 * f1);
    alphaC = alphaCsmooth * alphaC + (1.0 - alphaCsmooth) * std::max (alphaCtilda, alphaCmin);
    f2 = alphaMax * alphaC;

    for (k = 0; k < msize; k++)
        alphaHat[k] = f2 * alphaOptHat[k];

    for (k = 0; k < msize; k++)
        p[k] = alphaHat[k] * p[k] + (1.0 - alphaHat[k]) * lambda_y[k];

    invQbar = 0.0;

    for (k = 0; k < msize; k++)
    {
        beta = std::min (betamax, alphaHat[k] * alphaHat[k]);
        pbar[k] = beta * pbar[k] + (1.0 - beta) * p[k];
        p2bar[k] = beta * p2bar[k] + (1.0 - beta) * p[k] * p[k];
        varHat = p2bar[k] - pbar[k] * pbar[k];
        invQeq = varHat / (2.0 * sigma2N[k] * sigma2N[k]);
        if (invQeq > invQeqMax)
            invQeq = invQeqMax;
        Qeq[k] = 1.0 / invQeq;
        invQbar += invQeq;
    }
    invQbar /= (double) msize;
    bc = 1.0 + av * sqrt (invQbar);

    for (k = 0; k < msize; k++)
    {
        QeqTilda    = (Qeq[k] - 2.0 * MofD) / (1.0 - MofD);
        QeqTildaSub = (Qeq[k] - 2.0 * MofV) / (1.0 - MofV);
        bmin[k]     = 1.0 + 2.0 * (D - 1.0) / QeqTilda;
        bmin_sub[k] = 1.0 + 2.0 * (V - 1.0) / QeqTildaSub;
    }

    std::fill(k_mod, k_mod + msize, 0);

    for (k = 0; k < msize; k++)
    {
        f3 = p[k] * bmin[k] * bc;

        if (f3 < actmin[k])
        {
            actmin[k] = f3;
            actmin_sub[k] = p[k] * bmin_sub[k] * bc;
            k_mod[k] = 1;
        }
    }

    if (subwc == V)
    {
        if (invQbar < invQbar_points[0])
            noise_slope_max = nsmax[0];
        else if (invQbar < invQbar_points[1])
            noise_slope_max = nsmax[1];
        else if (invQbar < invQbar_points[2])
            noise_slope_max = nsmax[2];
        else
            noise_slope_max = nsmax[3];

        for (k = 0; k < msize; k++)
        {
            int ku;
            double min;

            if (k_mod[k])
                lmin_flag[k] = 0;

            actminbuff[amb_idx][k] = actmin[k];
            min = std::numeric_limits<double>::max();;

            for (ku = 0; ku < U; ku++)
            {
                if (actminbuff[ku][k] < min)
                    min = actminbuff[ku][k];
            }

            pmin_u[k] = min;

            if ((lmin_flag[k] == 1)
                && (actmin_sub[k] < noise_slope_max * pmin_u[k])
                && (actmin_sub[k] >                   pmin_u[k]))
            {
                pmin_u[k] = actmin_sub[k];
                for (ku = 0; ku < U; ku++)
                    actminbuff[ku][k] = actmin_sub[k];
            }

            lmin_flag[k] = 0;
            actmin[k] = std::numeric_limits<double>::max();;
            actmin_sub[k] = std::numeric_limits<double>::max();;
        }

        if (++amb_idx == U)
            amb_idx = 0;

        subwc = 1;
    }
    else
    {
        if (subwc > 1)
        {
            for (k = 0; k < msize; k++)
            {
                if (k_mod[k])
                {
                    lmin_flag[k] = 1;
                    sigma2N[k] = std::min (actmin_sub[k], pmin_u[k]);
                    pmin_u[k] = sigma2N[k];
                }
            }
        }

        ++subwc;
    }

    std::copy(sigma2N, sigma2N + msize, lambda_d);
}

EMNR::G::G(
    int _incr,
    double _rate,
    int _msize,
    double* _mask,
    float* _y
)
{
    incr = _incr;
    rate = _rate;
    msize = _msize;
    mask = _mask;
    y = _y;

    lambda_y = new double[msize]; // (float *)malloc0(msize * sizeof(float));
    lambda_d = new double[msize]; // (float *)malloc0(msize * sizeof(float));
    prev_gamma = new double[msize]; // (float *)malloc0(msize * sizeof(float));
    prev_mask = new double[msize]; // (float *)malloc0(msize * sizeof(float));

    gf1p5 = sqrt(PI) / 2.0;

    {
        double tau = -128.0 / 8000.0 / log(0.98);
        alpha = exp(-incr / rate / tau);
    }

    eps_floor = std::numeric_limits<double>::min();
    gamma_max = 1000.0;
    q = 0.2;

    std::fill(prev_mask, prev_mask + msize, 1.0);
    std::fill(prev_gamma, prev_gamma + msize, 1.0);

    gmax = 10000.0;
    //
    GG = new double[241 * 241]; // (float *)malloc0(241 * 241 * sizeof(float));
    GGS = new double[241 * 241]; // (float *)malloc0(241 * 241 * sizeof(float));

    if ((fileb = fopen("calculus", "rb")))
    {
        std::size_t lgg = fread(GG, sizeof(float), 241 * 241, fileb);
        if (lgg != 241 * 241) {
            fprintf(stderr, "GG file has an invalid size\n");
        }
        std::size_t lggs =fread(GGS, sizeof(float), 241 * 241, fileb);
        if (lggs != 241 * 241) {
            fprintf(stderr, "GGS file has an invalid size\n");
        }
        fclose(fileb);
    }
    else
    {
        std::copy(Calculus::GG, Calculus::GG + (241 * 241), GG);
        std::copy(Calculus::GGS, Calculus::GGS + (241 * 241), GGS);
    }
}

EMNR::G::~G()
{
    delete[] (GGS);
    delete[] (GG);
    delete[] (prev_mask);
    delete[] (prev_gamma);
    delete[] (lambda_d);
    delete[] (lambda_y);
}

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
    {
        res = 1.0;
    }
    else
    {
        if (x < 0.0)
            x = -x;

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
    {
        res = 0.0;
    }
    else
    {
        if (x < 0.0)
            x = -x;

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
    {
        e1 = std::numeric_limits<double>::max();;
    }
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

void EMNR::calc_window()
{
    int i;
    double arg, sum, inv_coherent_gain;

    switch (wintype)
    {
    case 0:
        arg = 2.0 * PI / (double) fsize;
        sum = 0.0;

        for (i = 0; i < fsize; i++)
        {
            window[i] = sqrt (0.54 - 0.46 * cos((float)i * arg));
            sum += window[i];
        }

        inv_coherent_gain = (double) fsize / sum;

        for (i = 0; i < fsize; i++)
            window[i] *= inv_coherent_gain;

        break;
    }
}

void EMNR::interpM (double* res, double x, int nvals, double* xvals, double* yvals)
{
    if (x <= xvals[0])
    {
        *res = yvals[0];
    }
    else if (x >= xvals[nvals - 1])
    {
        *res = yvals[nvals - 1];
    }
    else
    {
        int idx = 1;
        double xllow, xlhigh, frac;

        while ((x >= xvals[idx]) && (idx < nvals - 1))
            idx++;

        xllow = log10 (xvals[idx - 1]);
        xlhigh = log10(xvals[idx]);
        frac = (log10 (x) - xllow) / (xlhigh - xllow);
        *res = yvals[idx - 1] + frac * (yvals[idx] - yvals[idx - 1]);
    }
}

void EMNR::calc()
{
    // float Hvals[18] = { 0.000, 0.150, 0.480, 0.780, 0.980, 1.550, 2.000, 2.300, 2.520,
    //     3.100, 3.380, 4.150, 4.350, 4.250, 3.900, 4.100, 4.700, 5.000 };
    incr = fsize / ovrlp;
    gain = ogain / fsize / (float)ovrlp;

    if (fsize > bsize)
        iasize = fsize;
    else
        iasize = bsize + fsize - incr;

    iainidx = 0;
    iaoutidx = 0;

    if (fsize > bsize)
    {
        if (bsize > incr)
            oasize = bsize;
        else
            oasize = incr;

        oainidx = (fsize - bsize - incr) % oasize;
    }
    else
    {
        oasize = bsize;
        oainidx = fsize - incr;
    }

    init_oainidx = oainidx;
    oaoutidx = 0;
    msize = fsize / 2 + 1;
    window.resize(fsize); // (float *)malloc0(fsize * sizeof(float));
    inaccum.resize(iasize); // (float *)malloc0(iasize * sizeof(float));
    forfftin.resize(fsize); // (float *)malloc0(fsize * sizeof(float));
    forfftout.resize(msize * 2); // (float *)malloc0(msize * sizeof(complex));
    mask.resize(msize); // (float *)malloc0(msize * sizeof(float));
    std::fill(mask.begin(), mask.end(), 1.0);
    revfftin.resize(msize * 2); // (float *)malloc0(msize * sizeof(complex));
    revfftout.resize(fsize); // (float *)malloc0(fsize * sizeof(float));
    save.resize(ovrlp); // (float **)malloc0(ovrlp * sizeof(float *));

    for (int i = 0; i < ovrlp; i++)
        save[i].resize(fsize); // (float *)malloc0(fsize * sizeof(float));

    outaccum.resize(oasize); // (float *)malloc0(oasize * sizeof(float));
    nsamps = 0;
    saveidx = 0;
    Rfor = fftwf_plan_dft_r2c_1d(
        fsize,
        forfftin.data(),
        (fftwf_complex *)forfftout.data(),
        FFTW_ESTIMATE
    );
    Rrev = fftwf_plan_dft_c2r_1d(
        fsize,
        (fftwf_complex *)revfftin.data(),
        revfftout.data(),
        FFTW_ESTIMATE
    );

    calc_window();

    // G

    g = new G(
        incr,
        rate,
        msize,
        mask.data(),
        forfftout.data()
    );

    // NP

    np = new NP(
        incr,
        rate,
        msize,
        g->lambda_y,
        g->lambda_d
    );

    // NPS

    double tauNPS0 = -128.0 / 8000.0 / log(0.8);
    double alpha_pow = exp(-incr / rate / tauNPS0);

    double tauNPS1 = -128.0 / 8000.0 / log(0.9);
    double alpha_Pbar = exp(-incr / rate / tauNPS1);

    nps = new NPS(
        incr,
        rate,
        msize,
        g->lambda_y,
        g->lambda_d,
        alpha_pow,
        alpha_Pbar,
        pow(10.0, 15.0 / 10.0) // epsH1
    );

    // AE

    ae = new AE(
        msize,
        g->lambda_y,
        0.75, // zetaThresh
        10.0 // psi
    );
}

void EMNR::decalc()
{
    delete (ae);
    delete (nps);
    delete (np);
    delete (g);

    fftwf_destroy_plan(Rrev);
    fftwf_destroy_plan(Rfor);
}

EMNR::EMNR(
    int _run,
    int _position,
    int _size,
    float* _in,
    float* _out,
    int _fsize,
    int _ovrlp,
    int _rate,
    int _wintype,
    double _gain,
    int _gain_method,
    int _npe_method,
    int _ae_run
)
{
    run = _run;
    position = _position;
    bsize = _size;
    in = _in;
    out = _out;
    fsize = _fsize;
    ovrlp = _ovrlp;
    rate = _rate;
    wintype = _wintype;
    ogain = _gain;
    calc();
    g->gain_method = _gain_method;
    g->npe_method = _npe_method;
    g->ae_run = _ae_run;
}

void EMNR::flush()
{
    int i;
    std::fill(inaccum.begin(), inaccum.end(), 0);

    for (i = 0; i < ovrlp; i++)
        std::fill(save[i].begin(), save[i].end(), 0);

    std::fill(outaccum.begin(), outaccum.end(), 0);
    nsamps   = 0;
    iainidx  = 0;
    iaoutidx = 0;
    oainidx  = init_oainidx;
    oaoutidx = 0;
    saveidx  = 0;
}

EMNR::~EMNR()
{
    decalc();
}

void EMNR::aepf()
{
    int k, m;
    int N, n;
    double sumPre, sumPost, zeta, zetaT;
    sumPre = 0.0;
    sumPost = 0.0;

    for (k = 0; k < ae->msize; k++)
    {
        sumPre += ae->lambda_y[k];
        sumPost += mask[k] * mask[k] * ae->lambda_y[k];
    }

    zeta = sumPost / sumPre;

    if (zeta >= ae->zetaThresh)
        zetaT = 1.0;
    else
        zetaT = zeta;

    if (zetaT == 1.0)
        N = 1;
    else
        N = 1 + 2 * (int)(0.5 + ae->psi * (1.0 - zetaT / ae->zetaThresh));

    n = N / 2;

    for (k = n; k < (ae->msize - n); k++)
    {
        ae->nmask[k] = 0.0;
        for (m = k - n; m <= (k + n); m++)
            ae->nmask[k] += mask[m];
        ae->nmask[k] /= (float)N;
    }

    std::copy(ae->nmask, ae->nmask + (ae->msize - 2 * n), &mask[n]);
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

void EMNR::calc_gain()
{
    int k;

    for (k = 0; k < msize; k++)
    {
        double y0 = g->y[2 * k + 0];
        double y1 = g->y[2 * k + 1];
        g->lambda_y[k] = y0 * y0 + y1 * y1;
    }

    switch (g->npe_method)
    {
    case 0:
        np->LambdaD();
        break;
    case 1:
        nps->LambdaDs();
        break;
    }

    switch (g->gain_method)
    {
    case 0:
        {
            double gamma, eps_hat, v;

            for (k = 0; k < msize; k++)
            {
                gamma = std::min (g->lambda_y[k] / g->lambda_d[k], g->gamma_max);
                eps_hat = g->alpha * g->prev_mask[k] * g->prev_mask[k] * g->prev_gamma[k]
                    + (1.0 - g->alpha) * std::max (gamma - 1.0f, g->eps_floor);
                v = (eps_hat / (1.0 + eps_hat)) * gamma;
                g->mask[k] = g->gf1p5 * sqrt (v) / gamma * exp (- 0.5 * v)
                    * ((1.0 + v) * bessI0 (0.5 * v) + v * bessI1 (0.5 * v));
                {
                    double v2 = std::min (v, 700.0);
                    double eta = g->mask[k] * g->mask[k] * g->lambda_y[k] / g->lambda_d[k];
                    double eps = eta / (1.0 - g->q);
                    double witchHat = (1.0 - g->q) / g->q * exp (v2) / (1.0 + eps);
                    g->mask[k] *= witchHat / (1.0 + witchHat);
                }

                if (g->mask[k] > g->gmax)
                    g->mask[k] = g->gmax;

                if (g->mask[k] != g->mask[k])
                    g->mask[k] = 0.01;

                g->prev_gamma[k] = gamma;
                g->prev_mask[k] = g->mask[k];
            }

            break;
        }

    case 1:
        {
            double gamma, eps_hat, v, ehr;

            for (k = 0; k < msize; k++)
            {
                gamma = std::min (g->lambda_y[k] / g->lambda_d[k], g->gamma_max);
                eps_hat = g->alpha * g->prev_mask[k] * g->prev_mask[k] * g->prev_gamma[k]
                    + (1.0 - g->alpha) * std::max (gamma - 1.0f, g->eps_floor);
                ehr = eps_hat / (1.0 + eps_hat);
                v = ehr * gamma;

                if ((g->mask[k] = ehr * exp (std::min (700.0, 0.5 * e1xb(v)))) > g->gmax)
                    g->mask[k] = g->gmax;

                if (g->mask[k] != g->mask[k])
                    g->mask[k] = 0.01;

                g->prev_gamma[k] = gamma;
                g->prev_mask[k] = g->mask[k];
            }

            break;
        }

    case 2:
        {
            double gamma, eps_hat, eps_p;

            for (k = 0; k < msize; k++)
            {
                gamma = std::min(g->lambda_y[k] / g->lambda_d[k], g->gamma_max);
                eps_hat = g->alpha * g->prev_mask[k] * g->prev_mask[k] * g->prev_gamma[k]
                    + (1.0 - g->alpha) * std::max(gamma - 1.0f, g->eps_floor);
                eps_p = eps_hat / (1.0 - g->q);
                g->mask[k] = getKey(g->GG, gamma, eps_hat) * getKey(g->GGS, gamma, eps_p);
                g->prev_gamma[k] = gamma;
                g->prev_mask[k] = g->mask[k];
            }

            break;
        }
    }

    if (g->ae_run)
        aepf();
}

void EMNR::execute(int _pos)
{
    if (run && _pos == position)
    {
        int i, j, k, sbuff, sbegin;
        double g1;

        for (i = 0; i < 2 * bsize; i += 2)
        {
            inaccum[iainidx] = in[i];
            iainidx = (iainidx + 1) % iasize;
        }

        nsamps += bsize;

        while (nsamps >= fsize)
        {
            for (i = 0, j = iaoutidx; i < fsize; i++, j = (j + 1) % iasize)
                forfftin[i] = window[i] * inaccum[j];

            iaoutidx = (iaoutidx + incr) % iasize;
            nsamps -= incr;
            fftwf_execute (Rfor);
            calc_gain();

            for (i = 0; i < msize; i++)
            {
                g1 = gain * mask[i];
                revfftin[2 * i + 0] = g1 * forfftout[2 * i + 0];
                revfftin[2 * i + 1] = g1 * forfftout[2 * i + 1];
            }

            fftwf_execute (Rrev);

            for (i = 0; i < fsize; i++)
                save[saveidx][i] = window[i] * revfftout[i];

            for (i = ovrlp; i > 0; i--)
            {
                sbuff = (saveidx + i) % ovrlp;
                sbegin = incr * (ovrlp - i);

                for (j = sbegin, k = oainidx; j < incr + sbegin; j++, k = (k + 1) % oasize)
                {
                    if ( i == ovrlp)
                        outaccum[k]  = save[sbuff][j];
                    else
                        outaccum[k] += save[sbuff][j];
                }
            }

            saveidx = (saveidx + 1) % ovrlp;
            oainidx = (oainidx + incr) % oasize;
        }

        for (i = 0; i < bsize; i++)
        {
            out[2 * i + 0] = outaccum[oaoutidx];
            out[2 * i + 1] = 0.0;
            oaoutidx = (oaoutidx + 1) % oasize;
        }
    }
    else if (out != in)
    {
        std::copy(in, in + bsize * 2, out);
    }
}

void EMNR::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void EMNR::setSamplerate(int _rate)
{
    decalc();
    rate = _rate;
    calc();
}

void EMNR::setSize(int _size)
{
    decalc();
    bsize = _size;
    calc();
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void EMNR::setGainMethod(int _method)
{
    g->gain_method = _method;
}

void EMNR::setNpeMethod(int _method)
{
    g->npe_method = _method;
}

void EMNR::setAeRun(int _run)
{
    g->ae_run = _run;
}

void EMNR::setAeZetaThresh(double _zetathresh)
{
    ae->zetaThresh = _zetathresh;
}

void EMNR::setAePsi(double _psi)
{
    ae->psi = _psi;
}

} // namespace WDSP
