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

#include <limits>
#include <vector>

#include "fftw3.h"
#include "comm.hpp"
#include "fir.hpp"

namespace WDSP {

void FIR::fftcv_mults (std::vector<float>& mults, int NM, const float* c_impulse)
{
    mults.resize(NM * 2);
    std::vector<float> cfft_impulse(NM * 2);
    fftwf_plan ptmp = fftwf_plan_dft_1d(
        NM,
        (fftwf_complex *) cfft_impulse.data(),
        (fftwf_complex *) mults.data(),
        FFTW_FORWARD,
        FFTW_PATIENT
    );
    std::fill(cfft_impulse.begin(), cfft_impulse.end(), 0);
    // store complex coefs right-justified in the buffer
    std::copy(c_impulse, c_impulse + (NM / 2 + 1) * 2, &(cfft_impulse[NM - 2]));
    fftwf_execute (ptmp);
    fftwf_destroy_plan (ptmp);
}

void FIR::get_fsamp_window(std::vector<float>& window, int N, int wintype)
{
    double arg0;
    double arg1;
    window.resize(N);
    switch (wintype)
    {
    case 0:
        arg0 = 2.0 * PI / ((double)N - 1.0);
        for (int i = 0; i < N; i++)
        {
            arg1 = cos(arg0 * (double)i);
            double val =   +0.21747
                + arg1 *  (-0.45325
                + arg1 *  (+0.28256
                + arg1 *  (-0.04672)));
            window[i] = (float) val;
        }
        break;
    case 1:
        arg0 = 2.0 * PI / ((double)N - 1.0);
        for (int i = 0; i < N; ++i)
        {
            arg1 = cos(arg0 * (double)i);
            double val =   +6.3964424114390378e-02
                + arg1 *  (-2.3993864599352804e-01
                + arg1 *  (+3.5015956323820469e-01
                + arg1 *  (-2.4774111897080783e-01
                + arg1 *  (+8.5438256055858031e-02
                + arg1 *  (-1.2320203369293225e-02
                + arg1 *  (+4.3778825791773474e-04))))));
            window[i] = (float) val;
        }
        break;
    default:
        for (int i = 0; i < N; i++)
            window[i] = 1.0;
    }
}

void FIR::fir_fsamp_odd (std::vector<float>& c_impulse, int N, const float* A, int rtype, double scale, int wintype)
{
    int mid = (N - 1) / 2;
    double mag;
    double phs;
    std::vector<float> fcoef(N * 2);
    fftwf_plan ptmp = fftwf_plan_dft_1d(
        N,
        (fftwf_complex *)fcoef.data(),
        (fftwf_complex *)c_impulse.data(),
        FFTW_BACKWARD,
        FFTW_PATIENT
    );
    double local_scale = 1.0 / (double) N;
    for (int i = 0; i <= mid; i++)
    {
        mag = A[i] * local_scale;
        phs = - (double)mid * TWOPI * (double)i / (double)N;
        fcoef[2 * i + 0] = (float) (mag * cos (phs));
        fcoef[2 * i + 1] = (float) (mag * sin (phs));
    }
    for (int i = mid + 1, j = 0; i < N; i++, j++)
    {
        fcoef[2 * i + 0] = + fcoef[2 * (mid - j) + 0];
        fcoef[2 * i + 1] = - fcoef[2 * (mid - j) + 1];
    }
    fftwf_execute (ptmp);
    fftwf_destroy_plan (ptmp);
    std::vector<float> window;
    get_fsamp_window(window, N, wintype);
    switch (rtype)
    {
    case 0:
        for (int i = 0; i < N; i++)
            c_impulse[i] = (float) (scale * c_impulse[2 * i] * window[i]);
        break;
    case 1:
        for (int i = 0; i < N; i++)
        {
            c_impulse[2 * i + 0] *= (float) (scale * window[i]);
            c_impulse[2 * i + 1] = 0.0;
        }
        break;
    default:
        break;
    }
}

void FIR::fir_fsamp (std::vector<float>& c_impulse, int N, const float* A, int rtype, double scale, int wintype)
{
    double sum;

    if (N & 1)
    {
        int M = (N - 1) / 2;
        for (int n = 0; n < M + 1; n++)
        {
            sum = 0.0;
            for (int k = 1; k < M + 1; k++)
                sum += 2.0 * A[k] * cos(TWOPI * (n - M) * k / N);
            c_impulse[2 * n + 0] = (float) ((1.0 / N) * (A[0] + sum));
            c_impulse[2 * n + 1] = 0.0;
        }
        for (int n = M + 1, j = 1; n < N; n++, j++)
        {
            c_impulse[2 * n + 0] = c_impulse[2 * (M - j) + 0];
            c_impulse[2 * n + 1] = 0.0;
        }
    }
    else
    {
        double M = (double)(N - 1) / 2.0;
        for (int n = 0; n < N / 2; n++)
        {
            sum = 0.0;
            for (int k = 1; k < N / 2; k++)
                sum += 2.0 * A[k] * cos(TWOPI * (n - M) * k / N);
            c_impulse[2 * n + 0] = (float) ((1.0 / N) * (A[0] + sum));
            c_impulse[2 * n + 1] = 0.0;
        }
        for (int n = N / 2, j = 1; n < N; n++, j++)
        {
            c_impulse[2 * n + 0] = c_impulse[2 * (N / 2 - j) + 0];
            c_impulse[2 * n + 1] = 0.0;
        }
    }
    std::vector<float> window;
    get_fsamp_window (window, N, wintype);
    switch (rtype)
    {
    case 0:
        for (int i = 0; i < N; i++)
            c_impulse[i] = (float) (scale * c_impulse[2 * i] * window[i]);
        break;
    case 1:
        for (int i = 0; i < N; i++)
            {
                c_impulse[2 * i + 0] *= (float) (scale * window[i]);
                c_impulse[2 * i + 1] = 0.0;
            }
        break;
    default:
        break;
    }
}

void FIR::fir_bandpass (std::vector<float>& c_impulse, int N, double f_low, double f_high, double samplerate, int wintype, int rtype, double scale)
{
    c_impulse.resize(N * 2);
    double ft = (f_high - f_low) / (2.0 * samplerate);
    double ft_rad = TWOPI * ft;
    double w_osc = PI * (f_high + f_low) / samplerate;
    double m = 0.5 * (double)(N - 1);
    double delta = PI / m;
    double cosphi;
    double posi;
    double posj;
    double sinc;
    double window;
    double coef;

    if (N & 1)
    {
        switch (rtype)
        {
        case 0:
            c_impulse[N >> 1] = (float) (scale * 2.0 * ft);
            break;
        case 1:
            c_impulse[N - 1] = (float) (scale * 2.0 * ft);
            c_impulse[  N  ] = 0.0;
            break;
        default:
            break;
        }
    }
    for (int i = (N + 1) / 2, j = N / 2 - 1; i < N; i++, j--)
    {
        posi = (double)i - m;
        posj = (double)j - m;
        sinc = sin (ft_rad * posi) / (PI * posi);

        if (wintype == 1) // Blackman-Harris 7-term
        {
            cosphi = cos (delta * i);
            window  =             + 6.3964424114390378e-02
                    + cosphi *  ( - 2.3993864599352804e-01
                    + cosphi *  ( + 3.5015956323820469e-01
                    + cosphi *  ( - 2.4774111897080783e-01
                    + cosphi *  ( + 8.5438256055858031e-02
                    + cosphi *  ( - 1.2320203369293225e-02
                    + cosphi *  ( + 4.3778825791773474e-04 ))))));
        }
        else // Blackman-Harris 4-term
        {
            cosphi = cos (delta * i);
            window  =             + 0.21747
                    + cosphi *  ( - 0.45325
                    + cosphi *  ( + 0.28256
                    + cosphi *  ( - 0.04672 )));
        }

        coef = scale * sinc * window;

        switch (rtype)
        {
        case 0:
            c_impulse[i] = (float) (+ coef * cos (posi * w_osc));
            c_impulse[j] = (float) (+ coef * cos (posj * w_osc));
            break;
        case 1:
            c_impulse[2 * i + 0] = (float) (+ coef * cos (posi * w_osc));
            c_impulse[2 * i + 1] = (float) (- coef * sin (posi * w_osc));
            c_impulse[2 * j + 0] = (float) (+ coef * cos (posj * w_osc));
            c_impulse[2 * j + 1] = (float) (- coef * sin (posj * w_osc));
            break;
        default:
            break;
        }
    }
}

void FIR::fir_read (std::vector<float>& c_impulse, int N, const char *filename, int rtype, float scale)
    // N = number of real or complex coefficients (see rtype)
    // *filename = filename
    // rtype = 0:  real coefficients
    // rtype = 1:  complex coefficients
    // scale = a scale factor that will be applied to the returned coefficients;
    //      if this is not needed, set it to 1.0
    // NOTE:  The number of values in the file must NOT exceed those implied by N and rtype
{
    FILE *file;
    float I;
    float Q;
    c_impulse.resize(N * 2);
    std::fill(c_impulse.begin(), c_impulse.end(), 0);
    file = fopen (filename, "r");

    if (!file) {
        return;
    }

    for (int i = 0; i < N; i++)
    {
        // read in the complex impulse response
        // NOTE:  IF the freq response is symmetrical about 0, the imag coeffs will all be zero.
        switch (rtype)
        {
        case 0:
        {
            int r = fscanf (file, "%e", &I);
            fprintf(stderr, "^%d parameters read\n", r);
            c_impulse[i] = + scale * I;
            break;
        }
        case 1:
        {
            int r = fscanf (file, "%e", &I);
            fprintf(stderr, "%d parameters read\n", r);
            r = fscanf (file, "%e", &Q);
            fprintf(stderr, "%d parameters read\n", r);
            c_impulse[2 * i + 0] = + scale * I;
            c_impulse[2 * i + 1] = - scale * Q;
            break;
        }
        default:
            break;
        }
    }
    fclose (file);
}

void FIR::analytic (int N, float* in, float* out)
{
    if (N < 2) {
        return;
    }

    double inv_N = 1.0 / (double) N;
    double two_inv_N = 2.0 * inv_N;
    std::vector<float> x(N * 2);

    fftwf_plan pfor = fftwf_plan_dft_1d (
        N,
        (fftwf_complex *) in,
        (fftwf_complex *) x.data(),
        FFTW_FORWARD,
        FFTW_PATIENT
    );

    fftwf_plan prev = fftwf_plan_dft_1d (
        N,
        (fftwf_complex *) x.data(),
        (fftwf_complex *) out,
        FFTW_BACKWARD,
        FFTW_PATIENT
    );

    fftwf_execute (pfor);
    x[0] *= (float) inv_N;
    x[1] *= (float) inv_N;

    for (int i = 1; i < N / 2; i++)
    {
        x[2 * i + 0] *= (float) two_inv_N;
        x[2 * i + 1] *= (float) two_inv_N;
    }

    x[N + 0] *= (float) inv_N;
    x[N + 1] *= (float) inv_N;
    memset (&x[N + 2], 0, (N - 2) * sizeof (float));
    fftwf_execute (prev);
    fftwf_destroy_plan (prev);
    fftwf_destroy_plan (pfor);
}

void FIR::mp_imp (int N, std::vector<float>& fir, std::vector<float>& mpfir, int pfactor, int polarity)
{
    int i;
    int size = N * pfactor;
    double inv_PN = 1.0 / (double)size;
    std::vector<float> firpad(size * 2);
    std::vector<float> firfreq(size * 2);
    std::vector<double> mag(size);
    std::vector<float> ana(size * 2);
    std::vector<float> impulse(size * 2);
    std::vector<float> newfreq(size * 2);
    std::copy(fir.begin(), fir.begin() + N * 2, firpad.begin());
    fftwf_plan pfor = fftwf_plan_dft_1d (
        size,
        (fftwf_complex *) firpad.data(),
        (fftwf_complex *) firfreq.data(),
        FFTW_FORWARD,
        FFTW_PATIENT);
    fftwf_plan prev = fftwf_plan_dft_1d (
        size,
        (fftwf_complex *) newfreq.data(),
        (fftwf_complex *) impulse.data(),
        FFTW_BACKWARD,
        FFTW_PATIENT
    );

    fftwf_execute (pfor);
    for (i = 0; i < size; i++)
    {
        double xr = firfreq[2 * i + 0];
        double xi = firfreq[2 * i + 1];
        mag[i] = sqrt (xr*xr + xi*xi) * inv_PN;
        if (mag[i] > 0.0)
            ana[2 * i + 0] = (float) log (mag[i]);
        else
            ana[2 * i + 0] = log (std::numeric_limits<float>::min());
    }
    analytic (size, ana.data(), ana.data());
    for (i = 0; i < size; i++)
    {
        newfreq[2 * i + 0] = (float) (+ mag[i] * cos (ana[2 * i + 1]));
        if (polarity)
            newfreq[2 * i + 1] = (float) (+ mag[i] * sin (ana[2 * i + 1]));
        else
            newfreq[2 * i + 1] = (float) (- mag[i] * sin (ana[2 * i + 1]));
    }
    fftwf_execute (prev);
    if (polarity)
        std::copy(&impulse[2 * (pfactor - 1) * N], &impulse[2 * (pfactor - 1) * N] + N * 2, mpfir.begin());
    else
        std::copy(impulse.begin(), impulse.end(), mpfir.begin());

    fftwf_destroy_plan (prev);
    fftwf_destroy_plan (pfor);
}

// impulse response of a zero frequency filter comprising a cascade of two resonators,
//    each followed by a detrending filter
void FIR::zff_impulse(std::vector<float>& c_dresdet, int nc, float scale)
{
    // nc = number of coefficients (power of two)
    int n_resdet = nc / 2 - 1;          // size of single zero-frequency resonator with detrender
    int n_dresdet = 2 * n_resdet - 1;   // size of two cascaded units; when we convolve these we get 2 * n - 1 length
    // allocate the single and make the values
    std::vector<float> resdet(n_resdet); // (float*)malloc0 (n_resdet * sizeof(float));
    for (int i = 1, j = 0, k = n_resdet - 1; i < nc / 4; i++, j++, k--)
        resdet[j] = resdet[k] = (float)(i * (i + 1) / 2);
    resdet[nc / 4 - 1] = (float)(nc / 4 * (nc / 4 + 1) / 2);
    // print_impulse ("resdet", n_resdet, resdet, 0, 0);
    // allocate the float and complex versions and make the values
    std::vector<float> dresdet(n_dresdet);
    auto div = (float) ((nc / 2 + 1) * (nc / 2 + 1));                 // calculate divisor
    c_dresdet.resize(nc * 2);
    for (int n = 0; n < n_dresdet; n++) // convolve to make the cascade
    {
        for (int k = 0; k < n_resdet; k++)
            if ((n - k) >= 0 && (n - k) < n_resdet)
                dresdet[n] += resdet[k] * resdet[n - k];
        dresdet[n] /= div;
        c_dresdet[2 * n + 0] = dresdet[n] * scale;
        c_dresdet[2 * n + 1] = 0.0;
    }
}

} // namespace WDSP
