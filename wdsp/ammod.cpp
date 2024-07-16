/*  ammod.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2017 Warren Pratt, NR0V
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

#include <cmath>

#include "ammod.hpp"
#include "comm.hpp"
#include "TXA.hpp"

namespace WDSP {

AMMOD* AMMOD::create_ammod (int run, int mode, int size, float* in, float* out, float c_level)
{
    AMMOD *a = new AMMOD;
    a->run = run;
    a->mode = mode;
    a->size = size;
    a->in = in;
    a->out = out;
    a->c_level = c_level;
    a->a_level = 1.0 - a->c_level;
    a->mult = 1.0 / sqrt (2.0);
    return a;
}

void AMMOD::destroy_ammod(AMMOD *a)
{
    delete a;
}

void AMMOD::flush_ammod(AMMOD *)
{

}

void AMMOD::xammod(AMMOD *a)
{
    if (a->run)
    {
        int i;
        switch (a->mode)
        {
        case 0: // AM
            for (i = 0; i < a->size; i++)
                a->out[2 * i + 0] = a->out[2 * i + 1] = a->mult * (a->c_level + a->a_level * a->in[2 * i + 0]);
            break;
        case 1: // DSB
            for (i = 0; i < a->size; i++)
                a->out[2 * i + 0] = a->out[2 * i + 1] = a->mult * a->in[2 * i + 0];
            break;
        case 2: // SSB w/Carrier
            for (i = 0; i < a->size; i++)
            {
                a->out[2 * i + 0] = a->mult * a->c_level + a->a_level * a->in[2 * i + 0];
                a->out[2 * i + 1] = a->mult * a->c_level + a->a_level * a->in[2 * i + 1];
            }
            break;
        }
    }
    else if (a->in != a->out)
        std::copy( a->in,  a->in + a->size * 2, a->out);
}

void AMMOD::setBuffers_ammod(AMMOD *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
}

void AMMOD::setSamplerate_ammod(AMMOD *, int)
{

}

void AMMOD::setSize_ammod(AMMOD *a, int size)
{
    a->size = size;
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void AMMOD::SetAMCarrierLevel (TXA& txa, float c_level)
{
    txa.ammod.p->c_level = c_level;
    txa.ammod.p->a_level = 1.0 - c_level;
}

} // namespace WDSP
