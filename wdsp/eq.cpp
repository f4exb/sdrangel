/*  eq.c

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
#include "eq.hpp"
#include "eqp.hpp"
#include "fircore.hpp"
#include "fir.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                           Overlap-Save Equalizer                                      *
*                                                                                                       *
********************************************************************************************************/


void EQ::eq_mults (std::vector<float>& mults, int size, int nfreqs, float* F, float* G, float samplerate, float scale, int ctfmode, int wintype)
{
    std::vector<float> impulse;
    EQP::eq_impulse (impulse, size + 1, nfreqs, F, G, samplerate, scale, ctfmode, wintype);
    std::vector<float> _mults;
    FIR::fftcv_mults(_mults, 2 * size, impulse.data());
    mults.resize(2 * size * 2);
    std::copy(_mults.begin(), _mults.end(), mults.begin());
}

void EQ::calc()
{
    scale = (float) (1.0 / (float)(2 * size));
    infilt.resize(2 * size * 2);
    product.resize(2 * size * 2);
    CFor = fftwf_plan_dft_1d(
        2 * size,
        (fftwf_complex *) infilt.data(),
        (fftwf_complex *) product.data(),
        FFTW_FORWARD,
        FFTW_PATIENT
    );
    CRev = fftwf_plan_dft_1d(
        2 * size,
        (fftwf_complex *) product.data(),
        (fftwf_complex *) out,
        FFTW_BACKWARD,
        FFTW_PATIENT
    );
    EQ::eq_mults(mults, size, nfreqs, F.data(), G.data(), samplerate, scale, ctfmode, wintype);
}

void EQ::decalc()
{
    fftwf_destroy_plan(CRev);
    fftwf_destroy_plan(CFor);
}

EQ::EQ(
    int _run,
    int _size,
    float *_in,
    float *_out,
    int _nfreqs,
    const float* _F,
    const float* _G,
    int _ctfmode,
    int _wintype,
    int _samplerate
) :
    run(_run),
    size(_size),
    in(_in),
    out(_out),
    nfreqs(_nfreqs),
    ctfmode(_ctfmode),
    wintype(_wintype),
    samplerate((float) _samplerate)
{
    F.resize(nfreqs + 1);
    G.resize(nfreqs + 1);
    std::copy(_F, _F + nfreqs + 1, F.begin());
    std::copy(_G, _G + nfreqs + 1, G.begin());
    calc();
}

EQ::~EQ()
{
    decalc();
}

void EQ::flush()
{
    std::fill(infilt.begin(), infilt.end(), 0);
}

void EQ::execute()
{
    float I;
    float Q;
    if (run)
    {
        std::copy(in, in + size * 2, &(infilt[2 * size]));
        fftwf_execute (CFor);
        for (int i = 0; i < 2 * size; i++)
        {
            I = product[2 * i + 0];
            Q = product[2 * i + 1];
            product[2 * i + 0] = I * mults[2 * i + 0] - Q * mults[2 * i + 1];
            product[2 * i + 1] = I * mults[2 * i + 1] + Q * mults[2 * i + 0];
        }
        fftwf_execute (CRev);
        std::copy(&(infilt[2 * size]), &(infilt[2 * size]) + size * 2, infilt);
    }
    else if (in != out)
        std::copy( in,  in + size * 2, out);
}

void EQ::setBuffers(float* _in, float* _out)
{
    decalc();
    in = _in;
    out = _out;
    calc();
}

void EQ::setSamplerate(int _rate)
{
    decalc();
    samplerate = (float) _rate;
    calc();
}

void EQ::setSize(int _size)
{
    decalc();
    size = _size;
    calc();
}

} // namespace WDSP
