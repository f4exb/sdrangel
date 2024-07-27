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

namespace WDSP{

class RESAMPLE;

class SNBA
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
    double* inaccum;
    double* xbase;
    double* xaux;
    int nsamps;
    int oasize;
    int oainidx;
    int oaoutidx;
    int init_oaoutidx;
    double* outaccum;
    int resamprun;
    int isize;
    RESAMPLE *inresamp;
    RESAMPLE *outresamp;
    float* inbuff;
    float* outbuff;
    double out_low_cut;
    double out_high_cut;

    struct _exec
    {
        int asize;
        double* a;
        double* v;
        int* detout;
        double* savex;
        double* xHout;
        int* unfixed;
        int npasses;
    } exec;
    struct _det
    {
        double k1;
        double k2;
        int b;
        int pre;
        int post;
        double* vp;
        double* vpwr;
    } sdet;
    struct _scan
    {
        double pmultmin;
    } scan;
    struct _wrk
    {
        int xHat_a1rows_max;
        int xHat_a2cols_max;
        double* xHat_r;
        double* xHat_ATAI;
        double* xHat_A1;
        double* xHat_A2;
        double* xHat_P1;
        double* xHat_P2;
        double* trI_y;
        double* trI_v;
        double* dR_z;
        double* asolve_r;
        double* asolve_z;
    } wrk;

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
    static void ATAc0 (int n, int nr, double* A, double* r);
    static void multA1TA2(double* a1, double* a2, int m, int n, int q, double* c);
    static void multXKE(double* a, double* xk, int m, int q, int p, double* vout);
    static void multAv(double* a, double* v, int m, int q, double* vout);
    static void xHat(
        int xusize,
        int asize,
        double* xk,
        double* a,
        double* xout,
        double* r,
        double* ATAI,
        double* A1,
        double* A2,
        double* P1,
        double* P2,
        double* trI_y,
        double* trI_v,
        double* dR_z
    );
    static void invf(int xsize, int asize, double* a, double* x, double* v);
    static int scanFrame(
        int xsize,
        int pval,
        double pmultmin,
        int* det,
        int* bimp,
        int* limp,
        int* befimp,
        int* aftimp,
        int* p_opt,
        int* next
    );
    void det(int asize, double* v, int* detout);
    void execFrame(double* x);
};

} // namespace

#endif

