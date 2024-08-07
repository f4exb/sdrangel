/*  unit.hpp

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013 Warren Pratt, NR0V
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

#include <algorithm>

#include "unit.hpp"

namespace WDSP {

Unit::Unit(
    int _in_rate,                // input samplerate
    int _out_rate,               // output samplerate
    int _dsp_rate,               // sample rate for mainstream dsp processing
    int _dsp_size                // number complex samples processed per buffer in mainstream dsp processing
) :
    in_rate{_in_rate},
    out_rate(_out_rate),
    dsp_rate(_dsp_rate),
    dsp_size(_dsp_size)
{
    if (_in_rate  >= _dsp_rate)
        dsp_insize  = _dsp_size * (_in_rate  / _dsp_rate);
    else
        dsp_insize  = _dsp_size / (_dsp_rate /  _in_rate);

    if (_out_rate >= _dsp_rate)
        dsp_outsize = _dsp_size * (_out_rate / _dsp_rate);
    else
        dsp_outsize = _dsp_size / (_dsp_rate / _out_rate);

    // buffers
    inbuff  = new float[1 * dsp_insize  * 2];
    outbuff = new float[1 * dsp_outsize  * 2];
    midbuff = new float[2 * dsp_size  * 2];
}

Unit::~Unit()
{
    delete[] inbuff;
    delete[] outbuff;
    delete[] midbuff;
}

void Unit::flushBuffers()
{
    std::fill(inbuff,  inbuff  + 1 * dsp_insize  * 2, 0);
    std::fill(outbuff, outbuff + 1 * dsp_outsize * 2, 0);
    std::fill(midbuff, midbuff + 2 * dsp_size    * 2, 0);
}

void Unit::setBuffersInputSamplerate(int _in_rate)
{
    if (_in_rate  >= dsp_rate)
        dsp_insize  = dsp_size * (_in_rate  / dsp_rate);
    else
        dsp_insize  = dsp_size / (dsp_rate /  _in_rate);

    in_rate = _in_rate;

    // buffers
    delete[] (inbuff);
    inbuff = new float[1 * dsp_insize  * 2];
}

void Unit::setBuffersOutputSamplerate(int _out_rate)
{
    if (_out_rate >= dsp_rate)
        dsp_outsize = dsp_size * (_out_rate / dsp_rate);
    else
        dsp_outsize = dsp_size / (dsp_rate / _out_rate);

    out_rate = _out_rate;

    // buffers
    delete[] outbuff;
    outbuff = new float[1 * dsp_outsize * 2];
}

void Unit::setBuffersDSPSamplerate(int _dsp_rate)
{
    if (in_rate  >= _dsp_rate)
        dsp_insize  = dsp_size * (in_rate  / _dsp_rate);
    else
        dsp_insize  = dsp_size / (_dsp_rate /  in_rate);

    if (out_rate >= _dsp_rate)
        dsp_outsize = dsp_size * (out_rate / _dsp_rate);
    else
        dsp_outsize = dsp_size / (_dsp_rate / out_rate);

    dsp_rate = _dsp_rate;

    // buffers
    delete[] inbuff;
    inbuff = new float[1 * dsp_insize  * 2];
    delete[] outbuff;
    outbuff = new float[1 * dsp_outsize * 2];
}

void Unit::setBuffersDSPBuffsize(int _dsp_size)
{
    if (in_rate  >= dsp_rate)
        dsp_insize  = _dsp_size * (in_rate  / dsp_rate);
    else
        dsp_insize  = _dsp_size / (dsp_rate /  in_rate);

    if (out_rate >= dsp_rate)
        dsp_outsize = _dsp_size * (out_rate / dsp_rate);
    else
        dsp_outsize = _dsp_size / (dsp_rate / out_rate);

    dsp_size = _dsp_size;

    // buffers
    delete[]inbuff;
    inbuff = new float[1 * dsp_insize  * 2];
    delete[] midbuff;
    midbuff = new float[2 * dsp_size * 2];
    delete[] outbuff;
    outbuff = new float[1 * dsp_outsize * 2];
}


} // namespace
