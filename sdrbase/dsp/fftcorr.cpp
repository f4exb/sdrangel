///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// FFT based cross correlation                                                   //
//                                                                               //
// See: http://liquidsdr.org/blog/pll-howto/                                     //
// Fixed filter registers saturation                                             //
// Added order for PSK locking. This brilliant idea actually comes from this     //
// post: https://www.dsprelated.com/showthread/comp.dsp/36356-1.php              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include "fftcorr.h"

void fftcorr::init_fft()
{
    fftA = new g_fft<float>(flen);
    fftB = new g_fft<float>(flen);

    dataA   = new cmplx[flen];
    dataB   = new cmplx[flen];
    dataBj  = new cmplx[flen];
    dataP   = new cmplx[flen];

    std::fill(dataA, dataA+flen, 0);
    std::fill(dataB, dataB+flen, 0);

    inptrA = 0;
    inptrB = 0;
    outptr = 0;
}

fftcorr::fftcorr(int len) : flen(len), flen2(len>>1)
{
    init_fft();
}

fftcorr::~fftcorr()
{
    delete fftA;
    delete fftB;
    delete[] dataA;
    delete[] dataB;
    delete[] dataBj;
    delete[] dataP;
}

int fftcorr::run(const cmplx& inA, const cmplx* inB, cmplx **out)
{
    dataA[inptrA++] = inA;

    if (inB) {
        dataB[inptrB++] = *inB;
    }

    if (inptrA < flen2) {
        return 0;
    }

    fftA->ComplexFFT(dataA);

    if (inB) {
        fftB->ComplexFFT(dataB);
    }

    if (inB) {
        std::transform(dataB, dataB+flen, dataBj, [](const cmplx& c) -> cmplx { return std::conj(c); });
    } else {
        std::transform(dataA, dataA+flen, dataBj, [](const cmplx& c) -> cmplx { return std::conj(c); });
    }

    std::transform(dataA, dataA+flen, dataBj, dataP, [](const cmplx& a, const cmplx& b) -> cmplx { return a*b; });

    fftA->InverseComplexFFT(dataP);

    std::fill(dataA, dataA+flen, 0);
    inptrA = 0;

    if (inB)
    {
        std::fill(dataB, dataB+flen, 0);
        inptrB = 0;
    }

    *out = dataP;
    return flen2;
}

const fftcorr::cmplx& fftcorr::run(const cmplx& inA, const cmplx* inB)
{
    cmplx *dummy;

    if (run(inA, inB, &dummy)) {
        outptr = 0;
    }

    return dataP[outptr++];
}
