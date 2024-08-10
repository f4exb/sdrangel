/*  nbp.c

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

#include <array>

#include "comm.hpp"
#include "fir.hpp"
#include "fircore.hpp"
#include "bpsnba.hpp"
#include "nbp.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                           Notch Database                                              *
*                                                                                                       *
********************************************************************************************************/

NOTCHDB::NOTCHDB(int _master_run, int _maxnotches)
{
    master_run = _master_run;
    maxnotches = _maxnotches;
    nn = 0;
    fcenter.resize(maxnotches);
    fwidth.resize(maxnotches);
    nlow.resize(maxnotches);
    nhigh.resize(maxnotches);
    active.resize(maxnotches);
}

int NOTCHDB::addNotch(int notch, double _fcenter, double _fwidth, int _active)
{
    int i;
    int j;
    int rval;

    if (notch <= nn && nn < maxnotches)
    {
        nn++;

        for (i = nn - 2, j = nn - 1; i >= notch; i--, j--)
        {
            fcenter[j] = fcenter[i];
            fwidth[j] = fwidth[i];
            nlow[j] = nlow[i];
            nhigh[j] = nhigh[i];
            active[j] = active[i];
        }
        fcenter[notch] = _fcenter;
        fwidth[notch] = _fwidth;
        nlow[notch] = _fcenter - 0.5 * _fwidth;
        nhigh[notch] = _fcenter + 0.5 * _fwidth;
        active[notch] = _active;
        rval = 0;
    }
    else
        rval = -1;
    return rval;
}

int NOTCHDB::getNotch(int _notch, double* _fcenter, double* _fwidth, int* _active)
{
    int rval;

    if (_notch < nn)
    {
        *_fcenter = fcenter[_notch];
        *_fwidth = fwidth[_notch];
        *_active = active[_notch];
        rval = 0;
    }
    else
    {
        *_fcenter = -1.0;
        *_fwidth = 0.0;
        *_active = -1;
        rval = -1;
    }

    return rval;
}

int NOTCHDB::deleteNotch(int _notch)
{
    int i;
    int j;
    int rval;

    if (_notch < nn)
    {
        nn--;
        for (i = _notch, j = _notch + 1; i < nn; i++, j++)
        {
            fcenter[i] = fcenter[j];
            fwidth[i] = fwidth[j];
            nlow[i] = nlow[j];
            nhigh[i] = nhigh[j];
            active[i] = active[j];
        }
        rval = 0;
    }
    else
        rval = -1;
    return rval;
}

int NOTCHDB::editNotch(int _notch, double _fcenter, double _fwidth, int _active)
{
    int rval;

    if (_notch < nn)
    {
        fcenter[_notch] = _fcenter;
        fwidth[_notch] = _fwidth;
        nlow[_notch] = _fcenter - 0.5 * _fwidth;
        nhigh[_notch] = _fcenter + 0.5 * _fwidth;
        active[_notch] = _active;
        rval = 0;
    }
    else
        rval = -1;
    return rval;
}

void NOTCHDB::getNumNotches(int* _nnotches) const
{
    *_nnotches = nn;
}

/********************************************************************************************************
*                                                                                                       *
*                                       Notched Bandpass Filter                                         *
*                                                                                                       *
********************************************************************************************************/

void NBP::fir_mbandpass (std::vector<float>& impulse, int N, int nbp, const double* flow, const double* fhigh, double rate, double scale, int wintype)
{
    impulse.resize(N * 2);
    std::fill(impulse.begin(), impulse.end(), 0);
    for (int k = 0; k < nbp; k++)
    {
        std::vector<float> imp;
        FIR::fir_bandpass (imp, N, flow[k], fhigh[k], rate, wintype, 1, scale);
        for (int i = 0; i < N; i++)
        {
            impulse[2 * i + 0] += imp[2 * i + 0];
            impulse[2 * i + 1] += imp[2 * i + 1];
        }
    }
}

double NBP::min_notch_width() const
{
    double min_width;
    switch (wintype)
    {
    case 0:
        min_width = 1600.0 / (nc / 256) * (rate / 48000);
        break;
    case 1:
        min_width = 2200.0 / (nc / 256) * (rate / 48000);
        break;
    default:
        min_width = 0;
    }
    return min_width;
}

int NBP::make_nbp (
    int nn,
    std::vector<int>& active,
    std::vector<double>& center,
    std::vector<double>& width,
    std::vector<double>& nlow,
    std::vector<double>& nhigh,
    double minwidth,
    int autoincr,
    double flow,
    double fhigh,
    std::vector<double>& bplow,
    std::vector<double>& bphigh,
    int* havnotch
)
{
    int nbp;
    int nnbp;
    int adds;
    int i;
    double nl;
    double nh;
    std::array<int, 1024> del;
    if (fhigh > flow)
    {
        bplow[0]  = flow;
        bphigh[0] = fhigh;
        nbp = 1;
    }
    else
    {
        nbp = 0;
        return nbp;
    }
    *havnotch = 0;
    for (int k = 0; k < nn; k++)
    {
        if (autoincr && width[k] < minwidth)
        {
            nl = center[k] - 0.5 * minwidth;
            nh = center[k] + 0.5 * minwidth;
        }
        else
        {
            nl = nlow[k];
            nh = nhigh[k];
        }
        if (active[k] && (nh > flow && nl < fhigh))
        {
            *havnotch = 1;
            adds = 0;
            for (i = 0; i < nbp; i++)
            {
                if (nh > bplow[i] && nl < bphigh[i])
                {
                    if (nl <= bplow[i] && nh >= bphigh[i])
                    {
                        del[i] = 1;
                    }
                    else if (nl > bplow[i] && nh < bphigh[i])
                    {

                        bplow[nbp + adds] = nh;
                        bphigh[nbp + adds] = bphigh[i];
                        bphigh[i] = nl;
                        adds++;
                    }
                    else if (nl <= bplow[i] && nh > bplow[i])
                    {
                        bplow[i] = nh;
                    }
                    else if (nl < bphigh[i] && nh >= bphigh[i])
                    {
                        bphigh[i] = nl;
                    }
                }
            }
            nbp += adds;
            nnbp = nbp;
            for (i = 0; i < nbp; i++)
            {
                if (del[i] == 1)
                {
                    nnbp--;
                    for (int j = i; j < nnbp; j++)
                    {
                        bplow[j] = bplow[j + 1];
                        bphigh[j] = bphigh[j + 1];
                    }
                    del[i] = 0;
                }
            }
            nbp = nnbp;
        }
    }
    return nbp;
}

void NBP::calc_lightweight()
{   // calculate and set new impulse response; used when changing tune freq or shift freq
    double fl;
    double fh;
    double offset;
    NOTCHDB *b = notchdb;
    if (fnfrun)
    {
        offset = b->tunefreq + b->shift;
        fl = flow  + offset;
        fh = fhigh + offset;
        numpb = make_nbp (
            b->nn,
            b->active,
            b->fcenter,
            b->fwidth,
            b->nlow,
            b->nhigh,
            min_notch_width(),
            autoincr,
            fl,
            fh,
            bplow,
            bphigh,
            &havnotch
        );
        // when tuning, no need to recalc filter if there were not and are not any notches in passband
        if (hadnotch || havnotch)
        {
            for (int i = 0; i < numpb; i++)
            {
                bplow[i]  -= offset;
                bphigh[i] -= offset;
            }
            fir_mbandpass (
                impulse,
                nc,
                numpb,
                bplow.data(),
                bphigh.data(),
                rate,
                gain / (float)(2 * size),
                wintype
            );
            fircore->setImpulse(impulse, 1);
            // print_impulse ("nbp.txt", size + 1, impulse, 1, 0);
        }
        hadnotch = havnotch;
    }
    else
        hadnotch = 1;
}

void NBP::calc_impulse ()
{   // calculates impulse response; for create_fircore() and parameter changes
    double fl;
    double fh;
    double offset;
    NOTCHDB *b = notchdb;

    if (fnfrun)
    {
        offset = b->tunefreq + b->shift;
        fl = flow  + offset;
        fh = fhigh + offset;
        numpb = make_nbp (
            b->nn,
            b->active,
            b->fcenter,
            b->fwidth,
            b->nlow,
            b->nhigh,
            min_notch_width(),
            autoincr,
            fl,
            fh,
            bplow,
            bphigh,
            &havnotch
        );
        for (int i = 0; i < numpb; i++)
        {
            bplow[i]  -= offset;
            bphigh[i] -= offset;
        }
        fir_mbandpass (
            impulse,
            nc,
            numpb,
            bplow.data(),
            bphigh.data(),
            rate,
            gain / (float)(2 * size),
            wintype
        );
    }
    else
    {
        FIR::fir_bandpass(
            impulse,
            nc,
            flow,
            fhigh,
            rate,
            wintype,
            1,
            gain / (float)(2 * size)
        );
    }
}

NBP::NBP(
    int _run,
    int _fnfrun,
    int _position,
    int _size,
    int _nc,
    int _mp,
    float* _in,
    float* _out,
    double _flow,
    double _fhigh,
    int _rate,
    int _wintype,
    double _gain,
    int _autoincr,
    int _maxpb,
    NOTCHDB* _notchdb
) :
    run(_run),
    fnfrun(_fnfrun),
    position(_position),
    size(_size),
    nc(_nc),
    mp(_mp),
    rate((double) _rate),
    wintype(_wintype),
    gain(_gain),
    in(_in),
    out(_out),
    autoincr(_autoincr),
    flow(_flow),
    fhigh(_fhigh),
    maxpb(_maxpb),
    notchdb(_notchdb)
{
    bplow.resize(maxpb);
    bphigh.resize(maxpb);
    calc_impulse ();
    fircore = new FIRCORE(size, in, out, mp, impulse);
}

NBP::~NBP()
{
    delete fircore;
}

void NBP::flush()
{
    fircore->flush();
}

void NBP::execute (int pos)
{
    if (run && pos == position)
        fircore->execute();
    else if (in != out)
        std::copy( in,  in + size * 2, out);
}

void NBP::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
    fircore->setBuffers(in, out);
}

void NBP::setSamplerate(int _rate)
{
    rate = _rate;
    calc_impulse ();
    fircore->setImpulse(impulse, 1);
}

void NBP::setSize(int _size)
{
    // NOTE:  'size' must be <= 'nc'
    size = _size;
    fircore->setSize(size);
    calc_impulse ();
    fircore->setImpulse(impulse, 1);
}

void NBP::setNc()
{
    calc_impulse();
    fircore->setNc(impulse);
}

void NBP::setMp()
{
    fircore->setMp(mp);
}

/********************************************************************************************************
*                                                                                                       *
*                                           Public Properties                                           *
*                                                                                                       *
********************************************************************************************************/

// FILTER PROPERTIES

void NBP::SetRun(int _run)
{
    run = _run;
}

void NBP::SetFreqs(double _flow, double _fhigh)
{
    if ((flow != _flow) || (fhigh != _fhigh))
    {
        flow = _flow;
        fhigh = _fhigh;
        calc_impulse();
        fircore->setImpulse(impulse, 1);
    }
}

void NBP::SetNC(int _nc)
{
    // NOTE:  'nc' must be >= 'size'
    if (nc != _nc)
    {
        nc = _nc;
        setNc();
    }
}

void NBP::SetMP(int _mp)
{
    if (mp != _mp)
    {
        mp = _mp;
        setMp();
    }
}

void NBP::GetMinNotchWidth(double* minwidth) const
{
    *minwidth = min_notch_width();
}

} // namespace WDSP
