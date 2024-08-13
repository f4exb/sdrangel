/*  ssql.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2023 Warren Pratt, NR0V
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

warren@pratt.one

*/

#include "comm.hpp"
#include "cblock.hpp"
#include "ssql.hpp"
#include "dbqlp.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                      Frequency to Voltage Converter                                   *
*                                                                                                       *
********************************************************************************************************/

FTOV::FTOV(
    int _run,
    int _size,
    int _rate,
    int _rsize,
    double _fmax,
    float* _in,
    float* _out
)
{
    run = _run;
    size = _size;
    rate = _rate;
    rsize = _rsize;
    fmax = _fmax;
    in = _in;
    out = _out;
    eps = 0.01;
    ring.resize(rsize);
    rptr = 0;
    inlast = 0.0;
    rcount = 0;
    div = fmax * 2.0 * rsize / rate;                // fmax * 2 = zero-crossings/sec
                                                                // rsize / rate = sec of data in ring
                                                                // product is # zero-crossings in ring at fmax
}

void FTOV::flush()
{
    std::fill(ring.begin(), ring.end(), 0);
    rptr = 0;
    rcount = 0;
    inlast = 0.0;
}

void FTOV::execute()
{
    // 'ftov' does frequency to voltage conversion looking only at zero crossings of an
    //     AC (DC blocked) signal, i.e., ignoring signal amplitude.
    if (run)
    {
        if (ring[rptr] == 1)                              // if current ring location is a '1' ...
        {
            rcount--;                                        // decrement the count
            ring[rptr] = 0;                               // set the location to '0'
        }
        if ((inlast * in[0] < 0.0) &&                     // different signs mean zero-crossing
            (fabs (inlast - in[0]) > eps))
        {
            ring[rptr] = 1;                               // set the ring location to '1'
            rcount++;                                        // increment the count
        }
        if (++rptr == rsize) rptr = 0;                 // increment and wrap the pointer as needed
        out[0] = (float) std::min (1.0, (double)rcount / div);      // calculate the output sample
        inlast = in[size - 1];                         // save the last input sample for next buffer
        for (int i = 1; i < size; i++)
        {
            if (ring[rptr] == 1)                          // if current ring location is '1' ...
            {
                rcount--;                                    // decrement the count
                ring[rptr] = 0;                           // set the location to '0'
            }
            if ((in[i - 1] * in[i] < 0.0) &&              // different signs mean zero-crossing
                (fabs (in[i - 1] - in[i]) > eps))
            {
                ring[rptr] = 1;                           // set the ring location to '1'
                rcount++;                                    // increment the count
            }
            if (++rptr == rsize) rptr = 0;             // increment and wrap the pointer as needed
            out[i] = (float) std::min(1.0, (double)rcount / div);   // calculate the output sample
        }
    }
}
/*******************************************************************************************************/
/********************************** END Frequency to Voltage Converter *********************************/



void SSQL::compute_slews()
{
    double delta;
    double theta;
    delta = PI / (double) ntup;
    theta = 0.0;
    for (int i = 0; i <= ntup; i++)
    {
        cup[i] = muted_gain + (1.0 - muted_gain) * 0.5 * (1.0 - cos(theta));
        theta += delta;
    }
    delta = PI / (double)ntdown;
    theta = 0.0;
    for (int i = 0; i <= ntdown; i++)
    {
        cdown[i] = muted_gain + (1.0 - muted_gain) * 0.5 * (1.0 + cos(theta));
        theta += delta;
    }
}

void SSQL::calc()
{
    b1.resize(size * 2);
    dcbl = new CBL(1, size, in, b1.data(), 0, rate, 0.02);
    ibuff.resize(size);
    ftovbuff.resize(size);
    cvtr = new FTOV(
        1,
        size,
        rate,
        ftov_rsize,
        ftov_fmax,
        ibuff.data(),
        ftovbuff.data()
    );
    lpbuff.resize(size);
    filt = new DBQLP(
        1,
        size,
        ftovbuff.data(),
        lpbuff.data(),
        rate,
        11.3,
        1.0,
        1.0,
        1
    );
    wdbuff.resize(size);
    tr_signal.resize(size);
    // window detector
    wdmult = exp (-1.0 / (rate * wdtau));
    wdaverage = 0.0;
    // trigger
    tr_voltage = tr_thresh;
    mute_mult = 1.0 - exp (-1.0 / (rate * tr_tau_mute));
    unmute_mult = 1.0 - exp (-1.0 / (rate * tr_tau_unmute));
    // level change
    ntup = (int)(tup * rate);
    ntdown = (int)(tdown * rate);
    cup.resize(ntup + 1);
    cdown.resize(ntdown + 1);
    compute_slews();
    // control
    state = SSQLState::MUTED;
    count = 0;
}

void SSQL::decalc()
{
    delete filt;
    delete cvtr;
    delete dcbl;
}

SSQL::SSQL(
    int _run,
    int _size,
    float* _in,
    float* _out,
    int _rate,
    double _tup,
    double _tdown,
    double _muted_gain,
    double _tau_mute,
    double _tau_unmute,
    double _wthresh,
    double _tr_thresh,
    int _rsize,
    double _fmax
)
{
    run = _run;
    size = _size;
    in = _in;
    out = _out;
    rate = _rate;
    tup = _tup;
    tdown = _tdown;
    muted_gain = _muted_gain;
    tr_tau_mute = _tau_mute;
    tr_tau_unmute = _tau_unmute;
    wthresh = _wthresh;           // PRIMARY SQUELCH THRESHOLD CONTROL
    tr_thresh = _tr_thresh;       // value between tr_ss_unmute and tr_ss_mute, default = 0.8197
    tr_ss_mute = 1.0;
    tr_ss_unmute = 0.3125;
    wdtau = 0.5;
    ftov_rsize = _rsize;
    ftov_fmax = _fmax;
    calc();
}

SSQL::~SSQL()
{
    decalc();
}

void SSQL::flush()
{
    std::fill(b1.begin(), b1.end(), 0);
    dcbl->flush();
    std::fill(ibuff.begin(), ibuff.end(), 0);
    std::fill(ftovbuff.begin(), ftovbuff.end(), 0);
    cvtr->flush();
    std::fill(lpbuff.begin(), lpbuff.end(), 0);
    filt->flush();
    std::fill(wdbuff.begin(), wdbuff.end(), 0);
    std::fill(tr_signal.begin(), tr_signal.end(), 0);
}


void SSQL::execute()
{
    if (run)
    {
        dcbl->execute();                                         // dc block the input signal
        for (int i = 0; i < size; i++)                       // extract 'I' component
            ibuff[i] = b1[2 * i];
        cvtr->execute();                                        // convert frequency to voltage, ignoring amplitude
        // WriteAudioWDSP(20.0, rate, size, ftovbuff, 4, 0.99);
        filt->execute();                                       // low-pass filter
        // WriteAudioWDSP(20.0, rate, size, lpbuff, 4, 0.99);
        // calculate the output of the window detector for each sample
        for (int i = 0; i < size; i++)
        {
            wdaverage = wdmult * wdaverage + (1.0 - wdmult) * lpbuff[i];
            if ((lpbuff[i] - wdaverage) > wthresh || (wdaverage - lpbuff[i]) > wthresh)
                wdbuff[i] = 0;       // signal unmute
            else
                wdbuff[i] = 1;       // signal mute
        }
        // calculate the trigger signal for each sample
        for (int i = 0; i < size; i++)
        {
            if (wdbuff[i] == 0)
                tr_voltage += (tr_ss_unmute - tr_voltage) * unmute_mult;
            if (wdbuff[i] == 1)
                tr_voltage += (tr_ss_mute - tr_voltage) * mute_mult;
            if (tr_voltage > tr_thresh) tr_signal[i] = 0;  // muted
            else                              tr_signal[i] = 1;  // unmuted
        }
        // execute state machine; calculate audio output
        for (int i = 0; i < size; i++)
        {
            switch (state)
            {
            case SSQLState::MUTED:
                if (tr_signal[i] == 1)
                {
                    state = SSQLState::INCREASE;
                    count = ntup;
                }
                out[2 * i + 0] = (float) (muted_gain * in[2 * i + 0]);
                out[2 * i + 1] = (float) (muted_gain * in[2 * i + 1]);
                break;
            case SSQLState::INCREASE:
                out[2 * i + 0] = (float) (in[2 * i + 0] * cup[ntup - count]);
                out[2 * i + 1] = (float) (in[2 * i + 1] * cup[ntup - count]);
                if (count-- == 0)
                    state = SSQLState::UNMUTED;
                break;
            case SSQLState::UNMUTED:
                if (tr_signal[i] == 0)
                {
                    state = SSQLState::DECREASE;
                    count = ntdown;
                }
                out[2 * i + 0] = in[2 * i + 0];
                out[2 * i + 1] = in[2 * i + 1];
                break;
            case SSQLState::DECREASE:
                out[2 * i + 0] = (float) (in[2 * i + 0] * cdown[ntdown - count]);
                out[2 * i + 1] = (float) (in[2 * i + 1] * cdown[ntdown - count]);
                if (count-- == 0)
                    state = SSQLState::MUTED;
                break;
            }
        }
    }
    else if (in != out)
        std::copy(in, in + size * 2, out);
}

void SSQL::setBuffers(float* _in, float* _out)
{
    decalc();
    in = _in;
    out = _out;
    calc();
}

void SSQL::setSamplerate(int _rate)
{
    decalc();
    rate = _rate;
    calc();
}

void SSQL::setSize(int _size)
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

void SSQL::setRun(int _run)
{
    run = _run;
}

void SSQL::setThreshold(double _threshold)
{
    // 'threshold' should be between 0.0 and 1.0
    // WU2O testing:  0.16 is a good default for 'threshold'; => 0.08 for 'wthresh'
    wthresh = _threshold / 2.0;
}

void SSQL::setTauMute(double _tau_mute)
{
    // reasonable (wide) range is 0.1 to 2.0
    // WU2O testing:  0.1 is good default value
    tr_tau_mute = _tau_mute;
    mute_mult = 1.0 - exp (-1.0 / (rate * tr_tau_mute));
}

void SSQL::setTauUnMute(double _tau_unmute)
{
    // reasonable (wide) range is 0.1 to 1.0
    // WU2O testing:  0.1 is good default value
    tr_tau_unmute = _tau_unmute;
    unmute_mult = 1.0 - exp (-1.0 / (rate * tr_tau_unmute));
}

} // namespace WDSP
