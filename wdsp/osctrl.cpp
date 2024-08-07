/*  osctrl.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014, 2017 Warren Pratt, NR0V
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

// This file is part of the implementation of the Overshoot Controller from
// "Controlled Envelope Single Sideband" by David L. Hershberger, W9GR, in
// the November/December 2014 issue of QEX.

#include "comm.hpp"
#include "osctrl.hpp"
#include "TXA.hpp"

namespace WDSP {

void OSCTRL::calc()
{
    pn = (int)((0.3 / bw) * rate + 0.5);
    if ((pn & 1) == 0) pn += 1;
    if (pn < 3) pn = 3;
    dl_len = pn >> 1;
    dl.resize(pn * 2);
    dlenv.resize(pn);
    in_idx = 0;
    out_idx = in_idx + dl_len;
    max_env = 0.0;
}

OSCTRL::OSCTRL(
    int _run,
    int _size,
    float* _inbuff,
    float* _outbuff,
    int _rate,
    double _osgain
) :
    run(_run),
    size(_size),
    inbuff(_inbuff),
    outbuff(_outbuff),
    rate(_rate),
    osgain(_osgain)
{
    bw = 3000.0;
    calc();
}

void OSCTRL::flush()
{
    std::fill(dl.begin(), dl.end(), 0);
    std::fill(dlenv.begin(), dlenv.end(), 0);
}

void OSCTRL::execute()
{
    if (run)
    {
        double divisor;
        for (int i = 0; i < size; i++)
        {
            dl[2 * in_idx + 0] = inbuff[2 * i + 0];                            // put sample in delay line
            dl[2 * in_idx + 1] = inbuff[2 * i + 1];
            env_out = dlenv[in_idx];                                           // take env out of delay line
            dlenv[in_idx] = sqrt (inbuff[2 * i + 0] * inbuff[2 * i + 0]     // put env in delay line
                                      + inbuff[2 * i + 1] * inbuff[2 * i + 1]);
            if (dlenv[in_idx]  >  max_env) max_env = dlenv[in_idx];
            if (env_out >= max_env && env_out > 0.0)                           // run the buffer
            {
                max_env = 0.0;
                for (int j = 0; j < pn; j++)
                    if (dlenv[j] > max_env) max_env = dlenv[j];
            }
            if (max_env > 1.0) divisor = 1.0 + osgain * (max_env - 1.0);
            else                  divisor = 1.0;
            outbuff[2 * i + 0] = (float) (dl[2 * out_idx + 0] / divisor);                // output sample
            outbuff[2 * i + 1] = (float) (dl[2 * out_idx + 1] / divisor);
            if (--in_idx  < 0) in_idx  += pn;
            if (--out_idx < 0) out_idx += pn;
        }
    }
    else if (inbuff != outbuff)
        std::copy(inbuff, inbuff + size * 2, outbuff);
}

void OSCTRL::setBuffers(float* _in, float* _out)
{
    inbuff = _in;
    outbuff = _out;
}

void OSCTRL::setSamplerate(int _rate)
{
    rate = _rate;
    calc();
}

void OSCTRL::setSize(int _size)
{
    size = _size;
    flush();
}

} // namespace WDSP
