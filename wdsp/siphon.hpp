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

#include <vector>

#include "fftw3.h"
#include "export.h"

namespace WDSP {

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
    std::vector<float> sipbuff;
    int outsize;
    int idx;
    std::vector<float> sipout;
    int fftsize;
    std::vector<float> specout;
    long specmode;
    fftwf_plan sipplan;
    std::vector<float> window;

    SIPHON(
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
    SIPHON(const SIPHON&) = delete;
    SIPHON& operator=(const SIPHON& other) = delete;
    ~SIPHON();

    void flush();
    void execute(int pos);
    void setBuffers(float* in);
    void setSamplerate(int rate);
    void setSize(int size);
    // RXA Properties
    void getaSipF  (float* out, int size);
    void getaSipF1 (float* out, int size);
    // TXA Properties
    void setSipPosition(int pos);
    void setSipMode(int mode);
    void setSipDisplay(int disp);
    void getSpecF1(float* out);
    void setSipSpecmode(int mode);

private:
    void build_window();
    void suck();
    void sip_spectrum();
};

} // namespace WDSP

#endif
