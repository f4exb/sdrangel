///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// FFT based cross correlation. Uses FFTW/Kiss engine.                           //
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

#include <algorithm>

#include "dsp/fftengine.h"
#include "fftcorr2.h"

void fftcorr2::init_fft()
{
    fftA->configure(flen, false);
    fftB->configure(flen, false);
    fftInvA->configure(flen, true);
    m_window.create(FFTWindow::Hamming, flen);

    dataA    = new cmplx[flen];
    dataB    = new cmplx[flen];
    dataBj   = new cmplx[flen];
    dataP    = new cmplx[flen];

    std::fill(dataA, dataA+flen, 0);
    std::fill(dataB, dataB+flen, 0);

    inptrA = 0;
    inptrB = 0;
    outptr = 0;
}

fftcorr2::fftcorr2(int len) :
    flen(len),
    flen2(len>>1),
    fftA(FFTEngine::create()),
    fftB(FFTEngine::create()),
    fftInvA(FFTEngine::create())
{
    init_fft();
}

fftcorr2::~fftcorr2()
{
    delete fftA;
    delete fftB;
    delete[] dataA;
    delete[] dataB;
    delete[] dataBj;
    delete[] dataP;
}

int fftcorr2::run(const cmplx& inA, const cmplx* inB, cmplx **out)
{
    dataA[inptrA++] = inA;

    if (inB) {
        dataB[inptrB++] = *inB;
    }

    if (inptrA < flen2) {
        return 0;
    }

    m_window.apply(dataA, fftA->in());
    fftA->transform();

    if (inB)
    {
        m_window.apply(dataB, fftB->in());
        fftB->transform();
    }

    if (inB) {
        std::transform(fftB->out(), fftB->out()+flen, dataBj, [](const cmplx& c) -> cmplx { return std::conj(c); });
    } else {
        std::transform(fftA->out(), fftA->out()+flen, dataBj, [](const cmplx& c) -> cmplx { return std::conj(c); });
    }

    std::transform(fftA->out(), fftA->out()+flen, dataBj, fftInvA->in(), [](const cmplx& a, const cmplx& b) -> cmplx { return a*b; });

    fftInvA->transform();
    std::copy(fftInvA->out(), fftInvA->out()+flen, dataP);

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

const fftcorr2::cmplx& fftcorr2::run(const cmplx& inA, const cmplx* inB)
{
    cmplx *dummy;

    if (run(inA, inB, &dummy)) {
        outptr = 0;
    }

    return dataP[outptr++];
}
