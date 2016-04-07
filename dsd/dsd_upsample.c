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

void
upsample (dsd_state * state, float invalue)
{

  int i, j, sum;
  float *outbuf1, c, d;

  outbuf1 = state->audio_out_float_buf_p;
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

  if (state->audio_out_idx2 > 24)
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
