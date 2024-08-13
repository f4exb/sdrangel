/*  emph.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014, 2016, 2023 Warren Pratt, NR0V
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
#include "emphp.hpp"
#include "fcurve.hpp"
#include "fircore.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                               Partitioned Overlap-Save FM Pre-Emphasis                                *
*                                                                                                       *
********************************************************************************************************/

EMPHP::EMPHP(
    int _run,
    int _position,
    int _size,
    int _nc,
    int _mp,
    float* _in,
    float* _out,
    int _rate,
    int _ctype,
    double _f_low,
    double _f_high
)
{
    run = _run;
    position = _position;
    size = _size;
    nc = _nc;
    mp = _mp;
    in = _in;
    out = _out;
    rate = _rate;
    ctype = _ctype;
    f_low = _f_low;
    f_high = _f_high;
    std::vector<float> impulse(2 * nc);
    FCurve::fc_impulse (
        impulse,
        nc,
        (float) f_low,
        (float) f_high,
        (float) (-20.0 * log10(f_high / f_low)),
        0.0,
        ctype,
        (float) rate,
        (float) (1.0 / (2.0 * size)),
        0, 0
    );
    p = new FIRCORE(size, in, out, mp, impulse);
}

EMPHP::~EMPHP()
{
    delete p;
}

void EMPHP::flush()
{
    p->flush();
}

void EMPHP::execute(int _position)
{
    if (run && position == _position)
        p->execute();
    else if (in != out)
        std::copy( in,  in + size * 2, out);
}

void EMPHP::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
    p->setBuffers(in, out);
}

void EMPHP::setSamplerate(int _rate)
{
    rate = _rate;
    std::vector<float> impulse(2 * nc);
    FCurve::fc_impulse (
        impulse,
        nc,
        (float) f_low,
        (float) f_high,
        (float) (-20.0 * log10(f_high / f_low)),
        0.0,
        ctype,
        (float) rate,
        (float) (1.0 / (2.0 * size)),
        0, 0
    );
    p->setImpulse(impulse, 1);
}

void EMPHP::setSize(int _size)
{
    size = _size;
    p->setSize(size);
    std::vector<float> impulse(2 * nc);
    FCurve::fc_impulse (
        impulse,
        nc,
        (float) f_low,
        (float) f_high,
        (float) (-20.0 * log10(f_high / f_low)),
        0.0,
        ctype,
        (float) rate,
        (float) (1.0 / (2.0 * size)),
        0,
        0
    );
    p->setImpulse(impulse, 1);
}

/********************************************************************************************************
*                                                                                                       *
*                       Partitioned Overlap-Save FM Pre-Emphasis:  TXA Properties                       *
*                                                                                                       *
********************************************************************************************************/

void EMPHP::setPosition(int _position)
{
    position = _position;
}

void EMPHP::setMP(int _mp)
{
    if (mp != _mp)
    {
        mp = _mp;
        p->setMp(mp);
    }
}

void EMPHP::setNC(int _nc)
{
    if (nc != _nc)
    {
        nc = _nc;
        std::vector<float> impulse(2 * nc);
        FCurve::fc_impulse (
            impulse,
            nc,
            (float) f_low,
            (float) f_high,
            (float) (-20.0 * log10(f_high / f_low)),
            0.0,
            ctype,
            (float) rate,
            (float) (1.0 / (2.0 * size)),
            0,
            0
        );
        p->setNc(impulse);
    }
}

void EMPHP::setFreqs(double low, double high)
{
    if (f_low != low || f_high != high)
    {
        f_low = low;
        f_high = high;
        std::vector<float> impulse(2 * nc);
        FCurve::fc_impulse (
            impulse,
            nc,
            (float) f_low,
            (float) f_high,
            (float) (-20.0 * log10(f_high / f_low)),
            0.0,
            ctype,
            (float) rate,
            (float) (1.0 / (2.0 * size)),
            0,
            0
        );
        p->setImpulse(impulse, 1);
    }
}

} // namespace WDSP
