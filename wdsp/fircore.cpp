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
#include "fircore.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                   Partitioned Overlap-Save Filter Kernel                              *
*                                                                                                       *
********************************************************************************************************/


void FIRCORE::plan()
{
    // must call for change in 'nc', 'size', 'out'
    nfor = nc / size;
    cset = 0;
    buffidx = 0;
    idxmask = nfor - 1;
    fftin.resize(2 * size * 2);
    fftout.resize(nfor);
    fmask[0].resize(nfor);
    fmask[1].resize(nfor);
    maskgen.resize(2 * size * 2);
    pcfor.resize(nfor);
    maskplan[0].resize(nfor);
    maskplan[1].resize(nfor);
    for (int i = 0; i < nfor; i++)
    {
        fftout[i].resize(2 * size * 2);
        fmask[0][i].resize(2 * size * 2);
        fmask[1][i].resize(2 * size * 2);
        pcfor[i] = fftwf_plan_dft_1d(
            2 * size,
            (fftwf_complex *)fftin.data(),
            (fftwf_complex *)fftout[i].data(),
            FFTW_FORWARD,
            FFTW_PATIENT
        );
        maskplan[0][i] = fftwf_plan_dft_1d(
            2 * size,
            (fftwf_complex *)maskgen.data(),
            (fftwf_complex *)fmask[0][i].data(),
            FFTW_FORWARD,
            FFTW_PATIENT
        );
        maskplan[1][i] = fftwf_plan_dft_1d(
            2 * size,
            (fftwf_complex *)maskgen.data(),
            (fftwf_complex *)fmask[1][i].data(),
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
    masks_ready = 0;
}

void FIRCORE::calc(int _flip)
{
    // call for change in frequency, rate, wintype, gain
    // must also call after a call to plan_firopt()

    if (mp)
        FIR::mp_imp (nc, impulse, imp, 16, 0);
    else
        std::copy(impulse.begin(), impulse.end(), imp.begin());

    for (int i = 0; i < nfor; i++)
    {
        // I right-justified the impulse response => take output from left side of output buff, discard right side
        // Be careful about flipping an asymmetrical impulse response.
        std::copy(&(imp[2 * size * i]), &(imp[2 * size * i]) + size * 2, &(maskgen[2 * size]));
        fftwf_execute (maskplan[1 - cset][i]);
    }

    masks_ready = 1;

    if (_flip)
    {
        cset = 1 - cset;
        masks_ready = 0;
    }
}

FIRCORE::FIRCORE(
    int _size,
    float* _in,
    float* _out,
    int _mp,
    const std::vector<float>& _impulse
)
{
    size = _size;
    in = _in;
    out = _out;
    nc = (int) (_impulse.size() / 2);
    mp = _mp;
    plan();
    impulse.resize(_impulse.size());
    imp.resize(_impulse.size());
    std::copy(_impulse.begin(), _impulse.end(), impulse.begin());
    calc(1);
}

void FIRCORE::deplan()
{
    fftwf_destroy_plan (crev);
    for (int i = 0; i < nfor; i++)
    {
        fftwf_destroy_plan (pcfor[i]);
        fftwf_destroy_plan (maskplan[0][i]);
        fftwf_destroy_plan (maskplan[1][i]);
    }
}

FIRCORE::~FIRCORE()
{
    deplan();
}

void FIRCORE::flush()
{
    std::fill(fftin.begin(), fftin.end(), 0);
    for (int i = 0; i < nfor; i++)
        std::fill(fftout[i].begin(), fftout[i].end(), 0);
    buffidx = 0;
}

void FIRCORE::execute()
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
            accum[2 * i + 0] += fftout[k][2 * i + 0] * fmask[cset][j][2 * i + 0] - fftout[k][2 * i + 1] * fmask[cset][j][2 * i + 1];
            accum[2 * i + 1] += fftout[k][2 * i + 0] * fmask[cset][j][2 * i + 1] + fftout[k][2 * i + 1] * fmask[cset][j][2 * i + 0];
        }

        k = (k + idxmask) & idxmask;
    }

    buffidx = (buffidx + 1) & idxmask;
    fftwf_execute (crev);
    std::copy(&(fftin[2 * size]), &(fftin[2 * size]) + size * 2, fftin.begin());
}

void FIRCORE::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
    deplan();
    plan();
    calc(1);
}

void FIRCORE::setSize(int _size)
{
    size = _size;
    deplan();
    plan();
    calc(1);
}

void FIRCORE::setImpulse(const std::vector<float>& _impulse, int _update)
{
    auto imp_nc = (int) (_impulse.size() / 2);

    if (imp_nc == nc) // to be on the safe side but setNc would be called if impulse size changes
    {
        std::copy(_impulse.begin(), _impulse.end(), impulse.begin());
        calc(_update);
    }
    else{
        setNc(_impulse);
    }
}

void FIRCORE::setNc(const std::vector<float>& _impulse)
{
    // because of FFT planning, this will probably cause a glitch in audio if done during dataflow
    deplan();
    nc = (int) (_impulse.size() / 2);
    plan();
    imp.resize(nc * 2);
    impulse.resize(nc * 2);
    std::copy(_impulse.begin(), _impulse.end(), impulse.begin());
    calc(1);
}

void FIRCORE::setMp(int _mp)
{
    mp = _mp;
    calc(1);
}

void FIRCORE::setUpdate()
{
    if (masks_ready)
    {
        cset = 1 - cset;
        masks_ready = 0;
    }
}

} // namespace WDSP
