/*  resample.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013 Warren Pratt, NR0V
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
#include "resample.hpp"

namespace WDSP {

/************************************************************************************************
*                                                                                               *
*                             VERSION FOR COMPLEX DOUBLE-PRECISION                              *
*                                                                                               *
************************************************************************************************/

void RESAMPLE::calc()
{
    int x;
    int y;
    int z;
    int i;
    int min_rate;
    double full_rate;
    double fc_norm_high;
    double fc_norm_low;
    std::vector<float> impulse;
    fc = fcin;
    ncoef = ncoefin;
    x = in_rate;
    y = out_rate;

    while (y != 0)
    {
        z = y;
        y = x % y;
        x = z;
    }

    L = out_rate / x;
    M = in_rate / x;

    L = L <= 0 ? 1 : L;
    M = M <= 0 ? 1 : M;

    if (in_rate < out_rate)
        min_rate = in_rate;
    else
        min_rate = out_rate;

    if (fc == 0.0)
        fc = 0.45 * (float)min_rate;

    full_rate = (double) (in_rate * L);
    fc_norm_high = fc / full_rate;

    if (fc_low < 0.0)
        fc_norm_low = - fc_norm_high;
    else
        fc_norm_low = fc_low / full_rate;

    if (ncoef == 0)
        ncoef = (int)(140.0 * full_rate / min_rate);

    ncoef = (ncoef / L + 1) * L;
    cpp = ncoef / L;
    h.resize(ncoef);
    FIR::fir_bandpass(impulse, ncoef, fc_norm_low, fc_norm_high, 1.0, 1, 0, gain * (double)L);
    i = 0;

    for (int j = 0; j < L; j++)
    {
        for (int k = 0; k < ncoef; k += L)
            h[i++] = impulse[j + k];
    }

    ringsize = cpp;
    ring.resize(ringsize);
    idx_in = ringsize - 1;
    phnum = 0;
}

RESAMPLE::RESAMPLE (
    int _run,
    int _size,
    float* _in,
    float* _out,
    int _in_rate,
    int _out_rate,
    double _fc,
    int _ncoef,
    double _gain
) :
    run(_run),
    size(_size),
    in(_in),
    out(_out),
    in_rate(_in_rate),
    out_rate(_out_rate),
    fcin(_fc),
    fc_low(-1.0),       // could add to create_resample() parameters
    ncoefin(_ncoef),
    gain(_gain)
{
    calc();
}


void RESAMPLE::flush()
{
    std::fill(ring.begin(), ring.end(), 0);
    idx_in = ringsize - 1;
    phnum = 0;
}

int RESAMPLE::execute()
{
    int outsamps = 0;

    if (run)
    {
        int n;
        int idx_out;
        double I;
        double Q;

        for (int i = 0; i < size; i++)
        {
            ring[2 * idx_in + 0] = in[2 * i + 0];
            ring[2 * idx_in + 1] = in[2 * i + 1];

            while (phnum < L)
            {
                I = 0.0;
                Q = 0.0;
                n = cpp * phnum;

                for (int j = 0; j < cpp; j++)
                {
                    if ((idx_out = idx_in + j) >= ringsize)
                        idx_out -= ringsize;

                    I += h[n + j] * ring[2 * idx_out + 0];
                    Q += h[n + j] * ring[2 * idx_out + 1];
                }

                out[2 * outsamps + 0] = (float) I;
                out[2 * outsamps + 1] = (float) Q;
                outsamps++;
                phnum += M;
            }

            phnum -= L;

            if (--idx_in < 0)
                idx_in = ringsize - 1;
        }
    }
    else if (in != out)
    {
        std::copy( in,  in + size * 2, out);
    }

    return outsamps;
}

void RESAMPLE::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void RESAMPLE::setSize(int _size)
{
    size = _size;
    flush();
}

void RESAMPLE::setInRate(int _rate)
{
    in_rate = _rate;
    calc();
}

void RESAMPLE::setOutRate(int _rate)
{
    out_rate = _rate;
    calc();
}

void RESAMPLE::setFCLow(double _fc_low)
{
    if (fc_low != _fc_low)
    {
        fc_low = _fc_low;
        calc();
    }
}

void RESAMPLE::setBandwidth(double _fc_low, double _fc_high)
{
    if (fc_low != _fc_low || _fc_high != fcin)
    {
        fc_low = _fc_low;
        fcin = _fc_high;
        calc();
    }
}

// exported calls


RESAMPLE* RESAMPLE::Create(int in_rate, int out_rate)
{
    return new RESAMPLE(1, 0, nullptr, nullptr, in_rate, out_rate, 0.0, 0, 1.0);
}


void RESAMPLE::Execute(float* input, float* output, int numsamps, int* outsamps, RESAMPLE* ptr)
{
    ptr->in = input;
    ptr->out = output;
    ptr->size = numsamps;
    *outsamps = ptr->execute();
}


void RESAMPLE::Destroy(RESAMPLE* ptr)
{
    delete  ptr;
}

} // namespace WDSP
