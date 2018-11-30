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
#include <algorithm>
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
    filterOpp   = new cmplx[flen];
	data		= new cmplx[flen];
	output		= new cmplx[flen2];
	ovlbuf		= new cmplx[flen2];

	memset(filter, 0, flen * sizeof(cmplx));
    memset(filterOpp, 0, flen * sizeof(cmplx));
	memset(data, 0, flen * sizeof(cmplx));
	memset(output, 0, flen2 * sizeof(cmplx));
	memset(ovlbuf, 0, flen2 * sizeof(cmplx));

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
	pass    = 0;
	window  = 0;
	init_filter();
	create_filter(f1, f2);
}

fftfilt::fftfilt(float f2, int len)
{
	flen	= len;
    pass    = 0;
    window  = 0;
	init_filter();
	create_dsb_filter(f2);
}

fftfilt::~fftfilt()
{
	if (fft) delete fft;

	if (filter) delete [] filter;
    if (filterOpp) delete [] filterOpp;
	if (data) delete [] data;
	if (output) delete [] output;
	if (ovlbuf) delete [] ovlbuf;
}

void fftfilt::create_filter(float f1, float f2)
{
	// initialize the filter to zero
	memset(filter, 0, flen * sizeof(cmplx));

	// create the filter shape coefficients by fft
	bool b_lowpass, b_highpass;
	b_lowpass = (f2 != 0);
	b_highpass = (f1 != 0);

	for (int i = 0; i < flen2; i++) {
		filter[i] = 0;
	// lowpass @ f2
		if (b_lowpass)
			filter[i] += fsinc(f2, i, flen2);
	// highighpass @ f1
		if (b_highpass)
			filter[i] -= fsinc(f1, i, flen2);
	}
	// highpass is delta[flen2/2] - h(t)
	if (b_highpass && f2 < f1)
		filter[flen2 / 2] += 1;

	for (int i = 0; i < flen2; i++)
		filter[i] *= _blackman(i, flen2);

	fft->ComplexFFT(filter); // filter was expressed in the time domain (impulse response)

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

// Double the size of FFT used for equivalent SSB filter or assume FFT is half the size of the one used for SSB
void fftfilt::create_dsb_filter(float f2)
{
	// initialize the filter to zero
	memset(filter, 0, flen * sizeof(cmplx));

	for (int i = 0; i < flen2; i++) {
		filter[i] = fsinc(f2, i, flen2);
		filter[i] *= _blackman(i, flen2);
	}

	fft->ComplexFFT(filter); // filter was expressed in the time domain (impulse response)

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

// Double the size of FFT used for equivalent SSB filter or assume FFT is half the size of the one used for SSB
// used with runAsym for in band / opposite band asymmetrical filtering. Can be used for vestigial sideband modulation.
void fftfilt::create_asym_filter(float fopp, float fin)
{
    // in band
    // initialize the filter to zero
    memset(filter, 0, flen * sizeof(cmplx));

    for (int i = 0; i < flen2; i++) {
        filter[i] = fsinc(fin, i, flen2);
        filter[i] *= _blackman(i, flen2);
    }

    fft->ComplexFFT(filter); // filter was expressed in the time domain (impulse response)

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

    // opposite band
    // initialize the filter to zero
    memset(filterOpp, 0, flen * sizeof(cmplx));

    for (int i = 0; i < flen2; i++) {
        filterOpp[i] = fsinc(fopp, i, flen2);
        filterOpp[i] *= _blackman(i, flen2);
    }

    fft->ComplexFFT(filterOpp); // filter was expressed in the time domain (impulse response)

    // normalize the output filter for unity gain
    scale = 0;
    for (int i = 0; i < flen2; i++) {
        mag = abs(filterOpp[i]);
        if (mag > scale) scale = mag;
    }
    if (scale != 0) {
        for (int i = 0; i < flen; i++)
            filterOpp[i] /= scale;
    }
}

// This filter is constructed directly from frequency domain response. Run with runFilt.
void fftfilt::create_rrc_filter(float fb, float a)
{
    std::fill(filter, filter+flen, 0);

    for (int i = 0; i < flen; i++) {
        filter[i] = frrc(fb, a, i, flen);
    }

    // normalize the output filter for unity gain
    float scale = 0, mag;
    for (int i = 0; i < flen; i++)
    {
        mag = abs(filter[i]);
        if (mag > scale) {
            scale = mag;
        }
    }
    if (scale != 0)
    {
        for (int i = 0; i < flen; i++) {
            filter[i] /= scale;
        }
    }
}

// test bypass
int fftfilt::noFilt(const cmplx & in, cmplx **out)
{
	data[inptr++] = in;
	if (inptr < flen2)
		return 0;
	inptr = 0;

	*out = data;
	return flen2;
}

// Filter with fast convolution (overlap-add algorithm).
int fftfilt::runFilt(const cmplx & in, cmplx **out)
{
	data[inptr++] = in;
	if (inptr < flen2)
		return 0;
	inptr = 0;

	fft->ComplexFFT(data);
	for (int i = 0; i < flen; i++)
		data[i] *= filter[i];

	fft->InverseComplexFFT(data);

	for (int i = 0; i < flen2; i++) {
		output[i] = ovlbuf[i] + data[i];
		ovlbuf[i] = data[flen2 + i];
	}
	memset (data, 0, flen * sizeof(cmplx));

	*out = output;
	return flen2;
}

// Second version for single sideband
int fftfilt::runSSB(const cmplx & in, cmplx **out, bool usb, bool getDC)
{
	data[inptr++] = in;
	if (inptr < flen2)
		return 0;
	inptr = 0;

	fft->ComplexFFT(data);

	// get or reject DC component
	data[0] = getDC ? data[0]*filter[0] : 0;

	// Discard frequencies for ssb
	if (usb)
	{
		for (int i = 1; i < flen2; i++) {
			data[i] *= filter[i];
			data[flen2 + i] = 0;
		}
	}
	else
	{
		for (int i = 1; i < flen2; i++) {
			data[i] = 0;
			data[flen2 + i] *= filter[flen2 + i];
		}
	}

	// in-place FFT: freqdata overwritten with filtered timedata
	fft->InverseComplexFFT(data);

	// overlap and add
	for (int i = 0; i < flen2; i++) {
		output[i] = ovlbuf[i] + data[i];
		ovlbuf[i] = data[i+flen2];
	}
	memset (data, 0, flen * sizeof(cmplx));

	*out = output;
	return flen2;
}

// Version for double sideband. You have to double the FFT size used for SSB.
int fftfilt::runDSB(const cmplx & in, cmplx **out, bool getDC)
{
	data[inptr++] = in;
	if (inptr < flen2)
		return 0;
	inptr = 0;

	fft->ComplexFFT(data);

	for (int i = 0; i < flen2; i++) {
		data[i] *= filter[i];
		data[flen2 + i] *= filter[flen2 + i];
	}

    // get or reject DC component
    data[0] = getDC ? data[0] : 0;

	// in-place FFT: freqdata overwritten with filtered timedata
	fft->InverseComplexFFT(data);

	// overlap and add
	for (int i = 0; i < flen2; i++) {
		output[i] = ovlbuf[i] + data[i];
		ovlbuf[i] = data[i+flen2];
	}

	memset (data, 0, flen * sizeof(cmplx));

	*out = output;
	return flen2;
}

// Version for asymmetrical sidebands. You have to double the FFT size used for SSB.
int fftfilt::runAsym(const cmplx & in, cmplx **out, bool usb)
{
    data[inptr++] = in;
    if (inptr < flen2)
        return 0;
    inptr = 0;

    fft->ComplexFFT(data);

    data[0] *= filter[0]; // always keep DC

    if (usb)
    {
        for (int i = 1; i < flen2; i++)
        {
            data[i] *= filter[i]; // usb
            data[flen2 + i] *= filterOpp[flen2 + i]; // lsb is the opposite
        }
    }
    else
    {
        for (int i = 1; i < flen2; i++)
        {
            data[i] *= filterOpp[i]; // usb is the opposite
            data[flen2 + i] *= filter[flen2 + i]; // lsb
        }
    }

    // in-place FFT: freqdata overwritten with filtered timedata
    fft->InverseComplexFFT(data);

    // overlap and add
    for (int i = 0; i < flen2; i++) {
        output[i] = ovlbuf[i] + data[i];
        ovlbuf[i] = data[i+flen2];
    }

    memset (data, 0, flen * sizeof(cmplx));

    *out = output;
    return flen2;
}

/* Sliding FFT from Fldigi */

struct sfft::vrot_bins_pair {
	cmplx vrot;
	cmplx bins;
} ;

sfft::sfft(int len)
{
	vrot_bins = new vrot_bins_pair[len];
	delay  = new cmplx[len];
	fftlen = len;
	first = 0;
	last = len - 1;
	ptr = 0;
	double phi = 0.0, tau = 2.0 * M_PI/ len;
	k2 = 1.0;
	for (int i = 0; i < len; i++) {
		vrot_bins[i].vrot = cmplx( K1 * cos (phi), K1 * sin (phi) );
		phi += tau;
		delay[i] = vrot_bins[i].bins = 0.0;
		k2 *= K1;
	}
}

sfft::~sfft()
{
	delete [] vrot_bins;
	delete [] delay;
}

// Sliding FFT, cmplx input, cmplx output
// FFT is computed for each value from first to last
// Values are not stable until more than "len" samples have been processed.
void sfft::run(const cmplx& input)
{
	cmplx & de = delay[ptr];
	const cmplx z( input.real() - k2 * de.real(), input.imag() - k2 * de.imag());
	de = input;

	if (++ptr >= fftlen)
		ptr = 0;

	for (vrot_bins_pair *itr = vrot_bins + first, *end = vrot_bins + last; itr != end ; ++itr)
		itr->bins = (itr->bins + z) * itr->vrot;
}

// Copies the frequencies to a pointer.
void sfft::fetch(float *result)
{
	for (vrot_bins_pair *itr = vrot_bins, *end = vrot_bins + last;  itr != end; ++itr, ++result)
		*result = itr->bins.real() * itr->bins.real()
                        + itr->bins.imag() * itr->bins.imag();
}

