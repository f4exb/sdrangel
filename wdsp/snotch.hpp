/*  snotch.h

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
*                                           Bi-Quad Notch                                               *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_snotch_h
#define wdsp_snotch_h

#include "export.h"

namespace WDSP {

class WDSP_API SNOTCH
{
public:
    int run;
    int size;
    float* in;
    float* out;
    double rate;
    double f;
    double bw;
    double a0;
    double a1;
    double a2;
    double b1;
    double b2;
    double x0;
    double x1;
    double x2;
    double y1;
    double y2;

    SNOTCH(
        int run,
        int size,
        float* in,
        float* out,
        int rate,
        double f,
        double bw
    );
    SNOTCH(const SNOTCH&) = delete;
    SNOTCH& operator=(SNOTCH& other) = delete;
    ~SNOTCH() = default;

    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    void setFreq(double freq);
    void setRun(int run);

private:
    void calc();
};

} // namespace WDSP

#endif
