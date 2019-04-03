/*
  Copyright (C) 2018 James C. Ahlstrom

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>

#include "freedv_filter.h"
#include "freedv_filter_coef.h"

#include "fdv_arm_math.h"

#define cmplx(value) (std::complex<float>{cos(value), sin(value)})

namespace FreeDV
{

/*
 * This is a library of filter functions. They were copied from Quisk and converted to single precision.
 */

/*---------------------------------------------------------------------------*\

  FUNCTIONS...: quisk_filt_cfInit
  AUTHOR......: Jim Ahlstrom
  DATE CREATED: 27 August 2015
  MODIFIED: 4 June 2018

  Initialize a FIR filter that has complex samples, and either real or complex coefficients.

\*---------------------------------------------------------------------------*/

void quisk_filt_cfInit(struct quisk_cfFilter * filter, float * coefs, int taps) {
    // Prepare a new filter using coefs and taps.  Samples are complex. Coefficients can
    // be real or complex.
    filter->dCoefs = coefs;
    filter->cpxCoefs = NULL;
    filter->cSamples = (std::complex<float> *) malloc(taps * sizeof(std::complex<float>));
    memset(filter->cSamples, 0, taps * sizeof(std::complex<float>));
    filter->ptcSamp = filter->cSamples;
    filter->nTaps = taps;
    filter->cBuf = NULL;
    filter->nBuf = 0;
    filter->decim_index = 0;
}

/*---------------------------------------------------------------------------*\

  FUNCTIONS...: quisk_filt_destroy
  AUTHOR......: Jim Ahlstrom
  DATE CREATED: 27 August 2015
  MODIFIED: 4 June 2018

  Destroy the FIR filter and free all resources.

\*---------------------------------------------------------------------------*/

void quisk_filt_destroy(struct quisk_cfFilter * filter) {
    if (filter->cSamples) {
        free(filter->cSamples);
        filter->cSamples = NULL;
    }

    if (filter->cBuf) {
        free(filter->cBuf);
        filter->cBuf = NULL;
    }

    if (filter->cpxCoefs) {
        free(filter->cpxCoefs);
        filter->cpxCoefs = NULL;
    }
}

/*---------------------------------------------------------------------------*\

  FUNCTIONS...: quisk_cfInterpDecim
  AUTHOR......: Jim Ahlstrom
  DATE CREATED: 27 August 2015
  MODIFIED: 4 June 2018

  Take an array of samples cSamples of length count, multiply the sample rate
  by interp, and then divide the sample rate by decim.  Return the new number
  of samples.  Each specific interp and decim will require its own custom
  low pass FIR filter with real coefficients.

\*---------------------------------------------------------------------------*/

int quisk_cfInterpDecim(std::complex<float> * cSamples, int count, struct quisk_cfFilter * filter, int interp, int decim) {
    // Interpolate by interp, and then decimate by decim.
    // This uses the float coefficients of filter (not the complex).  Samples are complex.
    int i, k, nOut;
    float * ptCoef;
    std::complex<float> *ptSample;
    std::complex<float> csample;

    if (count > filter->nBuf) {    // increase size of sample buffer
        filter->nBuf = count * 2;

        if (filter->cBuf)
            free(filter->cBuf);

        filter->cBuf = (std::complex<float> *) malloc(filter->nBuf * sizeof(std::complex<float>));
    }

    memcpy(filter->cBuf, cSamples, count * sizeof(std::complex<float>));
    nOut = 0;

    for (i = 0; i < count; i++) {
        // Put samples into buffer left to right.  Use samples right to left.
        *filter->ptcSamp = filter->cBuf[i];

        while (filter->decim_index < interp) {
            ptSample = filter->ptcSamp;
            ptCoef = filter->dCoefs + filter->decim_index;
            csample = 0;

            for (k = 0; k < filter->nTaps / interp; k++, ptCoef += interp) {
                csample += *ptSample * *ptCoef;

                if (--ptSample < filter->cSamples)
                    ptSample = filter->cSamples + filter->nTaps - 1;
            }

            cSamples[nOut] = csample * (float) interp;
            nOut++;
            filter->decim_index += decim;
        }

        if (++filter->ptcSamp >= filter->cSamples + filter->nTaps)
            filter->ptcSamp = filter->cSamples;

        filter->decim_index = filter->decim_index - interp;
    }

    return nOut;
}

/*---------------------------------------------------------------------------*\

  FUNCTIONS...: quisk_ccfInterpDecim
  AUTHOR......: Jim Ahlstrom
  DATE CREATED: 7 June 2018

  Take an array of samples cSamples of length count, multiply the sample rate
  by interp, and then divide the sample rate by decim.  Return the new number
  of samples.  Each specific interp and decim will require its own custom
  low pass FIR filter with complex coefficients. This filter can be tuned.

  This filter is not currently used.

\*---------------------------------------------------------------------------*/
#if 0
int quisk_ccfInterpDecim(complex float * cSamples, int count, struct quisk_cfFilter * filter, int interp, int decim) {
    // Interpolate by interp, and then decimate by decim.
    // This uses the complex coefficients of filter (not the real).  Samples are complex.
    int i, k, nOut;
    complex float * ptCoef;
    complex float * ptSample;
    complex float csample;

    if (count > filter->nBuf) {    // increase size of sample buffer
        filter->nBuf = count * 2;
        if (filter->cBuf)
            FREE(filter->cBuf);
        filter->cBuf = (complex float *)MALLOC(filter->nBuf * sizeof(complex float));
    }

    memcpy(filter->cBuf, cSamples, count * sizeof(complex float));
    nOut = 0;

    for (i = 0; i < count; i++) {
        // Put samples into buffer left to right.  Use samples right to left.
        *filter->ptcSamp = filter->cBuf[i];

        while (filter->decim_index < interp) {
            ptSample = filter->ptcSamp;
            ptCoef = filter->cpxCoefs + filter->decim_index;
            csample = 0;

            for (k = 0; k < filter->nTaps / interp; k++, ptCoef += interp) {
                csample += *ptSample * *ptCoef;

                if (--ptSample < filter->cSamples)
                    ptSample = filter->cSamples + filter->nTaps - 1;
            }

            cSamples[nOut] = csample * interp;
            nOut++;
            filter->decim_index += decim;
        }

        if (++filter->ptcSamp >= filter->cSamples + filter->nTaps)
            filter->ptcSamp = filter->cSamples;

        filter->decim_index = filter->decim_index - interp;
    }

    return nOut;
}
#endif

/*---------------------------------------------------------------------------*\

  FUNCTIONS...: quisk_cfTune
  AUTHOR......: Jim Ahlstrom
  DATE CREATED: 4 June 2018

  Tune a low pass filter with float coefficients into an analytic I/Q bandpass filter
  with complex coefficients.  The "freq" is the center frequency / sample rate.
  If the float coefs represent a low pass filter with bandwidth 1 kHz, the new bandpass
  filter has width 2 kHz. The filter can be re-tuned repeatedly.

\*---------------------------------------------------------------------------*/

void quisk_cfTune(struct quisk_cfFilter * filter, float freq) {
    float D, tune;
    int i;

    if ( ! filter->cpxCoefs)
        filter->cpxCoefs = (std::complex<float> *) malloc(filter->nTaps * sizeof(std::complex<float>));

    tune = 2.0 * M_PI * freq;
    D = (filter->nTaps - 1.0) / 2.0;

    for (i = 0; i < filter->nTaps; i++) {
        float tval = tune * (i - D);
        filter->cpxCoefs[i] = cmplx(tval) * filter->dCoefs[i];
    }
}

/*---------------------------------------------------------------------------*\

  FUNCTIONS...: quisk_ccfFilter
  AUTHOR......: Jim Ahlstrom
  DATE CREATED: 4 June 2018

  Filter complex samples using complex coefficients. The inSamples and outSamples may be
  the same array. The loop runs forward over coefficients but backwards over samples.
  Therefore, the coefficients must be reversed unless they are created by quisk_cfTune.
  Low pass filter coefficients are symmetrical, so this does not usually matter.

\*---------------------------------------------------------------------------*/

void quisk_ccfFilter(std::complex<float> *inSamples, std::complex<float> *outSamples, int count, struct quisk_cfFilter * filter) {
    int i, k;
    std::complex<float> *ptSample;
    std::complex<float> *ptCoef;
    std::complex<float> accum;

    for (i = 0; i < count; i++) {
        *filter->ptcSamp = inSamples[i];
        accum = 0;
        ptSample = filter->ptcSamp;
        ptCoef = filter->cpxCoefs;

        for (k = 0; k < filter->nTaps; k++, ptCoef++) {
            accum += *ptSample  *  *ptCoef;

            if (--ptSample < filter->cSamples)
                ptSample = filter->cSamples + filter->nTaps - 1;
        }

        outSamples[i] = accum;

        if (++filter->ptcSamp >= filter->cSamples + filter->nTaps)
            filter->ptcSamp = filter->cSamples;
    }
}

} // freeDV
