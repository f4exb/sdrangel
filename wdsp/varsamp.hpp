/*  varsamp.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2017 Warren Pratt, NR0V
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

#ifndef wdsp_varsamp_h
#define wdsp_varsamp_h

#include "export.h"

namespace WDSP {

class WDSP_API VARSAMP
{
public:
    int run;
    int size;
    float* in;
    float* out;
    int in_rate;
    int out_rate;
    float fcin;
    float fc;
    float fc_low;
    float gain;
    int idx_in;
    int ncoef;
    float* h;
    int rsize;
    float* ring;
    float var;
    int varmode;
    float cvar;
    float inv_cvar;
    float old_inv_cvar;
    float dicvar;
    float delta;
    float* hs;
    int R;
    float h_offset;
    float isamps;
    float nom_ratio;

    static VARSAMP* create_varsamp (
        int run,
        int size,
        float* in,
        float* out,
        int in_rate,
        int out_rate,
        float fc,
        float fc_low,
        int R,
        float gain,
        float var,
        int varmode
    );
    static void destroy_varsamp (VARSAMP *a);
    static void flush_varsamp (VARSAMP *a);
    static int xvarsamp (VARSAMP *a, float var);
    static void setBuffers_varsamp (VARSAMP *a, float* in, float* out);
    static void setSize_varsamp (VARSAMP *a, int size);
    static void setInRate_varsamp (VARSAMP *a, int rate);
    static void setOutRate_varsamp (VARSAMP *a, int rate);
    static void setFCLow_varsamp (VARSAMP *a, float fc_low);
    static void setBandwidth_varsamp (VARSAMP *a, float fc_low, float fc_high);
    // Exported calls
    static void* create_varsampV (int in_rate, int out_rate, int R);
    static void xvarsampV (float* input, float* output, int numsamps, float var, int* outsamps, void* ptr);
    static void destroy_varsampV (void* ptr);

private:
    static void calc_varsamp (VARSAMP *a);
    static void decalc_varsamp (VARSAMP *a);
    static void hshift (VARSAMP *a);
};

} // namespace WDSP

#endif
