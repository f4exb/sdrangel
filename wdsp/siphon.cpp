/*  siphon.c

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

#include "comm.hpp"
#include "meterlog10.hpp"
#include "siphon.hpp"

namespace WDSP {

void SIPHON::build_window()
{
    int i;
    double arg0, cosphi;
    double sum, scale;
    arg0 = 2.0 * PI / ((double) fftsize - 1.0);
    sum = 0.0;
    for (i = 0; i < fftsize; i++)
    {
        cosphi = cos (arg0 * (float)i);
        window[i] =  + 6.3964424114390378e-02
          + cosphi *  ( - 2.3993864599352804e-01
          + cosphi *  ( + 3.5015956323820469e-01
          + cosphi *  ( - 2.4774111897080783e-01
          + cosphi *  ( + 8.5438256055858031e-02
          + cosphi *  ( - 1.2320203369293225e-02
          + cosphi *  ( + 4.3778825791773474e-04 ))))));
        sum += window[i];
    }
    scale = 1.0 / sum;
    for (i = 0; i < fftsize; i++)
        window[i] *= scale;
}

SIPHON::SIPHON(
    int _run,
    int _position,
    int _mode,
    int _disp,
    int _insize,
    float* _in,
    int _sipsize,
    int _fftsize,
    int _specmode
)
{
    run = _run;
    position = _position;
    mode = _mode;
    disp = _disp;
    insize = _insize;
    in = _in;
    sipsize = _sipsize;   // NOTE:  sipsize MUST BE A POWER OF TWO!!
    fftsize = _fftsize;
    specmode = _specmode;
    sipbuff.resize(sipsize * 2); // (float *) malloc0 (sipsize * sizeof (complex));
    idx = 0;
    sipout.resize(sipsize * 2); // (float *) malloc0 (sipsize * sizeof (complex));
    specout.resize(fftsize * 2); // (float *) malloc0 (fftsize * sizeof (complex));
    sipplan = fftwf_plan_dft_1d (fftsize, (fftwf_complex *) sipout.data(), (fftwf_complex *) specout.data(), FFTW_FORWARD, FFTW_PATIENT);
    window.resize(fftsize * 2); // (float *) malloc0 (fftsize * sizeof (complex));
    build_window();
}

SIPHON::~SIPHON()
{
    fftwf_destroy_plan (sipplan);
}

void SIPHON::flush()
{
    std::fill(sipbuff.begin(), sipbuff.end(), 0);
    std::fill(sipout.begin(),  sipout.end(), 0);
    std::fill(specout.begin(), specout.end(), 0);
    idx = 0;
}

void SIPHON::execute(int pos)
{
    int first, second;

    if (run && position == pos)
    {
        switch (mode)
        {
        case 0:
            if (insize >= sipsize)
                std::copy(&(in[2 * (insize - sipsize)]), &(in[2 * (insize - sipsize)]) + sipsize * 2, sipbuff.begin());
            else
            {
                if (insize > (sipsize - idx))
                {
                    first = sipsize - idx;
                    second = insize - first;
                }
                else
                {
                    first = insize;
                    second = 0;
                }
                std::copy(in, in + first * 2, sipbuff.begin() + 2 * idx);
                std::copy(in + 2 * first, in + 2 * first + second * 2, sipbuff.begin());
                if ((idx += insize) >= sipsize) idx -= sipsize;
            }
            break;
        case 1:
            // Spectrum0 (1, disp, 0, 0, in);
            break;
        }
    }
}

void SIPHON::setBuffers(float* _in)
{
    in = _in;
}

void SIPHON::setSamplerate(int)
{
    flush();
}

void SIPHON::setSize(int size)
{
    insize = size;
    flush();
}

void SIPHON::suck()
{
    if (outsize <= sipsize)
    {
        int mask = sipsize - 1;
        int j = (idx - outsize) & mask;
        int size = sipsize - j;
        if (size >= outsize)
            std::copy(&(sipbuff[2 * j]), &(sipbuff[2 * j]) + outsize * 2, sipout.begin());
        else
        {
            std::copy(&(sipbuff[2 * j]), &(sipbuff[2 * j]) + size * 2, sipout.begin());
            std::copy(sipbuff.begin(), sipbuff.begin() + (outsize - size) * 2, &(sipout[2 * size]));
        }
    }
}

void SIPHON::sip_spectrum()
{
    int i;
    for (i = 0; i < fftsize; i++)
    {
        sipout[2 * i + 0] *= window[i];
        sipout[2 * i + 1] *= window[i];
    }
    fftwf_execute (sipplan);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void SIPHON::getaSipF(float* _out, int _size)
{   // return raw samples as floats
    outsize = _size;
    suck ();

    for (int i = 0; i < _size; i++) {
        _out[i] = (float) sipout[2 * i + 0];
    }
}

void SIPHON::getaSipF1(float* _out, int _size)
{   // return raw samples as floats
    outsize = _size;
    suck();

    for (int i = 0; i < _size; i++)
    {
        _out[2 * i + 0] = (float) sipout[2 * i + 0];
        _out[2 * i + 1] = (float) sipout[2 * i + 1];
    }
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void SIPHON::setSipPosition(int _pos)
{
    position = _pos;
}

void SIPHON::setSipMode(int _mode)
{
    mode = _mode;
}

void SIPHON::setSipDisplay(int _disp)
{
    disp = _disp;
}

void SIPHON::setSipSpecmode(int _mode)
{
    if (_mode == 0)
        specmode = 0;
    else
        specmode = 1;
}

void SIPHON::getSpecF1(float* _out)
{   // return spectrum magnitudes in dB
    int i, j, mid, m, n;
    outsize = fftsize;
    suck();
    sip_spectrum();
    mid = fftsize / 2;

    if (specmode != 1)
    {
        // swap the halves of the spectrum
        for (i = 0, j = mid; i < mid; i++, j++)
        {
            _out[i] = (float)(10.0 * MemLog::mlog10 (specout[2 * j + 0] * specout[2 * j + 0] + specout[2 * j + 1] * specout[2 * j + 1] + 1.0e-60));
            _out[j] = (float)(10.0 * MemLog::mlog10 (specout[2 * i + 0] * specout[2 * i + 0] + specout[2 * i + 1] * specout[2 * i + 1] + 1.0e-60));
        }
    }
    else
    {
        // mirror each half of the spectrum in-place
        for (i = 0, j = mid - 1, m = mid, n = fftsize - 1; i < mid; i++, j--, m++, n--)
        {
            _out[i] = (float)(10.0 * MemLog::mlog10 (specout[2 * j + 0] * specout[2 * j + 0] + specout[2 * j + 1] * specout[2 * j + 1] + 1.0e-60));
            _out[m] = (float)(10.0 * MemLog::mlog10 (specout[2 * n + 0] * specout[2 * n + 0] + specout[2 * n + 1] * specout[2 * n + 1] + 1.0e-60));
        }
    }
}

/********************************************************************************************************
*                                                                                                       *
*                                       CALLS FOR EXTERNAL USE                                          *
*                                                                                                       *
********************************************************************************************************/

/*
#define MAX_EXT_SIPHONS (2)                                 // maximum number of Siphons called from outside wdsp
__declspec (align (16)) SIPHON psiphon[MAX_EXT_SIPHONS];    // array of pointers for Siphons used EXTERNAL to wdsp


PORT
void create_siphonEXT (int id, int run, int insize, int sipsize, int fftsize, int specmode)
{
    psiphon[id] = create_siphon (run, 0, 0, 0, insize, 0, sipsize, fftsize, specmode);
}

PORT
void destroy_siphonEXT (int id)
{
    destroy_siphon (psiphon[id]);
}

PORT
void flush_siphonEXT (int id)
{
    flush_siphon (psiphon[id]);
}

PORT
void xsiphonEXT (int id, float* buff)
{
    SIPHON a = psiphon[id];
    a->in = buff;
    xsiphon (a, 0);
}

PORT
void GetaSipF1EXT (int id, float* out, int size)
{   // return raw samples as floats
    SIPHON a = psiphon[id];
    int i;
    a->update.lock();
    a->outsize = size;
    suck (a);
    a->update.unlock();
    for (i = 0; i < size; i++)
    {
        out[2 * i + 0] = (float)a->sipout[2 * i + 0];
        out[2 * i + 1] = (float)a->sipout[2 * i + 1];
    }
}

PORT
void SetSiphonInsize (int id, int size)
{
    SIPHON a = psiphon[id];
    a->update.lock();
    a->insize = size;
    a->update.unlock();
}
*/

} // namespace WDSP

