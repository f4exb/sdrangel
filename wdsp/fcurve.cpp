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

void FCurve::fc_impulse (std::vector<float>& impulse, int nc, float f0, float f1, float g0, float, int curve, float samplerate, float scale, int ctfmode, int wintype)
{
    float* A  = new float[nc / 2 + 1]; // (float *) malloc0 ((nc / 2 + 1) * sizeof (float));
    int i;
    float fn, f;
    int mid = nc / 2;
    float g0_lin = pow(10.0, g0 / 20.0);
    if (nc & 1)
    {
        for (i = 0; i <= mid; i++)
        {
            fn = (float)i / (float)mid;
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
            fn = ((float)i + 0.5) / (float)mid;
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
        float lowmag, highmag, flow4, fhigh4;
        if (nc & 1)
        {
            low  = (int)(2.0 * f0 / samplerate * mid);
            high = (int)(2.0 * f1 / samplerate * mid + 0.5);
            lowmag = A[low];
            highmag = A[high];
            flow4 = pow((float)low / (float)mid, 4.0);
            fhigh4 = pow((float)high / (float)mid, 4.0);
            k = low;
            while (--k >= 0)
            {
                f = (float)k / (float)mid;
                lowmag *= (f * f * f * f) / flow4;
                if (lowmag < 1.0e-20) lowmag = 1.0e-20;
                A[k] = lowmag;
            }
            k = high;
            while (++k <= mid)
            {
                f = (float)k / (float)mid;
                highmag *= fhigh4 / (f * f * f * f);
                if (highmag < 1.0e-20) highmag = 1.0e-20;
                A[k] = highmag;
            }
        }
        else
        {
            low  = (int)(2.0 * f0 / samplerate * mid - 0.5);
            high = (int)(2.0 * f1 / samplerate * mid - 0.5);
            lowmag = A[low];
            highmag = A[high];
            flow4 = pow((float)low / (float)mid, 4.0);
            fhigh4 = pow((float)high / (float)mid, 4.0);
            k = low;
            while (--k >= 0)
            {
                f = (float)k / (float)mid;
                lowmag *= (f * f * f * f) / flow4;
                if (lowmag < 1.0e-20) lowmag = 1.0e-20;
                A[k] = lowmag;
            }
            k = high;
            while (++k < mid)
            {
                f = (float)k / (float)mid;
                highmag *= fhigh4 / (f * f * f * f);
                if (highmag < 1.0e-20) highmag = 1.0e-20;
                A[k] = highmag;
            }
        }
    }

    if (nc & 1)
        FIR::fir_fsamp_odd(impulse, nc, A, 1, 1.0, wintype);
    else
        FIR::fir_fsamp(impulse, nc, A, 1, 1.0, wintype);
    // print_impulse ("emph.txt", size + 1, impulse, 1, 0);
    delete[] (A);
}

// generate mask for Overlap-Save Filter
void FCurve::fc_mults (std::vector<float>& mults, int size, float f0, float f1, float g0, float g1, int curve, float samplerate, float scale, int ctfmode, int wintype)
{
    std::vector<float> impulse(2 * (size + 1));
    fc_impulse (impulse, size + 1, f0, f1, g0, g1, curve, samplerate, scale, ctfmode, wintype);
    FIR::fftcv_mults(mults, 2 * size, impulse.data());
}

} // namespace WDSP
