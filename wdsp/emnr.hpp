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

#include <array>
#include <vector>

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
    std::vector<float> window;
    int iasize;
    std::vector<float> inaccum;
    std::vector<float> forfftin;
    std::vector<float> forfftout;
    int msize;
    std::vector<double> mask;
    std::vector<float> revfftin;
    std::vector<float> revfftout;
    std::vector<std::vector<float>> save;
    int oasize;
    std::vector<float> outaccum;
    double rate;
    int wintype;
    double ogain;
    double gain;
    int nsamps;
    int iainidx;
    int iaoutidx;
    int init_oainidx;
    int oainidx;
    int oaoutidx;
    int saveidx;
    fftwf_plan Rfor;
    fftwf_plan Rrev;
    struct G
    {
        int incr;
        double rate;
        int msize;
        double* mask;
        float* y;
        int gain_method;
        int npe_method;
        int ae_run;
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
        G(
            int incr,
            double rate,
            int msize,
            double* mask,
            float* y
        );
        G(const G&) = delete;
        G& operator=(const G& other) = delete;
        ~G();
    } *g;
    struct NP
    {
        int incr;
        double rate;
        int msize;
        double* lambda_y;
        double* lambda_d;
        double alphaCsmooth;
        double alphaMax;
        double alphaCmin;
        double alphaMin_max_value;
        double snrq;
        double betamax;
        double invQeqMax;
        double av;
        double Dtime;
        int U;
        int V;
        int D;
        double* p;
        double* alphaOptHat;
        double alphaC;
        double* alphaHat;
        double* sigma2N;
        double* pbar;
        double* p2bar;
        double* Qeq;
        double MofD;
        double MofV;
        std::array<double, 4> invQbar_points;
        std::array<double, 4> nsmax;
        double* bmin;
        double* bmin_sub;
        int* k_mod;
        double* actmin;
        double* actmin_sub;
        int subwc;
        int* lmin_flag;
        double* pmin_u;
        double** actminbuff;
        int amb_idx;
        NP(
            int incr,
            double rate,
            int msize,
            double* lambda_y,
            double* lambda_d
        );
        NP(const NP&) = delete;
        NP& operator=(const NP& other) = delete;
        ~NP();
        void LambdaD();

    private:
        static const std::array<double, 18> DVals;
        static const std::array<double, 18> MVals;
        static void interpM (
            double* res,
            double x,
            int nvals,
            const std::array<double, 18>& xvals,
            const std::array<double, 18>& yvals
        );
    } *np;
    struct NPS
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
        NPS(
            int incr,
            double rate,
            int msize,
            double* lambda_y,
            double* lambda_d,

            double alpha_pow,
            double alpha_Pbar,
            double epsH1
        );
        NPS(const NPS&) = delete;
        NPS& operator=(const NPS& other) = delete;
        ~NPS();
        void LambdaDs();
    } *nps;
    struct AE
    {
        int msize;
        double* lambda_y;
        double zetaThresh;
        double psi;
        double* nmask;
        AE(
            int msize,
            double* lambda_y,
            double zetaThresh,
            double psi
        );
        AE(const AE&) = delete;
        AE& operator=(const AE& other) = delete;
        ~AE();
    } *ae;

    EMNR(
        int run,
        int position,
        int size,
        float* in,
        float* out,
        int fsize,
        int ovrlp,
        int rate,
        int wintype,
        double gain,
        int gain_method,
        int npe_method,
        int ae_run
    );
    EMNR(const EMNR&) = delete;
    EMNR& operator=(const EMNR& other) = delete;
    ~EMNR();

    void flush();
    void execute(int pos);
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    // Public Properties
    void setGainMethod(int method);
    void setNpeMethod(int method);
    void setAeRun(int run);
    void setAeZetaThresh(double zetathresh);
    void setAePsi(double psi);

private:
    static double bessI0 (double x);
    static double bessI1 (double x);
    static double e1xb (double x);
    static void interpM (double* res, double x, int nvals, double* xvals, double* yvals);
    static double getKey(double* type, double gamma, double xi);
    void calc_window();
    void calc();
    void decalc();
    void aepf();
    void calc_gain();
};

} // namespace WDSP

#endif
