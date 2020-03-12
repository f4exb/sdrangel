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

#ifndef SDRBASE_DSP_FFTCORR2_H_
#define SDRBASE_DSP_FFTCORR2_H_

#include <complex>

#include "dsp/fftwindow.h"
#include "export.h"

class FFTEngine;

class SDRBASE_API fftcorr {
public:
    typedef std::complex<float> cmplx;
    fftcorr(int len);
    ~fftcorr();

    int run(const cmplx& inA, const cmplx* inB, cmplx **out); //!< if inB = 0 then run auto-correlation
    const cmplx& run(const cmplx& inA, const cmplx* inB);

private:
    void init_fft();
    int flen;  //!< FFT length
    int flen2; //!< half FFT length
    FFTEngine *fftA;
    FFTEngine *fftB;
    FFTEngine *fftInvA;
    unsigned int fftASequence;
    unsigned int fftBSequence;
    unsigned int fftInvASequence;
    FFTWindow m_window;
    cmplx *dataA;  // from A input
    cmplx *dataB;  // from B input
    cmplx *dataBj; // conjugate of B
    cmplx *dataP;  // product of A with conjugate of B
    int inptrA;
    int inptrB;
    int outptr;
};


#endif /* SDRBASE_DSP_FFTCORR2_H_ */
