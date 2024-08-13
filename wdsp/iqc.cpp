/*  iqc.c

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

#include <thread>
#include <chrono>

#include "comm.hpp"
#include "iqc.hpp"
#include "TXA.hpp"

namespace WDSP {

void IQC::size_iqc()
{
    int i;
    t.resize(ints + 1);
    for (i = 0; i <= ints; i++)
        t[i] = (double)i / (double)ints;
    for (i = 0; i < 2; i++)
    {
        cm[i].resize(ints * 4);
        cc[i].resize(ints * 4);
        cs[i].resize(ints * 4);
    }
    dog.cpi.resize(ints);
    dog.count = 0;
    dog.full_ints = 0;
}

void IQC::calc()
{
    double delta;
    double theta;
    cset = 0;
    count = 0;
    state = IQCSTATE::RUN;
    busy = 0;
    ntup = (int)(tup * rate);
    cup.resize(ntup + 1);
    delta = PI / (double)ntup;
    theta = 0.0;
    for (int i = 0; i <= ntup; i++)
    {
        cup[i] = 0.5 * (1.0 - cos (theta));
        theta += delta;
    }
    size_iqc();
}

IQC::IQC(
    int _run,
    int _size,
    float* _in,
    float* _out,
    double _rate,
    int _ints,
    double _tup,
    int _spi
) :
    run(_run),
    size(_size),
    in(_in),
    out(_out),
    rate(_rate),
    ints(_ints),
    tup(_tup)
{
    dog.spi = _spi;
    calc();
}

void IQC::flush()
{
    // Nothing to do
}

void IQC::execute()
{
    if (run == 1)
    {
        int k;
        int icset;
        int mset;
        double I;
        double Q;
        double env;
        double dx;
        double ym;
        double yc;
        double ys;
        double PRE0;
        double PRE1;
        for (int i = 0; i < size; i++)
        {
            I = in[2 * i + 0];
            Q = in[2 * i + 1];
            env = sqrt (I * I + Q * Q);
            if ((k = (int)(env * ints)) > ints - 1) k = ints - 1;
            dx = env - t[k];
            icset = cset;
            ym = cm[icset][4 * k + 0] + dx * (cm[icset][4 * k + 1] + dx * (cm[icset][4 * k + 2] + dx * cm[icset][4 * k + 3]));
            yc = cc[icset][4 * k + 0] + dx * (cc[icset][4 * k + 1] + dx * (cc[icset][4 * k + 2] + dx * cc[icset][4 * k + 3]));
            ys = cs[icset][4 * k + 0] + dx * (cs[icset][4 * k + 1] + dx * (cs[icset][4 * k + 2] + dx * cs[icset][4 * k + 3]));
            PRE0 = ym * (I * yc - Q * ys);
            PRE1 = ym * (I * ys + Q * yc);

            switch (state)
            {
            case IQCSTATE::RUN:
                if ((dog.cpi[k] != dog.spi) && (++dog.cpi[k] == dog.spi))
                    dog.full_ints++;
                if (dog.full_ints == ints)
                {
                    ++dog.count;
                    dog.full_ints = 0;
                    std::fill(dog.cpi.begin(), dog.cpi.end(), 0);
                }
                break;
            case IQCSTATE::BEGIN:
                PRE0 = (1.0 - cup[count]) * I + cup[count] * PRE0;
                PRE1 = (1.0 - cup[count]) * Q + cup[count] * PRE1;
                if (count++ == ntup)
                {
                    state = IQCSTATE::RUN;
                    count = 0;
                    busy = 0;
                }
                break;
            case IQCSTATE::SWAP:
                mset = 1 - cset;
                ym = cm[mset][4 * k + 0] + dx * (cm[mset][4 * k + 1] + dx * (cm[mset][4 * k + 2] + dx * cm[mset][4 * k + 3]));
                yc = cc[mset][4 * k + 0] + dx * (cc[mset][4 * k + 1] + dx * (cc[mset][4 * k + 2] + dx * cc[mset][4 * k + 3]));
                ys = cs[mset][4 * k + 0] + dx * (cs[mset][4 * k + 1] + dx * (cs[mset][4 * k + 2] + dx * cs[mset][4 * k + 3]));
                PRE0 = (1.0 - cup[count]) * ym * (I * yc - Q * ys) + cup[count] * PRE0;
                PRE1 = (1.0 - cup[count]) * ym * (I * ys + Q * yc) + cup[count] * PRE1;
                if (count++ == ntup)
                {
                    state = IQCSTATE::RUN;
                    count = 0;
                    busy = 0;
                }
                break;
            case IQCSTATE::END:
                PRE0 = (1.0 - cup[count]) * PRE0 + cup[count] * I;
                PRE1 = (1.0 - cup[count]) * PRE1 + cup[count] * Q;
                if (count++ == ntup)
                {
                    state = IQCSTATE::DONE;
                    count = 0;
                    busy = 0;
                }
                break;
            case IQCSTATE::DONE:
                PRE0 = I;
                PRE1 = Q;
                break;
            }
            out[2 * i + 0] = (float) PRE0;
            out[2 * i + 1] = (float) PRE1;
        }
    }
    else if (out != in)
        std::copy( in,  in + size * 2, out);
}

void IQC::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void IQC::setSamplerate(int _rate)
{
    rate = _rate;
    calc();
}

void IQC::setSize(int _size)
{
    size = _size;
}

} // namespace WDSP
