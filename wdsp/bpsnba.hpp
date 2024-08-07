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

#include <vector>

#include "export.h"
namespace WDSP{

class NOTCHDB;
class NBP;

class WDSP_API BPSNBA
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
    double abs_low_freq;            // lowest positive freq supported by SNB
    double abs_high_freq;           // highest positive freq supported by SNG
    double f_low;                   // low cutoff frequency
    double f_high;                  // high cutoff frequency
    std::vector<float> buff;        // internal buffer
    int wintype;                    // filter window type
    double gain;                    // filter gain
    int autoincr;                   // use auto increment for notch width
    int maxpb;                      // maximum passband segments supported
    NOTCHDB* notchdb;               // pointer to address of NOTCH DATABASE
    NBP *bpsnba;                     // pointer to the notched bandpass filter, nbp

    BPSNBA(
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
        NOTCHDB* notchdb
    );
    BPSNBA(const BPSNBA&) = delete;
    BPSNBA& operator=(BPSNBA& other) = delete;
    ~BPSNBA();

    void flush();
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    void exec_in(int position);
    void exec_out(int position);
    void recalc_bpsnba_filter(int update);
    // Propertoes
    void SetNC(int nc);
    void SetMP(int mp);

private:
    void calc();
    void decalc();
};





#endif

} // namespace
