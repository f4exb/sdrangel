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
processLDU2 (dsd_opts * opts, dsd_state * state)
{
  // extracts IMBE frames rom LDU frame
  int i, j, k, dibit, stats, count, scount;
  char imbe_fr[8][23];
  char mi[73], algid[9], kid[17], lsd3[9], lsd4[9], status[25];
  const int *w, *x, *y, *z;
  int algidhex, kidhex;

  status[24] = 0;
  mi[72] = 0;
  algid[8] = 0;
  kid[16] = 0;
  lsd3[8] = 0;
  lsd4[8] = 0;

  skipDibit (opts, state, 3);
  status[0] = getDibit (opts, state) + 48;
  skipDibit (opts, state, 21);
  scount = 1;

  count = 57;

  if (opts->errorbars == 1)
    {
      printf ("e:");
    }

  // separate imbe frames and deinterleave
  stats = 21;
  // we skip the status dibits that occur every 36 symbols
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
      if ((i < 5) || (i == 8))
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
          mi[0] = (1 & (dibit >> 1)) + 48;      // bit 1
          mi[1] = (1 & dibit) + 48;     // bit 0
          dibit = getDibit (opts, state);
          mi[2] = (1 & (dibit >> 1)) + 48;      // bit 1
          mi[3] = (1 & dibit) + 48;     // bit 0
          dibit = getDibit (opts, state);
          mi[4] = (1 & (dibit >> 1)) + 48;      // bit 1
          mi[5] = (1 & dibit) + 48;     // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          mi[6] = (1 & (dibit >> 1)) + 48;      // bit 1
          mi[7] = (1 & dibit) + 48;     // bit 0
          dibit = getDibit (opts, state);
          mi[8] = (1 & (dibit >> 1)) + 48;      // bit 1
          mi[9] = (1 & dibit) + 48;     // bit 0
          dibit = getDibit (opts, state);
          mi[10] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[11] = (1 & dibit) + 48;    // bit 0
          skipDibit (opts, state, 2);
          status[scount] = getDibit (opts, state) + 48;
          scount++;
          dibit = getDibit (opts, state);
          mi[12] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[13] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[14] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[15] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[16] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[17] = (1 & dibit) + 48;    // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          mi[18] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[19] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[20] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[21] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[22] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[23] = (1 & dibit) + 48;    // bit 0
          skipDibit (opts, state, 2);
          stats = 10;
        }
      else if (i == 2)
        {
          dibit = getDibit (opts, state);
          mi[24] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[25] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[26] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[27] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[28] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[29] = (1 & dibit) + 48;    // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          mi[30] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[31] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[32] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[33] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[34] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[35] = (1 & dibit) + 48;    // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          mi[36] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[37] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[38] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[39] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[40] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[41] = (1 & dibit) + 48;    // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          mi[42] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[43] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[44] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[45] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[46] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[47] = (1 & dibit) + 48;    // bit 0
          skipDibit (opts, state, 2);
          stats = 32;
        }
      else if (i == 3)
        {
          dibit = getDibit (opts, state);
          mi[48] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[49] = (1 & dibit) + 48;    // bit 0
          status[scount] = getDibit (opts, state) + 48;
          scount++;
          dibit = getDibit (opts, state);
          mi[50] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[51] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[52] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[53] = (1 & dibit) + 48;    // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          mi[54] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[55] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[56] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[57] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[58] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[59] = (1 & dibit) + 48;    // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          mi[60] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[61] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[62] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[63] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[64] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[65] = (1 & dibit) + 48;    // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          mi[66] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[67] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[68] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[69] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          mi[70] = (1 & (dibit >> 1)) + 48;     // bit 1
          mi[71] = (1 & dibit) + 48;    // bit 0
          skipDibit (opts, state, 2);
          stats = 19;
        }
      else if (i == 4)
        {
          dibit = getDibit (opts, state);
          algid[0] = (1 & (dibit >> 1)) + 48;   // bit 1
          algid[1] = (1 & dibit) + 48;  // bit 0
          dibit = getDibit (opts, state);
          algid[2] = (1 & (dibit >> 1)) + 48;   // bit 1
          algid[3] = (1 & dibit) + 48;  // bit 0
          dibit = getDibit (opts, state);
          algid[4] = (1 & (dibit >> 1)) + 48;   // bit 1
          algid[5] = (1 & dibit) + 48;  // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          algid[6] = (1 & (dibit >> 1)) + 48;   // bit 1
          algid[7] = (1 & dibit) + 48;  // bit 0
          dibit = getDibit (opts, state);
          kid[0] = (1 & (dibit >> 1)) + 48;     // bit 1
          kid[1] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          kid[2] = (1 & (dibit >> 1)) + 48;     // bit 1
          kid[3] = (1 & dibit) + 48;    // bit 0
          skipDibit (opts, state, 2);
          dibit = getDibit (opts, state);
          kid[4] = (1 & (dibit >> 1)) + 48;     // bit 1
          kid[5] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          kid[6] = (1 & (dibit >> 1)) + 48;     // bit 1
          kid[7] = (1 & dibit) + 48;    // bit 0
          dibit = getDibit (opts, state);
          kid[8] = (1 & (dibit >> 1)) + 48;     // bit 1
          kid[9] = (1 & dibit) + 48;    // bit 0
          skipDibit (opts, state, 1);
          status[scount] = getDibit (opts, state) + 48;
          scount++;
          skipDibit (opts, state, 1);
          dibit = getDibit (opts, state);
          kid[10] = (1 & (dibit >> 1)) + 48;    // bit 1
          kid[11] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          kid[12] = (1 & (dibit >> 1)) + 48;    // bit 1
          kid[13] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          kid[14] = (1 & (dibit >> 1)) + 48;    // bit 1
          kid[15] = (1 & dibit) + 48;   // bit 0
          skipDibit (opts, state, 2);
          stats = 6;
        }
      else if (i == 7)
        {
          dibit = getDibit (opts, state);
          lsd3[0] = (1 & (dibit >> 1)) + 48;    // bit 1
          lsd3[1] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          lsd3[2] = (1 & (dibit >> 1)) + 48;    // bit 1
          lsd3[3] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          lsd3[4] = (1 & (dibit >> 1)) + 48;    // bit 1
          lsd3[5] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          lsd3[6] = (1 & (dibit >> 1)) + 48;    // bit 1
          lsd3[7] = (1 & dibit) + 48;   // bit 0
          skipDibit (opts, state, 4);
          dibit = getDibit (opts, state);
          lsd4[0] = (1 & (dibit >> 1)) + 48;    // bit 1
          lsd4[1] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          lsd4[2] = (1 & (dibit >> 1)) + 48;    // bit 1
          lsd4[3] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          lsd4[4] = (1 & (dibit >> 1)) + 48;    // bit 1
          lsd4[5] = (1 & dibit) + 48;   // bit 0
          dibit = getDibit (opts, state);
          lsd4[6] = (1 & (dibit >> 1)) + 48;    // bit 1
          lsd4[7] = (1 & dibit) + 48;   // bit 0
          skipDibit (opts, state, 4);
          stats = 33;
        }

    }

  //trailing status symbol
  status[scount] = getDibit (opts, state) + 48;
  scount++;

  if (opts->errorbars == 1)
    {
      printf ("\n");
    }

  if (opts->p25status == 1)
    {
      printf ("status: %s lsd3: %s lsd4: %s\n", status, lsd3, lsd4);
    }
  if (opts->p25enc == 1)
    {
      algidhex = strtol (algid, NULL, 2);
      kidhex = strtol (kid, NULL, 2);
      printf ("mi: %s algid: $%x kid: $%x\n", mi, algidhex, kidhex);
    }
}
