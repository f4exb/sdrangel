/*  gain.c

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
#include "gain.hpp"

namespace WDSP {

GAIN* GAIN::create_gain (int run, int* prun, int size, double* in, double* out, double Igain, double Qgain)
{
    GAIN *a = new GAIN;
    a->run = run;
    a->prun = prun;
    a->size = size;
    a->in = in;
    a->out = out;
    a->Igain = Igain;
    a->Qgain = Qgain;
    return a;
}

void GAIN::destroy_gain (GAIN *a)
{
    delete (a);
}

void GAIN::flush_gain (GAIN *)
{

}

void GAIN::xgain (GAIN *a)
{
    int srun;
    a->cs_update.lock();
    if (a->prun != 0)
        srun = *(a->prun);
    else
        srun = 1;
    if (a->run && srun)
    {
        int i;
        for (i = 0; i < a->size; i++)
        {
            a->out[2 * i + 0] = a->Igain * a->in[2 * i + 0];
            a->out[2 * i + 1] = a->Qgain * a->in[2 * i + 1];
        }
    }
    else if (a->in != a->out)
        memcpy (a->out, a->in, a->size * sizeof (wcomplex));
    a->cs_update.unlock();
}

void GAIN::setBuffers_gain (GAIN *a, double* in, double* out)
{
    a->in = in;
    a->out = out;
}

void GAIN::setSamplerate_gain (GAIN *, int)
{

}

void GAIN::setSize_gain (GAIN *a, int size)
{
    a->size = size;
}

/********************************************************************************************************
*                                                                                                       *
*                                         POINTER-BASED PROPERTIES                                      *
*                                                                                                       *
********************************************************************************************************/

void GAIN::pSetTXOutputLevel (GAIN *a, double level)
{
    a->cs_update.lock();
    a->Igain = level;
    a->Qgain = level;
    a->cs_update.unlock();
}

void GAIN::pSetTXOutputLevelRun (GAIN *a, int run)
{
    a->cs_update.lock();
    a->run = run;
    a->cs_update.unlock();
}

void GAIN::pSetTXOutputLevelSize (GAIN *a, int size)
{
    a->cs_update.lock();
    a->size = size;
    a->cs_update.unlock();
}

} // namespace WDSP
