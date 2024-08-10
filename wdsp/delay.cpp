/*  delay.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2019 Warren Pratt, NR0V
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
#include "delay.hpp"
#include "fir.hpp"

namespace WDSP {

DELAY::DELAY(
    int _run,
    int _size,
    float* _in,
    float* _out,
    int _rate,
    float _tdelta,
    float _tdelay
)
{
    run = _run;
    size = _size;
    in = _in;
    out = _out;
    rate = _rate;
    tdelta = _tdelta;
    tdelay = _tdelay;
    L = (int)(0.5 + 1.0 / (tdelta * (float)rate));
    adelta = 1.0f / (float) (rate * L);
    ft = 0.45f / (float)L;
    ncoef = (int)(60.0 / ft);
    ncoef = (ncoef / L + 1) * L;
    cpp = ncoef / L;
    phnum = (int)(0.5 + tdelay / adelta);
    snum = phnum / L;
    phnum %= L;
    idx_in = 0;
    adelay = adelta * (float) (snum * L + phnum);
    FIR::fir_bandpass (h, ncoef,-ft, +ft, 1.0, 1, 0, (float)L);
    rsize = cpp + (WSDEL - 1);
    ring.resize(rsize * 2);
}

void DELAY::flush()
{
    std::fill(ring.begin(), ring.end(), 0);
    idx_in = 0;
}

void DELAY::execute()
{
    if (run)
    {
        int j;
        int k;
        int idx;
        int n;
        float Itmp;
        float Qtmp;

        for (int i = 0; i < size; i++)
        {
            ring[2 * idx_in + 0] = in[2 * i + 0];
            ring[2 * idx_in + 1] = in[2 * i + 1];
            Itmp = 0.0;
            Qtmp = 0.0;

            if ((n = idx_in + snum) >= rsize)
                n -= rsize;

            for (j = 0, k = L - 1 - phnum; j < cpp; j++, k+= L)
            {
                if ((idx = n + j) >= rsize)
                    idx -= rsize;

                Itmp += ring[2 * idx + 0] * h[k];
                Qtmp += ring[2 * idx + 1] * h[k];
            }

            out[2 * i + 0] = Itmp;
            out[2 * i + 1] = Qtmp;

            if (--idx_in < 0)
                idx_in = rsize - 1;
        }
    }
    else if (out != in)
        std::copy( in,  in + size * 2, out);
}

/********************************************************************************************************
*                                                                                                       *
*                                             Properties                                                *
*                                                                                                       *
********************************************************************************************************/

void DELAY::setRun(int _run)
{
    run = _run;
}

float DELAY::setValue(float _tdelay)
{
    float _adelay;
    tdelay = _tdelay;
    phnum = (int)(0.5 + tdelay / adelta);
    snum = phnum / L;
    phnum %= L;
    _adelay = adelta * (float) (snum * L + phnum);
    adelay = _adelay;
    return adelay;
}

void DELAY::setBuffs(int _size, float* _in, float* _out)
{
    size = _size;
    in = _in;
    out = _out;
}

} // namespace WDSP

