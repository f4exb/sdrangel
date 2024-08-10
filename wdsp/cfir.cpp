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

void CFIR::calc()
{
    std::vector<float> impulse;
    scale = 1.0 / (float)(2 * size);
    cfir_impulse (impulse, nc, DD, R, Pairs, runrate, cicrate, cutoff, xtype, xbw, 1, scale, wintype);
    p = new FIRCORE(size, in, out, mp, impulse);
}

void CFIR::decalc()
{
    delete p;
}

CFIR::CFIR(
    int _run,
    int _size,
    int _nc,
    int _mp,
    float* _in,
    float* _out,
    int _runrate,
    int _cicrate,
    int _DD,
    int _R,
    int _Pairs,
    double _cutoff,
    int _xtype,
    double _xbw,
    int _wintype
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
    run = _run;
    size = _size;
    nc = _nc;
    mp = _mp;
    in = _in;
    out = _out;
    runrate = _runrate;
    cicrate = _cicrate;
    DD = _DD;
    R = _R;
    Pairs = _Pairs;
    cutoff = _cutoff;
    xtype = _xtype;
    xbw = _xbw;
    wintype = _wintype;
    calc();
}

CFIR::~CFIR()
{
    decalc();
}

void CFIR::flush()
{
    p->flush();
}

void CFIR::execute()
{
    if (run)
        p->execute();
    else if (in != out)
        std::copy( in,  in + size * 2, out);
}

void CFIR::setBuffers(float* _in, float* _out)
{
    decalc();
    in = _in;
    out = _out;
    calc();
}

void CFIR::setSamplerate(int rate)
{
    decalc();
    runrate = rate;
    calc();
}

void CFIR::setSize(int _size)
{
    decalc();
    size = _size;
    calc();
}

void CFIR::setOutRate(int rate)
{
    decalc();
    cicrate = rate;
    calc();
}

void CFIR::cfir_impulse (
    std::vector<float>& impulse,
    int _N,
    int _DD,
    int _R,
    int _Pairs,
    double _runrate,
    double _cicrate,
    double _cutoff,
    int _xtype,
    double _xbw,
    int _rtype,
    double _scale,
    int _wintype
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
    int i;
    int j;
    double tmp;
    double local_scale;
    double ri;
    double mag = 0;
    double fn;
    std::vector<float> A(_N);
    double ft = _cutoff / _cicrate;                                       // normalized cutoff frequency
    int u_samps = (_N + 1) / 2;                                          // number of unique samples,  OK for odd or even N
    int c_samps = (int)(_cutoff / _runrate * _N) + (_N + 1) / 2 - _N / 2;    // number of unique samples within bandpass, OK for odd or even N
    auto x_samps = (int)(_xbw / _runrate * _N);                             // number of unique samples in transition region, OK for odd or even N
    double offset = 0.5 - 0.5 * (double)((_N + 1) / 2 - _N / 2);          // sample offset from center, OK for odd or even N
    std::vector<double> xistion(x_samps + 1);
    double delta = PI / (double)x_samps;
    double L = _cicrate / _runrate;
    double _phs = 0.0;
    for (i = 0; i <= x_samps; i++)
    {
        xistion[i] = 0.5 * (cos (_phs) + 1.0);
        _phs += delta;
    }
    if ((tmp = _DD * _R * sin (PI * ft / _R) / sin (PI * _DD * ft)) < 0.0)  //normalize by peak gain
        tmp = -tmp;
    local_scale = _scale / pow (tmp, _Pairs);
    if (_xtype == 0)
    {
        for (i = 0, ri = offset; i < u_samps; i++, ri += 1.0)
        {
            fn = ri / (L * (double) _N);
            if (fn <= ft)
            {
                if (fn == 0.0)
                    tmp = 1.0;
                else if ((tmp = _DD * _R * sin (PI * fn / _R) / sin (PI * _DD * fn)) < 0.0)
                    tmp = -tmp;
                mag = pow (tmp, _Pairs) * local_scale;
            }
            else
                mag *= (ft * ft * ft * ft) / (fn * fn * fn * fn);
            A[i] = (float) mag;
        }
    }
    else if (_xtype == 1)
    {
        for (i = 0, ri = offset; i < u_samps; i++, ri += 1.0)
        {
            fn = ri / (L *(double) _N);
            if (i < c_samps)
            {
                if (fn == 0.0) tmp = 1.0;
                else if ((tmp = _DD * _R * sin (PI * fn / _R) / sin (PI * _DD * fn)) < 0.0)
                    tmp = -tmp;
                mag = pow (tmp, _Pairs) * local_scale;
                A[i] = (float) mag;
            }
            else if ( i >= c_samps && i <= c_samps + x_samps)
                A[i] = (float) (mag * xistion[i - c_samps]);
            else
                A[i] = 0.0;
        }
    }
    else if (_xtype == 2)
    {
        for (i = 0, ri = offset; i < u_samps; i++, ri += 1.0)
        {
            fn = ri / (L * (double) _N);
            if (fn <= ft)
            {
                if (fn == 0.0) tmp = 1.0;
                else if ((tmp = _DD * _R * sin(PI * fn / _R) / sin(PI * _DD * fn)) < 0.0)
                    tmp = -tmp;
                mag = pow (tmp, _Pairs) * local_scale;
            }
            else
                mag = 0.0;
            A[i] = (float) mag;
        }
    }
    if (_N & 1)
        for (i = u_samps, j = 2; i < _N; i++, j++)
            A[i] = A[u_samps - j];
    else
        for (i = u_samps, j = 1; i < _N; i++, j++)
            A[i] = A[u_samps - j];
    impulse.resize(2 * _N);
    FIR::fir_fsamp (impulse, _N, A.data(), _rtype, 1.0, _wintype);
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void CFIR::setRun(int _run)
{
    run = _run;
}

void CFIR::setNC(int _nc)
{
    // NOTE:  'nc' must be >= 'size'

    if (nc != _nc)
    {
        nc = _nc;
        decalc();
        calc();
    }
}

} // namespace WDSP
