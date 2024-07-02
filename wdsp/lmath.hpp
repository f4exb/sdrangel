/*  lmath.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2015, 2023 Warren Pratt, NR0V
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

#ifndef wdsp_lmath
#define wdsp_lmath

#include "export.h"

namespace WDSP {

class WDSP_API LMath {
public:
    static void dR (int n, float* r, float* y, float* z);
    static void trI (
        int n,
        float* r,
        float* B,
        float* y,
        float* v,
        float* dR_z
    );
    static void asolve(int xsize, int asize, float* x, float* a, float* r, float* z);
    static void median(int n, float* a, float* med);
};

class WDSP_API LMathd {
public:
    static void dR (int n, double* r, double* y, double* z);
    static void trI (
        int n,
        double* r,
        double* B,
        double* y,
        double* v,
        double* dR_z
    );
    static void asolve(int xsize, int asize, double* x, double* a, double* r, double* z);
    static void median(int n, double* a, double* med);
};

} // namespace WDSP

#endif

