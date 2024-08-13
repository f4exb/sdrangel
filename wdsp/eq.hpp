/*  eq.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2016 Warren Pratt, NR0V
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
*                                           Overlap-Save Equalizer                                      *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_eq_h
#define wdsp_eq_h

#include <vector>

#include "fftw3.h"
#include "export.h"

namespace WDSP {

class WDSP_API EQ
{
public:
    int run;
    int size;
    float* in;
    float* out;
    int nfreqs;
    std::vector<float> F;
    std::vector<float> G;
    std::vector<float> infilt;
    std::vector<float> product;
    std::vector<float> mults;
    float scale;
    int ctfmode;
    int wintype;
    float samplerate;
    fftwf_plan CFor;
    fftwf_plan CRev;

    EQ(
        int run,
        int size,
        float *in,
        float *out,
        int nfreqs,
        const float* F,
        const float* G,
        int ctfmode,
        int wintype,
        int samplerate
    );
    EQ(const EQ&) = delete;
    EQ& operator=(EQ& other) = delete;
    ~EQ() = default;

    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);

private:
    static void eq_mults (std::vector<float>& mults, int size, int nfreqs, float* F, float* G, float samplerate, float scale, int ctfmode, int wintype);
    void calc();
    void decalc();
};

} // namespace WDSP

#endif
