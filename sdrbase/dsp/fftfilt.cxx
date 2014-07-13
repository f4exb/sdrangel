// ----------------------------------------------------------------------------
//	fftfilt.cxx  --  Fast convolution Overlap-Add filter
//
// Filter implemented using overlap-add FFT convolution method
// h(t) characterized by Windowed-Sinc impulse response
//
// Reference:
//	 "The Scientist and Engineer's Guide to Digital Signal Processing"
//	 by Dr. Steven W. Smith, http://www.dspguide.com
//	 Chapters 16, 18 and 21
//
// Copyright (C) 2006-2008 Dave Freese, W1HKJ
//
// This file is part of fldigi.
//
// Fldigi is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Fldigi is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with fldigi.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#include <memory.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <typeinfo>

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <memory.h>

#include <dsp/misc.h>
#include <dsp/fftfilt.h>

//------------------------------------------------------------------------------
// initialize the filter
// create forward and reverse FFTs
//------------------------------------------------------------------------------

// Only need a single instance of g_fft, used for both forward and reverse
void fftfilt::init_filter()
{
	flen2	= flen >> 1;
	fft	= new g_fft<float>(flen);

	filter		= new cmplx[flen];
	timedata	= new cmplx[flen];
	freqdata	= new cmplx[flen];
	output		= new cmplx[flen];
	ovlbuf		= new cmplx[flen2];
	ht		= new cmplx[flen];

	memset(filter, 0, flen * sizeof(cmplx));
	memset(timedata, 0, flen * sizeof(cmplx));
	memset(freqdata, 0, flen * sizeof(cmplx));
	memset(output, 0, flen * sizeof(cmplx));
	memset(ovlbuf, 0, flen2 * sizeof(cmplx));
	memset(ht, 0, flen * sizeof(cmplx));

	inptr = 0;
}

//------------------------------------------------------------------------------
// fft filter
// f1 < f2 ==> band pass filter
// f1 > f2 ==> band reject filter
// f1 == 0 ==> low pass filter
// f2 == 0 ==> high pass filter
//------------------------------------------------------------------------------
fftfilt::fftfilt(float f1, float f2, int len)
{
	flen	= len;
	init_filter();
	create_filter(f1, f2);
}

fftfilt::~fftfilt()
{
	if (fft) delete fft;

	if (filter) delete [] filter;
	if (timedata) delete [] timedata;
	if (freqdata) delete [] freqdata;
	if (output) delete [] output;
	if (ovlbuf) delete [] ovlbuf;
	if (ht) delete [] ht;
}

void fftfilt::create_filter(float f1, float f2)
{
// initialize the filter to zero
	memset(ht, 0, flen * sizeof(cmplx));

// create the filter shape coefficients by fft
// filter values initialized to the ht response h(t)
	bool b_lowpass, b_highpass;//, window;
	b_lowpass = (f2 != 0);
	b_highpass = (f1 != 0);

	for (int i = 0; i < flen2; i++) {
		ht[i] = 0;
//combine lowpass / highpass
// lowpass @ f2
		if (b_lowpass) ht[i] += fsinc(f2, i, flen2);
// highighpass @ f1
		if (b_highpass) ht[i] -= fsinc(f1, i, flen2);
	}
// highpass is delta[flen2/2] - h(t)
	if (b_highpass && f2 < f1) ht[flen2 / 2] += 1;

	for (int i = 0; i < flen2; i++)
		ht[i] *= _blackman(i, flen2);

// this may change since green fft is in place fft
	memcpy(filter, ht, flen * sizeof(cmplx));

// ht is flen complex points with imaginary all zero
// first half describes h(t), second half all zeros
// perform the cmplx forward fft to obtain H(w)
// filter is flen/2 complex values

	fft->ComplexFFT(filter);

// normalize the output filter for unity gain
	float scale = 0, mag;
	for (int i = 0; i < flen2; i++) {
		mag = abs(filter[i]);
		if (mag > scale) scale = mag;
	}
	if (scale != 0) {
		for (int i = 0; i < flen; i++)
			filter[i] /= scale;
	}
}

// Filter with fast convolution (overlap-add algorithm).
int fftfilt::run(const cmplx & in, cmplx **out, bool usb)
{
	timedata[inptr++] = in;

	if (inptr < flen2)
		return 0;
	inptr = 0;

	memcpy(freqdata, timedata, flen * sizeof(cmplx));
	fft->ComplexFFT(freqdata);

	// Discard frequencies for ssb
	if ( usb )
		for (int i = 0; i < flen2; i++) {
			freqdata[i] *= filter[i];
			freqdata[flen2 + i] = 0;
		}
	else
		for (int i = 0; i < flen2; i++) {
			freqdata[i] = 0;
			freqdata[flen2 + i] *= filter[flen2 + i];
		}

	// in-place FFT: freqdata overwritten with filtered timedata
	fft->InverseComplexFFT(freqdata);

	// overlap and add
	for (int i = 0; i < flen2; i++) {
		output[i] = ovlbuf[i] + freqdata[i];
		ovlbuf[i] = freqdata[i+flen2];
	}

	*out = output;
	return flen2;
}

