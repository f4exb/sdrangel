/*  eq.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2016, 2017 Warren Pratt, NR0V
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
#include "eqp.hpp"
#include "fircore.hpp"
#include "fir.hpp"

namespace WDSP {

int EQP::fEQcompare (const void * a, const void * b)
{
    if (*(float*)a < *(float*)b)
        return -1;
    else if (*(float*)a == *(float*)b)
        return 0;
    else
        return 1;
}

void EQP::eq_impulse (
    std::vector<float>& impulse,
    int N,
    int _nfreqs,
    const float* F,
    const float* G,
    double samplerate,
    double scale,
    int ctfmode,
    int wintype
)
{
    std::vector<float> fp(_nfreqs + 2);
    std::vector<float> gp(_nfreqs + 2);
    std::vector<float> A(N / 2 + 1);
    float* sary = new float[2 * _nfreqs];

    double gpreamp;
    double f;
    double frac;
    int i;
    int j;
    int mid;
    fp[0] = 0.0;
    fp[_nfreqs + 1] = 1.0;
    gpreamp = G[0];

    for (i = 1; i <= _nfreqs; i++)
    {
        fp[i] = (float) (2.0 * F[i] / samplerate);

        if (fp[i] < 0.0)
            fp[i] = 0.0;

        if (fp[i] > 1.0)
            fp[i] = 1.0;

        gp[i] = G[i];
    }

    for (i = 1, j = 0; i <= _nfreqs; i++, j+=2)
    {
        sary[j + 0] = fp[i];
        sary[j + 1] = gp[i];
    }

    qsort (sary, _nfreqs, 2 * sizeof (float), fEQcompare);

    for (i = 1, j = 0; i <= _nfreqs; i++, j+=2)
    {
        fp[i] = sary[j + 0];
        gp[i] = sary[j + 1];
    }

    gp[0] = gp[1];
    gp[_nfreqs + 1] = gp[_nfreqs];
    mid = N / 2;
    j = 0;

    if (N & 1)
    {
        for (i = 0; i <= mid; i++)
        {
            f = (double)i / (double)mid;

            while ((f > fp[j + 1]) && (j < _nfreqs))
                j++;

            frac = (f - fp[j]) / (fp[j + 1] - fp[j]);
            A[i] = (float) (pow (10.0, 0.05 * (frac * gp[j + 1] + (1.0 - frac) * gp[j] + gpreamp)) * scale);
        }
    }
    else
    {
        for (i = 0; i < mid; i++)
        {
            f = ((double)i + 0.5) / (double)mid;

            while ((f > fp[j + 1]) && (j < _nfreqs))
                j++;

            frac = (f - fp[j]) / (fp[j + 1] - fp[j]);
            A[i] = (float) (pow (10.0, 0.05 * (frac * gp[j + 1] + (1.0 - frac) * gp[j] + gpreamp)) * scale);
        }
    }

    if (ctfmode == 0)
    {
        int k;
        int low;
        int high;
        double lowmag;
        double highmag;
        double flow4;
        double fhigh4;

        if (N & 1)
        {
            low = (int)(fp[1] * mid);
            high = (int)(fp[_nfreqs] * mid + 0.5);
            lowmag = A[low];
            highmag = A[high];
            flow4 = pow((double)low / (double)mid, 4.0);
            fhigh4 = pow((double)high / (double)mid, 4.0);
            k = low;

            while (--k >= 0)
            {
                f = (double)k / (double)mid;
                lowmag *= (f * f * f * f) / flow4;
                if (lowmag < 1.0e-20) lowmag = 1.0e-20;
                A[k] = (float) lowmag;
            }

            k = high;

            while (++k <= mid)
            {
                f = (double)k / (double)mid;
                highmag *= fhigh4 / (f * f * f * f);
                if (highmag < 1.0e-20) highmag = 1.0e-20;
                A[k] = (float) highmag;
            }
        }
        else
        {
            low = (int)(fp[1] * mid - 0.5);
            high = (int)(fp[_nfreqs] * mid - 0.5);
            lowmag = A[low];
            highmag = A[high];
            flow4 = pow((double)low / (double)mid, 4.0);
            fhigh4 = pow((double)high / (double)mid, 4.0);
            k = low;

            while (--k >= 0)
            {
                f = (double)k / (double)mid;
                lowmag *= (f * f * f * f) / flow4;
                if (lowmag < 1.0e-20) lowmag = 1.0e-20;
                A[k] = (float) lowmag;
            }

            k = high;

            while (++k < mid)
            {
                f = (double)k / (double)mid;
                highmag *= fhigh4 / (f * f * f * f);
                if (highmag < 1.0e-20) highmag = 1.0e-20;
                A[k] = (float) highmag;
            }
        }
    }

    impulse.resize(2 * N);

    if (N & 1)
        FIR::fir_fsamp_odd(impulse, N, A.data(), 1, 1.0, wintype);
    else
        FIR::fir_fsamp(impulse, N, A.data(), 1, 1.0, wintype);

    delete[] sary;
}

/********************************************************************************************************
*                                                                                                       *
*                                   Partitioned Overlap-Save Equalizer                                  *
*                                                                                                       *
********************************************************************************************************/

EQP::EQP(
    int _run,
    int _size,
    int _nc,
    int _mp,
    float *_in,
    float *_out,
    int _nfreqs,
    float* _F,
    float* _G,
    int _ctfmode,
    int _wintype,
    int _samplerate
)
{
    // NOTE:  'nc' must be >= 'size'
    std::vector<float> impulse;
    run = _run;
    size = _size;
    nc = _nc;
    mp = _mp;
    in = _in;
    out = _out;
    nfreqs = _nfreqs;
    F.resize(nfreqs + 1);
    G.resize(nfreqs + 1);
    std::copy(_F, _F + (_nfreqs + 1), F.begin());
    std::copy(_G, _G + (_nfreqs + 1), G.begin());
    ctfmode = _ctfmode;
    wintype = _wintype;
    samplerate = (double) _samplerate;
    eq_impulse (impulse, nc, nfreqs, F.data(), G.data(), samplerate, 1.0 / (2.0 * size), ctfmode, wintype);
    fircore = new FIRCORE(size, in, out, mp, impulse);
}

EQP::~EQP()
{
    delete (fircore);
}

void EQP::flush()
{
    fircore->flush();
}

void EQP::execute()
{
    if (run)
        fircore->execute();
    else
        std::copy(in,  in + size * 2, out);
}

void EQP::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
    fircore->setBuffers(in, out);
}

void EQP::setSamplerate(int rate)
{
    std::vector<float> impulse;
    samplerate = rate;
    eq_impulse (impulse, nc, nfreqs, F.data(), G.data(), samplerate, 1.0 / (2.0 * size), ctfmode, wintype);
    fircore->setImpulse(impulse, 1);
}

void EQP::setSize(int _size)
{
    std::vector<float> impulse;
    size = _size;
    fircore->setSize(size);
    eq_impulse (impulse, nc, nfreqs, F.data(), G.data(), samplerate, 1.0 / (2.0 * size), ctfmode, wintype);
    fircore->setImpulse(impulse, 1);
}

/********************************************************************************************************
*                                                                                                       *
*                           Partitioned Overlap-Save Equalizer:  Public Properties                      *
*                                                                                                       *
********************************************************************************************************/

void EQP::setRun(int _run)
{
    run = _run;
}

void EQP::setNC(int _nc)
{
    std::vector<float> impulse;

    if (nc != _nc)
    {
        nc = _nc;
        eq_impulse (impulse, nc, nfreqs, F.data(), G.data(), samplerate, 1.0 / (2.0 * size), ctfmode, wintype);
        fircore->setNc(impulse);
    }
}

void EQP::setMP(int _mp)
{
    if (mp != _mp)
    {
        mp = _mp;
        fircore->setMp(mp);
    }
}

void EQP::setProfile(int _nfreqs, const float* _F, const float* _G)
{
    std::vector<float> impulse;
    nfreqs = _nfreqs;
    F.resize(nfreqs + 1);
    G.resize(nfreqs + 1);
    std::copy(_F, _F + (_nfreqs + 1), F.begin());
    std::copy(_G, _G + (_nfreqs + 1), G.begin());
    eq_impulse (impulse, nc, nfreqs, F.data(), G.data(), samplerate, 1.0 / (2.0 * size), ctfmode, wintype);
    fircore->setImpulse(impulse, 1);
}

void EQP::setCtfmode(int _mode)
{
    std::vector<float> impulse;
    ctfmode = _mode;
    eq_impulse (impulse, nc, nfreqs, F.data(), G.data(), samplerate, 1.0 / (2.0 * size), ctfmode, wintype);
    fircore->setImpulse(impulse, 1);
}

void EQP::setWintype(int _wintype)
{
    std::vector<float> impulse;
    wintype = _wintype;
    eq_impulse (impulse, nc, nfreqs, F.data(), G.data(), samplerate, 1.0 / (2.0 * size), ctfmode, wintype);
    fircore->setImpulse(impulse, 1);
}

void EQP::setGrphEQ(const int *rxeq)
{   // three band equalizer (legacy compatibility)
    std::vector<float> impulse;
    nfreqs = 4;
    F.resize(nfreqs + 1);
    G.resize(nfreqs + 1);
    F[1] =  150.0;
    F[2] =  400.0;
    F[3] = 1500.0;
    F[4] = 6000.0;
    G[0] = (float)rxeq[0];
    G[1] = (float)rxeq[1];
    G[2] = (float)rxeq[1];
    G[3] = (float)rxeq[2];
    G[4] = (float)rxeq[3];
    ctfmode = 0;
    eq_impulse (impulse, nc, nfreqs, F.data(), G.data(), samplerate, 1.0 / (2.0 * size), ctfmode, wintype);
    fircore->setImpulse(impulse, 1);
}

void EQP::setGrphEQ10(const int *rxeq)
{   // ten band equalizer (legacy compatibility)
    std::vector<float> impulse;
    nfreqs = 10;
    F.resize(nfreqs + 1);
    G.resize(nfreqs + 1);
    F[1]  =    32.0;
    F[2]  =    63.0;
    F[3]  =   125.0;
    F[4]  =   250.0;
    F[5]  =   500.0;
    F[6]  =  1000.0;
    F[7]  =  2000.0;
    F[8]  =  4000.0;
    F[9]  =  8000.0;
    F[10] = 16000.0;
    for (int i = 0; i <= nfreqs; i++)
        G[i] = (float)rxeq[i];
    ctfmode = 0;
    eq_impulse (impulse, nc, nfreqs, F.data(), G.data(), samplerate, 1.0 / (2.0 * size), ctfmode, wintype);
    fircore->setImpulse(impulse, 1);
}

} // namespace WDSP
