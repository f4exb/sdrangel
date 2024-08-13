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

#ifndef wdsp_snba_h
#define wdsp_snba_h

#include <vector>

#include "export.h"

namespace WDSP{

class RESAMPLE;

class WDSP_API SNBA
{
public:
    int run;
    float* in;
    float* out;
    int inrate;
    int internalrate;
    int bsize;
    int xsize;
    int ovrlp;
    int incr;
    int iasize;
    int iainidx;
    int iaoutidx;
    std::vector<double> inaccum;
    std::vector<double> xbase;
    double* xaux;
    int nsamps;
    int oasize;
    int oainidx;
    int oaoutidx;
    int init_oaoutidx;
    std::vector<double> outaccum;
    int resamprun;
    int isize;
    RESAMPLE *inresamp;
    RESAMPLE *outresamp;
    std::vector<float> inbuff;
    std::vector<float> outbuff;
    double out_low_cut;
    double out_high_cut;
    static const int MAXIMP = 256;

    struct Exec
    {
        int asize;
        std::vector<double> a;
        std::vector<double> v;
        std::vector<int> detout;
        std::vector<double> savex;
        std::vector<double> xHout;
        std::vector<int> unfixed;
        int npasses;

        Exec(int xsize, int _asize, int _npasses);
        void fluxh();
    };
    Exec exec;
    struct Det
    {
        double k1;
        double k2;
        int b;
        int pre;
        int post;
        std::vector<double> vp;
        std::vector<double> vpwr;

        Det(
            int xsize,
            double k1,
            double k2,
            int b,
            int pre,
            int post
        );
        void flush();
    };
    Det sdet;
    struct Scan
    {
        double pmultmin;
    };
    Scan scan;
    struct Wrk
    {
        int xHat_a1rows_max;
        int xHat_a2cols_max;
        std::vector<double> xHat_r;
        std::vector<double> xHat_ATAI;
        std::vector<double> xHat_A1;
        std::vector<double> xHat_A2;
        std::vector<double> xHat_P1;
        std::vector<double> xHat_P2;
        std::vector<double> trI_y;
        std::vector<double> trI_v;
        std::vector<double> dR_z;
        std::vector<double> asolve_r;
        std::vector<double> asolve_z;

        Wrk(
            int xsize,
            int asize
        );
        void flush();
    };
    Wrk wrk;

    SNBA(
        int run,
        float* in,
        float* out,
        int inrate,
        int internalrate,
        int bsize,
        int ovrlp,
        int xsize,
        int asize,
        int npasses,
        double k1,
        double k2,
        int b,
        int pre,
        int post,
        double pmultmin,
        double out_low_cut,
        double out_high_cut
    );
    SNBA(const SNBA&) = delete;
    SNBA& operator=(const SNBA& other) = delete;
    ~SNBA();

    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    // Public Properties
    void setOvrlp(int ovrlp);
    void setAsize(int size);
    void setNpasses(int npasses);
    void setK1(double k1);
    void setK2(double k2);
    void setBridge(int bridge);
    void setPresamps(int presamps);
    void setPostsamps(int postsamps);
    void setPmultmin(double pmultmin);
    void setOutputBandwidth(double flow, double fhigh);

private:
    void calc();
    void decalc();
    static void ATAc0 (int n, int nr, std::vector<double>& A, std::vector<double>& r);
    static void multA1TA2(std::vector<double>& a1, std::vector<double>& a2, int m, int n, int q, std::vector<double>& c);
    static void multXKE(std::vector<double>& a, const double* xk, int m, int q, int p, std::vector<double>& vout);
    static void multAv(std::vector<double>& a, std::vector<double>& v, int m, int q, std::vector<double>& vout);
    static void xHat(
        int xusize,
        int asize,
        const double* xk,
        std::vector<double>& a,
        std::vector<double>& xout,
        std::vector<double>& r,
        std::vector<double>& ATAI,
        std::vector<double>& A1,
        std::vector<double>& A2,
        std::vector<double>& P1,
        std::vector<double>& P2,
        std::vector<double>& trI_y,
        std::vector<double>& trI_v,
        std::vector<double>& dR_z
    );
    static void invf(int xsize, int asize, std::vector<double>& a, const double* x, std::vector<double>& v);
    static int scanFrame(
        int xsize,
        int pval,
        double pmultmin,
        std::vector<int>& det,
        std::array<int, MAXIMP>& bimp,
        std::array<int, MAXIMP>& limp,
        std::array<int, MAXIMP>& befimp,
        std::array<int, MAXIMP>& aftimp,
        std::array<int, MAXIMP>& p_opt,
        int* next
    );
    void det(int asize, std::vector<double>& v, std::vector<int>& detout);
    void execFrame(double* x);
};

} // namespace

#endif

