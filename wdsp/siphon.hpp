/*  siphon.h

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

// 'siphon' collects samples in a buffer.  These samples can then be PULLED from the buffer
//  in either raw or FFT'd form.

#ifndef wdsp_siphon_h
#define wdsp_siphon_h

#include "fftw3.h"
#include "export.h"

#include <atomic>
#include <QRecursiveMutex>

namespace WDSP {

class RXA;
class TXA;

class WDSP_API SIPHON
{
public:
    int run;
    int position;
    int mode;
    int disp;
    int insize;
    float* in;
    int sipsize;    // NOTE:  sipsize MUST BE A POWER OF TWO!!
    float* sipbuff;
    int outsize;
    int idx;
    float* sipout;
    int fftsize;
    float* specout;
    std::atomic<long> specmode;
    fftwf_plan sipplan;
    float* window;
    QRecursiveMutex update;

    static SIPHON* create_siphon (
        int run,
        int position,
        int mode,
        int disp,
        int insize,
        float* in,
        int sipsize,
        int fftsize,
        int specmode
    );
    static void destroy_siphon (SIPHON *a);
    static void flush_siphon (SIPHON *a);
    static void xsiphon (SIPHON *a, int pos);
    static void setBuffers_siphon (SIPHON *a, float* in);
    static void setSamplerate_siphon (SIPHON *a, int rate);
    static void setSize_siphon (SIPHON *a, int size);
    // RXA Properties
    static void GetaSipF  (RXA& rxa, float* out, int size);
    static void GetaSipF1 (RXA& rxa, float* out, int size);
    // TXA Properties
    static void SetSipPosition (TXA& txa, int pos);
    static void SetSipMode (TXA& txa, int mode);
    static void SetSipDisplay (TXA& txa, int disp);
    static void GetaSipF  (TXA& txa, float* out, int size);
    static void GetaSipF1 (TXA& txa, float* out, int size);
    static void GetSpecF1 (TXA& txa, float* out);
    static void SetSipSpecmode (TXA& txa, int mode);
    // Calls for External Use
    // static void create_siphonEXT (int id, int run, int insize, int sipsize, int fftsize, int specmode);
    // static void destroy_siphonEXT (int id);
    // static void xsiphonEXT (int id, float* buff);
    // static void SetSiphonInsize (int id, int size);

private:
    static void build_window (SIPHON *a);
    static void suck (SIPHON *a);
    static void sip_spectrum (SIPHON *a);
};

} // namespace WDSP

#endif
