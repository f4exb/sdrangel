/*  sphp.h

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

/********************************************************************************************************
*                                                                                                       *
*                                      Double Single-Pole High-Pass                                     *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_dsphp_h
#define wdsp_dsphp_h

#include "export.h"

namespace WDSP {

class WDSP_API DSPHP
{
public:
    int run;
    int size;
    float* in;
    float* out;
    double rate;
    double fc;
    int nstages;
    double a1, b0, b1;
    double* x0, * x1, * y0, * y1;

    static DSPHP* create_dsphp(int run, int size, float* in, float* out, double rate, double fc, int nstages);
    static void destroy_dsphp(DSPHP *a);
    static void flush_dsphp(DSPHP *a);
    static void xdsphp(DSPHP *a);
    static void setBuffers_dsphp(DSPHP *a, float* in, float* out);
    static void setSamplerate_dsphp(DSPHP *a, int rate);
    static void setSize_dsphp(DSPHP *a, int size);

private:
    static void calc_dsphp(DSPHP *a);
    static void decalc_dsphp(DSPHP *a);
};

} // namespace WDSP

#endif


