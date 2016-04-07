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

#include <string.h>
#include "dsd_nocarrier.h"
#include "dsd.h"

void noCarrier(dsd_opts * opts, dsd_state * state)
{
    state->dibit_buf_p = state->dibit_buf + 200;
    memset(state->dibit_buf, 0, sizeof(int) * 200);
    if (opts->mbe_out_f != NULL)
    {
        closeMbeOutFile(opts, state);
    }
    state->jitter = -1;
    state->lastsynctype = -1;
    state->carrier = 0;
    state->max = 15000;
    state->min = -15000;
    state->center = 0;
    state->err_str[0] = 0;
    sprintf(state->fsubtype, "              ");
    sprintf(state->ftype, "             ");
    state->errs = 0;
    state->errs2 = 0;
    state->lasttg = 0;
    state->lastsrc = 0;
    state->lastp25type = 0;
    state->repeat = 0;
    state->nac = 0;
    state->numtdulc = 0;
    sprintf(state->slot0light, " slot0 ");
    sprintf(state->slot1light, " slot1 ");
    state->firstframe = 0;
    if (opts->audio_gain == (float) 0)
    {
        state->aout_gain = 25;
    }
    memset(state->aout_max_buf, 0, sizeof(float) * 200);
    state->aout_max_buf_p = state->aout_max_buf;
    state->aout_max_buf_idx = 0;
    sprintf(state->algid, "________");
    sprintf(state->keyid, "________________");
    mbe_initMbeParms(state->cur_mp, state->prev_mp, state->prev_mp_enhanced);
}
