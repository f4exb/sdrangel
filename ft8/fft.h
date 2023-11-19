///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2020, 2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
// reformatted and adapted to Qt and SDRangel context                            //
//                                                                               //
// Caution: this is intentionally not thread safe and one such engine should     //
// be allocated by thread. Due to optimization of FFT buffers these buffers are  //
// not shared among threads.                                                     //
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

#include "export.h"

namespace FT8
{
    class FFTBuffers;

class FT8_API FFTEngine
{
public:
    std::vector<std::complex<float>> one_fft(const std::vector<float> &samples, int i0, int block);
    std::vector<float> one_ifft(const std::vector<std::complex<float>> &bins);
    typedef std::vector<std::vector<std::complex<float>>> ffts_t;
    ffts_t ffts(const std::vector<float> &samples, int i0, int block);
    std::vector<std::complex<float>> one_fft_c(const std::vector<float> &samples, int i0, int block);
    std::vector<std::complex<float>> one_fft_cc(const std::vector<std::complex<float>> &samples, int i0, int block);
    std::vector<std::complex<float>> one_ifft_cc(const std::vector<std::complex<float>> &bins);
    std::vector<float> hilbert_shift(const std::vector<float> &x, float hz0, float hz1, int rate);

    FFTEngine();
    ~FFTEngine();

private:
    std::vector<std::complex<float>> analytic(const std::vector<float> &x);
    FFTBuffers *m_fftBuffers;
}; // FFTEngine

} // namespace FT8

#endif
