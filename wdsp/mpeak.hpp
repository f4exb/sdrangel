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

#include "export.h"

namespace WDSP {

class RXA;
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
    int* enable;
    double* f;
    double* bw;
    double* gain;
    int nstages;
    SPEAK** pfil;
    float* tmp;
    float* mix;

    static MPEAK* create_mpeak (
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
    static void destroy_mpeak (MPEAK *a);
    static void flush_mpeak (MPEAK *a);
    static void xmpeak (MPEAK *a);
    static void setBuffers_mpeak (MPEAK *a, float* in, float* out);
    static void setSamplerate_mpeak (MPEAK *a, int rate);
    static void setSize_mpeak (MPEAK *a, int size);
    // RXA
    static void SetmpeakRun (RXA& rxa, int run);
    static void SetmpeakNpeaks (RXA& rxa, int npeaks);
    static void SetmpeakFilEnable (RXA& rxa, int fil, int enable);
    static void SetmpeakFilFreq (RXA& rxa, int fil, double freq);
    static void SetmpeakFilBw (RXA& rxa, int fil, double bw);
    static void SetmpeakFilGain (RXA& rxa, int fil, double gain);

private:
    static void calc_mpeak (MPEAK *a);
    static void decalc_mpeak (MPEAK *a);
};

} // namespace WDSP

#endif
