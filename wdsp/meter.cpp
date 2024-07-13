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
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

void METER::calc_meter (METER *a)
{
    a->mult_average = exp(-1.0 / (a->rate * a->tau_average));
    a->mult_peak = exp(-1.0 / (a->rate * a->tau_peak_decay));
    flush_meter(a);
}

METER* METER::create_meter (
    int run,
    int* prun,
    int size,
    float* buff,
    int rate,
    double tau_av,
    double tau_decay,
    double* result,
    QRecursiveMutex** pmtupdate,
    int enum_av,
    int enum_pk,
    int enum_gain,
    double* pgain
)
{
    METER *a = new METER;
    a->run = run;
    a->prun = prun;
    a->size = size;
    a->buff = buff;
    a->rate = (float)rate;
    a->tau_average = tau_av;
    a->tau_peak_decay = tau_decay;
    a->result = result;
    a->enum_av = enum_av;
    a->enum_pk = enum_pk;
    a->enum_gain = enum_gain;
    a->pgain = pgain;
    calc_meter(a);
    pmtupdate[enum_av]   = &a->mtupdate;
    pmtupdate[enum_pk]   = &a->mtupdate;
    pmtupdate[enum_gain] = &a->mtupdate;
    return a;
}

void METER::destroy_meter (METER *a)
{
    delete a;
}

void METER::flush_meter (METER *a)
{
    a->avg  = 0.0;
    a->peak = 0.0;
    a->result[a->enum_av] = -400.0;
    a->result[a->enum_pk] = -400.0;
    if ((a->pgain != 0) && (a->enum_gain >= 0))
        a->result[a->enum_gain] = -400.0;
}

void METER::xmeter (METER *a)
{
    int srun;
    a->mtupdate.lock();
    if (a->prun != 0)
        srun = *(a->prun);
    else
        srun = 1;
    if (a->run && srun)
    {
        int i;
        double smag;
        double np = 0.0;
        for (i = 0; i < a->size; i++)
        {
            smag = a->buff[2 * i + 0] * a->buff[2 * i + 0] + a->buff[2 * i + 1] * a->buff[2 * i + 1];
            a->avg = a->avg * a->mult_average + (1.0 - a->mult_average) * smag;
            a->peak *= a->mult_peak;
            if (smag > np) np = smag;
        }
        if (np > a->peak) a->peak = np;
        a->result[a->enum_av] = 10.0 * MemLog::mlog10 (a->avg + 1.0e-40);
        a->result[a->enum_pk] = 10.0 * MemLog::mlog10 (a->peak + 1.0e-40);
        if ((a->pgain != 0) && (a->enum_gain >= 0))
            a->result[a->enum_gain] = 20.0 * MemLog::mlog10 (*a->pgain + 1.0e-40);
    }
    else
    {
        if (a->enum_av   >= 0) a->result[a->enum_av]   = -400.0;
        if (a->enum_pk   >= 0) a->result[a->enum_pk]   = -400.0;
        if (a->enum_gain >= 0) a->result[a->enum_gain] = 0.0;
    }
    a->mtupdate.unlock();
}

void METER::setBuffers_meter (METER *a, float* in)
{
    a->buff = in;
}

void METER::setSamplerate_meter (METER *a, int rate)
{
    a->rate = rate;
    calc_meter(a);
}

void METER::setSize_meter (METER *a, int size)
{
    a->size = size;
    flush_meter (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

double METER::GetMeter (RXA& rxa, int mt)
{
    double val;
    rxa.pmtupdate[mt]->lock();
    val = rxa.meter[mt];
    rxa.pmtupdate[mt]->unlock();
    return val;
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

double METER::GetMeter (TXA& txa, int mt)
{
    double val;
    txa.pmtupdate[mt]->lock();
    val = txa.meter[mt];
    txa.pmtupdate[mt]->unlock();
    return val;
}

} // namespace WDSP
