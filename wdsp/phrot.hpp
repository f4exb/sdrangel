/*  phrot.h

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
*                                            Phase Rotator                                              *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_phrot_h
#define wdsp_phrot_h

#include "export.h"

namespace WDSP {

class TXA;

class WDSP_API PHROT
{
public:
    int reverse;
    int run;
    int size;
    float* in;
    float* out;
    int rate;
    double fc;
    int nstages;
    // normalized such that a0 = 1
    double a1, b0, b1;
    double *x0, *x1, *y0, *y1;

    static PHROT* create_phrot (int run, int size, float* in, float* out, int rate, double fc, int nstages);
    static void destroy_phrot (PHROT *a);
    static void flush_phrot (PHROT *a);
    static void xphrot (PHROT *a);
    static void setBuffers_phrot (PHROT *a, float* in, float* out);
    static void setSamplerate_phrot (PHROT *a, int rate);
    static void setSize_phrot (PHROT *a, int size);
    // TXA Properties
    static void SetPHROTRun (TXA& txa, int run);
    static void SetPHROTCorner (TXA& txa, double corner);
    static void SetPHROTNstages (TXA& txa, int nstages);
    static void SetPHROTReverse (TXA& txa, int reverse);

private:
    static void calc_phrot (PHROT *a);
    static void decalc_phrot (PHROT *a);
};

} // namespace WDSP

#endif
