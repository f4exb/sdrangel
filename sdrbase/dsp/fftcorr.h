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

#ifndef SDRBASE_DSP_FFTCORR_H_
#define SDRBASE_DSP_FFTCORR_H_

#include <complex>
#include "gfft.h"
#include "export.h"

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
    g_fft<float> *fftA;
    g_fft<float> *fftB;
    cmplx *dataA;  // from A input
    cmplx *dataB;  // from B input
    cmplx *dataBj; // conjugate of B
    cmplx *dataP;  // product of A with conjugate of B
    int inptrA;
    int inptrB;
    int outptr;
};


#endif /* SDRBASE_DSP_FFTCORR_H_ */
