///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
// written by Robert Morris, AB1HL                                               //
// reformatted and adapted to Qt and SDRangel context                            //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef FFT_H
#define FFT_H

#include <QMutex>
#include <vector>
#include <complex>
#include <fftw3.h>

#include "export.h"

namespace FT8
{
class FT8_API FFTEngine
{
public:
    // a cached fftw plan, for both of:
    // fftwf_plan_dft_r2c_1d(n, m_in, m_out, FFTW_ESTIMATE);
    // fftwf_plan_dft_c2r_1d(n, m_in, m_out, FFTW_ESTIMATE);
    class Plan
    {
    public:
        int n_;
        int type_;

        //
        // real -> complex
        //
        fftwf_complex *c_; // (n_ / 2) + 1 of these
        float *r_;         // n_ of these
        fftwf_plan fwd_;   // forward plan
        fftwf_plan rev_;   // reverse plan

        //
        // complex -> complex
        //
        fftwf_complex *cc1_; // n
        fftwf_complex *cc2_; // n
        fftwf_plan cfwd_;    // forward plan
        fftwf_plan crev_;    // reverse plan

        // how much CPU time spent in FFTs that use this plan.
    #if TIMING
        double time_;
    #endif
        const char *why_;
        int uses_;
    }; // Plan

    FFTEngine(FFTEngine& other) = delete;
    void operator=(const FFTEngine &) = delete;
    static FFTEngine *GetInstance();

    Plan *get_plan(int n, const char *why);

    std::vector<std::complex<float>> one_fft(const std::vector<float> &samples, int i0, int block, const char *why, Plan *p);
    std::vector<float> one_ifft(const std::vector<std::complex<float>> &bins, const char *why);
    typedef std::vector<std::vector<std::complex<float>>> ffts_t;
    ffts_t ffts(const std::vector<float> &samples, int i0, int block, const char *why);
    std::vector<std::complex<float>> one_fft_c(const std::vector<float> &samples, int i0, int block, const char *why);
    std::vector<std::complex<float>> one_fft_cc(const std::vector<std::complex<float>> &samples, int i0, int block, const char *why);
    std::vector<std::complex<float>> one_ifft_cc(const std::vector<std::complex<float>> &bins, const char *why);
    std::vector<std::complex<float>> analytic(const std::vector<float> &x, const char *why);
    std::vector<float> hilbert_shift(const std::vector<float> &x, float hz0, float hz1, int rate);

protected:
    FFTEngine() :
        m_nplans(0)
    {}
    static FFTEngine *m_instance;

private:
    void fft_stats();
    QMutex m_plansmu;
    QMutex m_plansmu2;
    Plan *m_plans[1000];
    int m_nplans;
    // MEASURE=0, ESTIMATE=64, PATIENT=32
    static const int M_FFTW_TYPE = FFTW_ESTIMATE;
}; // FFTEngine

} // namespace FT8

#endif
