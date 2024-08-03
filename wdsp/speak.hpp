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

#include <vector>

#include "export.h"

namespace WDSP {

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
    double a0;
    double a1;
    double a2;
    double b1;
    double b2;
    std::vector<double> x0;
    std::vector<double> x1;
    std::vector<double> x2;
    std::vector<double> y0;
    std::vector<double> y1;
    std::vector<double> y2;

    SPEAK(
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
    SPEAK(const SPEAK&) = delete;
    SPEAK& operator=(const SPEAK& other) = delete;
    ~SPEAK() = default;

    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    // RXA
    void setRun(int run);
    void setFreq(double freq);
    void setBandwidth(double bw);
    void setGain(double gain);
    void calc();
};

} // namespace WDSP

#endif
