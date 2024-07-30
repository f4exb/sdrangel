/*  speak.h

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
*                                           Complex Bi-Quad Peaking                                     *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_speak_h
#define wdsp_speak_h

#include "export.h"

namespace WDSP {

class RXA;

class WDSP_API SPEAK
{
public:
    int run;
    int size;
    float* in;
    float* out;
    double rate;
    double f;
    double bw;
    double cbw;
    double gain;
    double fgain;
    int nstages;
    int design;
    double a0, a1, a2, b1, b2;
    double *x0, *x1, *x2, *y0, *y1, *y2;

    static SPEAK* create_speak (
        int run,
        int size,
        float* in,
        float* out,
        int rate,
        double f,
        double bw,
        double gain,
        int nstages,
        int design
    );
    static void destroy_speak (SPEAK *a);
    static void flush_speak (SPEAK *a);
    static void xspeak (SPEAK *a);
    static void setBuffers_speak (SPEAK *a, float* in, float* out);
    static void setSamplerate_speak (SPEAK *a, int rate);
    static void setSize_speak (SPEAK *a, int size);
    // RXA
    static void SetSPCWRun (RXA& rxa, int run);
    static void SetSPCWFreq (RXA& rxa, double freq);
    static void SetSPCWBandwidth (RXA& rxa, double bw);
    static void SetSPCWGain (RXA& rxa, double gain);
    static void calc_speak (SPEAK *a);
};

} // namespace WDSP

#endif
