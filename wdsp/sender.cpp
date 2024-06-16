/*  sender.c

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
#include "sender.hpp"
#include "RXA.hpp"

namespace WDSP {

void SENDER::calc_sender (SENDER *a)
{
    a->out = new double[a->size * 2]; // (double *) malloc0 (a->size * sizeof (complex));
}

void decalc_sender (SENDER *a)
{
    delete[] (a->out);
}

SENDER* SENDER::create_sender (int run, int flag, int mode, int size, double* in, int arg0, int arg1, int arg2, int arg3)
{
    SENDER *a = new SENDER;
    a->run = run;
    a->flag = flag;
    a->mode = mode;
    a->size = size;
    a->in = in;
    a->arg0 = arg0;
    a->arg1 = arg1;
    a->arg2 = arg2;
    a->arg3 = arg3;
    calc_sender (a);
    return a;
}

void SENDER::destroy_sender (SENDER *a)
{
    decalc_sender (a);
    delete (a);
}

void SENDER::flush_sender (SENDER *a)
{
    memset (a->out, 0, a->size * sizeof (dcomplex));
}

void SENDER::xsender (SENDER *a)
{
    if (a->run && a->flag)
    {
        switch (a->mode)
        {
        case 0:
            {
                int i;
                dINREAL* outf = (dINREAL *)a->out;
                for (i = 0; i < a->size; i++)
                {
                    outf [2 * i + 0] = (dINREAL)a->in[2 * i + 0];
                    outf [2 * i + 1] = (dINREAL)a->in[2 * i + 1];
                }
                // Spectrum2 (1, a->arg0, a->arg1, a->arg2, outf);
                break;
            }
        }
    }
}

void SENDER::setBuffers_sender (SENDER *a, double* in)
{
    a->in = in;
}

void SENDER::setSamplerate_sender (SENDER *a, int)
{
    flush_sender (a);
}

void SENDER::setSize_sender (SENDER *a, int size)
{
    decalc_sender (a);
    a->size = size;
    calc_sender (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void SENDER::SetSpectrum (RXA& rxa, int flag, int disp, int ss, int LO)
{
    SENDER *a;
    rxa.csDSP.lock();
    a = rxa.sender.p;
    a->flag = flag;
    a->arg0 = disp;
    a->arg1 = ss;
    a->arg2 = LO;
    rxa.csDSP.unlock();
}

} // namespace WDSP
