/*  bqlp.h

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
*                                   Complex Bi-Quad Low-Pass                                            *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_bqlp_h
#define wdsp_bqlp_h

#include "export.h"

namespace WDSP {

class WDSP_API BQLP
{
public:
    int run;
    int size;
    float* in;
    float* out;
    double rate;
    double fc;
    double Q;
    double gain;
    int nstages;
    double a0, a1, a2, b1, b2;
    double* x0, * x1, * x2, * y0, * y1, * y2;

    static BQLP* create_bqlp(int run, int size, float* in, float* out, double rate, double fc, double Q, double gain, int nstages);
    static void destroy_bqlp(BQLP *a);
    static void flush_bqlp(BQLP *a);
    static void xbqlp(BQLP *a);
    static void setBuffers_bqlp(BQLP *a, float* in, float* out);
    static void setSamplerate_bqlp(BQLP *a, int rate);
    static void setSize_bqlp(BQLP *a, int size);

private:
    static void calc_bqlp(BQLP *a);
};

} // namespace WDSP

#endif
