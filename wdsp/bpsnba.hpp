/*  snb.h

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

#ifndef wdsp_bpsnba_h
#define wdsp_bpsnba_h

namespace WDSP{

class RXA;

class NOTCHDB;
class NBP;

class BPSNBA
{
public:
    int run;                        // run the filter
    int run_notches;                // use the notches, vs straight bandpass
    int position;                   // position in the processing pipeline
    int size;                       // buffer size
    int nc;                         // number of filter coefficients
    int mp;                         // minimum phase flag
    float* in;                     // input buffer
    float* out;                    // output buffer
    int rate;                       // sample rate
    float* buff;                   // internal buffer
    NBP *bpsnba;                     // pointer to the notched bandpass filter, nbp
    double f_low;                   // low cutoff frequency
    double f_high;                  // high cutoff frequency
    double abs_low_freq;            // lowest positive freq supported by SNB
    double abs_high_freq;           // highest positive freq supported by SNG
    int wintype;                    // filter window type
    double gain;                    // filter gain
    int autoincr;                   // use auto increment for notch width
    int maxpb;                      // maximum passband segments supported
    NOTCHDB* ptraddr;               // pointer to address of NOTCH DATABASE

    static BPSNBA* create_bpsnba (
        int run,
        int run_notches,
        int position,
        int size,
        int nc,
        int mp,
        float* in,
        float* out,
        int rate,
        double abs_low_freq,
        double abs_high_freq,
        double f_low,
        double f_high,
        int wintype,
        double gain,
        int autoincr,
        int maxpb,
        NOTCHDB* ptraddr
    );
    static void destroy_bpsnba (BPSNBA *a);
    static void flush_bpsnba (BPSNBA *a);
    static void setBuffers_bpsnba (BPSNBA *a, float* in, float* out);
    static void setSamplerate_bpsnba (BPSNBA *a, int rate);
    static void setSize_bpsnba (BPSNBA *a, int size);
    static void xbpsnbain (BPSNBA *a, int position);
    static void xbpsnbaout (BPSNBA *a, int position);
    static void recalc_bpsnba_filter (BPSNBA *a, int update);
    // RXA Propertoes
    static void BPSNBASetNC (RXA& rxa, int nc);
    static void BPSNBASetMP (RXA& rxa, int mp);

private:
    static void calc_bpsnba (BPSNBA *a);
    static void decalc_bpsnba (BPSNBA *a);
};





#endif

} // namespace
