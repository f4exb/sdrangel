/*  icfir.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2018 Warren Pratt, NR0V
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

warren@pratt.one

*/

#ifndef wdsp_icfir_h
#define wdsp_icfir_h

#include "export.h"

namespace WDSP {

class FIRCORE;

class WDSP_API ICFIR
{
public:
    int run;
    int size;
    int nc;
    int mp;
    float* in;
    float* out;
    int runrate;
    int cicrate;
    int DD;
    int R;
    int Pairs;
    float cutoff;
    float scale;
    int xtype;
    float xbw;
    int wintype;
    FIRCORE *p;

    ICFIR(
        int run,
        int size,
        int nc,
        int mp,
        float* in,
        float* out,
        int runrate,
        int cicrate,
        int DD,
        int R,
        int Pairs,
        float cutoff,
        int xtype,
        float xbw,
        int wintype
    );
    ICFIR(const ICFIR&) = delete;
    ICFIR& operator=(const ICFIR& other) = delete;
    ~ICFIR();

    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    void setOutRate(int rate);
    static void icfir_impulse(
        std::vector<float>& impulse,
        int N,
        int DD,
        int R,
        int Pairs,
        float runrate,
        float cicrate,
        float cutoff,
        int xtype,
        float xbw,
        int rtype,
        float scale,
        int wintype
    );

private:
    void calc();
    void decalc();
};

} // namespace WDSP

#endif
