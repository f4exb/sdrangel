/*  cfir.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014, 2016 Warren Pratt, NR0V
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

#ifndef wdsp_cfir_h
#define wdsp_cfir_h

#include <vector>

#include "export.h"

namespace WDSP {

class FIRCORE;
class TXA;

class WDSP_API CFIR
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
    double cutoff;
    double scale;
    int xtype;
    double xbw;
    int wintype;
    FIRCORE *p;

    CFIR(
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
        double cutoff,
        int xtype,
        double xbw,
        int wintype
    );
    CFIR(const CFIR&) = delete;
    CFIR& operator=(CFIR& other) = delete;
    ~CFIR();

    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    void setOutRate(int rate);
    static void cfir_impulse (
        std::vector<float>& impulse,
        int N,
        int DD,
        int R,
        int Pairs,
        double runrate,
        double cicrate,
        double cutoff,
        int xtype,
        double xbw,
        int rtype,
        double scale,
        int wintype
    );
    // TXA Properties
    void setRun(int run);
    void setNC(int nc);

private:
    void calc();
    void decalc();
};

} // namespace WDSP

#endif
