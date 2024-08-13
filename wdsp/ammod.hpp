/*  ammod.h

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

#ifndef wdsp_ammod_h
#define wdsp_ammod_h

#include "export.h"

namespace WDSP {

class TXA;

class WDSP_API AMMOD
{
public:
    int run;
    int mode;
    int size;
    float* in;
    float* out;
    double c_level;
    double a_level;
    double mult;

    AMMOD(
        int run,
        int mode,
        int size,
        float* in,
        float* out,
        double c_level
    );
    AMMOD(const AMMOD&) = delete;
    AMMOD& operator=(const AMMOD& other) = delete;
    ~AMMOD() = default;

    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    // TXA Properties
    void setAMCarrierLevel(double c_level);
};

} // namespace WDSP

#endif
