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
) :
    // NOTE:  'nc' must be >= 'size'
    run(_run),
    position(_position),
    size(_size),
    nc(_nc),
    mp(_mp),
    in(_in),
    out(_out),
    f_low(_f_low),
    f_high(_f_high),
    samplerate(_samplerate),
    wintype(_wintype),
    gain(_gain)
{
    std::vector<float> impulse;
    FIR::fir_bandpass (
        impulse,
        nc,
        f_low,
        f_high,
        samplerate,
        wintype,
        1,
        gain / (double)(2 * size)
    );
    fircore = new FIRCORE(size, in, out, mp, impulse);
}

BANDPASS::~BANDPASS()
{
    delete (fircore);
}

void BANDPASS::flush()
{
    fircore->flush();
}

void BANDPASS::execute(int pos)
{
    if (run && position == pos)
        fircore->execute();
    else if (out != in)
        std::copy(in,  in + size * 2, out);
}

void BANDPASS::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
    fircore->setBuffers(in, out);
}

void BANDPASS::setSamplerate(int _rate)
{
    samplerate = _rate;
    std::vector<float> impulse;
    FIR::fir_bandpass (
        impulse,
        nc,
        f_low,
        f_high,
        samplerate,
        wintype,
        1,
        gain / (double) (2 * size)
    );
    fircore->setImpulse(impulse, 1);
}

void BANDPASS::setSize(int _size)
{
    // NOTE:  'size' must be <= 'nc'
    size = _size;
    fircore->setSize(size);
    // recalc impulse because scale factor is a function of size
    std::vector<float> impulse;
    FIR::fir_bandpass (
        impulse,
        nc,
        f_low,
        f_high,
        samplerate,
        wintype,
        1,
        gain / (double) (2 * size)
    );
    fircore->setImpulse(impulse, 1);
}

void BANDPASS::setGain(double _gain, int _update)
{
    gain = _gain;
    std::vector<float> impulse;
    FIR::fir_bandpass (
        impulse,
        nc,
        f_low,
        f_high,
        samplerate,
        wintype,
        1,
        gain / (double) (2 * size)
    );
    fircore->setImpulse(impulse, _update);
}

void BANDPASS::calcBandpassFilter(double _f_low, double _f_high, double _gain)
{
    if ((f_low != _f_low) || (f_high != _f_high) || (gain != _gain))
    {
        f_low = _f_low;
        f_high = _f_high;
        gain = _gain;
        std::vector<float> impulse;
        FIR::fir_bandpass (
            impulse,
            nc,
            f_low,
            f_high,
            samplerate,
            wintype,
            1,
            gain / (double)(2 * size)
        );
        fircore->setImpulse(impulse, 1);
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
        std::vector<float> impulse;
        FIR::fir_bandpass (
            impulse,
            nc,
            _f_low,
            _f_high,
            samplerate,
            wintype,
            1,
            gain / (double)(2 * size)
        );

        fircore->setImpulse(impulse, 0);
        f_low = _f_low;
        f_high = _f_high;
        fircore->setUpdate();
    }
}

void BANDPASS::SetBandpassNC(int _nc)
{
    // NOTE:  'nc' must be >= 'size'
    if (_nc != nc)
    {
        nc = _nc;
        std::vector<float> impulse;
        FIR::fir_bandpass (
            impulse,
            nc,
            f_low,
            f_high,
            samplerate,
            wintype,
            1,
            gain / (double)( 2 * size)
        );
        fircore->setNc(impulse);
    }
}

void BANDPASS::SetBandpassMP(int _mp)
{
    if (_mp != mp)
    {
        mp = _mp;
        fircore->setMp(mp);
    }
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

} // namespace WDSP
