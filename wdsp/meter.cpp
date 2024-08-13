/*  meter.c

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
#include "meterlog10.hpp"
#include "meter.hpp"

namespace WDSP {

void METER::calc()
{
    mult_average = exp(-1.0 / (rate * tau_average));
    mult_peak = exp(-1.0 / (rate * tau_peak_decay));
    flush();
}

METER::METER(
    int _run,
    int* _prun,
    int _size,
    float* _buff,
    int _rate,
    double _tau_av,
    double _tau_decay,
    double* _result,
    int _enum_av,
    int _enum_pk,
    int _enum_gain,
    double* _pgain
) :
    run(_run),
    prun(_prun),
    size(_size),
    buff(_buff),
    rate((double) _rate),
    tau_average(_tau_av),
    tau_peak_decay(_tau_decay),
    result(_result),
    enum_av(_enum_av),
    enum_pk(_enum_pk),
    enum_gain(_enum_gain),
    pgain(_pgain)
{
    calc();
}

void METER::flush()
{
    avg  = 0.0;
    peak = 0.0;
    result[enum_av] = -100.0;
    result[enum_pk] = -100.0;

    if ((pgain != nullptr) && (enum_gain >= 0))
        result[enum_gain] = 0.0;
}

void METER::execute()
{
    int srun;

    if (prun != nullptr)
        srun = *prun;
    else
        srun = 1;

    if (run && srun)
    {
        double smag;
        double np = 0.0;

        for (int i = 0; i < size; i++)
        {
            double xr = buff[2 * i + 0];
            double xi = buff[2 * i + 1];
            smag = xr*xr + xi*xi;
            avg = avg * mult_average + (1.0 - mult_average) * smag;
            peak *= mult_peak;

            if (smag > np)
                np = smag;
        }

        if (np > peak)
            peak = np;

        result[enum_av] = 10.0 * MemLog::mlog10 (avg  <= 0 ? 1.0e-20 : avg);
        result[enum_pk] = 10.0 * MemLog::mlog10 (peak <= 0 ? 1.0e-20 : peak);

        if ((pgain != nullptr) && (enum_gain >= 0))
            result[enum_gain] = 20.0 * MemLog::mlog10 (*pgain <= 0 ? 1.0e-20 : *pgain);
    }
    else
    {
        if (enum_av   >= 0)
            result[enum_av]   = -100.0;
        if (enum_pk   >= 0)
            result[enum_pk]   = -100.0;
        if (enum_gain >= 0)
            result[enum_gain] = 0.0;
    }
}

void METER::setBuffers(float* in)
{
    buff = in;
    flush();
}

void METER::setSamplerate(int _rate)
{
    rate = (double) _rate;
    calc();
}

void METER::setSize(int _size)
{
    size = _size;
    flush();
}

/********************************************************************************************************
*                                                                                                       *
*                                           Public Properties                                           *
*                                                                                                       *
********************************************************************************************************/

double METER::getMeter(int mt) const
{
    return result[mt];
}

} // namespace WDSP
