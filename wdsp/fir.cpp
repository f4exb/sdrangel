/*  fir.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2016, 2022 Warren Pratt, NR0V
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

warren@pratt.one
*/

#define _CRT_SECURE_NO_WARNINGS
#include "fftw3.h"
#include "comm.hpp"
#include "fir.hpp"

namespace WDSP {

double* FIR::fftcv_mults (int NM, double* c_impulse)
{
    double* mults        = new double[NM * 2]; // (double *) malloc0 (NM * sizeof (dcomplex));
    double* cfft_impulse = new double[NM * 2]; // (double *) malloc0 (NM * sizeof (dcomplex));
    fftw_plan ptmp = fftw_plan_dft_1d(NM, (fftw_complex *) cfft_impulse,
            (fftw_complex *) mults, FFTW_FORWARD, FFTW_PATIENT);
    memset (cfft_impulse, 0, NM * sizeof (dcomplex));
    // store complex coefs right-justified in the buffer
    memcpy (&(cfft_impulse[NM - 2]), c_impulse, (NM / 2 + 1) * sizeof(dcomplex));
    fftw_execute (ptmp);
    fftw_destroy_plan (ptmp);
    delete[] cfft_impulse;
    return mults;
}

double* FIR::get_fsamp_window(int N, int wintype)
{
    int i;
    double arg0, arg1;
    double* window = new double[N]; // (double *) malloc0 (N * sizeof(double));
    switch (wintype)
    {
    case 0:
        arg0 = 2.0 * PI / ((double)N - 1.0);
        for (i = 0; i < N; i++)
        {
            arg1 = cos(arg0 * (double)i);
            window[i]  =   +0.21747
                + arg1 *  (-0.45325
                + arg1 *  (+0.28256
                + arg1 *  (-0.04672)));
        }
        break;
    case 1:
        arg0 = 2.0 * PI / ((double)N - 1.0);
        for (i = 0; i < N; ++i)
        {
            arg1 = cos(arg0 * (double)i);
            window[i]  =   +6.3964424114390378e-02
                + arg1 *  (-2.3993864599352804e-01
                + arg1 *  (+3.5015956323820469e-01
                + arg1 *  (-2.4774111897080783e-01
                + arg1 *  (+8.5438256055858031e-02
                + arg1 *  (-1.2320203369293225e-02
                + arg1 *  (+4.3778825791773474e-04))))));
        }
        break;
    default:
        for (i = 0; i < N; i++)
            window[i] = 1.0;
    }
    return window;
}

double* FIR::fir_fsamp_odd (int N, double* A, int rtype, double scale, int wintype)
{
    int i, j;
    int mid = (N - 1) / 2;
    double mag, phs;
    double* window;
    double *fcoef     = new double[N * 2]; // (double *) malloc0 (N * sizeof (dcomplex));
    double *c_impulse = new double[N * 2]; // (double *) malloc0 (N * sizeof (dcomplex));
    fftw_plan ptmp = fftw_plan_dft_1d(N, (fftw_complex *)fcoef, (fftw_complex *)c_impulse, FFTW_BACKWARD, FFTW_PATIENT);
    double local_scale = 1.0 / (double)N;
    for (i = 0; i <= mid; i++)
    {
        mag = A[i] * local_scale;
        phs = - (double)mid * TWOPI * (double)i / (double)N;
        fcoef[2 * i + 0] = mag * cos (phs);
        fcoef[2 * i + 1] = mag * sin (phs);
    }
    for (i = mid + 1, j = 0; i < N; i++, j++)
    {
        fcoef[2 * i + 0] = + fcoef[2 * (mid - j) + 0];
        fcoef[2 * i + 1] = - fcoef[2 * (mid - j) + 1];
    }
    fftw_execute (ptmp);
    fftw_destroy_plan (ptmp);
    delete[] fcoef;
    window = get_fsamp_window(N, wintype);
    switch (rtype)
    {
    case 0:
        for (i = 0; i < N; i++)
            c_impulse[i] = scale * c_impulse[2 * i] * window[i];
        break;
    case 1:
        for (i = 0; i < N; i++)
        {
            c_impulse[2 * i + 0] *= scale * window[i];
            c_impulse[2 * i + 1] = 0.0;
        }
        break;
    }
    delete[] window;
    return c_impulse;
}

double* FIR::fir_fsamp (int N, double* A, int rtype, double scale, int wintype)
{
    int n, i, j, k;
    double sum;
    double* window;
    double *c_impulse = new double[N * 2]; // (double *) malloc0 (N * sizeof (complex));

    if (N & 1)
    {
        int M = (N - 1) / 2;
        for (n = 0; n < M + 1; n++)
        {
            sum = 0.0;
            for (k = 1; k < M + 1; k++)
                sum += 2.0 * A[k] * cos(TWOPI * (n - M) * k / N);
            c_impulse[2 * n + 0] = (1.0 / N) * (A[0] + sum);
            c_impulse[2 * n + 1] = 0.0;
        }
        for (n = M + 1, j = 1; n < N; n++, j++)
        {
            c_impulse[2 * n + 0] = c_impulse[2 * (M - j) + 0];
            c_impulse[2 * n + 1] = 0.0;
        }
    }
    else
    {
        double M = (double)(N - 1) / 2.0;
        for (n = 0; n < N / 2; n++)
        {
            sum = 0.0;
            for (k = 1; k < N / 2; k++)
                sum += 2.0 * A[k] * cos(TWOPI * (n - M) * k / N);
            c_impulse[2 * n + 0] = (1.0 / N) * (A[0] + sum);
            c_impulse[2 * n + 1] = 0.0;
        }
        for (n = N / 2, j = 1; n < N; n++, j++)
        {
            c_impulse[2 * n + 0] = c_impulse[2 * (N / 2 - j) + 0];
            c_impulse[2 * n + 1] = 0.0;
        }
    }
    window = get_fsamp_window (N, wintype);
    switch (rtype)
    {
    case 0:
        for (i = 0; i < N; i++)
            c_impulse[i] = scale * c_impulse[2 * i] * window[i];
        break;
    case 1:
        for (i = 0; i < N; i++)
            {
                c_impulse[2 * i + 0] *= scale * window[i];
                c_impulse[2 * i + 1] = 0.0;
            }
        break;
    }
    delete[] window;
    return c_impulse;
}

double* FIR::fir_bandpass (int N, double f_low, double f_high, double samplerate, int wintype, int rtype, double scale)
{
    double *c_impulse = new double[N * 2]; // (double *) malloc0 (N * sizeof (complex));
    double ft = (f_high - f_low) / (2.0 * samplerate);
    double ft_rad = TWOPI * ft;
    double w_osc = PI * (f_high + f_low) / samplerate;
    int i, j;
    double m = 0.5 * (double)(N - 1);
    double delta = PI / m;
    double cosphi;
    double posi, posj;
    double sinc, window, coef;

    if (N & 1)
    {
        switch (rtype)
        {
        case 0:
            c_impulse[N >> 1] = scale * 2.0 * ft;
            break;
        case 1:
            c_impulse[N - 1] = scale * 2.0 * ft;
            c_impulse[  N  ] = 0.0;
            break;
        }
    }
    for (i = (N + 1) / 2, j = N / 2 - 1; i < N; i++, j--)
    {
        posi = (double)i - m;
        posj = (double)j - m;
        sinc = sin (ft_rad * posi) / (PI * posi);
        switch (wintype)
        {
        case 0: // Blackman-Harris 4-term
            cosphi = cos (delta * i);
            window  =             + 0.21747
                    + cosphi *  ( - 0.45325
                    + cosphi *  ( + 0.28256
                    + cosphi *  ( - 0.04672 )));
            break;
        case 1: // Blackman-Harris 7-term
            cosphi = cos (delta * i);
            window  =             + 6.3964424114390378e-02
                    + cosphi *  ( - 2.3993864599352804e-01
                    + cosphi *  ( + 3.5015956323820469e-01
                    + cosphi *  ( - 2.4774111897080783e-01
                    + cosphi *  ( + 8.5438256055858031e-02
                    + cosphi *  ( - 1.2320203369293225e-02
                    + cosphi *  ( + 4.3778825791773474e-04 ))))));
            break;
        }
        coef = scale * sinc * window;
        switch (rtype)
        {
        case 0:
            c_impulse[i] = + coef * cos (posi * w_osc);
            c_impulse[j] = + coef * cos (posj * w_osc);
            break;
        case 1:
            c_impulse[2 * i + 0] = + coef * cos (posi * w_osc);
            c_impulse[2 * i + 1] = - coef * sin (posi * w_osc);
            c_impulse[2 * j + 0] = + coef * cos (posj * w_osc);
            c_impulse[2 * j + 1] = - coef * sin (posj * w_osc);
            break;
        }
    }
    return c_impulse;
}

double *FIR::fir_read (int N, const char *filename, int rtype, double scale)
    // N = number of real or complex coefficients (see rtype)
    // *filename = filename
    // rtype = 0:  real coefficients
    // rtype = 1:  complex coefficients
    // scale = a scale factor that will be applied to the returned coefficients;
    //      if this is not needed, set it to 1.0
    // NOTE:  The number of values in the file must NOT exceed those implied by N and rtype
{
    FILE *file;
    int i;
    double I, Q;
    double *c_impulse = new double[N * 2]; // (double *) malloc0 (N * sizeof (complex));
    file = fopen (filename, "r");
    for (i = 0; i < N; i++)
    {
        // read in the complex impulse response
        // NOTE:  IF the freq response is symmetrical about 0, the imag coeffs will all be zero.
        switch (rtype)
        {
        case 0:
        {
            int r = fscanf (file, "%le", &I);
            fprintf(stderr, "^%d parameters read\n", r);
            c_impulse[i] = + scale * I;
            break;
        }
        case 1:
        {
            int r = fscanf (file, "%le", &I);
            fprintf(stderr, "%d parameters read\n", r);
            r = fscanf (file, "%le", &Q);
            fprintf(stderr, "%d parameters read\n", r);
            c_impulse[2 * i + 0] = + scale * I;
            c_impulse[2 * i + 1] = - scale * Q;
            break;
        }
        }
    }
    fclose (file);
    return c_impulse;
}

void FIR::analytic (int N, double* in, double* out)
{
    int i;
    double inv_N = 1.0 / (double)N;
    double two_inv_N = 2.0 * inv_N;
    double* x = new double[N * 2]; // (double *) malloc0 (N * sizeof (complex));
    fftw_plan pfor = fftw_plan_dft_1d (N, (fftw_complex *) in,
            (fftw_complex *) x, FFTW_FORWARD, FFTW_PATIENT);
    fftw_plan prev = fftw_plan_dft_1d (N, (fftw_complex *) x,
            (fftw_complex *) out, FFTW_BACKWARD, FFTW_PATIENT);
    fftw_execute (pfor);
    x[0] *= inv_N;
    x[1] *= inv_N;
    for (i = 1; i < N / 2; i++)
    {
        x[2 * i + 0] *= two_inv_N;
        x[2 * i + 1] *= two_inv_N;
    }
    x[N + 0] *= inv_N;
    x[N + 1] *= inv_N;
    memset (&x[N + 2], 0, (N - 2) * sizeof (double));
    fftw_execute (prev);
    fftw_destroy_plan (prev);
    fftw_destroy_plan (pfor);
    delete[] x;
}

void FIR::mp_imp (int N, double* fir, double* mpfir, int pfactor, int polarity)
{
    int i;
    int size = N * pfactor;
    double inv_PN = 1.0 / (double)size;
    double* firpad  = new double[size * 2]; // (double *) malloc0 (size * sizeof (complex));
    double* firfreq = new double[size * 2]; // (double *) malloc0 (size * sizeof (complex));
    double* mag     = new double[size]; // (double *) malloc0 (size * sizeof (double));
    double* ana     = new double[size * 2]; // (double *) malloc0 (size * sizeof (complex));
    double* impulse = new double[size * 2]; // (double *) malloc0 (size * sizeof (complex));
    double* newfreq = new double[size * 2]; // (double *) malloc0 (size * sizeof (complex));
    memcpy (firpad, fir, N * sizeof (dcomplex));
    fftw_plan pfor = fftw_plan_dft_1d (size, (fftw_complex *) firpad,
            (fftw_complex *) firfreq, FFTW_FORWARD, FFTW_PATIENT);
    fftw_plan prev = fftw_plan_dft_1d (size, (fftw_complex *) newfreq,
            (fftw_complex *) impulse, FFTW_BACKWARD, FFTW_PATIENT);
    // print_impulse("orig_imp.txt", N, fir, 1, 0);
    fftw_execute (pfor);
    for (i = 0; i < size; i++)
    {
        mag[i] = sqrt (firfreq[2 * i + 0] * firfreq[2 * i + 0] + firfreq[2 * i + 1] * firfreq[2 * i + 1]) * inv_PN;
        if (mag[i] > 0.0)
            ana[2 * i + 0] = log (mag[i]);
        else
            ana[2 * i + 0] = log (1.0e-300);
    }
    analytic (size, ana, ana);
    for (i = 0; i < size; i++)
    {
        newfreq[2 * i + 0] = + mag[i] * cos (ana[2 * i + 1]);
        if (polarity)
            newfreq[2 * i + 1] = + mag[i] * sin (ana[2 * i + 1]);
        else
            newfreq[2 * i + 1] = - mag[i] * sin (ana[2 * i + 1]);
    }
    fftw_execute (prev);
    if (polarity)
        memcpy (mpfir, &impulse[2 * (pfactor - 1) * N], N * sizeof (dcomplex));
    else
        memcpy (mpfir, impulse, N * sizeof (dcomplex));
    // print_impulse("min_imp.txt", N, mpfir, 1, 0);
    fftw_destroy_plan (prev);
    fftw_destroy_plan (pfor);
    delete[] (newfreq);
    delete[] (impulse);
    delete[] (ana);
    delete[] (mag);
    delete[] (firfreq);
    delete[] (firpad);
}

// impulse response of a zero frequency filter comprising a cascade of two resonators,
//    each followed by a detrending filter
double* FIR::zff_impulse(int nc, double scale)
{
    // nc = number of coefficients (power of two)
    int n_resdet = nc / 2 - 1;          // size of single zero-frequency resonator with detrender
    int n_dresdet = 2 * n_resdet - 1;   // size of two cascaded units; when we convolve these we get 2 * n - 1 length
    // allocate the single and make the values
    double* resdet = new double[n_resdet]; // (double*)malloc0 (n_resdet * sizeof(double));
    for (int i = 1, j = 0, k = n_resdet - 1; i < nc / 4; i++, j++, k--)
        resdet[j] = resdet[k] = (double)(i * (i + 1) / 2);
    resdet[nc / 4 - 1] = (double)(nc / 4 * (nc / 4 + 1) / 2);
    // print_impulse ("resdet", n_resdet, resdet, 0, 0);
    // allocate the double and complex versions and make the values
    double* dresdet = new double[n_dresdet]; // (double*)malloc0 (n_dresdet * sizeof(double));
    double div = (double)((nc / 2 + 1) * (nc / 2 + 1));                 // calculate divisor
    double* c_dresdet = new double[nc * 2]; // (double*)malloc0 (nc * sizeof(complex));
    for (int n = 0; n < n_dresdet; n++) // convolve to make the cascade
    {
        for (int k = 0; k < n_resdet; k++)
            if ((n - k) >= 0 && (n - k) < n_resdet)
                dresdet[n] += resdet[k] * resdet[n - k];
        dresdet[n] /= div;
        c_dresdet[2 * n + 0] = dresdet[n] * scale;
        c_dresdet[2 * n + 1] = 0.0;
    }
    // print_impulse("dresdet", n_dresdet, dresdet, 0, 0);
    // print_impulse("c_dresdet", nc, c_dresdet, 1, 0);
    delete[] (dresdet);
    delete[] (resdet);
    return c_dresdet;
}

} // namespace WDSP
