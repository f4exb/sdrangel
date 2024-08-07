/*  cblock.h

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

#ifndef wdsp_cblock_h
#define wdsp_cblock_h

#include "export.h"

namespace WDSP {

class WDSP_API CBL
{
public:
    int run;                            //run
    int buff_size;                      //buffer size
    float *in_buff;                    //pointer to input buffer
    float *out_buff;                   //pointer to output buffer
    int mode;
    double sample_rate;                 //sample rate
    double prevIin;
    double prevQin;
    double prevIout;
    double prevQout;
    double tau;                         //carrier removal time constant
    double mtau;                        //carrier removal multiplier

    CBL(
        int run,
        int buff_size,
        float *in_buff,
        float *out_buff,
        int mode,
        int sample_rate,
        double tau
    );
    CBL(const CBL&) = delete;
    CBL& operator=(CBL& other) = delete;
    ~CBL() = default;

    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    // Public Properties
    void setRun(int setit);

private:
    void calc();
};

} // namespace WDSP

#endif
