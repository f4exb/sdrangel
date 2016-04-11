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

#include "dsd_state.h"

namespace DSDplus
{

DSDState::DSDState()
{
    int i, j;

    repeat = 0;

    dibit_buf = (int *) malloc(sizeof(int) * 1000000);
    memset(dibit_buf, 0, sizeof(int) * 200);
    dibit_buf_p = dibit_buf + 200;

    audio_out_buf = (short *) malloc(sizeof(short) * 2 * 48000); // 1s of L+R S16LE samples
    memset(audio_out_buf, 0, sizeof(short) * 2 * 48000);
    audio_out_buf_p = audio_out_buf;
    audio_out_nb_samples = 0;
    audio_out_buf_size = 48000; // given in number of unique samples

    audio_out_float_buf = (float *) malloc(sizeof(float) * 960); // 1 frame of 160 samples upampled 6 times
    memset(audio_out_float_buf, 0, sizeof(float) * 960);
    audio_out_float_buf_p = audio_out_float_buf;

    audio_out_temp_buf_p = audio_out_temp_buf;

    audio_out_idx = 0;
    audio_out_idx2 = 0;

    center = 0;
    jitter = -1;
    synctype = -1;
    min = -15000;
    max = 15000;
    lmid = 0;
    umid = 0;
    minref = -12000;
    maxref = 12000;
    lastsample = 0;

    for (i = 0; i < 128; i++)
    {
        sbuf[i] = 0;
    }

    sidx = 0;

    for (i = 0; i < 1024; i++)
    {
        maxbuf[i] = 15000;
    }

    for (i = 0; i < 1024; i++)
    {
        minbuf[i] = -15000;
    }

    midx = 0;
    err_str[0] = 0;
    sprintf(fsubtype, "              ");
    sprintf(ftype, "             ");
    symbolcnt = 0;
    rf_mod = 0;
    numflips = 0;
    lastsynctype = -1;
    lastp25type = 0;
    offset = 0;
    carrier = 0;

    for (i = 0; i < 25; i++)
    {
        for (j = 0; j < 16; j++)
        {
            tg[i][j] = 48;
        }
    }

    tgcount = 0;
    lasttg = 0;
    lastsrc = 0;
    nac = 0;
    errs = 0;
    errs2 = 0;
    mbe_file_type = -1;
    optind = 0;
    numtdulc = 0;
    firstframe = 0;
    sprintf(slot0light, " slot0 ");
    sprintf(slot1light, " slot1 ");
    aout_gain = 25;
    memset(aout_max_buf, 0, sizeof(float) * 200);
    aout_max_buf_p = aout_max_buf;
    aout_max_buf_idx = 0;
    samplesPerSymbol = 10;
    symbolCenter = 4;
    sprintf(algid, "________");
    sprintf(keyid, "________________");
    currentslot = 0;
    cur_mp = (mbe_parms *) malloc(sizeof(mbe_parms));
    prev_mp = (mbe_parms *) malloc(sizeof(mbe_parms));
    prev_mp_enhanced = (mbe_parms *) malloc(sizeof(mbe_parms));
    mbe_initMbeParms(cur_mp, prev_mp, prev_mp_enhanced);
    p25kid = 0;

    output_finished = 0;
    output_offset = 0;
    output_num_samples = 0;
    output_samples = 0;
    output_length = 0;
    output_buffer = 0;
}

DSDState::~DSDState()
{
    free(prev_mp_enhanced);
    free(prev_mp);
    free(cur_mp);
    free(audio_out_float_buf);
    free(audio_out_buf);
    free(dibit_buf);
}

} // namespace dsdplus
