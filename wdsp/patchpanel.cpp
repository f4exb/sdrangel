/*  patchpanel.c

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
#include "patchpanel.hpp"

namespace WDSP {

PANEL::PANEL(
    int _run,
    int _size,
    float* _in,
    float* _out,
    double _gain1,
    double _gain2I,
    double _gain2Q,
    int _inselect,
    int _copy
) :
    run(_run),
    size(_size),
    in(_in),
    out(_out),
    gain1(_gain1),
    gain2I(_gain2I),
    gain2Q(_gain2Q),
    inselect(_inselect),
    copy(_copy)
{
}

void PANEL::flush()
{
    // There is no data to be reset internally
}

void PANEL::execute()
{
    int i;
    double I;
    double Q;
    double gainI = gain1 * gain2I;
    double gainQ = gain1 * gain2Q;
    // inselect is either 0(neither), 1(Q), 2(I), or 3(both)
    switch (copy)
    {
    default: // 0 (default) no copy
        for (i = 0; i < size; i++)
        {
            I = in[2 * i + 0] * (inselect >> 1);
            Q = in[2 * i + 1] * (inselect &  1);
            out[2 * i + 0] = (float) (gainI * I);
            out[2 * i + 1] = (float) (gainQ * Q);
        }
        break;
    case 1: // copy I to Q (then Q == I)
        for (i = 0; i < size; i++)
        {
            I = in[2 * i + 0] * (inselect >> 1);
            Q = I;
            out[2 * i + 0] = (float) (gainI * I);
            out[2 * i + 1] = (float) (gainQ * Q);
        }
        break;
    case 2: // copy Q to I (then I == Q)
        for (i = 0; i < size; i++)
        {
            Q = in[2 * i + 1] * (inselect & 1);
            I = Q;
            out[2 * i + 0] = (float) (gainI * I);
            out[2 * i + 1] = (float) (gainQ * Q);
        }
        break;
    case 3: // reverse (I=>Q and Q=>I)
        for (i = 0; i < size; i++)
        {
            Q = in[2 * i + 0] * (inselect >> 1);
            I = in[2 * i + 1] * (inselect &  1);
            out[2 * i + 0] = (float) (gainI * I);
            out[2 * i + 1] = (float) (gainQ * Q);
        }
        break;
    }
}

void PANEL::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void PANEL::setSamplerate(int)
{
    // There is no sample rate to be set for this component
}

void PANEL::setSize(int _size)
{
    size = _size;
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void PANEL::setRun(int _run)
{
    run = _run;
}

void PANEL::setSelect(int _select)
{
    inselect = _select;
}

void PANEL::setGain1(double _gain)
{
    gain1 = _gain;
}

void PANEL::setGain2(double _gainI, double _gainQ)
{
    gain2I = _gainI;
    gain2Q = _gainQ;
}

void PANEL::setPan(double _pan)
{
    double _gain1;
    double _gain2;

    if (_pan <= 0.5)
    {
        _gain1 = 1.0;
        _gain2 = sin (_pan * PI);
    }
    else
    {
        _gain1 = sin (_pan * PI);
        _gain2 = 1.0;
    }

    gain2I = _gain1;
    gain2Q = _gain2;
}

void PANEL::setCopy(int _copy)
{
    copy = _copy;
}

void PANEL::setBinaural(int _bin)
{
    copy = 1 - _bin;
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void PANEL::setSelectTx(int _select)
{
    if (_select == 1)
        copy = 3;
    else
        copy = 0;

    inselect = _select;
}

} // namespace WDSP
