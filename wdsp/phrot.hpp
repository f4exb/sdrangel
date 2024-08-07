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

#include <vector>

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
    double a1;
    double b0;
    double b1;
    std::vector<double> x0;
    std::vector<double> x1;
    std::vector<double> y0;
    std::vector<double> y1;

    PHROT(
        int run,
        int size,
        float* in,
        float* out,
        int rate,
        double fc,
        int nstages
    );
    PHROT(const PHROT&) = delete;
    PHROT& operator=(const PHROT& other) = delete;
    ~PHROT() = default;

    void destroy();
    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    // TXA Properties
    void setRun(int run);
    void setCorner(double corner);
    void setNstages(int nstages);
    void setReverse(int reverse);

private:
    void calc();
};

} // namespace WDSP

#endif
