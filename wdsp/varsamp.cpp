/*  varsamp.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2017 Warren Pratt, NR0V
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
#include "varsamp.hpp"

namespace WDSP {

void VARSAMP::calc()
{
    float min_rate;
    float max_rate;
    float norm_rate;
    float fc_norm_high;
    float fc_norm_low;
    nom_ratio = (float)out_rate / (float)in_rate;
    cvar = var * nom_ratio;
    inv_cvar = 1.0f / cvar;
    old_inv_cvar = inv_cvar;
    dicvar = 0.0;
    delta = (float) fabs (1.0 / cvar - 1.0);
    fc = fcin;
    if (out_rate >= in_rate)
    {
        min_rate = (float)in_rate;
        norm_rate = min_rate;
    }
    else
    {
        min_rate = (float)out_rate;
        max_rate = (float)in_rate;
        norm_rate = max_rate;
    }
    if (fc == 0.0) fc = 0.95f * 0.45f * min_rate;
    fc_norm_high = fc / norm_rate;
    if (fc_low < 0.0)
        fc_norm_low = - fc_norm_high;
    else
        fc_norm_low = fc_low / norm_rate;
    rsize = (int)(140.0 * norm_rate / min_rate);
    ncoef = rsize + 1;
    ncoef += (R - 1) * (ncoef - 1);
    FIR::fir_bandpass(h, ncoef, fc_norm_low, fc_norm_high, (float)R, 1, 0, (float)R * gain);
    ring.resize(rsize * 2);
    idx_in = rsize - 1;
    h_offset = 0.0;
    hs.resize(rsize);
    isamps = 0.0;
}

VARSAMP::VARSAMP(
    int _run,
    int _size,
    float* _in,
    float* _out,
    int _in_rate,
    int _out_rate,
    float _fc,
    float _fc_low,
    int _R,
    float _gain,
    float _var,
    int _varmode
)
{
    run = _run;
    size = _size;
    in = _in;
    out = _out;
    in_rate = _in_rate;
    out_rate = _out_rate;
    fcin = _fc;
    fc_low = _fc_low;
    R = _R;
    gain = _gain;
    var = _var;
    varmode = _varmode;
    calc();
}

void VARSAMP::flush()
{
    std::fill(ring.begin(), ring.end(), 0);
    idx_in = rsize - 1;
    h_offset = 0.0;
    isamps = 0.0;
}

void VARSAMP::hshift()
{
    int i;
    int j;
    int k;
    int hidx;
    float frac;
    float pos;
    pos = (float)R * h_offset;
    hidx = (int)(pos);
    frac = pos - (float)hidx;
    for (i = rsize - 1, j = hidx, k = hidx + 1; i >= 0; i--, j += R, k += R)
        hs[i] = h[j] + frac * (h[k] - h[j]);
}

int VARSAMP::execute(float _var)
{
    int outsamps = 0;
    uint64_t const* picvar;
    uint64_t N;
    var = _var;
    old_inv_cvar = inv_cvar;
    cvar = var * nom_ratio;
    inv_cvar = 1.0f / cvar;
    if (varmode)
    {
        dicvar = (inv_cvar - old_inv_cvar) / (float)size;
        inv_cvar = old_inv_cvar;
    }
    else            dicvar = 0.0;
    if (run)
    {
        int idx_out;
        float I;
        float Q;
        for (int i = 0; i < size; i++)
        {
            ring[2 * idx_in + 0] = in[2 * i + 0];
            ring[2 * idx_in + 1] = in[2 * i + 1];
            inv_cvar += dicvar;
            picvar = (uint64_t*)(&inv_cvar);
            N = *picvar & 0xffffffffffff0000;
            inv_cvar = static_cast<float>(N);
            delta = 1.0f - inv_cvar;
            while (isamps < 1.0)
            {
                I = 0.0;
                Q = 0.0;
                hshift();
                h_offset += delta;
                while (h_offset >= 1.0) h_offset -= 1.0f;
                while (h_offset <  0.0) h_offset += 1.0f;
                for (int j = 0; j < rsize; j++)
                {
                    if ((idx_out = idx_in + j) >= rsize) idx_out -= rsize;
                    I += hs[j] * ring[2 * idx_out + 0];
                    Q += hs[j] * ring[2 * idx_out + 1];
                }
                out[2 * outsamps + 0] = I;
                out[2 * outsamps + 1] = Q;
                outsamps++;
                isamps += inv_cvar;
            }
            isamps -= 1.0f;
            if (--idx_in < 0) idx_in = rsize - 1;
        }
    }
    else if (in != out)
        std::copy( in,  in + size * 2, out);
    return outsamps;
}

void VARSAMP::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void VARSAMP::setSize(int _size)
{
    size = _size;
    flush();
}

void VARSAMP::setInRate(int _rate)
{
    in_rate = _rate;
    calc();
}

void VARSAMP::setOutRate(int _rate)
{
    out_rate = _rate;
    calc();
}

void VARSAMP::setFCLow(float _fc_low)
{
    if (_fc_low != fc_low)
    {
        fc_low = _fc_low;
        calc();
    }
}

void VARSAMP::setBandwidth(float _fc_low, float _fc_high)
{
    if (_fc_low != fc_low || _fc_high != fcin)
    {
        fc_low = _fc_low;
        fcin = _fc_high;
        calc();
    }
}

// exported calls

void* VARSAMP::create_varsampV (int _in_rate, int _out_rate, int R)
{
    return (void *) new VARSAMP(1, 0, nullptr, nullptr, _in_rate, _out_rate, 0.0, -1.0, R, 1.0, 1.0, 1);
}

void VARSAMP::xvarsampV (float* input, float* output, int numsamps, float var, int* outsamps, void* ptr)
{
    auto *a = (VARSAMP*) ptr;
    a->in = input;
    a->out = output;
    a->size = numsamps;
    *outsamps = a->execute(var);
}

void VARSAMP::destroy_varsampV (void* ptr)
{
    delete (VARSAMP*) ptr;
}

} // namespace WDSP
