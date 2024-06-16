/*  shift.h

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

#ifndef wdsp_shift_h
#define wdsp_shift_h

#include "export.h"

namespace WDSP {

class RXA;

class WDSP_API SHIFT
{
public:
    int run;
    int size;
    double* in;
    double* out;
    double rate;
    double shift;
    double phase;
    double delta;
    double cos_delta;
    double sin_delta;

    static SHIFT* create_shift (int run, int size, double* in, double* out, int rate, double fshift);
    static void destroy_shift (SHIFT *a);
    static void flush_shift (SHIFT *a);
    static void xshift (SHIFT *a);
    static void setBuffers_shift (SHIFT *a, double* in, double* out);
    static void setSamplerate_shift (SHIFT *a, int rate);
    static void setSize_shift (SHIFT *a, int size);
    // RXA Properties
    static void SetShiftRun (RXA& rxa, int run);
    static void SetShiftFreq (RXA& rxa, double fshift);

private:
    static void calc_shift (SHIFT *a);
};

} // namespace WDSP

#endif
