/*  snb.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2015, 2016 Warren Pratt, NR0V
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
#include "resample.hpp"
#include "lmath.hpp"
#include "fircore.hpp"
#include "nbp.hpp"
#include "amd.hpp"
#include "anf.hpp"
#include "anr.hpp"
#include "emnr.hpp"
#include "bpsnba.hpp"

#define MAXIMP          256

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                       BPSNBA Bandpass Filter                                          *
*                                                                                                       *
********************************************************************************************************/

// This is a thin wrapper for a notched-bandpass filter (nbp).  The basic difference is that it provides
// for its input and output to happen at different points in the processing pipeline.  This means it must
// include a buffer, 'buff'.  Its input and output are done via functions xbpshbain() and xbpshbaout().

void BPSNBA::calc()
{
    buff.resize(size * 2);
    bpsnba = new NBP (
        1,                          // run, always runs (use bpsnba 'run')
        run_notches,             // run the notches
        0,                          // position variable for nbp (not for bpsnba), always 0
        size,                    // buffer size
        nc,                      // number of filter coefficients
        mp,                      // minimum phase flag
        buff.data(),             // pointer to input buffer
        out,                     // pointer to output buffer
        f_low,                   // lower filter frequency
        f_high,                  // upper filter frequency
        rate,                    // sample rate
        wintype,                 // wintype
        gain,                    // gain
        autoincr,                // auto-increase notch width if below min
        maxpb,                   // max number of passbands
        notchdb);                // addr of database pointer
}

BPSNBA::BPSNBA(
    int _run,
    int _run_notches,
    int _position,
    int _size,
    int _nc,
    int _mp,
    float* _in,
    float* _out,
    int _rate,
    double _abs_low_freq,
    double _abs_high_freq,
    double _f_low,
    double _f_high,
    int _wintype,
    double _gain,
    int _autoincr,
    int _maxpb,
    NOTCHDB* _notchdb
) :
    run(_run),
    run_notches(_run_notches),
    position(_position),
    size(_size),
    nc(_nc),
    mp(_mp),
    in(_in),
    out(_out),
    rate(_rate),
    abs_low_freq(_abs_low_freq),
    abs_high_freq(_abs_high_freq),
    f_low(_f_low),
    f_high(_f_high),
    wintype(_wintype),
    gain(_gain),
    autoincr(_autoincr),
    maxpb(_maxpb),
    notchdb(_notchdb)
{
    calc();
}

void BPSNBA::decalc()
{
    delete bpsnba;
}

BPSNBA::~BPSNBA()
{
    decalc();
}

void BPSNBA::flush()
{
    std::fill(buff.begin(), buff.end(), 0);
    bpsnba->flush();
}

void BPSNBA::setBuffers(float* _in, float* _out)
{
    decalc();
    in = _in;
    out = _out;
    calc();
}

void BPSNBA::setSamplerate(int _rate)
{
    decalc();
    rate = _rate;
    calc();
}

void BPSNBA::setSize(int _size)
{
    decalc();
    size = _size;
    calc();
}

void BPSNBA::exec_in(int _position)
{
    if (run && position == _position)
        std::copy(in, in + size * 2, buff.begin());
}

void BPSNBA::exec_out(int _position)
{
    if (run && position == _position)
        bpsnba->execute(0);
}

void BPSNBA::recalc_bpsnba_filter(int update)
{
    // Call anytime one of the parameters listed below has been changed in
    // the BPSNBA struct.
    NBP *b = bpsnba;
    b->fnfrun = run_notches;
    b->flow = f_low;
    b->fhigh = f_high;
    b->wintype = wintype;
    b->gain = gain;
    b->autoincr = autoincr;
    b->calc_impulse();
    b->fircore->setImpulse(b->impulse, update);
}

/********************************************************************************************************
*                                                                                                       *
*                                           Properties                                                  *
*                                                                                                       *
********************************************************************************************************/

void BPSNBA::SetNC(int _nc)
{
    if (nc != _nc)
    {
        nc = _nc;
        bpsnba->nc = nc;
        bpsnba->setNc();
    }
}

void BPSNBA::SetMP(int _mp)
{
    if (mp != _mp)
    {
        mp = _mp;
        bpsnba->mp = mp;
        bpsnba->setMp();
    }
}

} // namespace
