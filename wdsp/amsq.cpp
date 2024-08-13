/*  amsq.c

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

#include <cstdio>

#include "comm.hpp"
#include "amsq.hpp"

namespace WDSP {

void AMSQ::compute_slews()
{
    double delta;
    double theta;
    delta = PI / (double)ntup;
    theta = 0.0;

    for (int i = 0; i <= ntup; i++)
    {
        cup[i] = muted_gain + (1.0 - muted_gain) * 0.5 * (1.0 - cos (theta));
        theta += delta;
    }

    delta = PI / (double)ntdown;
    theta = 0.0;

    for (int i = 0; i <= ntdown; i++)
    {
        cdown[i] = muted_gain + (1.0 - muted_gain) * 0.5 * (1.0 + cos (theta));
        theta += delta;
    }
}

void AMSQ::calc()
{
    // signal averaging
    trigsig.resize(size * 2);
    avm = exp(-1.0 / (rate * avtau));
    onem_avm = 1.0 - avm;
    avsig = 0.0;
    // level change
    ntup = (int)(tup * rate);
    ntdown = (int)(tdown * rate);
    cup.resize((ntup + 1) * 2);
    cdown.resize((ntdown + 1) * 2);
    compute_slews();
    // control
    state = AMSQState::MUTED;
}

AMSQ::AMSQ (
    int _run,
    int _size,
    float* _in,
    float* _out,
    float* _trigger,
    int _rate,
    double _avtau,
    double _tup,
    double _tdown,
    double _tail_thresh,
    double _unmute_thresh,
    double _min_tail,
    double _max_tail,
    double _muted_gain
) :
    run(_run),
    size(_size),
    in(_in),
    out(_out),
    trigger(_trigger),
    rate((double) _rate),
    avtau(_avtau),
    tup(_tup),
    tdown(_tdown),
    tail_thresh(_tail_thresh),
    unmute_thresh(_unmute_thresh),
    min_tail(_min_tail),
    max_tail(_max_tail),
    muted_gain(_muted_gain)
{
    calc();
}

void AMSQ::flush()
{
    std::fill(trigsig.begin(), trigsig.end(), 0);
    avsig = 0.0;
    state = AMSQState::MUTED;
}

void AMSQ::execute()
{
    if (run)
    {
        double sig;
        double siglimit;

        for (int i = 0; i < size; i++)
        {
            double trigr = trigsig[2 * i + 0];
            double trigi = trigsig[2 * i + 1];
            sig = sqrt (trigr*trigr + trigi*trigi);
            avsig = avm * avsig + onem_avm * sig;

            switch (state)
            {
            case AMSQState::MUTED:
                if (avsig > unmute_thresh)
                {
                    state = AMSQState::INCREASE;
                    count = ntup;
                }

                out[2 * i + 0] = (float) (muted_gain * in[2 * i + 0]);
                out[2 * i + 1] = (float) (muted_gain * in[2 * i + 1]);

                break;

            case AMSQState::INCREASE:
                out[2 * i + 0] = (float) (in[2 * i + 0] * cup[ntup - count]);
                out[2 * i + 1] = (float) (in[2 * i + 1] * cup[ntup - count]);

                if (count-- == 0)
                    state = AMSQState::UNMUTED;

                break;

            case AMSQState::UNMUTED:
                if (avsig < tail_thresh)
                {
                    state = AMSQState::TAIL;

                    if ((siglimit = avsig) > 1.0)
                        siglimit = 1.0;

                    count = (int)((min_tail + (max_tail - min_tail) * (1.0 - siglimit)) * rate);
                }

                out[2 * i + 0] = in[2 * i + 0];
                out[2 * i + 1] = in[2 * i + 1];

                break;

            case AMSQState::TAIL:
                out[2 * i + 0] = in[2 * i + 0];
                out[2 * i + 1] = in[2 * i + 1];

                if (avsig > unmute_thresh)
                {
                    state = AMSQState::UNMUTED;
                }
                else if (count-- == 0)
                {
                    state = AMSQState::DECREASE;
                    count = ntdown;
                }

                break;

            case AMSQState::DECREASE:
                out[2 * i + 0] = (float) (in[2 * i + 0] * cdown[ntdown - count]);
                out[2 * i + 1] = (float) (in[2 * i + 1] * cdown[ntdown - count]);

                if (count-- == 0)
                    state = AMSQState::MUTED;

                break;
            }
        }
    }
    else if (in != out)
    {
        std::copy( in,  in + size * 2, out);
    }
}

void AMSQ::xcap()
{
    std::copy(trigger, trigger + size * 2, trigsig.begin());
}

void AMSQ::setBuffers(float* _in, float* _out, float* _trigger)
{
    in = _in;
    out = _out;
    trigger = _trigger;
}

void AMSQ::setSamplerate(int _rate)
{
    rate = _rate;
    calc();
}

void AMSQ::setSize(int _size)
{
    size = _size;
    calc();
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void AMSQ::setRun(int _run)
{
    run = _run;
}

void AMSQ::setThreshold(double _threshold)
{
    double thresh = pow (10.0, _threshold / 20.0);
    tail_thresh = 0.9 * thresh;
    unmute_thresh =  thresh;
}

void AMSQ::setMaxTail(double _tail)
{
    if (_tail < min_tail)
        _tail = min_tail;

    max_tail = _tail;
}

void AMSQ::setMutedGain(double dBlevel)
{   // dBlevel is negative
    muted_gain = pow (10.0, dBlevel / 20.0);
    compute_slews();
}

} // namespace WDSP
