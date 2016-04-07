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

#ifndef INCLUDE_DSD_OPTS_H_
#define INCLUDE_DSD_OPTS_H_

#include "config.h"
#include <stdio.h>
#ifdef USE_LIBSNDFILE
#include <sndfile.h>
#endif
// Portaudio is not needed for bufferized (in-memory) operations
#ifdef USE_PORTAUDIO
#include "portaudio.h"
#endif

typedef struct
{
    int onesymbol;
    char mbe_in_file[1024];
    FILE *mbe_in_f;
    int errorbars;
    int datascope;
    int symboltiming;
    int verbose;
    int p25enc;
    int p25lc;
    int p25status;
    int p25tg;
    int scoperate;
    char audio_in_dev[1024];
    int audio_in_fd;
#ifdef USE_LIBSNDFILE
    SNDFILE *audio_in_file;
    SF_INFO *audio_in_file_info;
#endif
#ifdef USE_PORTAUDIO
    PaStream* audio_in_pa_stream;
#endif
    int audio_in_type; // 0 for device, 1 for file, 2 for portaudio
    char audio_out_dev[1024];
    int audio_out_fd;
#ifdef USE_LIBSNDFILE
    SNDFILE *audio_out_file;
    SF_INFO *audio_out_file_info;
#endif
#ifdef USE_PORTAUDIO
    PaStream* audio_out_pa_stream;
#endif
    int audio_out_type; // 0 for device, 1 for file, 2 for portaudio
    int split;
    int upsample; //!< Force audio output upsampling to 48kHz
    int playoffset;
    char mbe_out_dir[1024];
    char mbe_out_file[1024];
    char mbe_out_path[1024];
    FILE *mbe_out_f;
    float audio_gain;
    int audio_out;
    char wav_out_file[1024];
#ifdef USE_LIBSNDFILE
    SNDFILE *wav_out_f;
#endif
    //int wav_out_fd;
    int serial_baud;
    char serial_dev[1024];
    int serial_fd;
    int resume;
    int frame_dstar;
    int frame_x2tdma;
    int frame_p25p1;
    int frame_nxdn48;
    int frame_nxdn96;
    int frame_dmr;
    int frame_provoice;
    int mod_c4fm;
    int mod_qpsk;
    int mod_gfsk;
    int uvquality;
    int inverted_x2tdma;
    int inverted_dmr;
    int mod_threshold;
    int ssize;
    int msize;
    int playfiles;
    int delay;
    int use_cosine_filter;
    int unmute_encrypted_p25;
} dsd_opts;

void initOpts (dsd_opts * opts);

#endif /* INCLUDE_DSD_OPTS_H_ */
