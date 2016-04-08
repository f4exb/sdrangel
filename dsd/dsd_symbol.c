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

#include "dsd.h"
#include "dsd_cleanupexit.h"

int getSymbol(dsd_opts * opts, dsd_state * state, int have_sync)
{

    short sample;
    int i, sum, symbol, count;
    ssize_t result;

    sum = 0;
    count = 0;

    for (i = 0; i < state->samplesPerSymbol; i++)
    {
        // timing control
        if ((i == 0) && (have_sync == 0))
        {
            if (state->samplesPerSymbol == 20)
            {
                if ((state->jitter >= 7) && (state->jitter <= 10))
                {
                    i--;
                }
                else if ((state->jitter >= 11) && (state->jitter <= 14))
                {
                    i++;
                }
            }
            else if (state->rf_mod == 1)
            {
                if ((state->jitter >= 0) && (state->jitter < state->symbolCenter))
                {
                    i++;          // fall back
                }
                else if ((state->jitter > state->symbolCenter) && (state->jitter < 10))
                {
                    i--;          // catch up
                }
            }
            else if (state->rf_mod == 2)
            {
                if ((state->jitter >= state->symbolCenter - 1) && (state->jitter <= state->symbolCenter))
                {
                    i--;
                }
                else if ((state->jitter >= state->symbolCenter + 1) && (state->jitter <= state->symbolCenter + 2))
                {
                    i++;
                }
            }
            else if (state->rf_mod == 0)
            {
                if ((state->jitter > 0) && (state->jitter <= state->symbolCenter))
                {
                    i--;          // catch up
                }
                else if ((state->jitter > state->symbolCenter) && (state->jitter < state->samplesPerSymbol))
                {
                    i++;          // fall back
                }
            }

            state->jitter = -1;
        }

        // get sample in and push samples out
        if (opts->audio_in_type == 0)
        {
            if (opts->audio_in_fd == -1)
            {
                // Get the next sample from the buffer
                sample = state->input_samples[state->input_offset++]; // FIXME: get sample only if available

                if (state->input_offset == state->input_length) // all available samples have been read
                {
                    int i;

                    // We've reached the end of the buffer.  Wait for more next time.
                    state->input_length = 0;

                    state->output_num_samples = state->output_offset;

                    // GNUradio drivel
//                    if (state->output_num_samples > state->output_length)
//                    {
//                        state->output_num_samples = state->output_length;
//                    }
//
//                    for (i = 0; i < state->output_length - state->output_num_samples; i++)
//                    {
//                        state->output_samples[i] = 0;
//                    }
//
//                    for (; i < state->output_length; i++)
//                    {
//                        state->output_samples[i] = state->output_buffer[i - (state->output_length - state->output_num_samples)] / 32768.0;
//                    }
//
//                    state->output_offset -= state->output_num_samples;
//
//                    for (i = 0; i < state->output_offset; i++)
//                    {
//                        state->output_buffer[i] = state->output_buffer[i + state->output_num_samples];
//                    }

                    if (state->output_num_samples > state->output_length)
                    {
                        fprintf(stderr, "WARNING: audio buffer over-run! Truncating output");
                        state->output_num_samples = state->output_length;
                    }

                    for (i = 0; i < state->output_num_samples; i++)
                    {
                        state->output_samples[2*i] = state->output_buffer[i];    // L channel
                        state->output_samples[2*i+1] = state->output_buffer[i];  // R channel
                    }

                    state->output_finished = 1;
                }
            }
            else
            {
                result = read(opts->audio_in_fd, &sample, 2);
            }

        }
#ifdef USE_LIBSNDFILE
        else
        {
            result = sf_read_short(opts->audio_in_file, &sample, 1);
            if(result == 0)
            {
                cleanupAndExit (opts, state);
            }
        }
#endif
        // printf("res: %zd\n, offset: %lld", result, sf_seek(opts->audio_in_file, 0, SEEK_CUR));

        // process sample
        if (opts->use_cosine_filter)
        {
            if (state->lastsynctype >= 10 && state->lastsynctype <= 13)
            {
                sample = dmr_filter(sample);
            }
            else if (state->lastsynctype == 8 || state->lastsynctype == 9
                    || state->lastsynctype == 16 || state->lastsynctype == 17)
            {
                sample = nxdn_filter(sample);
            }
        }

        if ((sample > state->max) && (have_sync == 1) && (state->rf_mod == 0))
        {
            sample = state->max;
        }
        else if ((sample < state->min) && (have_sync == 1) && (state->rf_mod == 0))
        {
            sample = state->min;
        }

        if (sample > state->center)
        {
            if (state->lastsample < state->center)
            {
                state->numflips += 1;
            }
            if (sample > (state->maxref * 1.25))
            {
                if (state->lastsample < (state->maxref * 1.25))
                {
                    state->numflips += 1;
                }
                if ((state->jitter < 0) && (state->rf_mod == 1))
                {               // first spike out of place
                    state->jitter = i;
                }
                if ((opts->symboltiming == 1) && (have_sync == 0) && (state->lastsynctype != -1))
                {
                    printf("O");
                }
            }
            else
            {
                if ((opts->symboltiming == 1) && (have_sync == 0) && (state->lastsynctype != -1))
                {
                    printf("+");
                }
                if ((state->jitter < 0) && (state->lastsample < state->center) && (state->rf_mod != 1))
                {               // first transition edge
                    state->jitter = i;
                }
            }
        }
        else
        {                       // sample < 0
            if (state->lastsample > state->center)
            {
                state->numflips += 1;
            }
            if (sample < (state->minref * 1.25))
            {
                if (state->lastsample > (state->minref * 1.25))
                {
                    state->numflips += 1;
                }
                if ((state->jitter < 0) && (state->rf_mod == 1))
                {               // first spike out of place
                    state->jitter = i;
                }
                if ((opts->symboltiming == 1) && (have_sync == 0)
                        && (state->lastsynctype != -1))
                {
                    printf("X");
                }
            }
            else
            {
                if ((opts->symboltiming == 1) && (have_sync == 0) && (state->lastsynctype != -1))
                {
                    printf("-");
                }
                if ((state->jitter < 0) && (state->lastsample > state->center) && (state->rf_mod != 1))
                {               // first transition edge
                    state->jitter = i;
                }
            }
        }
        if (state->samplesPerSymbol == 20)
        {
            if ((i >= 9) && (i <= 11))
            {
                sum += sample;
                count++;
            }
        }
        if (state->samplesPerSymbol == 5)
        {
            if (i == 2)
            {
                sum += sample;
                count++;
            }
        }
        else
        {
            if (((i >= state->symbolCenter - 1)  && (i <= state->symbolCenter + 2) && (state->rf_mod == 0)) || (((i == state->symbolCenter) || (i == state->symbolCenter + 1)) && (state->rf_mod != 0)))
            {
                sum += sample;
                count++;
            }
        }

        state->lastsample = sample;
    } // for (i = 0; i < state->samplesPerSymbol; i++)

    symbol = (sum / count);

    if ((opts->symboltiming == 1) && (have_sync == 0) && (state->lastsynctype != -1))
    {
        if (state->jitter >= 0)
        {
            printf(" %i\n", state->jitter);
        }
        else
        {
            printf("\n");
        }
    }

    state->symbolcnt++;
    return (symbol);
}
