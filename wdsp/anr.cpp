/*  anr.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2012, 2013 Warren Pratt, NR0V
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
#include "anr.hpp"
#include "amd.hpp"
#include "snba.hpp"
#include "emnr.hpp"
#include "anf.hpp"
#include "bandpass.hpp"

namespace WDSP {

ANR::ANR(
    int _run,
    int _position,
    int _buff_size,
    float *_in_buff,
    float *_out_buff,
    int _dline_size,
    int _n_taps,
    int _delay,
    double _two_mu,
    double _gamma,
    double _lidx,
    double _lidx_min,
    double _lidx_max,
    double _ngamma,
    double _den_mult,
    double _lincr,
    double _ldecr
) :
    run(_run),
    position(_position),
    buff_size(_buff_size),
    in_buff(_in_buff),
    out_buff(_out_buff),
    dline_size(_dline_size),
    mask(_dline_size - 1),
    n_taps(_n_taps),
    delay(_delay),
    two_mu(_two_mu),
    gamma(_gamma),
    in_idx(0),
    lidx(_lidx),
    lidx_min(_lidx_min),
    lidx_max(_lidx_max),
    ngamma(_ngamma),
    den_mult(_den_mult),
    lincr(_lincr),
    ldecr(_ldecr)
{
    std::fill(d.begin(), d.end(), 0);
    std::fill(w.begin(), w.end(), 0);
}

void ANR::execute(int _position)
{
    int idx;
    double c0;
    double c1;
    double y;
    double error;
    double sigma;
    double inv_sigp;
    double nel;
    double nev;

    if (run && (position == _position))
    {
        for (int i = 0; i < buff_size; i++)
        {
            d[in_idx] = in_buff[2 * i + 0];

            y = 0;
            sigma = 0;

            for (int j = 0; j < n_taps; j++)
            {
                idx = (in_idx + j + delay) & mask;
                y += w[j] * d[idx];
                sigma += d[idx] * d[idx];
            }

            inv_sigp = 1.0 / (sigma + 1e-10);
            error = d[in_idx] - y;

            out_buff[2 * i + 0] = (float) y;
            out_buff[2 * i + 1] = 0.0;

            if ((nel = error * (1.0 - two_mu * sigma * inv_sigp)) < 0.0)
                nel = -nel;
            if ((nev = d[in_idx] - (1.0 - two_mu * ngamma) * y - two_mu * error * sigma * inv_sigp) < 0.0)
                nev = -nev;

            if (nev < nel)
            {
                if ((lidx += lincr) > lidx_max)
                    lidx = lidx_max;
            }
            else
            {
                if ((lidx -= ldecr) < lidx_min)
                    lidx = lidx_min;
            }

            ngamma = gamma * (lidx * lidx) * (lidx * lidx) * den_mult;
            c0 = 1.0 - two_mu * ngamma;
            c1 = two_mu * error * inv_sigp;

            for (int j = 0; j < n_taps; j++)
            {
                idx = (in_idx + j + delay) & mask;
                w[j] = c0 * w[j] + c1 * d[idx];
            }

            in_idx = (in_idx + mask) & mask;
        }
    }
    else if (in_buff != out_buff)
    {
        std::copy(in_buff, in_buff + buff_size * 2, out_buff);
    }
}

void ANR::flush()
{
    std::fill(d.begin(), d.end(), 0);
    std::fill(w.begin(), w.end(), 0);
    in_idx = 0;
}

void ANR::setBuffers(float* _in, float* _out)
{
    in_buff = _in;
    out_buff = _out;
}

void ANR::setSamplerate(int)
{
    flush();
}

void ANR::setSize(int _size)
{
    buff_size = _size;
    flush();
}

/********************************************************************************************************
*                                                                                                       *
*                                        Public Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void ANR::setVals(int _taps, int _delay, double _gain, double _leakage)
{
    n_taps = _taps;
    delay = _delay;
    two_mu = _gain;
    gamma = _leakage;
    flush();
}

void ANR::setTaps(int _taps)
{
    n_taps = _taps;
    flush();
}

void ANR::setDelay(int _delay)
{
    delay = _delay;
    flush();
}

void ANR::setGain(double _gain)
{
    two_mu = _gain;
    flush();
}

void ANR::setLeakage(double _leakage)
{
    gamma = _leakage;
    flush();
}

} // namespace WDSP
