/*  cfcomp.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2017, 2021 Warren Pratt, NR0V
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
#include "cfcomp.hpp"
#include "meterlog10.hpp"
#include "TXA.hpp"

namespace WDSP {

void CFCOMP::calc_cfcwindow()
{
    int i;
    double arg0;
    double arg1;
    double cgsum;
    double igsum;
    double coherent_gain;
    double inherent_power_gain;
    double wmult;
    switch (wintype)
    {
    case 0:
        arg0 = 2.0 * PI / (float)fsize;
        cgsum = 0.0;
        igsum = 0.0;
        for (i = 0; i < fsize; i++)
        {
            window[i] = sqrt (0.54 - 0.46 * cos((float)i * arg0));
            cgsum += window[i];
            igsum += window[i] * window[i];
        }
        coherent_gain = cgsum / (float)fsize;
        inherent_power_gain = igsum / (float)fsize;
        wmult = 1.0 / sqrt (inherent_power_gain);
        for (i = 0; i < fsize; i++)
            window[i] *= wmult;
        winfudge = sqrt (1.0 / coherent_gain);
        break;
    case 1:
        arg0 = 2.0 * PI / (float)fsize;
        cgsum = 0.0;
        igsum = 0.0;
        for (i = 0; i < fsize; i++)
        {
            arg1 = cos(arg0 * (float)i);
            window[i]  = sqrt   (+0.21747
                          + arg1 * (-0.45325
                          + arg1 * (+0.28256
                          + arg1 * (-0.04672))));
            cgsum += window[i];
            igsum += window[i] * window[i];
        }
        coherent_gain = cgsum / (float)fsize;
        inherent_power_gain = igsum / (float)fsize;
        wmult = 1.0 / sqrt (inherent_power_gain);
        for (i = 0; i < fsize; i++)
            window[i] *= wmult;
        winfudge = sqrt (1.0 / coherent_gain);
        break;
    default:
        break;
    }
}

int CFCOMP::fCOMPcompare (const void *a, const void *b)
{
    if (*(double*)a < *(double*)b)
        return -1;
    else if (*(double*)a == *(double*)b)
        return 0;
    else
        return 1;
}

void CFCOMP::calc_comp()
{
    int i;
    int j;
    double f;
    double frac;
    double fincr;
    double fmax;
    double* sary;
    precomplin = pow (10.0, 0.05 * precomp);
    prepeqlin  = pow (10.0, 0.05 * prepeq);
    fmax = 0.5 * rate;
    for (i = 0; i < nfreqs; i++)
    {
        F[i] = std::max (F[i], 0.0);
        F[i] = std::min (F[i], fmax);
        G[i] = std::max (G[i], 0.0);
    }
    sary = new double[3 * nfreqs];
    for (i = 0; i < nfreqs; i++)
    {
        sary[3 * i + 0] = F[i];
        sary[3 * i + 1] = G[i];
        sary[3 * i + 2] = E[i];
    }
    qsort (sary, nfreqs, 3 * sizeof (float), fCOMPcompare);
    for (i = 0; i < nfreqs; i++)
    {
        F[i] = sary[3 * i + 0];
        G[i] = sary[3 * i + 1];
        E[i] = sary[3 * i + 2];
    }
    fp[0] = 0.0;
    fp[nfreqs + 1] = fmax;
    gp[0] = G[0];
    gp[nfreqs + 1] = G[nfreqs - 1];
    ep[0] = E[0];                             // cutoff?
    ep[nfreqs + 1] = E[nfreqs - 1];     // cutoff?
    for (i = 0, j = 1; i < nfreqs; i++, j++)
    {
        fp[j] = F[i];
        gp[j] = G[i];
        ep[j] = E[i];
    }
    fincr = rate / (float)fsize;
    j = 0;

    for (i = 0; i < msize; i++)
    {
        f = fincr * (float)i;
        while (f >= fp[j + 1] && j < nfreqs) j++;
        frac = (f - fp[j]) / (fp[j + 1] - fp[j]);
        comp[i] = pow (10.0, 0.05 * (frac * gp[j + 1] + (1.0 - frac) * gp[j]));
        peq[i]  = pow (10.0, 0.05 * (frac * ep[j + 1] + (1.0 - frac) * ep[j]));
        cfc_gain[i] = precomplin * comp[i];
    }

    delete[] sary;
}

void CFCOMP::calc_cfcomp()
{
    incr = fsize / ovrlp;
    if (fsize > bsize)
        iasize = fsize;
    else
        iasize = bsize + fsize - incr;
    iainidx = 0;
    iaoutidx = 0;
    if (fsize > bsize)
    {
        if (bsize > incr)  oasize = bsize;
        else                     oasize = incr;
        oainidx = (fsize - bsize - incr) % oasize;
    }
    else
    {
        oasize = bsize;
        oainidx = fsize - incr;
    }
    init_oainidx = oainidx;
    oaoutidx = 0;
    msize = fsize / 2 + 1;
    window.resize(fsize);
    inaccum.resize(iasize);
    forfftin.resize(fsize);
    forfftout.resize(msize * 2);
    cmask.resize(msize);
    mask.resize(msize);
    cfc_gain.resize(msize);
    revfftin.resize(msize * 2);
    revfftout.resize(fsize);
    save.resize(ovrlp);
    for (int i = 0; i < ovrlp; i++)
        save[i].resize(fsize);
    outaccum.resize(oasize);
    nsamps = 0;
    saveidx = 0;
    Rfor = fftwf_plan_dft_r2c_1d(fsize, forfftin.data(), (fftwf_complex *)forfftout.data(), FFTW_ESTIMATE);
    Rrev = fftwf_plan_dft_c2r_1d(fsize, (fftwf_complex *)revfftin.data(), revfftout.data(), FFTW_ESTIMATE);
    calc_cfcwindow();

    pregain  = (2.0 * winfudge) / (double)fsize;
    postgain = 0.5 / ((double)ovrlp * winfudge);

    fp.resize(nfreqs + 2);
    gp.resize(nfreqs + 2);
    ep.resize(nfreqs + 2);
    comp.resize(msize);
    peq.resize(msize);
    calc_comp();

    gain = 0.0;
    mmult = exp (-1.0 / (rate * ovrlp * mtau));
    dmult = exp (-(float)fsize / (rate * ovrlp * dtau));

    delta.resize(msize);
    delta_copy.resize(msize);
    cfc_gain_copy.resize(msize);
}

void CFCOMP::decalc_cfcomp()
{
    fftwf_destroy_plan(Rrev);
    fftwf_destroy_plan(Rfor);
}

CFCOMP::CFCOMP(
    int _run,
    int _position,
    int _peq_run,
    int _size,
    float* _in,
    float* _out,
    int _fsize,
    int _ovrlp,
    int _rate,
    int _wintype,
    int _comp_method,
    int _nfreqs,
    double _precomp,
    double _prepeq,
    const double* _F,
    const double* _G,
    const double* _E,
    double _mtau,
    double _dtau
) :
    run (_run),
    position(_position),
    bsize(_size),
    in(_in),
    out(_out),
    fsize(_fsize),
    ovrlp(_ovrlp),
    rate(_rate),
    wintype(_wintype),
    comp_method(_comp_method),
    nfreqs(_nfreqs),
    precomp(_precomp),
    peq_run(_peq_run),
    prepeq(_prepeq),
    mtau(_mtau),                 // compression metering time constant
    dtau(_dtau)                 // compression display time constant
{
    F.resize(nfreqs);
    G.resize(nfreqs);
    E.resize(nfreqs);
    std::copy(_F, _F + nfreqs, F.begin());
    std::copy(_G, _G + nfreqs, G.begin());
    std::copy(_E, _E + nfreqs, E.begin());
    calc_cfcomp();
}

CFCOMP::~CFCOMP()
{
    decalc_cfcomp();
}

void CFCOMP::flush()
{
    std::fill(inaccum.begin(), inaccum.end(), 0);
    for (int i = 0; i < ovrlp; i++)
        std::fill(save[i].begin(), save[i].end(), 0);
    std::fill(outaccum.begin(), outaccum.end(), 0);
    nsamps   = 0;
    iainidx  = 0;
    iaoutidx = 0;
    oainidx  = init_oainidx;
    oaoutidx = 0;
    saveidx  = 0;
    gain = 0.0;
    std::fill(delta.begin(), delta.end(), 0);
}



void CFCOMP::calc_mask()
{
    int i;
    double _comp;
    double _mask;
    double _delta;
    if (comp_method == 0)
    {
        double mag;
        double test;
        for (i = 0; i < msize; i++)
        {
            mag = sqrt (forfftout[2 * i + 0] * forfftout[2 * i + 0]
                        + forfftout[2 * i + 1] * forfftout[2 * i + 1]);
            _comp = cfc_gain[i];
            test = _comp * mag;
            if (test > 1.0)
                _mask = 1.0 / mag;
            else
                _mask = _comp;
            cmask[i] = _mask;
            if (test > gain) gain = test;
            else gain = mmult * gain;

            _delta = cfc_gain[i] - cmask[i];
            if (_delta > delta[i]) delta[i] = _delta;
            else delta[i] *= dmult;
    }
    }
    if (peq_run)
    {
        for (i = 0; i < msize; i++)
        {
            mask[i] = cmask[i] * prepeqlin * peq[i];
        }
    }
    else
        std::copy(cmask.begin(), cmask.end(), mask.begin());
    mask_ready = 1;
}

void CFCOMP::execute(int pos)
{
    if (run && pos == position)
    {
        int i;
        int j;
        int k;
        int sbuff;
        int sbegin;
        for (i = 0; i < 2 * bsize; i += 2)
        {
            inaccum[iainidx] = in[i];
            iainidx = (iainidx + 1) % iasize;
        }
        nsamps += bsize;
        while (nsamps >= fsize)
        {
            for (i = 0, j = iaoutidx; i < fsize; i++, j = (j + 1) % iasize)
                forfftin[i] = (float) (pregain * window[i] * inaccum[j]);
            iaoutidx = (iaoutidx + incr) % iasize;
            nsamps -= incr;
            fftwf_execute (Rfor);
            calc_mask();
            for (i = 0; i < msize; i++)
            {
                revfftin[2 * i + 0] = (float) (mask[i] * forfftout[2 * i + 0]);
                revfftin[2 * i + 1] = (float) (mask[i] * forfftout[2 * i + 1]);
            }
            fftwf_execute (Rrev);
            for (i = 0; i < fsize; i++)
                save[saveidx][i] = postgain * window[i] * revfftout[i];
            for (i = ovrlp; i > 0; i--)
            {
                sbuff = (saveidx + i) % ovrlp;
                sbegin = incr * (ovrlp - i);
                for (j = sbegin, k = oainidx; j < incr + sbegin; j++, k = (k + 1) % oasize)
                {
                    if ( i == ovrlp)
                        outaccum[k]  = save[sbuff][j];
                    else
                        outaccum[k] += save[sbuff][j];
                }
            }
            saveidx = (saveidx + 1) % ovrlp;
            oainidx = (oainidx + incr) % oasize;
        }
        for (i = 0; i < bsize; i++)
        {
            out[2 * i + 0] = (float) (outaccum[oaoutidx]);
            out[2 * i + 1] = 0.0;
            oaoutidx = (oaoutidx + 1) % oasize;
        }
    }
    else if (out != in)
        std::copy(in, in + bsize * 2, out);
}

void CFCOMP::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void CFCOMP::setSamplerate(int _rate)
{
    decalc_cfcomp();
    rate = _rate;
    calc_cfcomp();
}

void CFCOMP::setSize(int size)
{
    decalc_cfcomp();
    bsize = size;
    calc_cfcomp();
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void CFCOMP::setRun(int _run)
{
    if (run != _run) {
        run = _run;
    }
}

void CFCOMP::setPosition(int pos)
{
    if (position != pos) {
        position = pos;
    }
}

void CFCOMP::setProfile(int _nfreqs, const double* _F, const double* _G, const double* _E)
{
    nfreqs = _nfreqs < 1 ? 1 : _nfreqs;
    F.resize(nfreqs);
    G.resize(nfreqs);
    E.resize(nfreqs);
    std::copy(_F, _F + nfreqs, F.begin());
    std::copy(_G, _G + nfreqs, G.begin());
    std::copy(_E, _E + nfreqs, E.begin());
    fp.resize(nfreqs + 2);
    gp.resize(nfreqs + 2);
    ep.resize(nfreqs + 2);
    calc_comp();
}

void CFCOMP::setPrecomp(double _precomp)
{
    if (precomp != _precomp)
    {
        precomp = _precomp;
        precomplin = pow (10.0, 0.05 * precomp);

        for (int i = 0; i < msize; i++)
        {
            cfc_gain[i] = precomplin * comp[i];
        }
    }
}

void CFCOMP::setPeqRun(int _run)
{
    if (peq_run != _run) {
        peq_run = _run;
    }
}

void CFCOMP::setPrePeq(double _prepeq)
{
    prepeq = _prepeq;
    prepeqlin = pow (10.0, 0.05 * prepeq);
}

void CFCOMP::getDisplayCompression(double* comp_values, int* ready)
{
    if ((*ready = mask_ready))
    {
        std::copy(delta.begin(), delta.end(), delta_copy.begin());
        std::copy(cfc_gain.begin(), cfc_gain.end(), cfc_gain_copy.begin());
        mask_ready = 0;
    }

    if (*ready)
    {
        for (int i = 0; i < msize; i++)
            comp_values[i] = 20.0 * MemLog::mlog10 (cfc_gain_copy[i] / (cfc_gain_copy[i] - delta_copy[i]));
    }
}

} // namespace WDSP

