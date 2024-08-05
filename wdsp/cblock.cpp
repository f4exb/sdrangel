/*  cblock.c

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

#include "comm.hpp"
#include "cblock.hpp"

namespace WDSP {

void CBL::calc()
{
    prevIin  = 0.0;
    prevQin  = 0.0;
    prevIout = 0.0;
    prevQout = 0.0;
    mtau = exp(-1.0 / (sample_rate * tau));
}

CBL::CBL(
    int _run,
    int _buff_size,
    float *_in_buff,
    float *_out_buff,
    int _mode,
    int _sample_rate,
    double _tau
) :
    run(_run),
    buff_size(_buff_size),
    in_buff(_in_buff),
    out_buff(_out_buff),
    mode(_mode),
    sample_rate((double) _sample_rate),
    tau(_tau)
{
    calc();
}

void CBL::flush()
{
    prevIin  = 0.0;
    prevQin  = 0.0;
    prevIout = 0.0;
    prevQout = 0.0;
}

void CBL::execute()
{
    if (run)
    {
        double tempI;
        double tempQ;

        for (int i = 0; i < buff_size; i++)
        {
            tempI  = in_buff[2 * i + 0];
            tempQ  = in_buff[2 * i + 1];
            out_buff[2 * i + 0] = (float) (in_buff[2 * i + 0] - prevIin + mtau * prevIout);
            out_buff[2 * i + 1] = (float) (in_buff[2 * i + 1] - prevQin + mtau * prevQout);
            prevIin  = tempI;
            prevQin  = tempQ;
            prevIout = out_buff[2 * i + 0];
            prevQout = out_buff[2 * i + 1];

            if (fabs(prevIout) < 1.0e-20)
                prevIout = 0.0;

            if (fabs(prevQout) < 1.0e-20)
                prevQout = 0.0;
        }
    }
    else if (in_buff != out_buff)
    {
        std::copy(in_buff, in_buff + buff_size * 2, out_buff);
    }
}

void CBL::setBuffers(float* _in, float* _out)
{
    in_buff = _in;
    out_buff = _out;
}

void CBL::setSamplerate(int _rate)
{
    sample_rate = _rate;
    calc();
}

void CBL::setSize(int _size)
{
    buff_size = _size;
    flush();
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void CBL::setRun(int setit)
{
    run = setit;
}

} // namespace WDSP
