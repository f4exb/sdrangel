/*  slew.h

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

#ifndef wdsp_slew_h
#define wdsp_slew_h

#include <vector>

#include "export.h"

namespace WDSP {

class TXA;

class WDSP_API USLEW
{
public:
    enum class _USLEW
    {
        BEGIN,
        WAIT,
        UP,
        ON
    };

    long *ch_upslew;
    int size;
    float* in;
    float* out;
    double rate;
    double tdelay;
    double tupslew;
    int runmode;
    _USLEW state;
    int count;
    int ndelup;
    int ntup;
    std::vector<double> cup;

    USLEW(
        long *ch_upslew,
        int size,
        float* in,
        float* out,
        double rate,
        double tdelay,
        double tupslew
    );
    USLEW(const USLEW&) = delete;
    USLEW& operator=(const USLEW& other) = delete;
    ~USLEW() = default;

    void flush();
    void execute (int check);
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    // TXA Properties
    void setuSlewTime(double time);
    void setRun(int run);

private:
    void calc();
};

} // namespace WDSP

#endif
