/*  nbp.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2015, 2016 Warren Pratt, NR0V
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

#ifndef wdsp_nbp_h
#define wdsp_nbp_h

#include "export.h"

namespace WDSP {

class FIRCORE;
class RXA;

class WDSP_API NOTCHDB
{
public:
    int master_run;
    double tunefreq;
    double shift;
    int nn;
    int* active;
    double* fcenter;
    double* fwidth;
    double* nlow;
    double* nhigh;
    int maxnotches;

    static NOTCHDB* create_notchdb (int master_run, int maxnotches);
    static void destroy_notchdb (NOTCHDB *b);
};


class NBP
{
public:
    int run;                // run the filter
    int fnfrun;             // use the notches
    int position;           // position in processing pipeline
    int size;               // buffer size
    int nc;                 // number of filter coefficients
    int mp;                 // minimum phase flag
    float* in;              // input buffer
    float* out;             // output buffer
    double flow;            // low bandpass cutoff freq
    double fhigh;           // high bandpass cutoff freq
    float* impulse;         // filter impulse response
    double rate;            // sample rate
    int wintype;            // filter window type
    double gain;            // filter gain
    int autoincr;           // auto-increment notch width
    int maxpb;              // maximum number of passbands
    NOTCHDB* ptraddr;       // ptr to addr of notch-database data structure
    double* bplow;          // array of passband lows
    double* bphigh;         // array of passband highs
    int numpb;              // number of passbands
    FIRCORE *p;
    int havnotch;
    int hadnotch;

    static NBP* create_nbp(
        int run,
        int fnfrun,
        int position,
        int size,
        int nc,
        int mp,
        float* in,
        float* out,
        double flow,
        double fhigh,
        int rate,
        int wintype,
        double gain,
        int autoincr,
        int maxpb,
        NOTCHDB* ptraddr
    );
    static void destroy_nbp (NBP *a);
    static void flush_nbp (NBP *a);
    static void xnbp (NBP *a, int pos);
    static void setBuffers_nbp (NBP *a, float* in, float* out);
    static void setSamplerate_nbp (NBP *a, int rate);
    static void setSize_nbp (NBP *a, int size);
    static void calc_nbp_impulse (NBP *a);
    static void setNc_nbp (NBP *a);
    static void setMp_nbp (NBP *a);
    // RXA Properties
    static void UpdateNBPFiltersLightWeight (RXA& rxa);
    static void UpdateNBPFilters(RXA& rxa);
    static int NBPAddNotch (RXA& rxa, int notch, double fcenter, double fwidth, int active);
    static int NBPGetNotch (RXA& rxa, int notch, double* fcenter, double* fwidth, int* active);
    static int NBPDeleteNotch (RXA& rxa, int notch);
    static int NBPEditNotch (RXA& rxa, int notch, double fcenter, double fwidth, int active);
    static void NBPGetNumNotches (RXA& rxa, int* nnotches);
    static void NBPSetTuneFrequency (RXA& rxa, double tunefreq);
    static void NBPSetShiftFrequency (RXA& rxa, double shift);
    static void NBPSetNotchesRun (RXA& rxa, int run);
    static void NBPSetRun (RXA& rxa, int run);
    static void NBPSetFreqs (RXA& rxa, double flow, double fhigh);
    static void NBPSetWindow (RXA& rxa, int wintype);

    static void NBPSetNC (RXA& rxa, int nc);
    static void NBPSetMP (RXA& rxa, int mp);

    static void NBPGetMinNotchWidth (RXA& rxa, double* minwidth);
    static void NBPSetAutoIncrease (RXA& rxa, int autoincr);

private:
    static float* fir_mbandpass (int N, int nbp, double* flow, double* fhigh, double rate, double scale, int wintype);
    static double min_notch_width (NBP *a);
    static int make_nbp (
        int nn,
        int* active,
        double* center,
        double* width,
        double* nlow,
        double* nhigh,
        double minwidth,
        int autoincr,
        double flow,
        double fhigh,
        double* bplow,
        double* bphigh,
        int* havnotch
    );
    static void calc_nbp_lightweight (NBP *a);
};

} // namespace WDSP

#endif
