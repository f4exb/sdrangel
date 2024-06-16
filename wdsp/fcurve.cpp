/*  fcurve.c

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

#include "comm.hpp"
#include "fcurve.hpp"
#include "fir.hpp"

namespace WDSP {

double* FCurve::fc_impulse (int nc, double f0, double f1, double g0, double, int curve, double samplerate, double scale, int ctfmode, int wintype)
{
    double* A  = new double[nc / 2 + 1]; // (double *) malloc0 ((nc / 2 + 1) * sizeof (double));
    int i;
    double fn, f;
    double* impulse;
    int mid = nc / 2;
    double g0_lin = pow(10.0, g0 / 20.0);
    if (nc & 1)
    {
        for (i = 0; i <= mid; i++)
        {
            fn = (double)i / (double)mid;
            f = fn * samplerate / 2.0;
            switch (curve)
            {
            case 0: // fm pre-emphasis
                if (f0 > 0.0)
                    A[i] = scale * (g0_lin * f / f0);
                else
                    A[i] = 0.0;
                break;
            case 1: // fm de-emphasis
                if (f > 0.0)
                    A[i] = scale * (g0_lin * f0 / f);
                else
                    A[i] = 0.0;
                break;
            }
        }
    }
    else
    {
        for (i = 0; i < mid; i++)
        {
            fn = ((double)i + 0.5) / (double)mid;
            f = fn * samplerate / 2.0;
            switch (curve)
            {
            case 0: // fm pre-emphasis
                if (f0 > 0.0)
                    A[i] = scale * (g0_lin * f / f0);
                else
                    A[i] = 0.0;
                break;
            case 1: // fm de-emphasis
                if (f > 0.0)
                    A[i] = scale * (g0_lin * f0 / f);
                else
                    A[i] = 0.0;
                break;
            }
        }
    }
    if (ctfmode == 0)
    {
        int k, low, high;
        double lowmag, highmag, flow4, fhigh4;
        if (nc & 1)
        {
            low  = (int)(2.0 * f0 / samplerate * mid);
            high = (int)(2.0 * f1 / samplerate * mid + 0.5);
            lowmag = A[low];
            highmag = A[high];
            flow4 = pow((double)low / (double)mid, 4.0);
            fhigh4 = pow((double)high / (double)mid, 4.0);
            k = low;
            while (--k >= 0)
            {
                f = (double)k / (double)mid;
                lowmag *= (f * f * f * f) / flow4;
                if (lowmag < 1.0e-100) lowmag = 1.0e-100;
                A[k] = lowmag;
            }
            k = high;
            while (++k <= mid)
            {
                f = (double)k / (double)mid;
                highmag *= fhigh4 / (f * f * f * f);
                if (highmag < 1.0e-100) highmag = 1.0e-100;
                A[k] = highmag;
            }
        }
        else
        {
            low  = (int)(2.0 * f0 / samplerate * mid - 0.5);
            high = (int)(2.0 * f1 / samplerate * mid - 0.5);
            lowmag = A[low];
            highmag = A[high];
            flow4 = pow((double)low / (double)mid, 4.0);
            fhigh4 = pow((double)high / (double)mid, 4.0);
            k = low;
            while (--k >= 0)
            {
                f = (double)k / (double)mid;
                lowmag *= (f * f * f * f) / flow4;
                if (lowmag < 1.0e-100) lowmag = 1.0e-100;
                A[k] = lowmag;
            }
            k = high;
            while (++k < mid)
            {
                f = (double)k / (double)mid;
                highmag *= fhigh4 / (f * f * f * f);
                if (highmag < 1.0e-100) highmag = 1.0e-100;
                A[k] = highmag;
            }
        }
    }
    if (nc & 1)
        impulse = FIR::fir_fsamp_odd(nc, A, 1, 1.0, wintype);
    else
        impulse = FIR::fir_fsamp(nc, A, 1, 1.0, wintype);
    // print_impulse ("emph.txt", size + 1, impulse, 1, 0);
    delete[] (A);
    return impulse;
}

// generate mask for Overlap-Save Filter
double* FCurve::fc_mults (int size, double f0, double f1, double g0, double g1, int curve, double samplerate, double scale, int ctfmode, int wintype)
{
    double* impulse = fc_impulse (size + 1, f0, f1, g0, g1, curve, samplerate, scale, ctfmode, wintype);
    double* mults = FIR::fftcv_mults(2 * size, impulse);
    delete[] (impulse);
    return mults;
}

} // namespace WDSP
