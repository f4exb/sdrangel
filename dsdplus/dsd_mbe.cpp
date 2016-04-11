///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include <math.h>
#include "dsd_mbe.h"
#include "dsd_decoder.h"

extern "C" {
#include <mbelib.h>
}

namespace DSDplus
{

DSDMBEDecoder::DSDMBEDecoder(DSDDecoder *dsdDecoder) :
        m_dsdDecoder(dsdDecoder)
{
}

DSDMBEDecoder::~DSDMBEDecoder()
{
}

void DSDMBEDecoder::processFrame(char imbe_fr[8][23], char ambe_fr[4][24], char imbe7100_fr[7][24])
{
//    for (int i = 0; i < 88; i++)
//    {
//        imbe_d[i] = 0;
//    }
    memset((void *) imbe_d, 0, 88);

    if ((m_dsdDecoder->m_state.synctype == 0) || (m_dsdDecoder->m_state.synctype == 1))
    {
        mbe_processImbe7200x4400Framef(m_dsdDecoder->m_state.audio_out_temp_buf, &m_dsdDecoder->m_state.errs,
                &m_dsdDecoder->m_state.errs2, m_dsdDecoder->m_state.err_str, imbe_fr, imbe_d, m_dsdDecoder->m_state.cur_mp,
                m_dsdDecoder->m_state.prev_mp, m_dsdDecoder->m_state.prev_mp_enhanced, m_dsdDecoder->m_opts.uvquality);
    }
    else if ((m_dsdDecoder->m_state.synctype == 14) || (m_dsdDecoder->m_state.synctype == 15))
    {
        mbe_processImbe7100x4400Framef(m_dsdDecoder->m_state.audio_out_temp_buf, &m_dsdDecoder->m_state.errs,
                &m_dsdDecoder->m_state.errs2, m_dsdDecoder->m_state.err_str, imbe7100_fr, imbe_d,
                m_dsdDecoder->m_state.cur_mp, m_dsdDecoder->m_state.prev_mp, m_dsdDecoder->m_state.prev_mp_enhanced,
                m_dsdDecoder->m_opts.uvquality);
    }
    else if ((m_dsdDecoder->m_state.synctype == 6) || (m_dsdDecoder->m_state.synctype == 7))
    {
        mbe_processAmbe3600x2400Framef(m_dsdDecoder->m_state.audio_out_temp_buf, &m_dsdDecoder->m_state.errs,
                &m_dsdDecoder->m_state.errs2, m_dsdDecoder->m_state.err_str, ambe_fr, ambe_d, m_dsdDecoder->m_state.cur_mp,
                m_dsdDecoder->m_state.prev_mp, m_dsdDecoder->m_state.prev_mp_enhanced, m_dsdDecoder->m_opts.uvquality);
    }
    else
    {
        mbe_processAmbe3600x2450Framef(m_dsdDecoder->m_state.audio_out_temp_buf, &m_dsdDecoder->m_state.errs,
                &m_dsdDecoder->m_state.errs2, m_dsdDecoder->m_state.err_str, ambe_fr, ambe_d, m_dsdDecoder->m_state.cur_mp,
                m_dsdDecoder->m_state.prev_mp, m_dsdDecoder->m_state.prev_mp_enhanced, m_dsdDecoder->m_opts.uvquality);
    }

    if (m_dsdDecoder->m_opts.errorbars == 1)
    {
        fprintf(stderr, "%s", m_dsdDecoder->m_state.err_str);
    }

    processAudio();
//    if (m_dsdDecoder->m_opts.wav_out_f != NULL)
//    {
//        writeSynthesizedVoice(opts, state);
//    }
//
//    if (m_dsdDecoder->m_opts.audio_out == 1)
//    {
//        playSynthesizedVoice(opts, state);
//    }
}

void DSDMBEDecoder::processAudio()
{
    int i, n;
    float aout_abs, max, gainfactor, gaindelta, maxbuf;

    if (m_dsdDecoder->m_opts.audio_gain == (float) 0)
    {
        // detect max level
        max = 0;
        m_dsdDecoder->m_state.audio_out_temp_buf_p = m_dsdDecoder->m_state.audio_out_temp_buf;

        for (n = 0; n < 160; n++)
        {
            aout_abs = fabsf(*m_dsdDecoder->m_state.audio_out_temp_buf_p);

            if (aout_abs > max)
            {
                max = aout_abs;
            }

            m_dsdDecoder->m_state.audio_out_temp_buf_p++;
        }

        *m_dsdDecoder->m_state.aout_max_buf_p = max;
        m_dsdDecoder->m_state.aout_max_buf_p++;
        m_dsdDecoder->m_state.aout_max_buf_idx++;

        if (m_dsdDecoder->m_state.aout_max_buf_idx > 24)
        {
            m_dsdDecoder->m_state.aout_max_buf_idx = 0;
            m_dsdDecoder->m_state.aout_max_buf_p = m_dsdDecoder->m_state.aout_max_buf;
        }

        // lookup max history
        for (i = 0; i < 25; i++)
        {
            maxbuf = m_dsdDecoder->m_state.aout_max_buf[i];

            if (maxbuf > max)
            {
                max = maxbuf;
            }
        }

        // determine optimal gain level
        if (max > (float) 0)
        {
            gainfactor = ((float) 30000 / max);
        }
        else
        {
            gainfactor = (float) 50;
        }

        if (gainfactor < m_dsdDecoder->m_state.aout_gain)
        {
            m_dsdDecoder->m_state.aout_gain = gainfactor;
            gaindelta = (float) 0;
        }
        else
        {
            if (gainfactor > (float) 50)
            {
                gainfactor = (float) 50;
            }

            gaindelta = gainfactor - m_dsdDecoder->m_state.aout_gain;

            if (gaindelta > ((float) 0.05 * m_dsdDecoder->m_state.aout_gain))
            {
                gaindelta = ((float) 0.05 * m_dsdDecoder->m_state.aout_gain);
            }
        }

        gaindelta /= (float) 160;
    }
    else
    {
        gaindelta = (float) 0;
    }

    if (m_dsdDecoder->m_opts.audio_gain >= 0)
    {
        // adjust output gain
        m_dsdDecoder->m_state.audio_out_temp_buf_p = m_dsdDecoder->m_state.audio_out_temp_buf;

        for (n = 0; n < 160; n++)
        {
            *m_dsdDecoder->m_state.audio_out_temp_buf_p = (m_dsdDecoder->m_state.aout_gain
                    + ((float) n * gaindelta)) * (*m_dsdDecoder->m_state.audio_out_temp_buf_p);
            m_dsdDecoder->m_state.audio_out_temp_buf_p++;
        }

        m_dsdDecoder->m_state.aout_gain += ((float) 160 * gaindelta);
    }

    // copy audio data to output buffer and upsample if necessary
    m_dsdDecoder->m_state.audio_out_temp_buf_p = m_dsdDecoder->m_state.audio_out_temp_buf;

    if ((m_dsdDecoder->m_opts.split == 0) || (m_dsdDecoder->m_opts.upsample != 0)) // upsampling to 48k
    {
        if (m_dsdDecoder->m_state.audio_out_nb_samples + 960 >= m_dsdDecoder->m_state.audio_out_buf_size)
        {
            m_dsdDecoder->resetAudio();
        }

        m_dsdDecoder->m_state.audio_out_float_buf_p = m_dsdDecoder->m_state.audio_out_float_buf;

        for (n = 0; n < 160; n++)
        {
            upsample(*m_dsdDecoder->m_state.audio_out_temp_buf_p);
            m_dsdDecoder->m_state.audio_out_temp_buf_p++;
            m_dsdDecoder->m_state.audio_out_float_buf_p += 6;
            m_dsdDecoder->m_state.audio_out_idx += 6;
            m_dsdDecoder->m_state.audio_out_idx2 += 6;
        }

        m_dsdDecoder->m_state.audio_out_float_buf_p = m_dsdDecoder->m_state.audio_out_float_buf;

        // copy to output (short) buffer
        for (n = 0; n < 960; n++)
        {
            if (*m_dsdDecoder->m_state.audio_out_float_buf_p > (float) 32760)
            {
                *m_dsdDecoder->m_state.audio_out_float_buf_p = (float) 32760;
            }
            else if (*m_dsdDecoder->m_state.audio_out_float_buf_p < (float) -32760)
            {
                *m_dsdDecoder->m_state.audio_out_float_buf_p = (float) -32760;
            }

            *m_dsdDecoder->m_state.audio_out_buf_p = (short) *m_dsdDecoder->m_state.audio_out_float_buf_p;
            m_dsdDecoder->m_state.audio_out_buf_p++;

            if (m_dsdDecoder->m_opts.stereo) // produce second channel
            {
                *m_dsdDecoder->m_state.audio_out_buf_p = (short) *m_dsdDecoder->m_state.audio_out_float_buf_p;
                m_dsdDecoder->m_state.audio_out_buf_p++;
            }

            m_dsdDecoder->m_state.audio_out_nb_samples++;
            m_dsdDecoder->m_state.audio_out_float_buf_p++;
        }

        m_dsdDecoder->m_state.audio_out_float_buf_p += m_dsdDecoder->m_opts.playoffset;
    }
    else // leave at 8k
    {
        if (m_dsdDecoder->m_state.audio_out_nb_samples + 160 >= m_dsdDecoder->m_state.audio_out_buf_size)
        {
            m_dsdDecoder->resetAudio();
        }

        for (n = 0; n < 160; n++)
        {
            if (*m_dsdDecoder->m_state.audio_out_temp_buf_p > (float) 32760)
            {
                *m_dsdDecoder->m_state.audio_out_temp_buf_p = (float) 32760;
            }
            else if (*m_dsdDecoder->m_state.audio_out_temp_buf_p < (float) -32760)
            {
                *m_dsdDecoder->m_state.audio_out_temp_buf_p = (float) -32760;
            }

            *m_dsdDecoder->m_state.audio_out_buf_p = (short) *m_dsdDecoder->m_state.audio_out_temp_buf_p;
            m_dsdDecoder->m_state.audio_out_buf_p++;

            if (m_dsdDecoder->m_opts.stereo) // produce second channel
            {
                *m_dsdDecoder->m_state.audio_out_buf_p = (short) *m_dsdDecoder->m_state.audio_out_float_buf_p;
                m_dsdDecoder->m_state.audio_out_buf_p++;
            }

            m_dsdDecoder->m_state.audio_out_nb_samples++;
            m_dsdDecoder->m_state.audio_out_temp_buf_p++;
            m_dsdDecoder->m_state.audio_out_idx++;
            m_dsdDecoder->m_state.audio_out_idx2++;
        }
    }
}

void DSDMBEDecoder::upsample(float invalue)
{
    int i, j, sum;
    float *outbuf1, c, d;

    outbuf1 = m_dsdDecoder->m_state.audio_out_float_buf_p;
    outbuf1--;
    c = *outbuf1;
    d = invalue;
    // basic triangle interpolation
    outbuf1++;
    *outbuf1 = ((invalue * (float) 0.166) + (c * (float) 0.834));
    outbuf1++;
    *outbuf1 = ((invalue * (float) 0.332) + (c * (float) 0.668));
    outbuf1++;
    *outbuf1 = ((invalue * (float) 0.5) + (c * (float) 0.5));
    outbuf1++;
    *outbuf1 = ((invalue * (float) 0.668) + (c * (float) 0.332));
    outbuf1++;
    *outbuf1 = ((invalue * (float) 0.834) + (c * (float) 0.166));
    outbuf1++;
    *outbuf1 = d;
    outbuf1++;

    if (m_dsdDecoder->m_state.audio_out_idx2 > 24)
    {
        // smoothing
        outbuf1 -= 16;
        for (j = 0; j < 4; j++)
        {
            for (i = 0; i < 6; i++)
            {
                sum = 0;
                outbuf1 -= 2;
                sum += *outbuf1;
                outbuf1 += 2;
                sum += *outbuf1;
                outbuf1 += 2;
                sum += *outbuf1;
                outbuf1 -= 2;
                *outbuf1 = (sum / (float) 3);
                outbuf1++;
            }
            outbuf1 -= 8;
        }
    }
}


}
