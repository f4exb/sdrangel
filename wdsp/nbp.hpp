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

#include <vector>

#include "export.h"

namespace WDSP {

class FIRCORE;

class WDSP_API NOTCHDB
{
public:
    int master_run;
    double tunefreq;
    double shift;
    int nn;
    std::vector<int> active;
    std::vector<double> fcenter;
    std::vector<double> fwidth;
    std::vector<double> nlow;
    std::vector<double> nhigh;
    int maxnotches;

    NOTCHDB(int master_run, int maxnotches);
    NOTCHDB(const NOTCHDB&) = delete;
    NOTCHDB& operator=(const NOTCHDB& other) = delete;
    ~NOTCHDB() = default;

    int addNotch (int notch, double fcenter, double fwidth, int active);
    int getNotch (int notch, double* fcenter, double* fwidth, int* active);
    int deleteNotch (int notch);
    int editNotch (int notch, double fcenter, double fwidth, int active);
    void getNumNotches (int* nnotches) const;
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
    double rate;            // sample rate
    int wintype;            // filter window type
    double gain;            // filter gain
    float* in;              // input buffer
    float* out;             // output buffer
    int autoincr;           // auto-increment notch width
    double flow;            // low bandpass cutoff freq
    double fhigh;           // high bandpass cutoff freq
    std::vector<float> impulse; // filter impulse response
    int maxpb;              // maximum number of passbands
    NOTCHDB* notchdb;       // ptr to addr of notch-database data structure
    std::vector<double> bplow;  // array of passband lows
    std::vector<double> bphigh; // array of passband highs
    int numpb;              // number of passbands
    FIRCORE *fircore;
    int havnotch;
    int hadnotch;

    NBP(
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
        NOTCHDB* notchdb
    );
    NBP(const NBP&) = delete;
    NBP& operator=(const NBP& other) = delete;
    ~NBP();

    void flush();
    void execute(int pos);
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    void calc_impulse();
    void setNc();
    void setMp();
    // public Properties
    void SetRun(int run);
    void SetFreqs(double flow, double fhigh);
    void SetNC(int nc);
    void SetMP(int mp);
    void GetMinNotchWidth(double* minwidth) const;
    void calc_lightweight();

private:
    static void fir_mbandpass (std::vector<float>& impulse, int N, int nbp, const double* flow, const double* fhigh, double rate, double scale, int wintype);
    double min_notch_width () const;
    static int make_nbp (
        int nn,
        std::vector<int>& active,
        std::vector<double>& center,
        std::vector<double>& width,
        std::vector<double>& nlow,
        std::vector<double>& nhigh,
        double minwidth,
        int autoincr,
        double flow,
        double fhigh,
        std::vector<double>& bplow,
        std::vector<double>& bphigh,
        int* havnotch
    );
};

} // namespace WDSP

#endif
