/*  fmsq.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2016 Warren Pratt, NR0V
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
#include "fircore.hpp"
#include "eqp.hpp"
#include "fmsq.hpp"

namespace WDSP {

void FMSQ::calc()
{
    double delta;
    double theta;
    std::vector<float> impulse;
    int i;
    // noise filter
    noise.resize(2 * size * 2);
    F[0] = 0.0;
    F[1] = (float) fc;
    F[2] = (float) *pllpole;
    F[3] = 20000.0;
    G[0] = 0.0;
    G[1] = 0.0;
    G[2] = 3.0;
    G[3] = (float) (+20.0 * log10(20000.0 / *pllpole));
    EQP::eq_impulse (impulse, nc, 3, F.data(), G.data(), rate, 1.0 / (2.0 * size), 0, 0);
    p = new FIRCORE(size, trigger, noise.data(), mp, impulse);
    // noise averaging
    avm = exp(-1.0 / (rate * avtau));
    onem_avm = 1.0 - avm;
    avnoise = 100.0;
    longavm = exp(-1.0 / (rate * longtau));
    onem_longavm = 1.0 - longavm;
    longnoise = 1.0;
    // level change
    ntup   = (int)(tup   * rate);
    ntdown = (int)(tdown * rate);
    cup.resize(ntup + 1);
    cdown.resize(ntdown + 1);
    delta = PI / (double) ntup;
    theta = 0.0;

    for (i = 0; i <= ntup; i++)
    {
        cup[i] = 0.5 * (1.0 - cos(theta));
        theta += delta;
    }

    delta = PI / (double) ntdown;
    theta = 0.0;

    for (i = 0; i <= ntdown; i++)
    {
        cdown[i] = 0.5 * (1 + cos(theta));
        theta += delta;
    }
    // control
    state = FMSQState::MUTED;
    ready = 0;
    ramp = 0.0;
    rstep = 1.0 / rate;
}

void FMSQ::decalc()
{
    delete (p);
}

FMSQ::FMSQ(
    int _run,
    int _size,
    float* _insig,
    float* _outsig,
    float* _trigger,
    int _rate,
    double _fc,
    double* _pllpole,
    double _tdelay,
    double _avtau,
    double _longtau,
    double _tup,
    double _tdown,
    double _tail_thresh,
    double _unmute_thresh,
    double _min_tail,
    double _max_tail,
    int _nc,
    int _mp
) :
    run(_run),
    size(_size),
    insig(_insig),
    outsig(_outsig),
    trigger(_trigger),
    rate((double) _rate),
    fc(_fc),
    pllpole(_pllpole),
    avtau(_avtau),
    longtau(_longtau),
    tup(_tup),
    tdown(_tdown),
    tail_thresh(_tail_thresh),
    unmute_thresh(_unmute_thresh),
    min_tail(_min_tail),
    max_tail(_max_tail),
    tdelay(_tdelay),
    nc(_nc),
    mp(_mp)
{
    calc();
}

FMSQ::~FMSQ()
{
    decalc();
}

void FMSQ::flush()
{
    p->flush();
    avnoise = 100.0;
    longnoise = 1.0;
    state = FMSQState::MUTED;
    ready = 0;
    ramp = 0.0;
}

void FMSQ::execute()
{
    if (run)
    {
        double _noise;
        double lnlimit;
        p->execute();

        for (int i = 0; i < size; i++)
        {
            double noise0 = noise[2 * i + 0];
            double noise1 = noise[2 * i + 1];
            _noise = sqrt(noise0 * noise0 + noise1 * noise1);
            avnoise = avm * avnoise + onem_avm * _noise;
            longnoise = longavm * longnoise + onem_longavm * _noise;

            if (!ready)
                ramp += rstep;

            if (ramp >= tdelay)
                ready = 1;

            switch (state)
            {
            case FMSQState::MUTED:
                if (avnoise < unmute_thresh && ready)
                {
                    state = FMSQState::INCREASE;
                    count = ntup;
                }

                outsig[2 * i + 0] = 0.0;
                outsig[2 * i + 1] = 0.0;

                break;

            case FMSQState::INCREASE:
                outsig[2 * i + 0] = (float) (insig[2 * i + 0] * cup[ntup - count]);
                outsig[2 * i + 1] = (float) (insig[2 * i + 1] * cup[ntup - count]);

                if (count-- == 0)
                    state = FMSQState::UNMUTED;

                break;

            case FMSQState::UNMUTED:
                if (avnoise > tail_thresh)
                {
                    state = FMSQState::TAIL;

                    if ((lnlimit = longnoise) > 1.0)
                        lnlimit = 1.0;

                    count = (int)((min_tail + (max_tail - min_tail) * lnlimit) * rate);
                }

                outsig[2 * i + 0] = insig[2 * i + 0];
                outsig[2 * i + 1] = insig[2 * i + 1];

                break;

            case FMSQState::TAIL:
                outsig[2 * i + 0] = insig[2 * i + 0];
                outsig[2 * i + 1] = insig[2 * i + 1];

                if (avnoise < unmute_thresh)
                {
                    state = FMSQState::UNMUTED;
                }
                else if (count-- == 0)
                {
                    state = FMSQState::DECREASE;
                    count = ntdown;
                }

                break;

            case FMSQState::DECREASE:
                outsig[2 * i + 0] = (float) (insig[2 * i + 0] * cdown[ntdown - count]);
                outsig[2 * i + 1] = (float) (insig[2 * i + 1] * cdown[ntdown - count]);

                if (count-- == 0)
                    state = FMSQState::MUTED;

                break;
            }
        }
    }
    else if (insig != outsig)
    {
        std::copy(insig, insig + size * 2, outsig);
    }
}

void FMSQ::setBuffers(float* in, float* out, float* trig)
{
    insig = in;
    outsig = out;
    trigger = trig;
    p->setBuffers(trigger, noise.data());
}

void FMSQ::setSamplerate(int _rate)
{
    decalc();
    rate = _rate;
    calc();
}

void FMSQ::setSize(int _size)
{
    decalc();
    size = _size;
    calc();
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void FMSQ::setRun(int _run)
{
    run = _run;
}

void FMSQ::setThreshold(double threshold)
{
    tail_thresh = threshold;
    unmute_thresh = 0.9 * threshold;
}

void FMSQ::setNC(int _nc)
{
    std::vector<float> impulse;

    if (nc != _nc)
    {
        nc = _nc;
        EQP::eq_impulse (impulse, nc, 3, F.data(), G.data(), rate, 1.0 / (2.0 * size), 0, 0);
        p->setNc(impulse);
    }
}

void FMSQ::setMP(int _mp)
{
    if (mp != _mp)
    {
        mp = _mp;
        p->setMp(mp);
    }
}

} // namespace WDSP
