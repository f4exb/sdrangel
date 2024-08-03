/*  wcpAGC.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2011 - 2017 Warren Pratt, NR0V
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

or by paper mail at

Warren Pratt
11303 Empire Grade
Santa Cruz, CA  95060

*/

#include "comm.hpp"
#include "nbp.hpp"
#include "wcpAGC.hpp"

namespace WDSP {

void WCPAGC::calc()
{
    //assign constants
    //do one-time initialization
    out_index = -1;
    ring_max = 0.0;
    volts = 0.0;
    save_volts = 0.0;
    fast_backaverage = 0.0;
    hang_backaverage = 0.0;
    hang_counter = 0;
    decay_type = 0;
    state = 0;
    loadWcpAGC();
}

WCPAGC::WCPAGC(
    int _run,
    int _mode,
    int _pmode,
    float* _in,
    float* _out,
    int _io_buffsize,
    int _sample_rate,
    double _tau_attack,
    double _tau_decay,
    int _n_tau,
    double _max_gain,
    double _var_gain,
    double _fixed_gain,
    double _max_input,
    double _out_targ,
    double _tau_fast_backaverage,
    double _tau_fast_decay,
    double _pop_ratio,
    int _hang_enable,
    double _tau_hang_backmult,
    double _hangtime,
    double _hang_thresh,
    double _tau_hang_decay
) :
    //initialize per call parameters
    run(_run),
    mode(_mode),
    pmode(_pmode),
    in(_in),
    out(_out),
    io_buffsize(_io_buffsize),
    sample_rate((double) _sample_rate),
    tau_attack(_tau_attack),
    tau_decay(_tau_decay),
    n_tau(_n_tau),
    max_gain(_max_gain),
    var_gain(_var_gain),
    fixed_gain(_fixed_gain),
    max_input(_max_input),
    out_targ(_out_targ),
    tau_fast_backaverage(_tau_fast_backaverage),
    tau_fast_decay(_tau_fast_decay),
    pop_ratio(_pop_ratio),
    hang_enable(_hang_enable),
    tau_hang_backmult(_tau_hang_backmult),
    hangtime(_hangtime),
    hang_thresh(_hang_thresh),
    tau_hang_decay(_tau_hang_decay)
{
    calc();
}

void WCPAGC::loadWcpAGC()
{
    double tmp;
    //calculate internal parameters
    attack_buffsize = (int)ceil(sample_rate * n_tau * tau_attack);
    in_index = attack_buffsize + out_index;
    attack_mult = 1.0 - exp(-1.0 / (sample_rate * tau_attack));
    decay_mult = 1.0 - exp(-1.0 / (sample_rate * tau_decay));
    fast_decay_mult = 1.0 - exp(-1.0 / (sample_rate * tau_fast_decay));
    fast_backmult = 1.0 - exp(-1.0 / (sample_rate * tau_fast_backaverage));
    onemfast_backmult = 1.0 - fast_backmult;

    out_target = out_targ * (1.0 - exp(-(double)n_tau)) * 0.9999;
    min_volts = out_target / (var_gain * max_gain);
    inv_out_target = 1.0 / out_target;

    tmp = log10(out_target / (max_input * var_gain * max_gain));

    if (tmp == 0.0)
        tmp = 1e-16;

    slope_constant = (out_target * (1.0 - 1.0 / var_gain)) / tmp;
    inv_max_input = 1.0 / max_input;
    tmp = pow (10.0, (hang_thresh - 1.0) / 0.125);
    hang_level = (max_input * tmp + (out_target /
        (var_gain * max_gain)) * (1.0 - tmp)) * 0.637;
    hang_backmult = 1.0 - exp(-1.0 / (sample_rate * tau_hang_backmult));
    onemhang_backmult = 1.0 - hang_backmult;
    hang_decay_mult = 1.0 - exp(-1.0 / (sample_rate * tau_hang_decay));
}

void WCPAGC::flush()
{
    std::fill(ring.begin(), ring.end(), 0);
    std::fill(abs_ring.begin(), abs_ring.end(), 0);
    ring_max = 0.0;
}

void WCPAGC::execute()
{
    int i;
    int k;
    double mult;

    if (run)
    {
        if (mode == 0)
        {
            for (i = 0; i < io_buffsize; i++)
            {
                out[2 * i + 0] = (float) (fixed_gain * in[2 * i + 0]);
                out[2 * i + 1] = (float) (fixed_gain * in[2 * i + 1]);
            }

            return;
        }

        for (i = 0; i < io_buffsize; i++)
        {
            if (++out_index >= ring_buffsize)
                out_index -= ring_buffsize;

            if (++in_index >= ring_buffsize)
                in_index -= ring_buffsize;

            out_sample[0] = ring[2 * out_index + 0];
            out_sample[1] = ring[2 * out_index + 1];
            abs_out_sample = abs_ring[out_index];
            ring[2 * in_index + 0] = in[2 * i + 0];
            ring[2 * in_index + 1] = in[2 * i + 1];
            double xr = ring[2 * in_index + 0];
            double xi = ring[2 * in_index + 1];

            if (pmode == 0)
                abs_ring[in_index] = std::max(fabs(xr), fabs(xi));
            else
                abs_ring[in_index] = sqrt(xr*xr + xi*xi);

            fast_backaverage = fast_backmult * abs_out_sample + onemfast_backmult * fast_backaverage;
            hang_backaverage = hang_backmult * abs_out_sample + onemhang_backmult * hang_backaverage;

            if ((abs_out_sample >= ring_max) && (abs_out_sample > 0.0))
            {
                ring_max = 0.0;
                k = out_index;

                for (int j = 0; j < attack_buffsize; j++)
                {
                    if (++k == ring_buffsize)
                        k = 0;
                    if (abs_ring[k] > ring_max)
                        ring_max = abs_ring[k];
                }
            }

            if (abs_ring[in_index] > ring_max)
                ring_max = abs_ring[in_index];

            if (hang_counter > 0)
                --hang_counter;

            switch (state)
            {
            case 0:
                {
                    if (ring_max >= volts)
                    {
                        volts += (ring_max - volts) * attack_mult;
                    }
                    else
                    {
                        if (volts > pop_ratio * fast_backaverage)
                        {
                            state = 1;
                            volts += (ring_max - volts) * fast_decay_mult;
                        }
                        else
                        {
                            if (hang_enable && (hang_backaverage > hang_level))
                            {
                                state = 2;
                                hang_counter = (int)(hangtime * sample_rate);
                                decay_type = 1;
                            }
                            else
                            {
                                state = 3;
                                volts += (ring_max - volts) * decay_mult;
                                decay_type = 0;
                            }
                        }
                    }
                    break;
                }

            case 1:
                {
                    if (ring_max >= volts)
                    {
                        state = 0;
                        volts += (ring_max - volts) * attack_mult;
                    }
                    else
                    {
                        if (volts > save_volts)
                        {
                            volts += (ring_max - volts) * fast_decay_mult;
                        }
                        else
                        {
                            if (hang_counter > 0)
                            {
                                state = 2;
                            }
                            else
                            {
                                if (decay_type == 0)
                                {
                                    state = 3;
                                    volts += (ring_max - volts) * decay_mult;
                                }
                                else
                                {
                                    state = 4;
                                    volts += (ring_max - volts) * hang_decay_mult;
                                }
                            }
                        }
                    }
                    break;
                }

            case 2:
                {
                    if (ring_max >= volts)
                    {
                        state = 0;
                        save_volts = volts;
                        volts += (ring_max - volts) * attack_mult;
                    }
                    else
                    {
                        if (hang_counter == 0)
                        {
                            state = 4;
                            volts += (ring_max - volts) * hang_decay_mult;
                        }
                    }
                    break;
                }

            case 3:
                {
                    if (ring_max >= volts)
                    {
                        state = 0;
                        save_volts = volts;
                        volts += (ring_max - volts) * attack_mult;
                    }
                    else
                    {
                        volts += (ring_max - volts) * decay_mult;
                    }
                    break;
                }

            case 4:
                {
                    if (ring_max >= volts)
                    {
                        state = 0;
                        save_volts = volts;
                        volts += (ring_max - volts) * attack_mult;
                    }
                    else
                    {
                        volts += (ring_max - volts) * hang_decay_mult;
                    }
                    break;
                }
            default:
                break;
            }

            if (volts < min_volts)
                volts = min_volts;

            gain = volts * inv_out_target;
            mult = (out_target - slope_constant * std::min (0.0, log10(inv_max_input * volts))) / volts;
            out[2 * i + 0] = (float) (out_sample[0] * mult);
            out[2 * i + 1] = (float) (out_sample[1] * mult);
        }
    }
    else if (out != in)
    {
        std::copy(in, in + io_buffsize * 2, out);
    }
}

void WCPAGC::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void WCPAGC::setSamplerate(int _rate)
{
    sample_rate = _rate;
    calc();
}

void WCPAGC::setSize(int _size)
{
    io_buffsize = _size;
    calc();
}

/********************************************************************************************************
*                                                                                                       *
*                                        Public Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void WCPAGC::setMode(int _mode)
{
    switch (_mode)
    {
        case 0: //agcOFF
            mode = 0;
            loadWcpAGC();
            break;
        case 1: //agcLONG
            mode = 1;
            hangtime = 2.000;
            tau_decay = 2.000;
            loadWcpAGC();
            break;
        case 2: //agcSLOW
            mode = 2;
            hangtime = 1.000;
            tau_decay = 0.500;
            loadWcpAGC();
            break;
        case 3: //agcMED
            mode = 3;
            hang_thresh = 1.0;
            hangtime = 0.000;
            tau_decay = 0.250;
            loadWcpAGC();
            break;
        case 4: //agcFAST
            mode = 4;
            hang_thresh = 1.0;
            hangtime = 0.000;
            tau_decay = 0.050;
            loadWcpAGC();
            break;
        default:
            mode = 5;
            break;
    }
}

void WCPAGC::setFixed(double _fixed_agc)
{
    fixed_gain = pow (10.0, _fixed_agc / 20.0);
    loadWcpAGC();
}

void WCPAGC::setAttack(int _attack)
{
    tau_attack = (double) _attack / 1000.0;
    loadWcpAGC();
}

void WCPAGC::setDecay(int _decay)
{
    tau_decay = (double) _decay / 1000.0;
    loadWcpAGC();
}

void WCPAGC::setHang(int _hang)
{
    hangtime = (double) _hang / 1000.0;
    loadWcpAGC();
}

void WCPAGC::getHangLevel(double *hangLevel) const
//for line on bandscope
{
    *hangLevel = 20.0 * log10(hang_level / 0.637);
}

void WCPAGC::setHangLevel(double _hangLevel)
//for line on bandscope
{
    double convert;
    double tmp;

    if (max_input > min_volts)
    {
        convert = pow (10.0, _hangLevel / 20.0);
        tmp = std::max(1e-8, (convert - min_volts) / (max_input - min_volts));
        hang_thresh = 1.0 + 0.125 * log10 (tmp);
    }
    else
        hang_thresh = 1.0;

    loadWcpAGC();
}

void WCPAGC::getHangThreshold(int *hangthreshold) const
//for slider in setup
{
    *hangthreshold = (int) (100.0 * hang_thresh);
}

void WCPAGC::setHangThreshold(int _hangthreshold)
//For slider in setup
{
    hang_thresh = (double) _hangthreshold / 100.0;
    loadWcpAGC();
}

void WCPAGC::getTop(double *max_agc) const
//for AGC Max Gain in setup
{
    *max_agc = 20 * log10 (max_gain);
}

void WCPAGC::setTop(double _max_agc)
//for AGC Max Gain in setup
{
    max_gain = pow (10.0, _max_agc / 20.0);
    loadWcpAGC();
}

void WCPAGC::setSlope(int _slope)
{
    var_gain = pow (10.0, (double) _slope / 20.0 / 10.0);
    loadWcpAGC();
}

void WCPAGC::setMaxInputLevel(double _level)
{
    max_input = _level;
    loadWcpAGC();
}

void WCPAGC::setRun(int _state)
{
    run = _state;
}

} // namespace WDSP
