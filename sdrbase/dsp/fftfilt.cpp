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
//
// Augmented with more filter types
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB
// ----------------------------------------------------------------------------

#include <memory.h>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <typeinfo>
#include <array>

#include <stdio.h>
#include <sys/types.h>

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

	std::fill(filter, filter + flen, cmplx{0, 0});
    std::fill(filterOpp, filterOpp + flen, cmplx{0, 0});
	std::fill(data, data + flen , cmplx{0, 0});
	std::fill(output, output + flen2, cmplx{0, 0});
	std::fill(ovlbuf, ovlbuf + flen2, cmplx{0, 0});

	inptr = 0;
}

//------------------------------------------------------------------------------
// fft filter
// f1 < f2 ==> band pass filter
// f1 > f2 ==> band reject filter
// f1 == 0 ==> low pass filter
// f2 == 0 ==> high pass filter
//------------------------------------------------------------------------------
fftfilt::fftfilt(int len)
{
	flen	= len;
	pass    = 0;
	window  = 0;
	init_filter();
}

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

void fftfilt::create_filter(float f1, float f2, FFTWindow::Function wf)
{
	// initialize the filter to zero
	std::fill(filter, filter + flen, cmplx{0, 0});

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

    FFTWindow fwin;
    fwin.create(wf, flen2);
    fwin.apply(filter);

	// for (int i = 0; i < flen2; i++)
	// 	filter[i] *= _blackman(i, flen2);

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

void fftfilt::create_filter(const std::vector<std::pair<float, float>>& limits, bool pass, FFTWindow::Function wf)
{
    std::vector<int> canvasNeg(flen2, pass ? 0 : 1); // initialize the negative frequencies filter canvas
    std::vector<int> canvasPos(flen2, pass ? 0 : 1); // initialize the positive frequencies filter canvas
	std::fill(filter, filter + flen, cmplx{0, 0}); // initialize the positive filter to zero
    std::fill(filterOpp, filterOpp + flen, cmplx{0, 0}); // initialize the negative filter to zero

    for (const auto& fs : limits)
    {
        const float& f1 = fs.first + 0.5;
        const float& w = fs.second > 0.0 ? fs.second : 0.0;
        const float& f2 = f1 + w;

        for (int i = 0; i < flen; i++)
        {
            if (pass) // pass
            {
                if ((i >= f1*flen) && (i <= f2*flen))
                {
                    if (i < flen2) {
                        canvasNeg[flen2-1-i] = 1;
                    } else {
                        canvasPos[i-flen2] = 1;
                    }
                }
            }
            else // reject
            {
                if ((i >= f1*flen) && (i <= f2*flen))
                {
                    if (i < flen2) {
                        canvasNeg[flen2-1-i] = 0;
                    } else {
                        canvasPos[i-flen2] = 0;
                    }
                }
            }
        }
    }

    std::vector<std::pair<int,int>> indexesNegList;
    std::vector<std::pair<int,int>> indexesPosList;
    int cn = 0;
    int cp = 0;
    int defaultSecond = pass ? 0 : flen2 - 1;

    for (int i = 0; i < flen2; i++)
    {
        if ((canvasNeg[i] == 1) && (cn == 0)) {
            indexesNegList.push_back(std::pair<int,int>{i, defaultSecond});
        }

        if ((canvasNeg[i] == 0) && (cn == 1)) {
            indexesNegList.back().second = i;
        }

        if ((canvasPos[i] == 1) && (cp == 0)) {
            indexesPosList.push_back(std::pair<int,int>{i, defaultSecond});
        }

        if ((canvasPos[i] == 0) && (cp == 1)) {
            indexesPosList.back().second = i;
        }

        cn = canvasNeg[i];
        cp = canvasPos[i];
    }

    for (const auto& indexes : indexesPosList)
    {
        const float f1 = indexes.first / (float) flen;
        const float f2 = indexes.second / (float) flen;

        for (int i = 0; i < flen2; i++)
        {
            if (f2 != 0) {
                filter[i] += fsinc(f2, i, flen2);
            }
            if (f1 != 0) {
                filter[i] -= fsinc(f1, i, flen2);
            }
        }

        if (f2 == 0 && f1 != 0) {
            filter[flen2 / 2] += 1;
        }
    }

    for (const auto& indexes : indexesNegList)
    {
        const float f1 = indexes.first / (float) flen;
        const float f2 = indexes.second / (float) flen;

        for (int i = 0; i < flen2; i++)
        {
            if (f2 != 0) {
                filterOpp[i] += fsinc(f2, i, flen2);
            }
            if (f1 != 0) {
                filterOpp[i] -= fsinc(f1, i, flen2);
            }
        }

        if (f2 == 0 && f1 != 0) {
            filterOpp[flen2 / 2] += 1;
        }
    }

    FFTWindow fwin;
    fwin.create(wf, flen2);
    fwin.apply(filter);
    fwin.apply(filterOpp);

	fft->ComplexFFT(filter); // filter was expressed in the time domain (impulse response)
    fft->ComplexFFT(filterOpp); // filter was expressed in the time domain (impulse response)

    float scalen = 0, scalep = 0, magn, magp; // normalize the output filter for unity gain

	for (int i = 0; i < flen2; i++)
    {
		magp = abs(filter[i]);

        if (magp > scalep) {
            scalep = magp;
        }

        magn = abs(filterOpp[i]);

        if (magn > scalen) {
            scalen = magn;
        }
	}

    if (scalep != 0)
    {
        std::for_each(
            filter,
            filter + flen,
            [scalep](fftfilt::cmplx& s) { s /= scalep; }
        );
	}

    if (scalen != 0)
    {
        std::for_each(
            filterOpp,
            filterOpp + flen,
            [scalen](fftfilt::cmplx& s) { s /= scalen; }
        );
	}
}

// Double the size of FFT used for equivalent SSB filter or assume FFT is half the size of the one used for SSB
void fftfilt::create_dsb_filter(float f2, FFTWindow::Function wf)
{
	// initialize the filter to zero
	std::fill(filter, filter + flen, cmplx{0, 0});

	for (int i = 0; i < flen2; i++) {
		filter[i] = fsinc(f2, i, flen2);
		// filter[i] *= _blackman(i, flen2);
	}

    FFTWindow fwin;
    fwin.create(wf, flen2);
    fwin.apply(filter);

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
void fftfilt::create_asym_filter(float fopp, float fin, FFTWindow::Function wf)
{
    // in band
    // initialize the filter to zero
    std::fill(filter, filter + flen, cmplx{0, 0});

    for (int i = 0; i < flen2; i++) {
        filter[i] = fsinc(fin, i, flen2);
        // filter[i] *= _blackman(i, flen2);
    }

    FFTWindow fwin;
    fwin.create(wf, flen2);
    fwin.apply(filter);

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
    std::fill(filterOpp, filterOpp + flen, cmplx{0, 0});

    for (int i = 0; i < flen2; i++) {
        filterOpp[i] = fsinc(fopp, i, flen2);
        // filterOpp[i] *= _blackman(i, flen2);
    }

    fwin.apply(filterOpp);
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
	std::fill(data, data + flen , cmplx{0, 0});

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
	std::fill(data, data + flen , cmplx{0, 0});

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

	std::fill(data, data + flen , cmplx{0, 0});

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

    std::fill(data, data + flen , cmplx{0, 0});

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

