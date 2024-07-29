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
#include "bandpass.hpp"
#include "fir.hpp"
#include "fircore.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                   Partitioned Overlap-Save Bandpass                                   *
*                                                                                                       *
********************************************************************************************************/

BANDPASS::BANDPASS(
    int _run,
    int _position,
    int _size,
    int _nc,
    int _mp,
    float* _in,
    float* _out,
    double _f_low,
    double _f_high,
    int _samplerate,
    int _wintype,
    double _gain
)
{
    // NOTE:  'nc' must be >= 'size'
    run = _run;
    position = _position;
    size = _size;
    nc = _nc;
    mp = _mp;
    in = _in;
    out = _out;
    f_low = _f_low;
    f_high = _f_high;
    samplerate = _samplerate;
    wintype = _wintype;
    gain = _gain;
    float* impulse = FIR::fir_bandpass (
        nc,
        f_low,
        f_high,
        samplerate,
        wintype,
        1,
        gain / (double)(2 * size)
    );
    fircore = FIRCORE::create_fircore (size, in, out, nc, mp, impulse);
    delete[] impulse;
}

BANDPASS::~BANDPASS()
{
    FIRCORE::destroy_fircore (fircore);
}

void BANDPASS::flush()
{
    FIRCORE::flush_fircore(fircore);
}

void BANDPASS::execute(int pos)
{
    if (run && position == pos)
        FIRCORE::xfircore(fircore);
    else if (out != in)
        std::copy(in,  in + size * 2, out);
}

void BANDPASS::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
    FIRCORE::setBuffers_fircore(fircore, in, out);
}

void BANDPASS::setSamplerate(int _rate)
{
    samplerate = _rate;
    float* impulse = FIR::fir_bandpass (
        nc,
        f_low,
        f_high,
        samplerate,
        wintype,
        1,
        gain / (double) (2 * size)
    );
    FIRCORE::setImpulse_fircore (fircore, impulse, 1);
    delete[] impulse;
}

void BANDPASS::setSize(int _size)
{
    // NOTE:  'size' must be <= 'nc'
    size = _size;
    FIRCORE::setSize_fircore (fircore, size);
    // recalc impulse because scale factor is a function of size
    float* impulse = FIR::fir_bandpass (
        nc,
        f_low,
        f_high,
        samplerate,
        wintype,
        1,
        gain / (double) (2 * size)
    );
    FIRCORE::setImpulse_fircore (fircore, impulse, 1);
    delete[] (impulse);
}

void BANDPASS::setGain(double _gain, int _update)
{
    gain = _gain;
    float* impulse = FIR::fir_bandpass (
        nc,
        f_low,
        f_high,
        samplerate,
        wintype,
        1,
        gain / (double) (2 * size)
    );
    FIRCORE::setImpulse_fircore (fircore, impulse, _update);
    delete[] (impulse);
}

void BANDPASS::calcBandpassFilter(double _f_low, double _f_high, double _gain)
{
    if ((f_low != _f_low) || (f_high != _f_high) || (gain != _gain))
    {
        f_low = _f_low;
        f_high = _f_high;
        gain = _gain;
        float* impulse = FIR::fir_bandpass (
            nc,
            f_low,
            f_high,
            samplerate,
            wintype,
            1,
            gain / (double)(2 * size)
        );
        FIRCORE::setImpulse_fircore (fircore, impulse, 1);
        delete[] (impulse);
    }
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void BANDPASS::setBandpassFreqs(double _f_low, double _f_high)
{
    if ((_f_low != f_low) || (_f_high != f_high))
    {
        float* impulse = FIR::fir_bandpass (
            nc,
            _f_low,
            _f_high,
            samplerate,
            wintype,
            1,
            gain / (double)(2 * size)
        );

        FIRCORE::setImpulse_fircore (fircore, impulse, 0);
        delete[] (impulse);
        f_low = _f_low;
        f_high = _f_high;
        FIRCORE::setUpdate_fircore (fircore);
    }
}

void BANDPASS::SetBandpassNC(int _nc)
{
    // NOTE:  'nc' must be >= 'size'
    if (_nc != nc)
    {
        nc = _nc;
        float* impulse = FIR::fir_bandpass (
            nc,
            f_low,
            f_high,
            samplerate,
            wintype,
            1,
            gain / (double)( 2 * size)
        );
        FIRCORE::setNc_fircore (fircore, nc, impulse);
        delete[] (impulse);
    }
}

void BANDPASS::SetBandpassMP(int _mp)
{
    if (_mp != mp)
    {
        mp = _mp;
        FIRCORE::setMp_fircore (fircore, mp);
    }
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

//PORT
//void SetTXABandpassFreqs (int channel, float f_low, float f_high)
//{
//  float* impulse;
//  BANDPASS a;
//  a = txa.bp0;
//  if ((f_low != a->f_low) || (f_high != a->f_high))
//  {
//      a->f_low = f_low;
//      a->f_high = f_high;
//      impulse = fir_bandpass (a->nc, a->f_low, a->f_high, a->samplerate, a->wintype, 1, a->gain / (float)(2 * a->size));
//      setImpulse_fircore (a->fircore, impulse, 1);
//      delete[] (impulse);
//  }
//  a = txa.bp1;
//  if ((f_low != a->f_low) || (f_high != a->f_high))
//  {
//      a->f_low = f_low;
//      a->f_high = f_high;
//      impulse = fir_bandpass (a->nc, a->f_low, a->f_high, a->samplerate, a->wintype, 1, a->gain / (float)(2 * a->size));
//      setImpulse_fircore (a->fircore, impulse, 1);
//      delete[] (impulse);
//  }
//  a = txa.bp2;
//  if ((f_low != a->f_low) || (f_high != a->f_high))
//  {
//      a->f_low = f_low;
//      a->f_high = f_high;
//      impulse = fir_bandpass (a->nc, a->f_low, a->f_high, a->samplerate, a->wintype, 1, a->gain / (float)(2 * a->size));
//      setImpulse_fircore (a->fircore, impulse, 1);
//      delete[] (impulse);
//  }
//}

} // namespace WDSP
