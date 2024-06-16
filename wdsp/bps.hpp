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

#ifndef wdsp_bps_h
#define wdsp_bps_h

#include "fftw3.h"
#include "export.h"

namespace WDSP {

class RXA;
class TXA;

class WDSP_API BPS
{
public:
    int run;
    int position;
    int size;
    double* in;
    double* out;
    double f_low;
    double f_high;
    double* infilt;
    double* product;
    double* mults;
    double samplerate;
    int wintype;
    double gain;
    fftw_plan CFor;
    fftw_plan CRev;

    static BPS* create_bps (int run, int position, int size, double* in, double* out,
        double f_low, double f_high, int samplerate, int wintype, double gain);
    static void destroy_bps (BPS *a);
    static void flush_bps (BPS *a);
    static void xbps (BPS *a, int pos);
    static void setBuffers_bps (BPS *a, double* in, double* out);
    static void setSamplerate_bps (BPS *a, int rate);
    static void setSize_bps (BPS *a, int size);
    static void setFreqs_bps (BPS *a, double f_low, double f_high);
    // RXA Prototypes
    static void SetBPSRun (RXA& rxa, int run);
    static void SetBPSFreqs (RXA& rxa, double low, double high);
    static void SetBPSWindow (RXA& rxa, int wintype);
    // TXA Prototypes
    static void SetBPSRun (TXA& txa, int run);
    static void SetBPSFreqs (TXA& txa, double low, double high);
    static void SetBPSWindow (TXA& txa, int wintype);

private:
    static void calc_bps (BPS *a);
    static void decalc_bps (BPS *a);
};

} // namespace WDSP

#endif


