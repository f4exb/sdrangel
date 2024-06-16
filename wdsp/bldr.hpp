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

#ifndef wdsp_bldr_h
#define wdsp_bldr_h

#include "export.h"

namespace WDSP {

class WDSP_API BLDR
{
public:
    double* catxy;
    double* sx;
    double* sy;
    double* h;
    int* p;
    int* np;
    double* taa;
    double* tab;
    double* tag;
    double* tad;
    double* tbb;
    double* tbg;
    double* tbd;
    double* tgg;
    double* tgd;
    double* tdd;
    double* A;
    double* B;
    double* C;
    double* D;
    double* E;
    double* F;
    double* G;
    double* MAT;
    double* RHS;
    double* SLN;
    double* z;
    double* zp;
    double* wrk;
    int* ipiv;

    static BLDR* create_builder(int points, int ints);
    static void destroy_builder(BLDR *a);
    static void flush_builder(BLDR *a, int points, int ints);
    static void xbuilder(BLDR *a, int points, double* x, double* y, int ints, double* t, int* info, double* c, double ptol);

private:
    static int fcompare(const void* a, const void* b);
    static void decomp(int n, double* a, int* piv, int* info, double* wrk);
    static void dsolve(int n, double* a, int* piv, double* b, double* x);
    static void cull(int* n, int ints, double* x, double* t, double ptol);
};

} // namespace WDSP

#endif
