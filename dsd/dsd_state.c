/*
 * Copyright (C) 2010 DSD Author
 * GPG Key ID: 0x3F1D7FD0 (74EF 430D F7F2 0A48 FCE6  F630 FAA2 635D 3F1D 7FD0)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dsd_state.h"

void initState(dsd_state * state)
{
    int i, j;

    state->dibit_buf = malloc(sizeof(int) * 1000000);
    state->dibit_buf_p = state->dibit_buf + 200;
    memset(state->dibit_buf, 0, sizeof(int) * 200);
    state->repeat = 0;
    state->audio_out_buf = malloc(sizeof(short) * 1000000);
    memset(state->audio_out_buf, 0, 100 * sizeof(short));
    state->audio_out_buf_p = state->audio_out_buf + 100;
    state->audio_out_float_buf = malloc(sizeof(float) * 1000000);
    memset(state->audio_out_float_buf, 0, 100 * sizeof(float));
    state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
    state->audio_out_idx = 0;
    state->audio_out_idx2 = 0;
    state->audio_out_temp_buf_p = state->audio_out_temp_buf;
    //state->wav_out_bytes = 0;
    state->center = 0;
    state->jitter = -1;
    state->synctype = -1;
    state->min = -15000;
    state->max = 15000;
    state->lmid = 0;
    state->umid = 0;
    state->minref = -12000;
    state->maxref = 12000;
    state->lastsample = 0;
    for (i = 0; i < 128; i++)
    {
        state->sbuf[i] = 0;
    }
    state->sidx = 0;
    for (i = 0; i < 1024; i++)
    {
        state->maxbuf[i] = 15000;
    }
    for (i = 0; i < 1024; i++)
    {
        state->minbuf[i] = -15000;
    }
    state->midx = 0;
    state->err_str[0] = 0;
    sprintf(state->fsubtype, "              ");
    sprintf(state->ftype, "             ");
    state->symbolcnt = 0;
    state->rf_mod = 0;
    state->numflips = 0;
    state->lastsynctype = -1;
    state->lastp25type = 0;
    state->offset = 0;
    state->carrier = 0;
    for (i = 0; i < 25; i++)
    {
        for (j = 0; j < 16; j++)
        {
            state->tg[i][j] = 48;
        }
    }
    state->tgcount = 0;
    state->lasttg = 0;
    state->lastsrc = 0;
    state->nac = 0;
    state->errs = 0;
    state->errs2 = 0;
    state->mbe_file_type = -1;
    state->optind = 0;
    state->numtdulc = 0;
    state->firstframe = 0;
    sprintf(state->slot0light, " slot0 ");
    sprintf(state->slot1light, " slot1 ");
    state->aout_gain = 25;
    memset(state->aout_max_buf, 0, sizeof(float) * 200);
    state->aout_max_buf_p = state->aout_max_buf;
    state->aout_max_buf_idx = 0;
    state->samplesPerSymbol = 10;
    state->symbolCenter = 4;
    sprintf(state->algid, "________");
    sprintf(state->keyid, "________________");
    state->currentslot = 0;
    state->cur_mp = malloc(sizeof(mbe_parms));
    state->prev_mp = malloc(sizeof(mbe_parms));
    state->prev_mp_enhanced = malloc(sizeof(mbe_parms));
    mbe_initMbeParms(state->cur_mp, state->prev_mp, state->prev_mp_enhanced);
    state->p25kid = 0;

    state->debug_audio_errors = 0;
    state->debug_header_errors = 0;
    state->debug_header_critical_errors = 0;

#ifdef TRACE_DSD
    state->debug_sample_index = 0;
    state->debug_label_file = NULL;
    state->debug_label_dibit_file = NULL;
    state->debug_label_imbe_file = NULL;
#endif

    initialize_p25_heuristics(&state->p25_heuristics);
}

