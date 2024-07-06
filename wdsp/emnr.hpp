/*  emnr.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2015 Warren Pratt, NR0V
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

#ifndef wdsp_emnr_h
#define wdsp_emnr_h

#include "fftw3.h"
#include "export.h"

namespace WDSP {

class RXA;

class WDSP_API EMNR
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
    float* window;
    int iasize;
    float* inaccum;
    float* forfftin;
    float* forfftout;
    int msize;
    double* mask;
    float* revfftin;
    float* revfftout;
    float** save;
    int oasize;
    float* outaccum;
    float rate;
    int wintype;
    float ogain;
    float gain;
    int nsamps;
    int iainidx;
    int iaoutidx;
    int init_oainidx;
    int oainidx;
    int oaoutidx;
    int saveidx;
    fftwf_plan Rfor;
    fftwf_plan Rrev;
    struct _g
    {
        int gain_method;
        int npe_method;
        int ae_run;
        double msize;
        double* mask;
        float* y;
        double* lambda_y;
        double* lambda_d;
        double* prev_mask;
        double* prev_gamma;
        double gf1p5;
        double alpha;
        double eps_floor;
        double gamma_max;
        double q;
        double gmax;
        //
        double* GG;
        double* GGS;
        FILE* fileb;
    } g;
    struct _npest
    {
        int incr;
        double rate;
        int msize;
        double* lambda_y;
        double* lambda_d;
        double* p;
        double* alphaOptHat;
        double alphaC;
        double alphaCsmooth;
        double alphaCmin;
        double* alphaHat;
        double alphaMax;
        double* sigma2N;
        double alphaMin_max_value;
        double snrq;
        double betamax;
        double* pbar;
        double* p2bar;
        double invQeqMax;
        double av;
        double* Qeq;
        int U;
        double Dtime;
        int V;
        int D;
        double MofD;
        double MofV;
        double* bmin;
        double* bmin_sub;
        int* k_mod;
        double* actmin;
        double* actmin_sub;
        int subwc;
        int* lmin_flag;
        double* pmin_u;
        double invQbar_points[4];
        double nsmax[4];
        double** actminbuff;
        int amb_idx;
    } np;
    struct _npests
    {
        int incr;
        double rate;
        int msize;
        double* lambda_y;
        double* lambda_d;

        double alpha_pow;
        double alpha_Pbar;
        double epsH1;
        double epsH1r;

        double* sigma2N;
        double* PH1y;
        double* Pbar;
        double* EN2y;
    } nps;
    struct _ae
    {
        int msize;
        double* lambda_y;
        double zetaThresh;
        double psi;
        double* nmask;
    } ae;

    static EMNR* create_emnr (
        int run,
        int position,
        int size,
        float* in,
        float* out,
        int fsize,
        int ovrlp,
        int rate,
        int wintype,
        float gain,
        int gain_method,
        int npe_method,
        int ae_run
    );
    static void destroy_emnr (EMNR *a);
    static void flush_emnr (EMNR *a);
    static void xemnr (EMNR *a, int pos);
    static void setBuffers_emnr (EMNR *a, float* in, float* out);
    static void setSamplerate_emnr (EMNR *a, int rate);
    static void setSize_emnr (EMNR *a, int size);
    // RXA Properties
    static void SetEMNRRun (RXA& rxa, int run);
    static void SetEMNRgainMethod (RXA& rxa, int method);
    static void SetEMNRnpeMethod (RXA& rxa, int method);
    static void SetEMNRaeRun (RXA& rxa, int run);
    static void SetEMNRPosition (RXA& rxa, int position);
    static void SetEMNRaeZetaThresh (RXA& rxa, double zetathresh);
    static void SetEMNRaePsi (RXA& rxa, double psi);

private:
    static double bessI0 (double x);
    static double bessI1 (double x);
    static double e1xb (double x);
    static void calc_window (EMNR *a);
    static void interpM (double* res, double x, int nvals, double* xvals, double* yvals);
    static void calc_emnr(EMNR *a);
    static void decalc_emnr(EMNR *a);
    static void LambdaD(EMNR *a);
    static void LambdaDs (EMNR *a);
    static void aepf(EMNR *a);
    static double getKey(double* type, double gamma, double xi);
    static void calc_gain (EMNR *a);
};

} // namespace WDSP

#endif
