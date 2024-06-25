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
    float cutoff;
    float scale;
    int xtype;
    float xbw;
    int wintype;
    FIRCORE *p;

    static CFIR* create_cfir (
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
    static void destroy_cfir (CFIR *a);
    static void flush_cfir (CFIR *a);
    static void xcfir (CFIR *a);
    static void setBuffers_cfir (CFIR *a, float* in, float* out);
    static void setSamplerate_cfir (CFIR *a, int rate);
    static void setSize_cfir (CFIR *a, int size);
    static void setOutRate_cfir (CFIR *a, int rate);
    static float* cfir_impulse (
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
    // TXA Properties
    static void SetCFIRRun(TXA& txa, int run);
    static void SetCFIRNC(TXA& txa, int nc);

private:
    static void calc_cfir (CFIR *a);
    static void decalc_cfir (CFIR *a);
};

} // namespace WDSP

#endif
