/*  cblock.h

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

#ifndef wdsp_cblock_h
#define wdsp_cblock_h

#include "export.h"

namespace WDSP {

class RXA;

class WDSP_API CBL
{
public:
    int run;                            //run
    int buff_size;                      //buffer size
    double *in_buff;                    //pointer to input buffer
    double *out_buff;                   //pointer to output buffer
    int mode;
    double sample_rate;                 //sample rate
    double prevIin;
    double prevQin;
    double prevIout;
    double prevQout;
    double tau;                         //carrier removal time constant
    double mtau;                        //carrier removal multiplier

    static CBL* create_cbl
        (
        int run,
        int buff_size,
        double *in_buff,
        double *out_buff,
        int mode,
        int sample_rate,
        double tau
        );
    static void destroy_cbl (CBL *a);
    static void flush_cbl (CBL *a);
    static void xcbl (CBL *a);
    static void setBuffers_cbl (CBL *a, double* in, double* out);
    static void setSamplerate_cbl (CBL *a, int rate);
    static void setSize_cbl (CBL *a, int size);
    // RXA Properties
    static  void SetCBLRun(RXA& rxa, int setit);

private:
    static void calc_cbl (CBL *a);
};

} // namespace WDSP

#endif
