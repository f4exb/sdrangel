/*  emph.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014, 2016, 2023 Warren Pratt, NR0V
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
*                               Partitioned Overlap-Save FM Pre-Emphasis                                *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_emphp_h
#define wdsp_emphp_h

#include "export.h"

namespace WDSP {

class FIRCORE;
class TXA;

class WDSP_API EMPHP
{
public:
    int run;
    int position;
    int size;
    int nc;
    int mp;
    float* in;
    float* out;
    int ctype;
    double f_low;
    double f_high;
    double rate;
    FIRCORE *p;

    EMPHP(
        int run,
        int position,
        int size,
        int nc,
        int mp,
        float* in,
        float* out,
        int rate,
        int ctype,
        double f_low,
        double f_high
    );
    EMPHP(const EMPHP&) = delete;
    EMPHP& operator=(const EMPHP& other) = delete;
    ~EMPHP();

    void flush();
    void execute(int position);
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    // TXA Properties
    void setPosition(int position);
    void setMP(int mp);
    void setNC(int nc);
    void setFreqs(double low, double high);
};

} // namespace WDSP

#endif
