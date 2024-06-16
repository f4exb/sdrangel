/*  delay.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2019 Warren Pratt, NR0V
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

#ifndef wdsp_delay_h
#define wdsp_delay_h

#include <QRecursiveMutex>

#include "export.h"

#define WSDEL   1025    // number of supported whole sample delays

namespace WDSP {

class WDSP_API DELAY
{
public:
    int run;            // run
    int size;           // number of input samples per buffer
    double* in;         // input buffer
    double* out;        // output buffer
    int rate;           // samplerate
    double tdelta;      // delay increment required (seconds)
    double tdelay;      // delay requested (seconds)

    int L;              // interpolation factor
    int ncoef;          // number of coefficients
    int cpp;            // coefficients per phase
    double ft;          // normalized cutoff frequency
    double* h;          // coefficients
    int snum;           // starting sample number (0 for sub-sample delay)
    int phnum;          // phase number

    int idx_in;         // index for input into ring
    int rsize;          // ring size in complex samples
    double* ring;       // ring buffer

    double adelta;      // actual delay increment
    double adelay;      // actual delay

    QRecursiveMutex cs_update;

    static DELAY* create_delay (int run, int size, double* in, double* out, int rate, double tdelta, double tdelay);
    static void destroy_delay (DELAY *a);
    static void flush_delay (DELAY *a);
    static void xdelay (DELAY *a);
    // Properties
    static void SetDelayRun (DELAY *a, int run);
    static double SetDelayValue (DELAY *a, double delay);        // returns actual delay in seconds
    static void SetDelayBuffs (DELAY *a, int size, double* in, double* out);
};

} // namespace WDSP

#endif
