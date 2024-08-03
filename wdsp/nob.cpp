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

#define MAX_ADV_SLEW_TIME           (0.01) // Slew time
#define MAX_ADV_TIME                (0.01) // Lead time
#define MAX_HANG_SLEW_TIME          (0.01) // Slew time
#define MAX_HANG_TIME               (0.01) // Lag time
#define MAX_SEQ_TIME                (0.025)
#define MAX_SAMPLERATE              (1536000.0)

#include "nob.hpp"

namespace WDSP {

void NOB::init()
{
    int i;
    double coef;

    adv_slew_count = (int)(advslewtime * samplerate);
    adv_count = (int)(advtime * samplerate);
    hang_count = (int)(hangtime * samplerate);
    hang_slew_count = (int)(hangslewtime * samplerate);
    max_imp_seq = (int)(max_imp_seq_time * samplerate);
    backmult = exp (-1.0 / (samplerate * backtau));
    ombackmult = 1.0 - backmult;

    if (adv_slew_count > 0)
    {
        coef = PI / (adv_slew_count + 1);

        for (i = 0; i < adv_slew_count; i++)
            awave[i] = 0.5 * cos ((i + 1) * coef);
    }

    if (hang_slew_count > 0)
    {
        coef = PI / hang_slew_count;

        for (i = 0; i < hang_slew_count; i++)
            hwave[i] = 0.5 * cos (i * coef);
    }

    flush();
}

NOB::NOB (
    int _run,
    int _buffsize,
    float* _in,
    float* _out,
    double _samplerate,
    int _mode,
    double _advslewtime,
    double _advtime,
    double _hangslewtime,
    double _hangtime,
    double _max_imp_seq_time,
    double _backtau,
    double _threshold
) :
    run(_run),
    buffsize(_buffsize),
    in(_in),
    out(_out),
    samplerate(_samplerate),
    mode(_mode),
    advslewtime(_advslewtime),
    advtime(_advtime),
    hangslewtime(_hangslewtime),
    hangtime(_hangtime),
    max_imp_seq_time(_max_imp_seq_time),
    backtau(_backtau),
    threshold(_threshold)
{
    dline_size = (int)(MAX_SAMPLERATE * (
        MAX_ADV_SLEW_TIME +
        MAX_ADV_TIME +
        MAX_HANG_SLEW_TIME +
        MAX_HANG_TIME +
        MAX_SEQ_TIME ) + 2);
    dline.resize(dline_size * 2);
    imp.resize(dline_size);
    awave.resize((int)(MAX_ADV_SLEW_TIME  * MAX_SAMPLERATE + 1));
    hwave.resize((int)(MAX_HANG_SLEW_TIME * MAX_SAMPLERATE + 1));

    filterlen = 10;
    bfbuff.resize(filterlen * 2);
    ffbuff.resize(filterlen * 2);
    fcoefs.resize(filterlen);
    fcoefs[0] = 0.308720593;
    fcoefs[1] = 0.216104415;
    fcoefs[2] = 0.151273090;
    fcoefs[3] = 0.105891163;
    fcoefs[4] = 0.074123814;
    fcoefs[5] = 0.051886670;
    fcoefs[6] = 0.036320669;
    fcoefs[7] = 0.025424468;
    fcoefs[8] = 0.017797128;
    fcoefs[9] = 0.012457989;

    init();
}

void NOB::flush()
{
    out_idx = 0;
    scan_idx = out_idx + adv_slew_count + adv_count + 1;
    in_idx = scan_idx + max_imp_seq + hang_count + hang_slew_count + filterlen;
    state = 0;
    overflow = 0;
    avg = 1.0;
    bfb_in_idx = filterlen - 1;
    ffb_in_idx = filterlen - 1;
    std::fill(dline.begin(), dline.end(), 0);
    std::fill(imp.begin(), imp.end(), 0);
    std::fill(bfbuff.begin(), bfbuff.end(), 0);
    std::fill(ffbuff.begin(), ffbuff.end(), 0);
}

void NOB::execute()
{
    double scale;
    double mag;
    int bf_idx;
    int ff_idx;
    int lidx;
    int tidx;
    int j;
    int k;
    int bfboutidx;
    int ffboutidx;
    int hcount;
    int len;
    int ffcount;
    int staydown;

    if (run)
    {
        for (int i = 0; i < buffsize; i++)
        {
            dline[2 * in_idx + 0] = in[2 * i + 0];
            dline[2 * in_idx + 1] = in[2 * i + 1];
            mag = sqrt(dline[2 * in_idx + 0] * dline[2 * in_idx + 0] + dline[2 * in_idx + 1] * dline[2 * in_idx + 1]);
            avg = backmult * avg + ombackmult * mag;

            if (mag > (avg * threshold))
                imp[in_idx] = 1;
            else
                imp[in_idx] = 0;

            if ((bf_idx = out_idx + adv_slew_count) >= dline_size)
                bf_idx -= dline_size;

            if (imp[bf_idx] == 0)
            {
                if (++bfb_in_idx == filterlen)
                    bfb_in_idx -= filterlen;

                bfbuff[2 * bfb_in_idx + 0] = dline[2 * bf_idx + 0];
                bfbuff[2 * bfb_in_idx + 1] = dline[2 * bf_idx + 1];
            }

            switch (state)
            {
                case 0:     // normal output & impulse setup
                    {
                        out[2 * i + 0] = (float) (dline[2 * out_idx + 0]);
                        out[2 * i + 1] = (float) (dline[2 * out_idx + 1]);
                        Ilast = dline[2 * out_idx + 0];
                        Qlast = dline[2 * out_idx + 1];

                        if (imp[scan_idx] > 0)
                        {
                            time = 0;

                            if (adv_slew_count > 0)
                                state = 1;
                            else if (adv_count > 0)
                                state = 2;
                            else
                                state = 3;

                            tidx = scan_idx;
                            blank_count = 0;

                            do
                            {
                                hcount = 0;

                                while ((imp[tidx] > 0 || hcount > 0) && blank_count < max_imp_seq)
                                {
                                    blank_count++;
                                    if (hcount > 0)
                                        hcount--;
                                    if (imp[tidx] > 0)
                                        hcount = hang_count + hang_slew_count;
                                    if (++tidx >= dline_size)
                                        tidx -= dline_size;
                                }

                                j = 1;
                                len = 0;
                                lidx = tidx;

                                while (j <= adv_slew_count + adv_count && len == 0)
                                {
                                    if (imp[lidx] == 1)
                                    {
                                        len = j;
                                        tidx = lidx;
                                    }

                                    if (++lidx >= dline_size)
                                        lidx -= dline_size;

                                    j++;
                                }

                                if((blank_count += len) > max_imp_seq)
                                {
                                    blank_count = max_imp_seq;
                                    overflow = 1;
                                    break;
                                }
                            }
                            while (len != 0);

                            if (overflow == 0)
                            {
                                blank_count -= hang_slew_count;
                                Inext = dline[2 * tidx + 0];
                                Qnext = dline[2 * tidx + 1];

                                if (mode == 1 || mode == 2 || mode == 4)
                                {
                                    bfboutidx = bfb_in_idx;
                                    I1 = 0.0;
                                    Q1 = 0.0;

                                    for (k = 0; k < filterlen; k++)
                                    {
                                        I1 += fcoefs[k] * bfbuff[2 * bfboutidx + 0];
                                        Q1 += fcoefs[k] * bfbuff[2 * bfboutidx + 1];

                                        if (--bfboutidx < 0)
                                            bfboutidx += filterlen;
                                    }
                                }

                                if (mode == 2 || mode == 3 || mode == 4)
                                {
                                    if ((ff_idx = scan_idx + blank_count) >= dline_size)
                                        ff_idx -= dline_size;

                                    ffcount = 0;

                                    while (ffcount < filterlen)
                                    {
                                        if (imp[ff_idx] == 0)
                                        {
                                            if (++ffb_in_idx == filterlen)
                                                ffb_in_idx -= filterlen;

                                            ffbuff[2 * ffb_in_idx + 0] = dline[2 * ff_idx + 0];
                                            ffbuff[2 * ffb_in_idx + 1] = dline[2 * ff_idx + 1];
                                            ++ffcount;
                                        }

                                        if (++ff_idx >= dline_size)
                                            ff_idx -= dline_size;
                                    }

                                    if ((ffboutidx = ffb_in_idx + 1) >= filterlen)
                                        ffboutidx -= filterlen;

                                    I2 = 0.0;
                                    Q2 = 0.0;

                                    for (k = 0; k < filterlen; k++)
                                    {
                                        I2 += fcoefs[k] * ffbuff[2 * ffboutidx + 0];
                                        Q2 += fcoefs[k] * ffbuff[2 * ffboutidx + 1];

                                        if (++ffboutidx >= filterlen)
                                            ffboutidx -= filterlen;
                                    }
                                }

                                switch (mode)
                                {
                                    default: // zero
                                        deltaI = 0.0;
                                        deltaQ = 0.0;
                                        I = 0.0;
                                        Q = 0.0;
                                        break;
                                    case 1: // sample-hold
                                        deltaI = 0.0;
                                        deltaQ = 0.0;
                                        I = I1;
                                        Q = Q1;
                                        break;
                                    case 2: // mean-hold
                                        deltaI = 0.0;
                                        deltaQ = 0.0;
                                        I = 0.5 * (I1 + I2);
                                        Q = 0.5 * (Q1 + Q2);
                                        break;
                                    case 3: // hold-sample
                                        deltaI = 0.0;
                                        deltaQ = 0.0;
                                        I = I2;
                                        Q = Q2;
                                        break;
                                    case 4: // linear interpolation
                                        deltaI = (I2 - I1) / (adv_count + blank_count);
                                        deltaQ = (Q2 - Q1) / (adv_count + blank_count);
                                        I = I1;
                                        Q = Q1;
                                        break;
                                }
                            }
                            else
                            {
                                if (adv_slew_count > 0)
                                {
                                    state = 5;
                                }
                                else
                                {
                                    state = 6;
                                    time = 0;
                                    blank_count += adv_count + filterlen;
                                }
                            }
                        }

                        break;
                    }

                case 1:     // slew output in advance of blanking period
                    {
                        scale = 0.5 + awave[time];
                        out[2 * i + 0] = (float) (Ilast * scale + (1.0 - scale) * I);
                        out[2 * i + 1] = (float) (Qlast * scale + (1.0 - scale) * Q);

                        if (++time == adv_slew_count)
                        {
                            time = 0;

                            if (adv_count > 0)
                                state = 2;
                            else
                                state = 3;
                        }

                        break;
                    }

                case 2:     // initial advance period
                    {
                        out[2 * i + 0] = (float) I;
                        out[2 * i + 1] = (float) Q;
                        I += deltaI;
                        Q += deltaQ;

                        if (++time == adv_count)
                        {
                            state = 3;
                            time = 0;
                        }

                        break;
                    }

                case 3:     // impulse & hang period
                    {
                        out[2 * i + 0] = (float) I;
                        out[2 * i + 1] = (float) Q;
                        I += deltaI;
                        Q += deltaQ;

                        if (++time == blank_count)
                        {
                            if (hang_slew_count > 0)
                            {
                                state = 4;
                                time = 0;
                            }
                            else
                            {
                                state = 0;
                            }
                        }

                        break;
                    }

                case 4:     // slew output after blanking period
                    {
                        scale = 0.5 - hwave[time];
                        out[2 * i + 0] = (float) (Inext * scale + (1.0 - scale) * I);
                        out[2 * i + 1] = (float) (Qnext * scale + (1.0 - scale) * Q);

                        if (++time == hang_slew_count)
                            state = 0;

                        break;
                    }

                case 5:
                    {
                        scale = 0.5 + awave[time];
                        out[2 * i + 0] = (float) (Ilast * scale);
                        out[2 * i + 1] = (float) (Qlast * scale);

                        if (++time == adv_slew_count)
                        {
                            state = 6;
                            time = 0;
                            blank_count += adv_count + filterlen;
                        }

                        break;
                    }

                case 6:
                    {
                        out[2 * i + 0] = 0.0;
                        out[2 * i + 1] = 0.0;

                        if (++time == blank_count)
                            state = 7;

                        break;
                    }

                case 7:
                    {
                        out[2 * i + 0] = 0.0;
                        out[2 * i + 1] = 0.0;
                        staydown = 0;
                        time = 0;

                        if ((tidx = scan_idx + hang_slew_count + hang_count) >= dline_size)
                            tidx -= dline_size;

                        while (time++ <= adv_count + adv_slew_count + hang_slew_count + hang_count)                                                                            //  CHECK EXACT COUNTS!!!!!!!!!!!!!!!!!!!!!!!
                        {
                            if (imp[tidx] == 1) staydown = 1;
                            if (--tidx < 0) tidx += dline_size;
                        }

                        if (staydown == 0)
                        {
                            if (hang_count > 0)
                            {
                                state = 8;
                                time = 0;
                            }
                            else if (hang_slew_count > 0)
                            {
                                state = 9;
                                time = 0;

                                if ((tidx = scan_idx + hang_slew_count + hang_count - adv_count - adv_slew_count) >= dline_size)
                                    tidx -= dline_size;

                                if (tidx < 0)
                                    tidx += dline_size;

                                Inext = dline[2 * tidx + 0];
                                Qnext = dline[2 * tidx + 1];
                            }
                            else
                            {
                                state = 0;
                                overflow = 0;
                            }
                        }

                        break;
                    }

                case 8:
                    {
                        out[2 * i + 0] = 0.0;
                        out[2 * i + 1] = 0.0;

                        if (++time == hang_count)
                        {
                            if (hang_slew_count > 0)
                            {
                                state = 9;
                                time = 0;

                                if ((tidx = scan_idx + hang_slew_count - adv_count - adv_slew_count) >= dline_size)
                                    tidx -= dline_size;

                                if (tidx < 0)
                                    tidx += dline_size;

                                Inext = dline[2 * tidx + 0];
                                Qnext = dline[2 * tidx + 1];
                            }
                            else
                            {
                                state = 0;
                                overflow = 0;
                            }
                        }

                        break;
                    }

                case 9:
                    {
                        scale = 0.5 - hwave[time];
                        out[2 * i + 0] = (float) (Inext * scale);
                        out[2 * i + 1] = (float) (Qnext * scale);

                        if (++time >= hang_slew_count)
                        {
                            state = 0;
                            overflow = 0;
                        }

                        break;
                    }
                default:
                    break;
            }

            if (++in_idx == dline_size)
                in_idx = 0;

            if (++scan_idx == dline_size)
                scan_idx = 0;

            if (++out_idx == dline_size)
                out_idx = 0;
        }
    }
    else if (in != out)
    {
        std::copy(in, in + buffsize * 2, out);
    }
}

void NOB::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void NOB::setSize(int size)
{
    buffsize = size;
    flush();
}

/********************************************************************************************************
*                                                                                                       *
*                                        Common interface                                               *
*                                                                                                       *
********************************************************************************************************/

void NOB::setRun(int _run)
{
    run = _run;
}

void NOB::setMode(int _mode)
{
    mode = _mode;
}

void NOB::setBuffsize(int size)
{
    buffsize = size;
}

void NOB::setSamplerate(int rate)
{
    samplerate = (double) rate;
    init();
}

void NOB::setTau(double tau)
{
    advslewtime = tau;
    hangslewtime = tau;
    init();
}

void NOB::setHangtime(double _time)
{
    hangtime = _time;
    init();
}

void NOB::setAdvtime(double _time)
{
    advtime = _time;
    init();
}

void NOB::setBacktau(double tau)
{
    backtau = tau;
    init();
}

void NOB::setThreshold(double thresh)
{
    threshold = thresh;
}

} // namespace
