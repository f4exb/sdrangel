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

#include "dsd_opts.h"

void initOpts(dsd_opts * opts)
{
    opts->onesymbol = 10;
    opts->mbe_in_file[0] = 0;
    opts->mbe_in_f = NULL;
    opts->errorbars = 1;
    opts->datascope = 0;
    opts->symboltiming = 0;
    opts->verbose = 2;
    opts->p25enc = 0;
    opts->p25lc = 0;
    opts->p25status = 0;
    opts->p25tg = 0;
    opts->scoperate = 15;
    sprintf(opts->audio_in_dev, "/dev/audio");
    opts->audio_in_fd = -1; // with audio_out_type = 0 and this fd = -1 it will use the bufferized (in-memory) version
#ifdef USE_PORTAUDIO
    opts->audio_in_pa_stream = NULL;
#endif
    sprintf(opts->audio_out_dev, "/dev/audio");
    opts->audio_out_fd = -1; // with audio_out_type = 0 and this fd = -1 it will use the bufferized (in-memory)version
#ifdef USE_PORTAUDIO
    opts->audio_out_pa_stream = NULL;
#endif
    opts->split = 0;
    opts->upsample = 0;
    opts->playoffset = 0;
    opts->mbe_out_dir[0] = 0;
    opts->mbe_out_file[0] = 0;
    opts->mbe_out_path[0] = 0;
    opts->mbe_out_f = NULL;
    opts->audio_gain = 0;
    opts->audio_out = 1;
    opts->wav_out_file[0] = 0;
#ifdef USE_LIBSNDFILE
    opts->wav_out_f = NULL;
#endif
    //opts->wav_out_fd = -1;
    opts->serial_baud = 115200;
    sprintf(opts->serial_dev, "/dev/ttyUSB0");
    opts->resume = 0;
    opts->frame_dstar = 0;
    opts->frame_x2tdma = 1;
    opts->frame_p25p1 = 1;
    opts->frame_nxdn48 = 0;
    opts->frame_nxdn96 = 1;
    opts->frame_dmr = 1;
    opts->frame_provoice = 0;
    opts->mod_c4fm = 1;
    opts->mod_qpsk = 1;
    opts->mod_gfsk = 1;
    opts->uvquality = 3;
    opts->inverted_x2tdma = 1; // most transmitter + scanner + sound card combinations show inverted signals for this
    opts->inverted_dmr = 0; // most transmitter + scanner + sound card combinations show non-inverted signals for this
    opts->mod_threshold = 26;
    opts->ssize = 36;
    opts->msize = 15;
    opts->playfiles = 0;
    opts->delay = 0;
    opts->use_cosine_filter = 1;
    opts->unmute_encrypted_p25 = 0;
}

