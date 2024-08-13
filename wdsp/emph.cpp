/*  emph.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014, 2016, 2023 Warren Pratt, NR0V
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
#include "emph.hpp"
#include "fcurve.hpp"
#include "fircore.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                       Overlap-Save FM Pre-Emphasis                                    *
*                                                                                                       *
********************************************************************************************************/

void EMPH::calc()
{
    infilt.resize(2 * size * 2);
    product.resize(2 * size * 2);
    FCurve::fc_mults(
        mults,
        size,
        (float) f_low,
        (float) f_high,
        (float) (-20.0 * log10(f_high / f_low)),
        0.0,
        ctype,
        (float) rate,
        (float) (1.0 / (2.0 * size)),
        0,
        0
    );
    CFor = fftwf_plan_dft_1d(
        2 * size,
        (fftwf_complex *)infilt.data(),
        (fftwf_complex *)product.data(),
        FFTW_FORWARD,
        FFTW_PATIENT)
    ;
    CRev = fftwf_plan_dft_1d(
        2 * size,
        (fftwf_complex *)product.data(),
        (fftwf_complex *)out,
        FFTW_BACKWARD,
        FFTW_PATIENT
    );
}

void EMPH::decalc()
{
    fftwf_destroy_plan(CRev);
    fftwf_destroy_plan(CFor);
}

EMPH::EMPH(
    int _run,
    int _position,
    int _size,
    float* _in,
    float* _out,
    int _rate,
    int _ctype,
    double _f_low,
    double _f_high
) :
    run(_run),
    position(_position),
    size(_size),
    in(_in),
    out(_out),
    ctype(_ctype),
    f_low(_f_low),
    f_high(_f_high),
    rate((double) _rate)
{
    calc();
}

EMPH::~EMPH()
{
    decalc();
}

void EMPH::flush()
{
    std::fill(infilt.begin(), infilt.end(), 0);
}

void EMPH::execute(int _position)
{
    double I;
    double Q;
    if (run && position == _position)
    {
        std::copy(in, in + size * 2, &(infilt[2 * size]));
        fftwf_execute (CFor);
        for (int i = 0; i < 2 * size; i++)
        {
            I = product[2 * i + 0];
            Q = product[2 * i + 1];
            product[2 * i + 0] = (float) (I * mults[2 * i + 0] - Q * mults[2 * i + 1]);
            product[2 * i + 1] = (float) (I * mults[2 * i + 1] + Q * mults[2 * i + 0]);
        }
        fftwf_execute (CRev);
        std::copy(&(infilt[2 * size]), &(infilt[2 * size]) + size * 2, infilt.begin());
    }
    else if (in != out)
        std::copy( in,  in + size * 2, out);
}

void EMPH::setBuffers(float* _in, float* _out)
{
    decalc();
    in = _in;
    out = _out;
    calc();
}

void EMPH::setSamplerate(int _rate)
{
    decalc();
    rate = _rate;
    calc();
}

void EMPH::setSize(int _size)
{
    decalc();
    size = _size;
    calc();
}

} // namespace WDSP
