/*  mpeak.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014, 2022, 2023 Warren Pratt, NR0V
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
#include "mpeak.hpp"
#include "speak.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                       Complex Multiple Peaking                                        *
*                                                                                                       *
********************************************************************************************************/

void MPEAK::calc()
{
    tmp.resize(size * 2);
    mix.resize(size * 2);
    for (int i = 0; i < npeaks; i++)
    {
        pfil[i] = new SPEAK(
            1,
            size,
            in,
            tmp.data(),
            rate,
            f[i],
            bw[i],
            gain[i],
            nstages,
            1
        );
    }
}

void MPEAK::decalc()
{
    for (int i = 0; i < npeaks; i++)
        delete (pfil[i]);
}

MPEAK::MPEAK(
    int _run,
    int _size,
    float* _in,
    float* _out,
    int _rate,
    int _npeaks,
    int* _enable,
    double* _f,
    double* _bw,
    double* _gain,
    int _nstages
)
{
    run = _run;
    size = _size;
    in = _in;
    out = _out;
    rate = _rate;
    npeaks = _npeaks;
    nstages = _nstages;
    enable.resize(npeaks);
    f.resize(npeaks);
    bw.resize(npeaks);
    gain.resize(npeaks);
    std::copy(_enable, _enable + npeaks, enable.begin());
    std::copy(_f, _f + npeaks, f.begin());
    std::copy(_bw, _bw + npeaks, bw.begin());
    std::copy(_gain, _gain + npeaks, gain.begin());
    pfil.resize(npeaks);
    calc();
}

MPEAK::~MPEAK()
{
    decalc();
}

void MPEAK::flush()
{
    for (int i = 0; i < npeaks; i++)
        pfil[i]->flush();
}

void MPEAK::execute()
{
    if (run)
    {
        std::fill(mix.begin(), mix.end(), 0);

        for (int i = 0; i < npeaks; i++)
        {
            if (enable[i])
            {
                pfil[i]->execute();
                for (int j = 0; j < 2 * size; j++)
                    mix[j] += tmp[j];
            }
        }

        std::copy(mix.begin(), mix.end(), out);
    }
    else if (in != out)
    {
        std::copy( in,  in + size * 2, out);
    }
}

void MPEAK::setBuffers(float* _in, float* _out)
{
    decalc();
    in = _in;
    out = _out;
    calc();
}

void MPEAK::setSamplerate(int _rate)
{
    decalc();
    rate = _rate;
    calc();
}

void MPEAK::setSize(int _size)
{
    decalc();
    size = _size;
    calc();
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void MPEAK::setRun(int _run)
{
    run = _run;
}

void MPEAK::setNpeaks(int _npeaks)
{
    npeaks = _npeaks;
}

void MPEAK::setFilEnable(int _fil, int _enable)
{
    enable[_fil] = _enable;
}

void MPEAK::setFilFreq(int _fil, double _freq)
{
    f[_fil] = _freq;
    pfil[_fil]->f = _freq;
    pfil[_fil]->calc();
}

void MPEAK::setFilBw(int _fil, double _bw)
{
    bw[_fil] = _bw;
    pfil[_fil]->bw = _bw;
    pfil[_fil]->calc();
}

void MPEAK::setFilGain(int _fil, double _gain)
{
    gain[_fil] = _gain;
    pfil[_fil]->gain = _gain;
    pfil[_fil]->calc();
}

} // namespace WDSP
