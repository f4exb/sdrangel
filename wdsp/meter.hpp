/*  meter.h

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

#ifndef _meter_h
#define _meter_h

#include "export.h"

namespace WDSP {

class RXA;
class TXA;

class WDSP_API METER
{
public:
    int run;
    int* prun;
    int size;
    float* buff;
    double rate;
    double tau_average;
    double tau_peak_decay;
    double mult_average;
    double mult_peak;
    double* result;
    int enum_av;
    int enum_pk;
    int enum_gain;
    double* pgain;
    double avg;
    double peak;

    static METER* create_meter (
        int run,
        int* prun,
        int size,
        float* buff,
        int rate,
        double tau_av,
        double tau_decay,
        double* result,
        int enum_av,
        int enum_pk,
        int enum_gain,
        double* pgain
    );
    static void destroy_meter (METER *a);
    static void flush_meter (METER *a);
    static void xmeter (METER *a);
    static void setBuffers_meter (METER *a, float* in);
    static void setSamplerate_meter (METER *a, int rate);
    static void setSize_meter (METER *a, int size);
    // RXA Properties
    static double GetMeter (RXA& rxa, int mt);
    // TXA Properties
    static double GetMeter (TXA& txa, int mt);

private:
    static void calc_meter (METER *a);
};



} // namespace WDSP

#endif
