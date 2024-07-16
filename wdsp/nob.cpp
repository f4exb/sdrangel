/*  nobII.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014 Warren Pratt, NR0V
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

#define MAX_ADV_SLEW_TIME           (0.002)
#define MAX_ADV_TIME                (0.002)
#define MAX_HANG_SLEW_TIME          (0.002)
#define MAX_HANG_TIME               (0.002)
#define MAX_SEQ_TIME                (0.025)
#define MAX_SAMPLERATE              (1536000.0)

#include "nob.hpp"
#include "RXA.hpp"

namespace WDSP {

void NOB::init_nob (NOB *a)
{
    int i;
    double coef;
    a->adv_slew_count = (int)(a->advslewtime * a->samplerate);
    a->adv_count = (int)(a->advtime * a->samplerate);
    a->hang_count = (int)(a->hangtime * a->samplerate);
    a->hang_slew_count = (int)(a->hangslewtime * a->samplerate);
    a->max_imp_seq = (int)(a->max_imp_seq_time * a->samplerate);
    a->backmult = exp (-1.0 / (a->samplerate * a->backtau));
    a->ombackmult = 1.0 - a->backmult;
    if (a->adv_slew_count > 0)
    {
        coef = PI / (a->adv_slew_count + 1);
        for (i = 0; i < a->adv_slew_count; i++)
            a->awave[i] = 0.5 * cos ((i + 1) * coef);
    }
    if (a->hang_slew_count > 0)
    {
        coef = PI / a->hang_slew_count;
        for (i = 0; i < a->hang_slew_count; i++)
            a->hwave[i] = 0.5 * cos (i * coef);
    }

    flush_nob (a);
}

NOB* NOB::create_nob (
    int run,
    int buffsize,
    float* in,
    float* out,
    double samplerate,
    int mode,
    double advslewtime,
    double advtime,
    double hangslewtime,
    double hangtime,
    double max_imp_seq_time,
    double backtau,
    double threshold
    )
{
    NOB *a = new NOB;
    a->run = run;
    a->buffsize = buffsize;
    a->in = in;
    a->out = out;
    a->samplerate = samplerate;
    a->mode = mode;
    a->advslewtime = advslewtime;
    a->advtime = advtime;
    a->hangslewtime = hangslewtime;
    a->hangtime = hangtime;
    a->max_imp_seq_time = max_imp_seq_time;
    a->backtau = backtau;
    a->threshold = threshold;
    a->dline_size = (int)(MAX_SAMPLERATE * (MAX_ADV_SLEW_TIME +
                                            MAX_ADV_TIME +
                                            MAX_HANG_SLEW_TIME +
                                            MAX_HANG_TIME +
                                            MAX_SEQ_TIME ) + 2);
    a->dline = new double[a->dline_size * 2];
    a->imp = new int[a->dline_size];
    a->awave = new double[(int)(MAX_ADV_SLEW_TIME  * MAX_SAMPLERATE + 1)];
    a->hwave = new double[(int)(MAX_HANG_SLEW_TIME * MAX_SAMPLERATE + 1)];

    a->filterlen = 10;
    a->bfbuff = new double[a->filterlen * 2];
    a->ffbuff = new double[a->filterlen * 2];
    a->fcoefs = new double[a->filterlen];
    a->fcoefs[0] = 0.308720593;
    a->fcoefs[1] = 0.216104415;
    a->fcoefs[2] = 0.151273090;
    a->fcoefs[3] = 0.105891163;
    a->fcoefs[4] = 0.074123814;
    a->fcoefs[5] = 0.051886670;
    a->fcoefs[6] = 0.036320669;
    a->fcoefs[7] = 0.025424468;
    a->fcoefs[8] = 0.017797128;
    a->fcoefs[9] = 0.012457989;

    init_nob (a);

    a->legacy = new double[2048 * 2];    /////////////// legacy interface - remove
    return a;
}

void NOB::destroy_nob (NOB *a)
{
    delete[] (a->legacy); ///////////////  remove
    delete[] (a->fcoefs);
    delete[] (a->ffbuff);
    delete[] (a->bfbuff);
    delete[] (a->hwave);
    delete[] (a->awave);
    delete[] (a->imp);
    delete[] (a->dline);
    delete (a);
}

void NOB::flush_nob (NOB *a)
{
    a->out_idx = 0;
    a->scan_idx = a->out_idx + a->adv_slew_count + a->adv_count + 1;
    a->in_idx = a->scan_idx + a->max_imp_seq + a->hang_count + a->hang_slew_count + a->filterlen;
    a->state = 0;
    a->overflow = 0;
    a->avg = 1.0;
    a->bfb_in_idx = a->filterlen - 1;
    a->ffb_in_idx = a->filterlen - 1;
    std::fill(a->dline, a->dline + a->dline_size * 2, 0);
    std::fill(a->imp, a->imp + a->dline_size, 0);
    std::fill(a->bfbuff, a->bfbuff + a->filterlen * 2, 0);
    std::fill(a->ffbuff, a->ffbuff + a->filterlen * 2, 0);
}

void NOB::xnob (NOB *a)
{
    double scale;
    double mag;
    int bf_idx;
    int ff_idx;
    int lidx, tidx;
    int i, j, k;
    int bfboutidx;
    int ffboutidx;
    int hcount;
    int len;
    int ffcount;
    int staydown;

    if (a->run)
    {
        for (i = 0; i < a->buffsize; i++)
        {
            a->dline[2 * a->in_idx + 0] = a->in[2 * i + 0];
            a->dline[2 * a->in_idx + 1] = a->in[2 * i + 1];
            mag = sqrt(a->dline[2 * a->in_idx + 0] * a->dline[2 * a->in_idx + 0] + a->dline[2 * a->in_idx + 1] * a->dline[2 * a->in_idx + 1]);
            a->avg = a->backmult * a->avg + a->ombackmult * mag;
            if (mag > (a->avg * a->threshold))
                a->imp[a->in_idx] = 1;
            else
                a->imp[a->in_idx] = 0;
            if ((bf_idx = a->out_idx + a->adv_slew_count) >= a->dline_size) bf_idx -= a->dline_size;
            if (a->imp[bf_idx] == 0)
            {
                if (++a->bfb_in_idx == a->filterlen) a->bfb_in_idx -= a->filterlen;
                a->bfbuff[2 * a->bfb_in_idx + 0] = a->dline[2 * bf_idx + 0];
                a->bfbuff[2 * a->bfb_in_idx + 1] = a->dline[2 * bf_idx + 1];
            }

            switch (a->state)
            {
                case 0:     // normal output & impulse setup
                    {
                        a->out[2 * i + 0] = a->dline[2 * a->out_idx + 0];
                        a->out[2 * i + 1] = a->dline[2 * a->out_idx + 1];
                        a->Ilast = a->dline[2 * a->out_idx + 0];
                        a->Qlast = a->dline[2 * a->out_idx + 1];
                        if (a->imp[a->scan_idx] > 0)
                        {
                            a->time = 0;
                            if (a->adv_slew_count > 0)
                                a->state = 1;
                            else if (a->adv_count > 0)
                                a->state = 2;
                            else
                                a->state = 3;
                            tidx = a->scan_idx;
                            a->blank_count = 0;
                            do
                            {
                                len = 0;
                                hcount = 0;
                                while ((a->imp[tidx] > 0 || hcount > 0) && a->blank_count < a->max_imp_seq)
                                {
                                    a->blank_count++;
                                    if (hcount > 0) hcount--;
                                    if (a->imp[tidx] > 0) hcount = a->hang_count + a->hang_slew_count;
                                    if (++tidx >= a->dline_size) tidx -= a->dline_size;
                                }
                                j = 1;
                                len = 0;
                                lidx = tidx;
                                while (j <= a->adv_slew_count + a->adv_count && len == 0)
                                {
                                    if (a->imp[lidx] == 1)
                                    {
                                        len = j;
                                        tidx = lidx;
                                    }
                                    if (++lidx >= a->dline_size) lidx -= a->dline_size;
                                    j++;
                                }
                                if((a->blank_count += len) > a->max_imp_seq)
                                {
                                    a->blank_count = a->max_imp_seq;
                                    a->overflow = 1;
                                    break;
                                }
                            } while (len != 0);
                            if (a->overflow == 0)
                            {
                                a->blank_count -= a->hang_slew_count;
                                a->Inext = a->dline[2 * tidx + 0];
                                a->Qnext = a->dline[2 * tidx + 1];

                                if (a->mode == 1 || a->mode == 2 || a->mode == 4)
                                {
                                    bfboutidx = a->bfb_in_idx;
                                    a->I1 = 0.0;
                                    a->Q1 = 0.0;
                                    for (k = 0; k < a->filterlen; k++)
                                    {
                                        a->I1 += a->fcoefs[k] * a->bfbuff[2 * bfboutidx + 0];
                                        a->Q1 += a->fcoefs[k] * a->bfbuff[2 * bfboutidx + 1];
                                        if (--bfboutidx < 0) bfboutidx += a->filterlen;
                                    }
                                }

                                if (a->mode == 2 || a->mode == 3 || a->mode == 4)
                                {
                                    if ((ff_idx = a->scan_idx + a->blank_count) >= a->dline_size) ff_idx -= a->dline_size;
                                    ffcount = 0;
                                    while (ffcount < a->filterlen)
                                    {
                                        if (a->imp[ff_idx] == 0)
                                        {
                                            if (++a->ffb_in_idx == a->filterlen) a->ffb_in_idx -= a->filterlen;
                                            a->ffbuff[2 * a->ffb_in_idx + 0] = a->dline[2 * ff_idx + 0];
                                            a->ffbuff[2 * a->ffb_in_idx + 1] = a->dline[2 * ff_idx + 1];
                                            ++ffcount;
                                        }
                                        if (++ff_idx >= a->dline_size) ff_idx -= a->dline_size;
                                    }
                                    if ((ffboutidx = a->ffb_in_idx + 1) >= a->filterlen) ffboutidx -= a->filterlen;
                                    a->I2 = 0.0;
                                    a->Q2 = 0.0;
                                    for (k = 0; k < a->filterlen; k++)
                                    {
                                        a->I2 += a->fcoefs[k] * a->ffbuff[2 * ffboutidx + 0];
                                        a->Q2 += a->fcoefs[k] * a->ffbuff[2 * ffboutidx + 1];
                                        if (++ffboutidx >= a->filterlen) ffboutidx -= a->filterlen;
                                    }
                                }

                                switch (a->mode)
                                {
                                    case 0: // zero
                                        a->deltaI = 0.0;
                                        a->deltaQ = 0.0;
                                        a->I = 0.0;
                                        a->Q = 0.0;
                                        break;
                                    case 1: // sample-hold
                                        a->deltaI = 0.0;
                                        a->deltaQ = 0.0;
                                        a->I = a->I1;
                                        a->Q = a->Q1;
                                        break;
                                    case 2: // mean-hold
                                        a->deltaI = 0.0;
                                        a->deltaQ = 0.0;
                                        a->I = 0.5 * (a->I1 + a->I2);
                                        a->Q = 0.5 * (a->Q1 + a->Q2);
                                        break;
                                    case 3: // hold-sample
                                        a->deltaI = 0.0;
                                        a->deltaQ = 0.0;
                                        a->I = a->I2;
                                        a->Q = a->Q2;
                                        break;
                                    case 4: // linear interpolation
                                        a->deltaI = (a->I2 - a->I1) / (a->adv_count + a->blank_count);
                                        a->deltaQ = (a->Q2 - a->Q1) / (a->adv_count + a->blank_count);
                                        a->I = a->I1;
                                        a->Q = a->Q1;
                                        break;
                                }
                            }
                            else
                            {
                                if (a->adv_slew_count > 0)
                                    a->state = 5;
                                else
                                {
                                    a->state = 6;
                                    a->time = 0;
                                    a->blank_count += a->adv_count + a->filterlen;
                                }
                            }
                        }
                        break;
                    }
                case 1:     // slew output in advance of blanking period
                    {
                        scale = 0.5 + a->awave[a->time];
                        a->out[2 * i + 0] = a->Ilast * scale + (1.0 - scale) * a->I;
                        a->out[2 * i + 1] = a->Qlast * scale + (1.0 - scale) * a->Q;
                        if (++a->time == a->adv_slew_count)
                        {
                            a->time = 0;
                            if (a->adv_count > 0)
                                a->state = 2;
                            else
                                a->state = 3;
                        }
                        break;
                    }
                case 2:     // initial advance period
                    {
                        a->out[2 * i + 0] = a->I;
                        a->out[2 * i + 1] = a->Q;
                        a->I += a->deltaI;
                        a->Q += a->deltaQ;

                        if (++a->time == a->adv_count)
                        {
                            a->state = 3;
                            a->time = 0;
                        }
                        break;
                    }
                case 3:     // impulse & hang period
                    {
                        a->out[2 * i + 0] = a->I;
                        a->out[2 * i + 1] = a->Q;
                        a->I += a->deltaI;
                        a->Q += a->deltaQ;

                        if (++a->time == a->blank_count)
                        {
                            if (a->hang_slew_count > 0)
                            {
                                a->state = 4;
                                a->time = 0;
                            }
                            else
                                a->state = 0;
                        }
                        break;
                    }
                case 4:     // slew output after blanking period
                    {
                        scale = 0.5 - a->hwave[a->time];
                        a->out[2 * i + 0] = a->Inext * scale + (1.0 - scale) * a->I;
                        a->out[2 * i + 1] = a->Qnext * scale + (1.0 - scale) * a->Q;
                        if (++a->time == a->hang_slew_count)
                            a->state = 0;
                        break;
                    }
                case 5:
                    {
                        scale = 0.5 + a->awave[a->time];
                        a->out[2 * i + 0] = a->Ilast * scale;
                        a->out[2 * i + 1] = a->Qlast * scale;
                        if (++a->time == a->adv_slew_count)
                        {
                            a->state = 6;
                            a->time = 0;
                            a->blank_count += a->adv_count + a->filterlen;
                        }
                        break;
                    }
                case 6:
                    {
                        a->out[2 * i + 0] = 0.0;
                        a->out[2 * i + 1] = 0.0;
                        if (++a->time == a->blank_count)
                            a->state = 7;
                        break;
                    }
                case 7:
                    {
                        a->out[2 * i + 0] = 0.0;
                        a->out[2 * i + 1] = 0.0;
                        staydown = 0;
                        a->time = 0;
                        if ((tidx = a->scan_idx + a->hang_slew_count + a->hang_count) >= a->dline_size) tidx -= a->dline_size;
                        while (a->time++ <= a->adv_count + a->adv_slew_count + a->hang_slew_count + a->hang_count)                                                                            //  CHECK EXACT COUNTS!!!!!!!!!!!!!!!!!!!!!!!
                        {
                            if (a->imp[tidx] == 1) staydown = 1;
                            if (--tidx < 0) tidx += a->dline_size;
                        }
                        if (staydown == 0)
                        {
                            if (a->hang_count > 0)
                            {
                                a->state = 8;
                                a->time = 0;
                            }
                            else if (a->hang_slew_count > 0)
                            {
                                a->state = 9;
                                a->time = 0;
                                if ((tidx = a->scan_idx + a->hang_slew_count + a->hang_count - a->adv_count - a->adv_slew_count) >= a->dline_size) tidx -= a->dline_size;
                                if (tidx < 0) tidx += a->dline_size;
                                a->Inext = a->dline[2 * tidx + 0];
                                a->Qnext = a->dline[2 * tidx + 1];
                            }
                            else
                            {
                                a->state = 0;
                                a->overflow = 0;
                            }
                        }
                        break;
                    }
                case 8:
                    {
                        a->out[2 * i + 0] = 0.0;
                        a->out[2 * i + 1] = 0.0;
                        if (++a->time == a->hang_count)
                        {
                            if (a->hang_slew_count > 0)
                            {
                                a->state = 9;
                                a->time = 0;
                                if ((tidx = a->scan_idx + a->hang_slew_count - a->adv_count - a->adv_slew_count) >= a->dline_size) tidx -= a->dline_size;
                                if (tidx < 0) tidx += a->dline_size;
                                a->Inext = a->dline[2 * tidx + 0];
                                a->Qnext = a->dline[2 * tidx + 1];
                            }
                            else
                            {
                                a->state = 0;
                                a->overflow = 0;
                            }
                        }
                        break;
                    }
                case 9:
                    {
                        scale = 0.5 - a->hwave[a->time];
                        a->out[2 * i + 0] = a->Inext * scale;
                        a->out[2 * i + 1] = a->Qnext * scale;

                        if (++a->time >= a->hang_slew_count)
                        {
                            a->state = 0;
                            a->overflow = 0;
                        }
                        break;
                    }
            }
            if (++a->in_idx == a->dline_size) a->in_idx = 0;
            if (++a->scan_idx == a->dline_size) a->scan_idx = 0;
            if (++a->out_idx == a->dline_size) a->out_idx = 0;
        }
    }
    else if (a->in != a->out)
        std::copy(a->in, a->in + a->buffsize * 2, a->out);
}

void NOB::setBuffers_nob (NOB *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
}

void NOB::setSamplerate_nob (NOB *a, int rate)
{
    a->samplerate = rate;
    init_nob (a);
}

void NOB::setSize_nob (NOB *a, int size)
{
    a->buffsize = size;
    flush_nob (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                             RXA PROPERTIES                                            *
*                                                                                                       *
********************************************************************************************************/

void NOB::SetNOBRun (RXA& rxa, int run)
{
    NOB *a = rxa.nob.p;
    a->run = run;
}

void NOB::SetNOBMode (RXA& rxa, int mode)
{
    NOB *a = rxa.nob.p;
    a->mode = mode;
}

void NOB::SetNOBBuffsize (RXA& rxa, int size)
{
    NOB *a = rxa.nob.p;
    a->buffsize = size;
}

void NOB::SetNOBSamplerate (RXA& rxa, int rate)
{
    NOB *a = rxa.nob.p;
    a->samplerate = (double) rate;
    init_nob (a);
}

void NOB::SetNOBTau (RXA& rxa, double tau)
{
    NOB *a = rxa.nob.p;
    a->advslewtime = tau;
    a->hangslewtime = tau;
    init_nob (a);
}

void NOB::SetNOBHangtime (RXA& rxa, double time)
{
    NOB *a = rxa.nob.p;
    a->hangtime = time;
    init_nob (a);
}

void NOB::SetNOBAdvtime (RXA& rxa, double time)
{
    NOB *a = rxa.nob.p;
    a->advtime = time;
    init_nob (a);
}

void NOB::SetNOBBacktau (RXA& rxa, double tau)
{
    NOB *a = rxa.nob.p;
    a->backtau = tau;
    init_nob (a);
}

void NOB::SetNOBThreshold (RXA& rxa, double thresh)
{
    NOB *a = rxa.nob.p;
    a->threshold = thresh;
}

} // namespace
