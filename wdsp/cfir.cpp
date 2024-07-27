/*  cfir.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014, 2016, 2021 Warren Pratt, NR0V
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
#include "cfir.hpp"
#include "fir.hpp"
#include "fircore.hpp"
#include "TXA.hpp"

namespace WDSP {

void CFIR::calc_cfir (CFIR *a)
{
    float* impulse;
    a->scale = 1.0 / (float)(2 * a->size);
    impulse = cfir_impulse (a->nc, a->DD, a->R, a->Pairs, a->runrate, a->cicrate, a->cutoff, a->xtype, a->xbw, 1, a->scale, a->wintype);
    a->p = FIRCORE::create_fircore (a->size, a->in, a->out, a->nc, a->mp, impulse);
    delete[] (impulse);
}

void CFIR::decalc_cfir (CFIR *a)
{
    FIRCORE::destroy_fircore (a->p);
}

CFIR* CFIR::create_cfir (
    int run,
    int size,
    int nc,
    int mp,
    float* in,
    float* out,
    int runrate,
    int cicrate,
    int DD,
    int R,
    int Pairs,
    double cutoff,
    int xtype,
    double xbw,
    int wintype
)
//  run:  0 - no action; 1 - operate
//  size:  number of complex samples in an input buffer to the CFIR filter
//  nc:  number of filter coefficients
//  mp:  minimum phase flag
//  in:  pointer to the input buffer
//  out:  pointer to the output buffer
//  rate:  samplerate
//  DD:  differential delay of the CIC to be compensated (usually 1 or 2)
//  R:  interpolation factor of CIC
//  Pairs:  number of comb-integrator pairs in the CIC
//  cutoff:  cutoff frequency
//  xtype:  0 - fourth power transition; 1 - raised cosine transition; 2 - brick wall
//  xbw:  width of raised cosine transition
{
    CFIR *a = new CFIR;
    a->run = run;
    a->size = size;
    a->nc = nc;
    a->mp = mp;
    a->in = in;
    a->out = out;
    a->runrate = runrate;
    a->cicrate = cicrate;
    a->DD = DD;
    a->R = R;
    a->Pairs = Pairs;
    a->cutoff = cutoff;
    a->xtype = xtype;
    a->xbw = xbw;
    a->wintype = wintype;
    calc_cfir (a);
    return a;
}

void CFIR::destroy_cfir (CFIR *a)
{
    decalc_cfir (a);
    delete (a);
}

void CFIR::flush_cfir (CFIR *a)
{
    FIRCORE::flush_fircore (a->p);
}

void CFIR::xcfir (CFIR *a)
{
    if (a->run)
        FIRCORE::xfircore (a->p);
    else if (a->in != a->out)
        std::copy( a->in,  a->in + a->size * 2, a->out);
}

void CFIR::setBuffers_cfir (CFIR *a, float* in, float* out)
{
    decalc_cfir (a);
    a->in = in;
    a->out = out;
    calc_cfir (a);
}

void CFIR::setSamplerate_cfir (CFIR *a, int rate)
{
    decalc_cfir (a);
    a->runrate = rate;
    calc_cfir (a);
}

void CFIR::setSize_cfir (CFIR *a, int size)
{
    decalc_cfir (a);
    a->size = size;
    calc_cfir (a);
}

void CFIR::setOutRate_cfir (CFIR *a, int rate)
{
    decalc_cfir (a);
    a->cicrate = rate;
    calc_cfir (a);
}

float* CFIR::cfir_impulse (
    int N,
    int DD,
    int R,
    int Pairs,
    double runrate,
    double cicrate,
    double cutoff,
    int xtype,
    double xbw,
    int rtype,
    double scale,
    int wintype
)
{
    // N:       number of impulse response samples
    // DD:      differential delay used in the CIC filter
    // R:       interpolation / decimation factor of the CIC
    // Pairs:   number of comb-integrator pairs in the CIC
    // runrate: sample rate at which this filter is to run (assumes there may be flat interp. between this filter and the CIC)
    // cicrate: sample rate at interface to CIC
    // cutoff:  cutoff frequency
    // xtype:   transition type, 0 for 4th-power rolloff, 1 for raised cosine, 2 for brick wall
    // xbw:     transition bandwidth for raised cosine
    // rtype:   0 for real output, 1 for complex output
    // scale:   scale factor to be applied to the output
    int i, j;
    double tmp, local_scale, ri, mag, fn;
    float* impulse;
    float* A = new float[N]; // (float *) malloc0 (N * sizeof (float));
    double ft = cutoff / cicrate;                                       // normalized cutoff frequency
    int u_samps = (N + 1) / 2;                                          // number of unique samples,  OK for odd or even N
    int c_samps = (int)(cutoff / runrate * N) + (N + 1) / 2 - N / 2;    // number of unique samples within bandpass, OK for odd or even N
    int x_samps = (int)(xbw / runrate * N);                             // number of unique samples in transition region, OK for odd or even N
    double offset = 0.5 - 0.5 * (float)((N + 1) / 2 - N / 2);          // sample offset from center, OK for odd or even N
    double* xistion = new double[x_samps + 1]; // (float *) malloc0 ((x_samps + 1) * sizeof (float));
    double delta = PI / (float)x_samps;
    double L = cicrate / runrate;
    double phs = 0.0;
    for (i = 0; i <= x_samps; i++)
    {
        xistion[i] = 0.5 * (cos (phs) + 1.0);
        phs += delta;
    }
    if ((tmp = DD * R * sin (PI * ft / R) / sin (PI * DD * ft)) < 0.0)  //normalize by peak gain
        tmp = -tmp;
    local_scale = scale / pow (tmp, Pairs);
    if (xtype == 0)
    {
        for (i = 0, ri = offset; i < u_samps; i++, ri += 1.0)
        {
            fn = ri / (L * (float)N);
            if (fn <= ft)
            {
                if (fn == 0.0)
                    tmp = 1.0;
                else if ((tmp = DD * R * sin (PI * fn / R) / sin (PI * DD * fn)) < 0.0)
                    tmp = -tmp;
                mag = pow (tmp, Pairs) * local_scale;
            }
            else
                mag *= (ft * ft * ft * ft) / (fn * fn * fn * fn);
            A[i] = mag;
        }
    }
    else if (xtype == 1)
    {
        for (i = 0, ri = offset; i < u_samps; i++, ri += 1.0)
        {
            fn = ri / (L *(float)N);
            if (i < c_samps)
            {
                if (fn == 0.0) tmp = 1.0;
                else if ((tmp = DD * R * sin (PI * fn / R) / sin (PI * DD * fn)) < 0.0)
                    tmp = -tmp;
                mag = pow (tmp, Pairs) * local_scale;
                A[i] = mag;
            }
            else if ( i >= c_samps && i <= c_samps + x_samps)
                A[i] = mag * xistion[i - c_samps];
            else
                A[i] = 0.0;
        }
    }
    else if (xtype == 2)
    {
        for (i = 0, ri = offset; i < u_samps; i++, ri += 1.0)
        {
            fn = ri / (L * (float)N);
            if (fn <= ft)
            {
                if (fn == 0.0) tmp = 1.0;
                else if ((tmp = DD * R * sin(PI * fn / R) / sin(PI * DD * fn)) < 0.0)
                    tmp = -tmp;
                mag = pow (tmp, Pairs) * local_scale;
            }
            else
                mag = 0.0;
            A[i] = mag;
        }
    }
    if (N & 1)
        for (i = u_samps, j = 2; i < N; i++, j++)
            A[i] = A[u_samps - j];
    else
        for (i = u_samps, j = 1; i < N; i++, j++)
            A[i] = A[u_samps - j];
    impulse = FIR::fir_fsamp (N, A, rtype, 1.0, wintype);
    // print_impulse ("cfirImpulse.txt", N, impulse, 1, 0);
    delete[] A;
    delete[] xistion;
    return impulse;
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void CFIR::SetCFIRRun (TXA& txa, int run)
{
    txa.cfir->run = run;
}

void CFIR::SetCFIRNC(TXA& txa, int nc)
{
    // NOTE:  'nc' must be >= 'size'
    CFIR *a;
    a = txa.cfir;

    if (a->nc != nc)
    {
        a->nc = nc;
        decalc_cfir(a);
        calc_cfir(a);
    }
}

} // namespace WDSP
