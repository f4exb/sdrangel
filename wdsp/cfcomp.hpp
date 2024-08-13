/*  cfcomp.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2017, 2021 Warren Pratt, NR0V
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

#ifndef wdsp_cfcomp_h
#define wdsp_cfcomp_h

#include <vector>

#include "fftw3.h"
#include "export.h"

namespace WDSP {

class TXA;

class WDSP_API CFCOMP
{
public:
    int run;
    int position;
    int bsize;
    float* in;
    float* out;
    int fsize;
    int ovrlp;
    int incr;
    std::vector<double> window;
    int iasize;
    std::vector<double> inaccum;
    std::vector<float> forfftin;
    std::vector<float> forfftout;
    int msize;
    std::vector<double> cmask;
    std::vector<double> mask;
    int mask_ready;
    std::vector<double> cfc_gain;
    std::vector<float> revfftin;
    std::vector<float> revfftout;
    std::vector<std::vector<double>> save;
    int oasize;
    std::vector<double> outaccum;
    double rate;
    int wintype;
    double pregain;
    double postgain;
    int nsamps;
    int iainidx;
    int iaoutidx;
    int init_oainidx;
    int oainidx;
    int oaoutidx;
    int saveidx;
    fftwf_plan Rfor;
    fftwf_plan Rrev;

    int comp_method;
    int nfreqs;
    std::vector<double> F;
    std::vector<double> G;
    std::vector<double> E;
    std::vector<double> fp;
    std::vector<double> gp;
    std::vector<double> ep;
    std::vector<double> comp;
    double precomp;
    double precomplin;
    std::vector<double> peq;
    int peq_run;
    double prepeq;
    double prepeqlin;
    double winfudge;

    double gain;
    double mtau;
    double mmult;
    // display stuff
    double dtau;
    double dmult;
    std::vector<double> delta;
    std::vector<double> delta_copy;
    std::vector<double> cfc_gain_copy;

    CFCOMP(
        int run,
        int position,
        int peq_run,
        int size,
        float* in,
        float* out,
        int fsize,
        int ovrlp,
        int rate,
        int wintype,
        int comp_method,
        int nfreqs,
        double precomp,
        double prepeq,
        const double* F,
        const double* G,
        const double* E,
        double mtau,
        double dtau
    );
    CFCOMP(const CFCOMP&) = delete;
    CFCOMP& operator=(CFCOMP& other) = delete;
    ~CFCOMP();

    void flush();
    void execute(int pos);
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    // TXA Properties
    void setRun(int run);
    void setPosition(int pos);
    void setProfile(int nfreqs, const double* F, const double* G, const double *E);
    void setPrecomp(double precomp);
    void setPeqRun(int run);
    void setPrePeq(double prepeq);
    void getDisplayCompression(double* comp_values, int* ready);

private:
    void calc_cfcwindow();
    static int fCOMPcompare (const void *a, const void *b);
    void calc_comp();
    void calc_cfcomp();
    void decalc_cfcomp();
    void calc_mask();
};

} // namespace WDSP

#endif
