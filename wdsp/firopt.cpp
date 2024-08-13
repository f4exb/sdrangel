/*  firmin.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2016 Warren Pratt, NR0V
Copyright (C) 2024 Edouard Griffiths, F4EXB Adapted to SDRangel

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

warren@wpratt.com

*/

#include "comm.hpp"
#include "fir.hpp"
#include "firopt.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                               Standalone Partitioned Overlap-Save Bandpass                            *
*                                                                                                       *
********************************************************************************************************/

void FIROPT::plan()
{
    // must call for change in 'nc', 'size', 'out'
    nfor = nc / size;
    buffidx = 0;
    idxmask = nfor - 1;
    fftin.resize(2 * size * 2);
    fftout.resize(nfor);
    fmask.resize(nfor);
    maskgen.resize(2 * size * 2);
    pcfor.resize(nfor);
    maskplan.resize(nfor);
    for (int i = 0; i < nfor; i++)
    {
        fftout[i].resize(2 * size * 2);
        fmask[i].resize(2 * size * 2);
        pcfor[i] = fftwf_plan_dft_1d(
            2 * size,
            (fftwf_complex *)fftin.data(),
            (fftwf_complex *)fftout[i].data(),
            FFTW_FORWARD,
            FFTW_PATIENT
        );
        maskplan[i] = fftwf_plan_dft_1d(
            2 * size,
            (fftwf_complex *)maskgen.data(),
            (fftwf_complex *)fmask[i].data(),
            FFTW_FORWARD,
            FFTW_PATIENT
        );
    }
    accum.resize(2 * size * 2);
    crev = fftwf_plan_dft_1d(
        2 * size,
        (fftwf_complex *)accum.data(),
        (fftwf_complex *)out,
        FFTW_BACKWARD,
        FFTW_PATIENT
    );
}

void FIROPT::calc()
{
    // call for change in frequency, rate, wintype, gain
    // must also call after a call to plan_firopt()
    std::vector<float> impulse;
    FIR::fir_bandpass (impulse, nc, f_low, f_high, samplerate, wintype, 1, gain);
    buffidx = 0;
    for (int i = 0; i < nfor; i++)
    {
        // I right-justified the impulse response => take output from left side of output buff, discard right side
        // Be careful about flipping an asymmetrical impulse response.
        std::copy(&(impulse[2 * size * i]), &(impulse[2 * size * i]) + size * 2, &(maskgen[2 * size]));
        fftwf_execute (maskplan[i]);
    }
}

FIROPT::FIROPT(
    int _run,
    int _position,
    int _size,
    float* _in,
    float* _out,
    int _nc,
    float _f_low,
    float _f_high,
    int _samplerate,
    int _wintype,
    float _gain
)
{
    run = _run;
    position = _position;
    size = _size;
    in = _in;
    out = _out;
    nc = _nc;
    f_low = _f_low;
    f_high = _f_high;
    samplerate = (float) _samplerate;
    wintype = _wintype;
    gain = _gain;
    plan();
    calc();
}

void FIROPT::deplan()
{
    fftwf_destroy_plan (crev);
    for (int i = 0; i < nfor; i++)
    {
        fftwf_destroy_plan (pcfor[i]);
        fftwf_destroy_plan (maskplan[i]);
    }
}

FIROPT::~FIROPT()
{
    deplan();
}

void FIROPT::flush()
{
    std::fill(fftin.begin(), fftin.end(), 0);
    for (int i = 0; i < nfor; i++)
        std::fill(fftout[i].begin(), fftout[i].end(), 0);
    buffidx = 0;
}

void FIROPT::execute(int pos)
{
    if (run && (position == pos))
    {
        int k;
        std::copy(in, in + size * 2, &(fftin[2 * size]));
        fftwf_execute (pcfor[buffidx]);
        k = buffidx;
        std::fill(accum.begin(), accum.end(), 0);
        for (int j = 0; j < nfor; j++)
        {
            for (int i = 0; i < 2 * size; i++)
            {
                accum[2 * i + 0] += fftout[k][2 * i + 0] * fmask[j][2 * i + 0] - fftout[k][2 * i + 1] * fmask[j][2 * i + 1];
                accum[2 * i + 1] += fftout[k][2 * i + 0] * fmask[j][2 * i + 1] + fftout[k][2 * i + 1] * fmask[j][2 * i + 0];
            }
            k = (k + idxmask) & idxmask;
        }
        buffidx = (buffidx + 1) & idxmask;
        fftwf_execute (crev);
        std::copy(&(fftin[2 * size]), &(fftin[2 * size]) + size * 2, fftin.begin());
    }
    else if (in != out)
        std::copy( in,  in + size * 2, out);
}

void FIROPT::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
    deplan();
    plan();
    calc();
}

void FIROPT::setSamplerate(int _rate)
{
    samplerate = (float) _rate;
    calc();
}

void FIROPT::setSize(int _size)
{
    size = _size;
    deplan();
    plan();
    calc();
}

void FIROPT::setFreqs(float _f_low, float _f_high)
{
    f_low = _f_low;
    f_high = _f_high;
    calc();
}


} // namespace WDSP
