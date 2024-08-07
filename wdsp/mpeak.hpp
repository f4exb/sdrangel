/*  mpeak.h

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
*                                       Complex Multiple Peaking                                        *
*                                                                                                       *
********************************************************************************************************/

#ifndef _mpeak_h
#define _mpeak_h

#include <vector>

#include "export.h"

namespace WDSP {

class SPEAK;

class WDSP_API MPEAK
{
public:
    int run;
    int size;
    float* in;
    float* out;
    int rate;
    int npeaks;
    std::vector<int> enable;
    std::vector<double> f;
    std::vector<double> bw;
    std::vector<double> gain;
    int nstages;
    std::vector<SPEAK*> pfil;
    std::vector<float> tmp;
    std::vector<float> mix;

    MPEAK(
        int run,
        int size,
        float* in,
        float* out,
        int rate,
        int npeaks,
        int* enable,
        double* f,
        double* bw,
        double* gain,
        int nstages
    );
    MPEAK(const MPEAK&) = delete;
    MPEAK& operator=(const MPEAK& other) = delete;
    ~MPEAK();

    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    // RXA
    void setRun(int run);
    void setNpeaks(int npeaks);
    void setFilEnable(int fil, int enable);
    void setFilFreq(int fil, double freq);
    void setFilBw(int fil, double bw);
    void setFilGain(int fil, double gain);

private:
    void calc();
    void decalc();
};

} // namespace WDSP

#endif
