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
#include "p25p1_const.h"

void
processLDU1 (dsd_opts * opts, dsd_state * state)
{
  // extracts IMBE frames rom LDU frame
  int i, j, k, dibit, stats, count, scount;
  char imbe_fr[8][23];
  char lcformat[9], mfid[9], lcinfo[57], lsd1[9], lsd2[9], status[25];
  const int *w, *x, *y, *z;

  lcformat[8] = 0;
  mfid[8] = 0;
  lcinfo[56] = 0;
  lsd1[8] = 0;
  lsd2[8] = 0;
  status[24] = 0;

  skipDibit (opts, state, 3);
  status[0] = getDibit (opts, state) + 48;
  skipDibit (opts, state, 21);
  count = 57;
  scount = 1;

  if (opts->errorbars == 1)
    {
      printf ("e:");
    }

  // separate imbe frames and deinterleave
  stats = 21;                   // we skip the status dibits that occur every 36 symbols
  // the first IMBE frame starts 14 symbols before next status
  // so we start counter at 22
  for (i = 0; i < 9; i++)
    {                           // 9 IMBE frames per LDU
      w = iW;
      x = iX;
      y = iY;
      z = iZ;
      for (j = 0; j < 72; j++)
        {
          if (stats == 35)
            {
              status[scount] = getDibit (opts, state) + 48;
              scount++;
              stats = 1;
              count++;
            }
          else
            {
              stats++;
            }
          dibit = getDibit (opts, state);
          count++;
          imbe_fr[*w][*x] = (1 & (dibit >> 1)); // bit 1
          imbe_fr[*y][*z] = (1 & dibit);        // bit 0
          w++;
          x++;
          y++;
          z++;
        }
      if (state->p25kid == 0 || opts->unmute_encrypted_p25 == 1)
        {
          processMbeFrame (opts, state, imbe_fr, NULL, NULL);
        }

      // skip over non imbe data sometimes between frames
      if ((i < 4) || (i == 8))
        {
          k = 0;
        }
      else if (i == 7)
        {
          //k=16;
          k = 0;
        }
      else
        {
          k = 20;
        }
      for (j = 0; j < k; j++)
        {
          if (stats == 35)
            {
              status[scount] = getDibit (opts, state) + 48;
              scount++;
              count++;
              stats = 1;
            }
          else
            {
              stats++;
            }
          skipDibit (opts, state, 1);
          count++;
        }

      if (i == 1)
        {
          dibit = getDibit (opts, state);
          lcformat[0] = (1 & (dibit >> 1)) + 48;        // bit 1
          lcformat[1] = (1 & dibit) + 48;       // bit 0
          dibit = getDibit (opts, state);
          lcformat[2] = (1 & (dibit >> 1)) + 48;        // bit 1
          lcformat[3] = (1 & dibit) + 48;       // bit 0
          dibit = getDibit (opts, state);
          lcformat[4] = (1 & (dibit >> 1)) + 48;        // bit 1
          lcformat[5] = (1 & dibit) + 48;       // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          lcformat[6] = (1 & (dibit >> 1)) + 48;        // bit 1
          lcformat[7] = (1 & dibit) + 48;       // bit 0
          dibit = getDibit (opts, state);
          mfid[0] = (1 & (dibit >> 1)) + 48;    // bit 1
          mfid[1] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          mfid[2] = (1 & (dibit >> 1)) + 48;    // bit 1
          mfid[3] = (1 & dibit) + 48;   // bit 0
          skipDibit (opts, state, 2);
          status[scount] = getDibit (opts, state) + 48;
          scount++;
          dibit = getDibit (opts, state);
          mfid[4] = (1 & (dibit >> 1)) + 48;    // bit 1
          mfid[5] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          mfid[6] = (1 & (dibit >> 1)) + 48;    // bit 1
          mfid[7] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          lcinfo[0] = (1 & (dibit >> 1)) + 48;  // bit 1
          lcinfo[1] = (1 & dibit) + 48; // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          lcinfo[2] = (1 & (dibit >> 1)) + 48;  // bit 1
          lcinfo[3] = (1 & dibit) + 48; // bit 0
          dibit = getDibit (opts, state);
          lcinfo[4] = (1 & (dibit >> 1)) + 48;  // bit 1
          lcinfo[5] = (1 & dibit) + 48; // bit 0
          dibit = getDibit (opts, state);
          lcinfo[6] = (1 & (dibit >> 1)) + 48;  // bit 1
          lcinfo[7] = (1 & dibit) + 48; // bit 0
          skipDibit (opts, state, 2);
          stats = 10;
        }
      else if (i == 2)
        {
          dibit = getDibit (opts, state);
          lcinfo[8] = (1 & (dibit >> 1)) + 48;  // bit 1
          lcinfo[9] = (1 & dibit) + 48; // bit 0
          dibit = getDibit (opts, state);
          lcinfo[10] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[11] = (1 & dibit) + 48;        // bit 0
          dibit = getDibit (opts, state);
          lcinfo[12] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[13] = (1 & dibit) + 48;        // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          lcinfo[14] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[15] = (1 & dibit) + 48;        // bit 0
          dibit = getDibit (opts, state);
          lcinfo[16] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[17] = (1 & dibit) + 48;        // bit 0
          dibit = getDibit (opts, state);
          lcinfo[18] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[19] = (1 & dibit) + 48;        // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          lcinfo[20] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[21] = (1 & dibit) + 48;        // bit 0
          dibit = getDibit (opts, state);
          lcinfo[22] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[23] = (1 & dibit) + 48;        // bit 0
          dibit = getDibit (opts, state);
          lcinfo[24] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[25] = (1 & dibit) + 48;        // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          lcinfo[26] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[27] = (1 & dibit) + 48;        // bit 0
          dibit = getDibit (opts, state);
          lcinfo[28] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[29] = (1 & dibit) + 48;        // bit 0
          dibit = getDibit (opts, state);
          lcinfo[30] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[31] = (1 & dibit) + 48;        // bit 0
          skipDibit (opts, state, 2);
          stats = 32;
        }
      else if (i == 3)
        {
          dibit = getDibit (opts, state);
          lcinfo[32] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[33] = (1 & dibit) + 48;        // bit 0
          status[scount] = getDibit (opts, state) + 48;
          scount++;
          dibit = getDibit (opts, state);
          lcinfo[34] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[35] = (1 & dibit) + 48;        // bit 0
          dibit = getDibit (opts, state);
          lcinfo[36] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[37] = (1 & dibit) + 48;        // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          lcinfo[38] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[39] = (1 & dibit) + 48;        // bit 0
          dibit = getDibit (opts, state);
          lcinfo[40] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[41] = (1 & dibit) + 48;        // bit 0
          dibit = getDibit (opts, state);
          lcinfo[42] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[43] = (1 & dibit) + 48;        // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          lcinfo[44] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[45] = (1 & dibit) + 48;        // bit 0
          dibit = getDibit (opts, state);
          lcinfo[46] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[47] = (1 & dibit) + 48;        // bit 0
          dibit = getDibit (opts, state);
          lcinfo[48] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[49] = (1 & dibit) + 48;        // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          lcinfo[50] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[51] = (1 & dibit) + 48;        // bit 0
          dibit = getDibit (opts, state);
          lcinfo[52] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[53] = (1 & dibit) + 48;        // bit 0
          dibit = getDibit (opts, state);
          lcinfo[54] = (1 & (dibit >> 1)) + 48; // bit 1
          lcinfo[55] = (1 & dibit) + 48;        // bit 0
          skipDibit (opts, state, 2);
          stats = 19;
        }
      else if (i == 7)
        {
          dibit = getDibit (opts, state);
          lsd1[0] = (1 & (dibit >> 1)) + 48;    // bit 1
          lsd1[1] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          lsd1[2] = (1 & (dibit >> 1)) + 48;    // bit 1
          lsd1[3] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          lsd1[4] = (1 & (dibit >> 1)) + 48;    // bit 1
          lsd1[5] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          lsd1[6] = (1 & (dibit >> 1)) + 48;    // bit 1
          lsd1[7] = (1 & dibit) + 48;   // bit 0
          skipDibit (opts, state, 4);
          dibit = getDibit (opts, state);
          lsd2[0] = (1 & (dibit >> 1)) + 48;    // bit 1
          lsd2[1] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          lsd2[2] = (1 & (dibit >> 1)) + 48;    // bit 1
          lsd2[3] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          lsd2[4] = (1 & (dibit >> 1)) + 48;    // bit 1
          lsd2[5] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          lsd2[6] = (1 & (dibit >> 1)) + 48;    // bit 1
          lsd2[7] = (1 & dibit) + 48;   // bit 0
          skipDibit (opts, state, 4);
          stats = 33;
        }
    }
  // trailing status symbol
  status[scount] = getDibit (opts, state) + 48;
  scount++;

  if (opts->errorbars == 1)
    {
      printf ("\n");
    }

  if (opts->p25status == 1)
    {
      printf ("status: %s lsd1: %s lsd2: %s\n", status, lsd1, lsd2);
    }

  processP25lcw (opts, state, lcformat, mfid, lcinfo);
}
