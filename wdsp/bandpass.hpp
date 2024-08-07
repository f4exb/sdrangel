/*  bandpass.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2016, 2017 Warren Pratt, NR0V
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
*                                       Overlap-Save Bandpass                                           *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_bandpass_h
#define wdsp_bandpass_h

#include "fftw3.h"
#include "export.h"

/********************************************************************************************************
*                                                                                                       *
*                                   Partitioned Overlap-Save Bandpass                                   *
*                                                                                                       *
********************************************************************************************************/

namespace WDSP {

class FIRCORE;

class WDSP_API BANDPASS
{
public:
    int run;
    int position;
    int size;
    int nc;
    int mp;
    float* in;
    float* out;
    double f_low;
    double f_high;
    double samplerate;
    int wintype;
    double gain;
    FIRCORE *fircore;

    BANDPASS(
        int run,
        int position,
        int size,
        int nc,
        int mp,
        float* in,
        float* out,
        double f_low,
        double f_high,
        int samplerate,
        int wintype,
        double gain
    );
    BANDPASS(const BANDPASS&) = delete;
    BANDPASS& operator=(const BANDPASS& other) = delete;
    ~BANDPASS();

    void flush();
    void execute(int pos);
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    void setGain(double gain, int update);
    void calcBandpassFilter(double f_low, double f_high, double gain);
    // RXA Prototypes
    void setBandpassFreqs(double f_low, double f_high);
    void SetBandpassNC(int nc);
    void SetBandpassMP(int mp);
};

} // namespace WDSP

#endif
