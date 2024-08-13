/*  slew.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2023 Warren Pratt, NR0V
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
#include "slew.hpp"
#include "TXA.hpp"

namespace WDSP {

void USLEW::calc()
{
    double delta;
    double theta;
    runmode = 0;
    state = _USLEW::BEGIN;
    count = 0;
    ndelup = (int)(tdelay * rate);
    ntup = (int)(tupslew * rate);
    cup.resize(ntup + 1);
    delta = PI / (float)ntup;
    theta = 0.0;
    for (int i = 0; i <= ntup; i++)
    {
        cup[i] = 0.5 * (1.0 - cos (theta));
        theta += delta;
    }
    *ch_upslew &= ~((long)1);
}

USLEW::USLEW(
    long *_ch_upslew,
    int _size,
    float* _in,
    float* _out,
    double _rate,
    double _tdelay,
    double _tupslew
) :
    ch_upslew(_ch_upslew),
    size(_size),
    in(_in),
    out(_out),
    rate(_rate),
    tdelay(_tdelay),
    tupslew(_tupslew)
{
    calc();
}

void USLEW::flush()
{
    state = _USLEW::BEGIN;
    runmode = 0;
    *ch_upslew &= ~1L;
}

void USLEW::execute (int check)
{
    if (!runmode && check)
        runmode = 1;

    long upslew = *ch_upslew;
    *ch_upslew = 1L;
    if (runmode && upslew) //_InterlockedAnd (ch_upslew, 1))
    {
        double I;
        double Q;
        for (int i = 0; i < size; i++)
        {
            I = in[2 * i + 0];
            Q = in[2 * i + 1];
            switch (state)
            {
            case _USLEW::BEGIN:
                out[2 * i + 0] = 0.0;
                out[2 * i + 1] = 0.0;
                if ((I != 0.0) || (Q != 0.0))
                {
                    if (ndelup > 0)
                    {
                        state = _USLEW::WAIT;
                        count = ndelup;
                    }
                    else if (ntup > 0)
                    {
                        state = _USLEW::UP;
                        count = ntup;
                    }
                    else
                        state = _USLEW::ON;
                }
                break;
            case _USLEW::WAIT:
                out[2 * i + 0] = 0.0;
                out[2 * i + 1] = 0.0;
                if (count-- == 0)
                {
                    if (ntup > 0)
                    {
                        state = _USLEW::UP;
                        count = ntup;
                    }
                    else
                        state = _USLEW::ON;
                }
                break;
            case _USLEW::UP:
                out[2 * i + 0] = (float) (I * cup[ntup - count]);
                out[2 * i + 1] = (float) (Q * cup[ntup - count]);
                if (count-- == 0)
                    state = _USLEW::ON;
                break;
            case _USLEW::ON:
                out[2 * i + 0] = (float) I;
                out[2 * i + 1] = (float) Q;
                *ch_upslew &= ~((long)1);
                runmode = 0;
                break;
            default:
                break;
            }
        }
    }
    else if (out != in)
        std::copy( in,  in + size * 2, out);
}

void USLEW::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void USLEW::setSamplerate(int _rate)
{
    rate = _rate;
    calc();
}

void USLEW::setSize(int _size)
{
    size = _size;
    flush();
}

/********************************************************************************************************
*                                                                                                       *
*                                            TXA Properties                                             *
*                                                                                                       *
********************************************************************************************************/

void USLEW::setuSlewTime(double _time)
{
    // NOTE:  'time' is in seconds
    tupslew = _time;
    calc();
}

void USLEW::setRun(int run)
{
    runmode = run;
}

} // namespace WDSP
