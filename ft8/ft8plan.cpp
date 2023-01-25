//////////////////////////////////////////////////////////////////////////////////
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
#include "ft8plan.h"

namespace FT8
{

Plan::Plan(int n)
{
    n_ = n;

    r_ = (float *) fftwf_malloc(n * sizeof(float));
    c_ = (fftwf_complex *) fftwf_malloc(((n / 2) + 1) * sizeof(fftwf_complex));
    cc1_ = (fftwf_complex *)fftwf_malloc(n * sizeof(fftwf_complex));
    cc2_ = (fftwf_complex *)fftwf_malloc(n * sizeof(fftwf_complex));
    //
    // real -> complex
    //
    // FFTW_ESTIMATE
    // FFTW_MEASURE
    // FFTW_PATIENT
    // FFTW_EXHAUSTIVE
    int type = M_FFTW_TYPE;
    type_ = type;
    fwd_ = fftwf_plan_dft_r2c_1d(n, r_, c_, type);
    rev_ = fftwf_plan_dft_c2r_1d(n, c_, r_, type);

    //
    // complex -> complex
    //
    cfwd_ = fftwf_plan_dft_1d(n, cc1_, cc2_, FFTW_FORWARD, type);
    crev_ = fftwf_plan_dft_1d(n, cc2_, cc1_, FFTW_BACKWARD, type);
}

Plan::~Plan()
{
    fftwf_destroy_plan(fwd_);
    fftwf_destroy_plan(rev_);
    fftwf_destroy_plan(cfwd_);
    fftwf_destroy_plan(crev_);
    fftwf_free(r_);
    fftwf_free(c_);
    fftwf_free(cc1_);
    fftwf_free(cc2_);
}

} // namesoace FT8
