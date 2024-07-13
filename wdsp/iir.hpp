/*  iir.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014, 2022, 2023 Warren Pratt, NR0V
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
*                                           Bi-Quad Notch                                               *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_snotch_h
#define wdsp_snotch_h

#include "export.h"

namespace WDSP {

class WDSP_API SNOTCH
{
public:
    int run;
    int size;
    float* in;
    float* out;
    float rate;
    float f;
    float bw;
    float a0, a1, a2, b1, b2;
    float x0, x1, x2, y1, y2;

    static SNOTCH* create_snotch (int run, int size, float* in, float* out, int rate, float f, float bw);
    static void destroy_snotch (SNOTCH *a);
    static void flush_snotch (SNOTCH *a);
    static void xsnotch (SNOTCH *a);
    static void setBuffers_snotch (SNOTCH *a, float* in, float* out);
    static void setSamplerate_snotch (SNOTCH *a, int rate);
    static void setSize_snotch (SNOTCH *a, int size);
    static void SetSNCTCSSFreq (SNOTCH *a, float freq);
    static void SetSNCTCSSRun (SNOTCH *a, int run);

private:
    static void calc_snotch (SNOTCH *a);
};

} // namespace WDSP

#endif

/********************************************************************************************************
*                                                                                                       *
*                                           Complex Bi-Quad Peaking                                     *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_speak_h
#define wdsp_speak_h

#include "export.h"

namespace WDSP {

class RXA;

class WDSP_API SPEAK
{
public:
    int run;
    int size;
    float* in;
    float* out;
    float rate;
    float f;
    float bw;
    float cbw;
    float gain;
    float fgain;
    int nstages;
    int design;
    float a0, a1, a2, b1, b2;
    float *x0, *x1, *x2, *y0, *y1, *y2;

    static SPEAK* create_speak (int run, int size, float* in, float* out, int rate, float f, float bw, float gain, int nstages, int design);
    static void destroy_speak (SPEAK *a);
    static void flush_speak (SPEAK *a);
    static void xspeak (SPEAK *a);
    static void setBuffers_speak (SPEAK *a, float* in, float* out);
    static void setSamplerate_speak (SPEAK *a, int rate);
    static void setSize_speak (SPEAK *a, int size);
    // RXA
    static void SetSPCWRun (RXA& rxa, int run);
    static void SetSPCWFreq (RXA& rxa, float freq);
    static void SetSPCWBandwidth (RXA& rxa, float bw);
    static void SetSPCWGain (RXA& rxa, float gain);
    static void calc_speak (SPEAK *a);
};

} // namespace WDSP

#endif

/********************************************************************************************************
*                                                                                                       *
*                                       Complex Multiple Peaking                                        *
*                                                                                                       *
********************************************************************************************************/

#ifndef _mpeak_h
#define _mpeak_h

#include "export.h"

namespace WDSP {

class RXA;

class WDSP_API MPEAK
{
public:
    int run;
    int size;
    float* in;
    float* out;
    int rate;
    int npeaks;
    int* enable;
    float* f;
    float* bw;
    float* gain;
    int nstages;
    SPEAK** pfil;
    float* tmp;
    float* mix;

    static MPEAK* create_mpeak (int run, int size, float* in, float* out, int rate, int npeaks, int* enable, float* f, float* bw, float* gain, int nstages);
    static void destroy_mpeak (MPEAK *a);
    static void flush_mpeak (MPEAK *a);
    static void xmpeak (MPEAK *a);
    static void setBuffers_mpeak (MPEAK *a, float* in, float* out);
    static void setSamplerate_mpeak (MPEAK *a, int rate);
    static void setSize_mpeak (MPEAK *a, int size);
    // RXA
    static void SetmpeakRun (RXA& rxa, int run);
    static void SetmpeakNpeaks (RXA& rxa, int npeaks);
    static void SetmpeakFilEnable (RXA& rxa, int fil, int enable);
    static void SetmpeakFilFreq (RXA& rxa, int fil, float freq);
    static void SetmpeakFilBw (RXA& rxa, int fil, float bw);
    static void SetmpeakFilGain (RXA& rxa, int fil, float gain);

private:
    static void calc_mpeak (MPEAK *a);
    static void decalc_mpeak (MPEAK *a);
};

} // namespace WDSP

#endif

/********************************************************************************************************
*                                                                                                       *
*                                            Phase Rotator                                              *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_phrot_h
#define wdsp_phrot_h

#include "export.h"

namespace WDSP {

class TXA;

class WDSP_API PHROT
{
public:
    int reverse;
    int run;
    int size;
    float* in;
    float* out;
    int rate;
    float fc;
    int nstages;
    // normalized such that a0 = 1
    float a1, b0, b1;
    float *x0, *x1, *y0, *y1;

    static PHROT* create_phrot (int run, int size, float* in, float* out, int rate, float fc, int nstages);
    static void destroy_phrot (PHROT *a);
    static void flush_phrot (PHROT *a);
    static void xphrot (PHROT *a);
    static void setBuffers_phrot (PHROT *a, float* in, float* out);
    static void setSamplerate_phrot (PHROT *a, int rate);
    static void setSize_phrot (PHROT *a, int size);
    // TXA Properties
    static void SetPHROTRun (TXA& txa, int run);
    static void SetPHROTCorner (TXA& txa, float corner);
    static void SetPHROTNstages (TXA& txa, int nstages);
    static void SetPHROTReverse (TXA& txa, int reverse);

private:
    static void calc_phrot (PHROT *a);
    static void decalc_phrot (PHROT *a);
};

} // namespace WDSP

#endif

/********************************************************************************************************
*                                                                                                       *
*                                   Complex Bi-Quad Low-Pass                                            *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_bqlp_h
#define wdsp_bqlp_h

#include "export.h"

namespace WDSP {

class WDSP_API BQLP
{
public:
    int run;
    int size;
    float* in;
    float* out;
    float rate;
    float fc;
    float Q;
    float gain;
    int nstages;
    float a0, a1, a2, b1, b2;
    float* x0, * x1, * x2, * y0, * y1, * y2;

    static BQLP* create_bqlp(int run, int size, float* in, float* out, float rate, float fc, float Q, float gain, int nstages);
    static void destroy_bqlp(BQLP *a);
    static void flush_bqlp(BQLP *a);
    static void xbqlp(BQLP *a);
    static void setBuffers_bqlp(BQLP *a, float* in, float* out);
    static void setSamplerate_bqlp(BQLP *a, int rate);
    static void setSize_bqlp(BQLP *a, int size);

private:
    static void calc_bqlp(BQLP *a);
};

} // namespace WDSP

#endif

/********************************************************************************************************
*                                                                                                       *
*                                      Double Bi-Quad Low-Pass                                          *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_dbqlp_h
#define wdsp_dbqlp_h

#include "export.h"

namespace WDSP {

class WDSP_API DBQLP
{
public:
    static BQLP* create_dbqlp(int run, int size, float* in, float* out, float rate, float fc, float Q, float gain, int nstages);
    static void destroy_dbqlp(BQLP *a);
    static void flush_dbqlp(BQLP *a);
    static void xdbqlp(BQLP *a);
    static void setBuffers_dbqlp(BQLP *a, float* in, float* out);
    static void setSamplerate_dbqlp(BQLP *a, int rate);
    static void setSize_dbqlp(BQLP *a, int size);

private:
    static void calc_dbqlp(BQLP *a);
};

} // namespace WDSP

#endif

/********************************************************************************************************
*                                                                                                       *
*                                   Complex Bi-Quad Band-Pass                                           *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_bqbp_h
#define wdsp_bqbp_h

#include "export.h"

namespace WDSP {

class WDSP_API BQBP
{
public:
    int run;
    int size;
    float* in;
    float* out;
    float rate;
    float f_low;
    float f_high;
    float gain;
    int nstages;
    float a0, a1, a2, b1, b2;
    float* x0, * x1, * x2, * y0, * y1, * y2;

    static BQBP* create_bqbp(int run, int size, float* in, float* out, float rate, float f_low, float f_high, float gain, int nstages);
    static void destroy_bqbp(BQBP *a);
    static void flush_bqbp(BQBP *a);
    static void xbqbp(BQBP *a);
    static void setBuffers_bqbp(BQBP *a, float* in, float* out);
    static void setSamplerate_bqbp(BQBP *a, int rate);
    static void setSize_bqbp(BQBP *a, int size);

    // Double Bi-Quad Band-Pass
    static BQBP* create_dbqbp(int run, int size, float* in, float* out, float rate, float f_low, float f_high, float gain, int nstages);
    static void destroy_dbqbp(BQBP *a);
    static void flush_dbqbp(BQBP *a);
    static void xdbqbp(BQBP *a);
    static void setBuffers_dbqbp(BQBP *a, float* in, float* out);
    static void setSamplerate_dbqbp(BQBP *a, int rate);
    static void setSize_dbqbp(BQBP *a, int size);

private:
    static void calc_bqbp(BQBP *a);
    static void calc_dbqbp(BQBP *a);
};

} // namespace WDSP

#endif

/********************************************************************************************************
*                                                                                                       *
*                                      Double Single-Pole High-Pass                                     *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_dsphp_h
#define wdsp_dsphp_h

#include "export.h"

namespace WDSP {

class WDSP_API SPHP
{
public:
    int run;
    int size;
    float* in;
    float* out;
    float rate;
    float fc;
    int nstages;
    float a1, b0, b1;
    float* x0, * x1, * y0, * y1;

    static SPHP* create_dsphp(int run, int size, float* in, float* out, float rate, float fc, int nstages);
    static void destroy_dsphp(SPHP *a);
    static void flush_dsphp(SPHP *a);
    static void xdsphp(SPHP *a);
    static void setBuffers_dsphp(SPHP *a, float* in, float* out);
    static void setSamplerate_dsphp(SPHP *a, int rate);
    static void setSize_dsphp(SPHP *a, int size);

    // Complex Single-Pole High-Pass
    static SPHP* create_sphp(int run, int size, float* in, float* out, float rate, float fc, int nstages);
    static void destroy_sphp(SPHP *a);
    static void flush_sphp(SPHP *a);
    static void xsphp(SPHP *a);
    static void setBuffers_sphp(SPHP *a, float* in, float* out);
    static void setSamplerate_sphp(SPHP *a, int rate);
    static void setSize_sphp(SPHP *a, int size);

private:
    static void calc_sphp(SPHP *a);
    static void decalc_sphp(SPHP *a);
    static void calc_dsphp(SPHP *a);
    static void decalc_dsphp(SPHP *a);
};

} // namespace WDSP

#endif


