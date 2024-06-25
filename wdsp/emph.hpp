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
    float f_low;
    float f_high;
    float rate;
    FIRCORE *p;

    static EMPHP* create_emphp (int run, int position, int size, int nc, int mp,
        float* in, float* out, int rate, int ctype, float f_low, float f_high);
    static void destroy_emphp (EMPHP *a);
    static void flush_emphp (EMPHP *a);
    static void xemphp (EMPHP *a, int position);
    static void setBuffers_emphp (EMPHP *a, float* in, float* out);
    static void setSamplerate_emphp (EMPHP *a, int rate);
    static void setSize_emphp (EMPHP *a, int size);
    // TXA Properties
    static void SetFMEmphPosition (TXA& txa, int position);
    static void SetFMEmphMP (TXA& txa, int mp);
    static void SetFMEmphNC (TXA& txa, int nc);
    static void SetFMPreEmphFreqs(TXA& txa, float low, float high);
};

} // namespace WDSP

#endif

/********************************************************************************************************
*                                                                                                       *
*                                       Overlap-Save FM Pre-Emphasis                                    *
*                                                                                                       *
********************************************************************************************************/

#ifndef _emph_h
#define _emph_h

#include "fftw3.h"
#include "export.h"

namespace WDSP {

class WDSP_API EMPH
{
    int run;
    int position;
    int size;
    float* in;
    float* out;
    int ctype;
    float f_low;
    float f_high;
    float* infilt;
    float* product;
    float* mults;
    float rate;
    fftwf_plan CFor;
    fftwf_plan CRev;

    static EMPH* create_emph (int run, int position, int size, float* in, float* out, int rate, int ctype, float f_low, float f_high);
    static void destroy_emph (EMPH *a);
    static void flush_emph (EMPH *a);
    static void xemph (EMPH *a, int position);
    static void setBuffers_emph (EMPH *a, float* in, float* out);
    static void setSamplerate_emph (EMPH *a, int rate);
    static void setSize_emph (EMPH *a, int size);

private:
    static void calc_emph (EMPH *a);
    static void decalc_emph (EMPH *a);
};

} // namespace WDSP

#endif
