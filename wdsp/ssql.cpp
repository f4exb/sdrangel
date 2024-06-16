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
#include "iir.hpp"
#include "RXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                      Frequency to Voltage Converter                                   *
*                                                                                                       *
********************************************************************************************************/

FTOV* FTOV::create_ftov (int run, int size, int rate, int rsize, double fmax, double* in, double* out)
{
    FTOV *a = new FTOV;
    a->run = run;
    a->size = size;
    a->rate = rate;
    a->rsize = rsize;
    a->fmax = fmax;
    a->in = in;
    a->out = out;
    a->eps = 0.01;
    a->ring = new int[a->rsize]; // (int*) malloc0 (a->rsize * sizeof (int));
    a->rptr = 0;
    a->inlast = 0.0;
    a->rcount = 0;
    a->div = a->fmax * 2.0 * a->rsize / a->rate;                // fmax * 2 = zero-crossings/sec
                                                                // rsize / rate = sec of data in ring
                                                                // product is # zero-crossings in ring at fmax
    return a;
}

void FTOV::destroy_ftov (FTOV *a)
{
    delete[] (a->ring);
    delete (a);
}

void FTOV::flush_ftov (FTOV *a)
{
    memset (a->ring, 0, a->rsize * sizeof (int));
    a->rptr = 0;
    a->rcount = 0;
    a->inlast = 0.0;
}

void FTOV::xftov (FTOV *a)
{
    // 'ftov' does frequency to voltage conversion looking only at zero crossings of an
    //     AC (DC blocked) signal, i.e., ignoring signal amplitude.
    if (a->run)
    {
        if (a->ring[a->rptr] == 1)                              // if current ring location is a '1' ...
        {
            a->rcount--;                                        // decrement the count
            a->ring[a->rptr] = 0;                               // set the location to '0'
        }
        if ((a->inlast * a->in[0] < 0.0) &&                     // different signs mean zero-crossing
            (fabs (a->inlast - a->in[0]) > a->eps))
        {
            a->ring[a->rptr] = 1;                               // set the ring location to '1'
            a->rcount++;                                        // increment the count
        }
        if (++a->rptr == a->rsize) a->rptr = 0;                 // increment and wrap the pointer as needed
        a->out[0] = std::min (1.0, (double)a->rcount / a->div);      // calculate the output sample
        a->inlast = a->in[a->size - 1];                         // save the last input sample for next buffer
        for (int i = 1; i < a->size; i++)
        {
            if (a->ring[a->rptr] == 1)                          // if current ring location is '1' ...
            {
                a->rcount--;                                    // decrement the count
                a->ring[a->rptr] = 0;                           // set the location to '0'
            }
            if ((a->in[i - 1] * a->in[i] < 0.0) &&              // different signs mean zero-crossing
                (fabs (a->in[i - 1] - a->in[i]) > a->eps))
            {
                a->ring[a->rptr] = 1;                           // set the ring location to '1'
                a->rcount++;                                    // increment the count
            }
            if (++a->rptr == a->rsize) a->rptr = 0;             // increment and wrap the pointer as needed
            a->out[i] = std::min(1.0, (double)a->rcount / a->div);   // calculate the output sample
        }
    }
}
/*******************************************************************************************************/
/********************************** END Frequency to Voltage Converter *********************************/



void SSQL::compute_ssql_slews(SSQL *a)
{
    int i;
    double delta, theta;
    delta = PI / (double)a->ntup;
    theta = 0.0;
    for (i = 0; i <= a->ntup; i++)
    {
        a->cup[i] = a->muted_gain + (1.0 - a->muted_gain) * 0.5 * (1.0 - cos(theta));
        theta += delta;
    }
    delta = PI / (double)a->ntdown;
    theta = 0.0;
    for (i = 0; i <= a->ntdown; i++)
    {
        a->cdown[i] = a->muted_gain + (1.0 - a->muted_gain) * 0.5 * (1.0 + cos(theta));
        theta += delta;
    }
}

void SSQL::calc_ssql (SSQL *a)
{
    a->b1 = new double[a->size * 2]; // (double*) malloc0 (a->size * sizeof (complex));
    a->dcbl = CBL::create_cbl (1, a->size, a->in, a->b1, 0, a->rate, 0.02);
    a->ibuff = new double[a->size]; // (double*) malloc0 (a->size * sizeof (double));
    a->ftovbuff = new double[a->size]; // (double*) malloc0(a->size * sizeof (double));
    a->cvtr = FTOV::create_ftov (1, a->size, a->rate, a->ftov_rsize, a->ftov_fmax, a->ibuff, a->ftovbuff);
    a->lpbuff = new double[a->size]; // (double*) malloc0 (a->size * sizeof (double));
    a->filt = DBQLP::create_dbqlp (1, a->size, a->ftovbuff, a->lpbuff, a->rate, 11.3, 1.0, 1.0, 1);
    a->wdbuff = new int[a->size]; // (int*) malloc0 (a->size * sizeof (int));
    a->tr_signal = new int[a->size]; // (int*) malloc0 (a->size * sizeof (int));
    // window detector
    a->wdmult = exp (-1.0 / (a->rate * a->wdtau));
    a->wdaverage = 0.0;
    // trigger
    a->tr_voltage = a->tr_thresh;
    a->mute_mult = 1.0 - exp (-1.0 / (a->rate * a->tr_tau_mute));
    a->unmute_mult = 1.0 - exp (-1.0 / (a->rate * a->tr_tau_unmute));
    // level change
    a->ntup = (int)(a->tup * a->rate);
    a->ntdown = (int)(a->tdown * a->rate);
    a->cup = new double[a->ntup + 1]; // (double*) malloc0 ((a->ntup + 1) * sizeof (double));
    a->cdown = new double[a->ntdown + 1]; // (double*) malloc0 ((a->ntdown + 1) * sizeof (double));
    compute_ssql_slews (a);
    // control
    a->state = 0;
    a->count = 0;
}

void SSQL::decalc_ssql (SSQL *a)
{
    delete[] (a->tr_signal);
    delete[] (a->wdbuff);
    DBQLP::destroy_dbqlp (a->filt);
    delete[] (a->lpbuff);
    FTOV::destroy_ftov (a->cvtr);
    delete[] (a->ftovbuff);
    delete[] (a->ibuff);
    CBL::destroy_cbl (a->dcbl);
    delete[] (a->b1);
    delete[] (a->cdown);
    delete[] (a->cup);
}

SSQL* SSQL::create_ssql (
    int run,
    int size,
    double* in,
    double* out,
    int rate,
    double tup,
    double tdown,
    double muted_gain,
    double tau_mute,
    double tau_unmute,
    double wthresh,
    double tr_thresh,
    int rsize,
    double fmax
)
{
    SSQL *a = new SSQL;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->tup = tup;
    a->tdown = tdown;
    a->muted_gain = muted_gain;
    a->tr_tau_mute = tau_mute;
    a->tr_tau_unmute = tau_unmute;
    a->wthresh = wthresh;           // PRIMARY SQUELCH THRESHOLD CONTROL
    a->tr_thresh = tr_thresh;       // value between tr_ss_unmute and tr_ss_mute, default = 0.8197
    a->tr_ss_mute = 1.0;
    a->tr_ss_unmute = 0.3125;
    a->wdtau = 0.5;
    a->ftov_rsize = rsize;
    a->ftov_fmax = fmax;
    calc_ssql (a);
    return a;
}

void SSQL::destroy_ssql (SSQL *a)
{
    decalc_ssql (a);
    delete (a);
}

void SSQL::flush_ssql (SSQL *a)
{

    memset (a->b1, 0, a->size * sizeof (dcomplex));
    CBL::flush_cbl (a->dcbl);
    memset (a->ibuff, 0, a->size * sizeof (double));
    memset (a->ftovbuff, 0, a->size * sizeof (double));
    FTOV::flush_ftov (a->cvtr);
    memset (a->lpbuff, 0, a->size * sizeof (double));
    DBQLP::flush_dbqlp (a->filt);
    memset (a->wdbuff, 0, a->size * sizeof (int));
    memset (a->tr_signal, 0, a->size * sizeof (int));
}

enum _ssqlstate
{
    MUTED,
    INCREASE,
    UNMUTED,
    DECREASE
};

void SSQL::xssql (SSQL *a)
{
    if (a->run)
    {
        CBL::xcbl (a->dcbl);                                         // dc block the input signal
        for (int i = 0; i < a->size; i++)                       // extract 'I' component
            a->ibuff[i] = a->b1[2 * i];
        FTOV::xftov (a->cvtr);                                        // convert frequency to voltage, ignoring amplitude
        // WriteAudioWDSP(20.0, a->rate, a->size, a->ftovbuff, 4, 0.99);
        DBQLP::xdbqlp (a->filt);                                       // low-pass filter
        // WriteAudioWDSP(20.0, a->rate, a->size, a->lpbuff, 4, 0.99);
        // calculate the output of the window detector for each sample
        for (int i = 0; i < a->size; i++)
        {
            a->wdaverage = a->wdmult * a->wdaverage + (1.0 - a->wdmult) * a->lpbuff[i];
            if ((a->lpbuff[i] - a->wdaverage) > a->wthresh || (a->wdaverage - a->lpbuff[i]) > a->wthresh)
                a->wdbuff[i] = 0;       // signal unmute
            else
                a->wdbuff[i] = 1;       // signal mute
        }
        // calculate the trigger signal for each sample
        for (int i = 0; i < a->size; i++)
        {
            if (a->wdbuff[i] == 0)
                a->tr_voltage += (a->tr_ss_unmute - a->tr_voltage) * a->unmute_mult;
            if (a->wdbuff[i] == 1)
                a->tr_voltage += (a->tr_ss_mute - a->tr_voltage) * a->mute_mult;
            if (a->tr_voltage > a->tr_thresh) a->tr_signal[i] = 0;  // muted
            else                              a->tr_signal[i] = 1;  // unmuted
        }
        // execute state machine; calculate audio output
        for (int i = 0; i < a->size; i++)
        {
            switch (a->state)
            {
            case MUTED:
                if (a->tr_signal[i] == 1)
                {
                    a->state = INCREASE;
                    a->count = a->ntup;
                }
                a->out[2 * i + 0] = a->muted_gain * a->in[2 * i + 0];
                a->out[2 * i + 1] = a->muted_gain * a->in[2 * i + 1];
                break;
            case INCREASE:
                a->out[2 * i + 0] = a->in[2 * i + 0] * a->cup[a->ntup - a->count];
                a->out[2 * i + 1] = a->in[2 * i + 1] * a->cup[a->ntup - a->count];
                if (a->count-- == 0)
                    a->state = UNMUTED;
                break;
            case UNMUTED:
                if (a->tr_signal[i] == 0)
                {
                    a->state = DECREASE;
                    a->count = a->ntdown;
                }
                a->out[2 * i + 0] = a->in[2 * i + 0];
                a->out[2 * i + 1] = a->in[2 * i + 1];
                break;
            case DECREASE:
                a->out[2 * i + 0] = a->in[2 * i + 0] * a->cdown[a->ntdown - a->count];
                a->out[2 * i + 1] = a->in[2 * i + 1] * a->cdown[a->ntdown - a->count];
                if (a->count-- == 0)
                    a->state = MUTED;
                break;
            }
        }
    }
    else if (a->in != a->out)
        memcpy (a->out, a->in, a->size * sizeof(dcomplex));
}

void SSQL::setBuffers_ssql (SSQL *a, double* in, double* out)
{
    decalc_ssql (a);
    a->in = in;
    a->out = out;
    calc_ssql (a);
}

void SSQL::setSamplerate_ssql (SSQL *a, int rate)
{
    decalc_ssql (a);
    a->rate = rate;
    calc_ssql (a);
}

void SSQL::setSize_ssql (SSQL *a, int size)
{
    decalc_ssql (a);
    a->size = size;
    calc_ssql (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void SSQL::SetSSQLRun (RXA& rxa, int run)
{
    rxa.csDSP.lock();
    rxa.ssql.p->run = run;
    rxa.csDSP.unlock();
}

void SSQL::SetSSQLThreshold (RXA& rxa, double threshold)
{
    // 'threshold' should be between 0.0 and 1.0
    // WU2O testing:  0.16 is a good default for 'threshold'; => 0.08 for 'wthresh'
    rxa.csDSP.lock();
    rxa.ssql.p->wthresh = threshold / 2.0;
    rxa.csDSP.unlock();
}

void SSQL::SetSSQLTauMute (RXA& rxa, double tau_mute)
{
    // reasonable (wide) range is 0.1 to 2.0
    // WU2O testing:  0.1 is good default value
    SSQL *a = rxa.ssql.p;
    rxa.csDSP.lock();
    a->tr_tau_mute = tau_mute;
    a->mute_mult = 1.0 - exp (-1.0 / (a->rate * a->tr_tau_mute));
    rxa.csDSP.unlock();
}

void SSQL::SetSSQLTauUnMute (RXA& rxa, double tau_unmute)
{
    // reasonable (wide) range is 0.1 to 1.0
    // WU2O testing:  0.1 is good default value
    SSQL *a = rxa.ssql.p;
    rxa.csDSP.lock();
    a->tr_tau_unmute = tau_unmute;
    a->unmute_mult = 1.0 - exp (-1.0 / (a->rate * a->tr_tau_unmute));
    rxa.csDSP.unlock();
}

} // namespace WDSP
