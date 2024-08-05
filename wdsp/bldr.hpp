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

#include <vector>

#include "export.h"

namespace WDSP {

class WDSP_API BLDR
{
public:
    double* catxy;
    std::vector<double> sx;
    std::vector<double> sy;
    std::vector<double> h;
    std::vector<int> p;
    std::vector<int> np;
    std::vector<double> taa;
    std::vector<double> tab;
    std::vector<double> tag;
    std::vector<double> tad;
    std::vector<double> tbb;
    std::vector<double> tbg;
    std::vector<double> tbd;
    std::vector<double> tgg;
    std::vector<double> tgd;
    std::vector<double> tdd;
    std::vector<double> A;
    std::vector<double> B;
    std::vector<double> C;
    std::vector<double> D;
    std::vector<double> E;
    std::vector<double> F;
    std::vector<double> G;
    std::vector<double> MAT;
    std::vector<double> RHS;
    std::vector<double> SLN;
    std::vector<double> z;
    std::vector<double> zp;
    std::vector<double> wrk;
    std::vector<int> ipiv;

    BLDR(int points, int ints);
    BLDR(const BLDR&) = delete;
    BLDR& operator=(const BLDR& other) = delete;
    ~BLDR();

    void flush(int points);
    void execute(int points, const double* x, const double* y, int ints, const double* t, int* info, double* c, double ptol);

private:
    static int fcompare(const void* a, const void* b);
    static void decomp(int n, std::vector<double>& a, std::vector<int>& piv, int* info, std::vector<double>& wrk);
    static void dsolve(int n, std::vector<double>& a, std::vector<int>& piv, std::vector<double>& b, std::vector<double>& x);
    static void cull(int* n, int ints, std::vector<double>& x, const double* t, double ptol);
};

} // namespace WDSP

#endif
