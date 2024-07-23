/*  anb.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2014 Warren Pratt, NR0V
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
#include "anb.hpp"
#include "RXA.hpp"

#define MAX_TAU         (0.01)     // maximum transition time, signal<->zero (slew time)
#define MAX_ADVTIME     (0.01)     // maximum deadtime (zero output) in advance of detected noise
#define MAX_SAMPLERATE  (1536000)

namespace WDSP {

void ANB::initBlanker()
{
    int i;
    trans_count = (int)(tau * samplerate);

    if (trans_count < 2)
        trans_count = 2;

    hang_count = (int)(hangtime * samplerate);
    adv_count = (int)(advtime * samplerate);
    count = 0;
    in_idx = trans_count + adv_count;
    out_idx = 0;
    coef = PI / trans_count;
    state = 0;
    avg = 1.0;
    power = 1.0;
    backmult = exp(-1.0 / (samplerate * backtau));
    ombackmult = 1.0 - backmult;

    for (i = 0; i <= trans_count; i++)
        wave[i] = 0.5 * cos(i * coef);

    std::fill(dline, dline + dline_size * 2, 0);
}

ANB::ANB  (
    int _run,
    int _buffsize,
    float* _in,
    float* _out,
    double _samplerate,
    double _tau,
    double _hangtime,
    double _advtime,
    double _backtau,
    double _threshold
)
{
    run = _run;
    buffsize = _buffsize;
    in = _in;
    out = _out;
    samplerate = _samplerate;
    tau = _tau;
    hangtime = _hangtime;
    advtime = _advtime;
    backtau = _backtau;
    threshold = _threshold;
    wave = new double[((int)(MAX_SAMPLERATE * MAX_TAU) + 1)];
    dline_size = (int)((MAX_TAU + MAX_ADVTIME) * MAX_SAMPLERATE) + 1;
    dline = new float[dline_size * 2];
    initBlanker();
    legacy = new float[2048 * 2];  /////////////// legacy interface - remove
}

ANB::~ANB()
{
    delete[] legacy;                                                                                      /////////////// legacy interface - remove
    delete[] dline;
    delete[] wave;
}

void ANB::x()
{
    double scale;
    double mag;
    int i;

    if (run)
    {
        for (i = 0; i < buffsize; i++)
        {
            double xr = in[2 * i + 0];
            double xi = in[2 * i + 1];
            mag = sqrt(xr*xr + xi*xi);
            avg = backmult * avg + ombackmult * mag;
            dline[2 * in_idx + 0] = in[2 * i + 0];
            dline[2 * in_idx + 1] = in[2 * i + 1];

            if (mag > (avg * threshold))
                count = trans_count + adv_count;

            switch (state)
            {
                case 0:
                    out[2 * i + 0] = dline[2 * out_idx + 0];
                    out[2 * i + 1] = dline[2 * out_idx + 1];

                    if (count > 0)
                    {
                        state = 1;
                        dtime = 0;
                        power = 1.0;
                    }

                    break;

                case 1:
                    scale = power * (0.5 + wave[dtime]);
                    out[2 * i + 0] = dline[2 * out_idx + 0] * scale;
                    out[2 * i + 1] = dline[2 * out_idx + 1] * scale;

                    if (++dtime > trans_count)
                    {
                        state = 2;
                        atime = 0;
                    }

                    break;

                case 2:
                    out[2 * i + 0] = 0.0;
                    out[2 * i + 1] = 0.0;

                    if (++atime > adv_count)
                        state = 3;

                    break;

                case 3:
                    if (count > 0)
                        htime = -count;

                    out[2 * i + 0] = 0.0;
                    out[2 * i + 1] = 0.0;

                    if (++htime > hang_count)
                    {
                        state = 4;
                        itime = 0;

                    }

                    break;

                case 4:
                    scale = 0.5 - wave[itime];
                    out[2 * i + 0] = dline[2 * out_idx + 0] * scale;
                    out[2 * i + 1] = dline[2 * out_idx + 1] * scale;

                    if (count > 0)
                    {
                        state = 1;
                        dtime = 0;
                        power = scale;
                    }
                    else if (++itime > trans_count)
                    {
                        state = 0;
                    }

                    break;
            }

            if (count > 0)
                count--;

            if (++in_idx == dline_size)
                in_idx = 0;

            if (++out_idx == dline_size)
                out_idx = 0;
        }
    }
    else if (in != out)
    {
        std::copy(in, in + buffsize * 2, out);
    }
}

void ANB::setBuffers(float* in, float* out)
{
    in = in;
    out = out;
}

void ANB::setSize(int size)
{
    buffsize = size;
    initBlanker();
}

/********************************************************************************************************
*                                                                                                       *
*                                         Common interface                                              *
*                                                                                                       *
********************************************************************************************************/

void ANB::setRun (int _run)
{
    run = _run;
}

void ANB::setBuffsize (int size)
{
    buffsize = size;
}

void ANB::setSamplerate (int rate)
{
    samplerate = (double) rate;
    initBlanker();
}

void ANB::setTau (double _tau)
{
    tau = _tau;
    initBlanker();
}

void ANB::setHangtime (double time)
{
    hangtime = time;
    initBlanker();
}

void ANB::setAdvtime (double time)
{
    advtime = time;
    initBlanker();
}

void ANB::setBacktau (double tau)
{
    backtau = tau;
    initBlanker();
}

void ANB::setThreshold (double thresh)
{
    threshold = thresh;
}

}

