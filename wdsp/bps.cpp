/*  bandpass.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2016, 2017 Warren Pratt, NR0V
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
#include "bps.hpp"
#include "fir.hpp"
#include "bandpass.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                       Overlap-Save Bandpass                                           *
*                                                                                                       *
********************************************************************************************************/

void BPS::calc()
{
    infilt.resize(2 * size * 2);
    product.resize(2 * size * 2);
    std::vector<float> impulse;
    FIR::fir_bandpass(
        impulse,
        size + 1,
        f_low,
        f_high,
        samplerate,
        wintype,
        1,
        1.0 / (float)(2 * size)
    );
    FIR::fftcv_mults(mults, 2 * size, impulse.data());
    CFor = fftwf_plan_dft_1d(2 * size, (fftwf_complex *) infilt.data(), (fftwf_complex *) product.data(), FFTW_FORWARD, FFTW_PATIENT);
    CRev = fftwf_plan_dft_1d(2 * size, (fftwf_complex *) product.data(), (fftwf_complex *) out, FFTW_BACKWARD, FFTW_PATIENT);
}

void BPS::decalc()
{
    fftwf_destroy_plan(CRev);
    fftwf_destroy_plan(CFor);
}

BPS::BPS(
    int _run,
    int _position,
    int _size,
    float* _in,
    float* _out,
    double _f_low,
    double _f_high,
    int _samplerate,
    int _wintype,
    double _gain
) :
    run(_run),
    position(_position),
    size(_size),
    in(_in),
    out(_out),
    f_low(_f_low),
    f_high(_f_high),
    samplerate((double) _samplerate),
    wintype(_wintype),
    gain(_gain)
{
    calc();
}

BPS::~BPS()
{
    decalc();
}

void BPS::flush()
{
    std::fill(infilt.begin(), infilt.end(), 0);
}

void BPS::execute(int pos)
{
    double I;
    double Q;
    if (run && pos == position)
    {
        std::copy(in, in + size * 2, &(infilt[2 * size]));
        fftwf_execute (CFor);
        for (int i = 0; i < 2 * size; i++)
        {
            I = gain * product[2 * i + 0];
            Q = gain * product[2 * i + 1];
            product[2 * i + 0] = (float) (I * mults[2 * i + 0] - Q * mults[2 * i + 1]);
            product[2 * i + 1] = (float) (I * mults[2 * i + 1] + Q * mults[2 * i + 0]);
        }
        fftwf_execute (CRev);
        std::copy(&(infilt[2 * size]), &(infilt[2 * size]) + size * 2, infilt.begin());
    }
    else if (in != out)
        std::copy( in,  in + size * 2, out);
}

void BPS::setBuffers(float* _in, float* _out)
{
    decalc();
    in = _in;
    out = _out;
    calc();
}

void BPS::setSamplerate(int rate)
{
    decalc();
    samplerate = rate;
    calc();
}

void BPS::setSize(int _size)
{
    decalc();
    size = _size;
    calc();
}

void BPS::setFreqs(double _f_low, double _f_high)
{
    decalc();
    f_low = _f_low;
    f_high = _f_high;
    calc();
}

void BPS::setRun(int _run)
{
    run = _run;
}

/********************************************************************************************************
*                                                                                                       *
*                               Overlap-Save Bandpass:  RXA Properties                                  *
*                                                                                                       *
********************************************************************************************************/

} // namespace WDSP
