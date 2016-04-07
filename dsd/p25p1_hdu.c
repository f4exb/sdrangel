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
processHDU (dsd_opts * opts, dsd_state * state)
{

  char mi[73], mfid[9], algid[9], kid[17], tgid[17], tmpstr[255];
  int dibit, count, i, j;
  long talkgroup;
  int algidhex, kidhex;

  mi[72] = 0;
  mfid[8] = 0;
  algid[8] = 0;
  kid[16] = 0;
  tgid[16] = 0;

  skipDibit (opts, state, 25);
  count = 57;

  dibit = getDibit (opts, state);
  mi[0] = (1 & (dibit >> 1)) + 48;      // bit 1
  mi[1] = (1 & dibit) + 48;     // bit 0
  dibit = getDibit (opts, state);
  mi[2] = (1 & (dibit >> 1)) + 48;      // bit 1
  mi[3] = (1 & dibit) + 48;     // bit 0
  dibit = getDibit (opts, state);
  mi[4] = (1 & (dibit >> 1)) + 48;      // bit 1
  mi[5] = (1 & dibit) + 48;     // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  mi[6] = (1 & (dibit >> 1)) + 48;      // bit 1
  mi[7] = (1 & dibit) + 48;     // bit 0
  dibit = getDibit (opts, state);
  mi[8] = (1 & (dibit >> 1)) + 48;      // bit 1
  mi[9] = (1 & dibit) + 48;     // bit 0
  dibit = getDibit (opts, state);
  mi[10] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[11] = (1 & dibit) + 48;    // bit 0
  skipDibit (opts, state, 7);

  dibit = getDibit (opts, state);
  mi[12] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[13] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[14] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[15] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[16] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[17] = (1 & dibit) + 48;    // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  mi[18] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[19] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[20] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[21] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[22] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[23] = (1 & dibit) + 48;    // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  mi[24] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[25] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[26] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[27] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[28] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[29] = (1 & dibit) + 48;    // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  mi[30] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[31] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[32] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[33] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[34] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[35] = (1 & dibit) + 48;    // bit 0
  skipDibit (opts, state, 7);

  dibit = getDibit (opts, state);
  mi[36] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[37] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[38] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[39] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[40] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[41] = (1 & dibit) + 48;    // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  mi[42] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[43] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[44] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[45] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[46] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[47] = (1 & dibit) + 48;    // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  mi[48] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[49] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[50] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[51] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[52] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[53] = (1 & dibit) + 48;    // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  mi[54] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[55] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[56] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[57] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[58] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[59] = (1 & dibit) + 48;    // bit 0
  skipDibit (opts, state, 7);

  dibit = getDibit (opts, state);
  mi[60] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[61] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[62] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[63] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[64] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[65] = (1 & dibit) + 48;    // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  mi[66] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[67] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[68] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[69] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  mi[70] = (1 & (dibit >> 1)) + 48;     // bit 1
  mi[71] = (1 & dibit) + 48;    // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  mfid[0] = (1 & (dibit >> 1)) + 48;    // bit 1
  mfid[1] = (1 & dibit) + 48;   // bit 0
  dibit = getDibit (opts, state);
  mfid[2] = (1 & (dibit >> 1)) + 48;    // bit 1
  mfid[3] = (1 & dibit) + 48;   // bit 0
  dibit = getDibit (opts, state);
  mfid[4] = (1 & (dibit >> 1)) + 48;    // bit 1
  mfid[5] = (1 & dibit) + 48;   // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  mfid[6] = (1 & (dibit >> 1)) + 48;    // bit 1
  mfid[7] = (1 & dibit) + 48;   // bit 0
  dibit = getDibit (opts, state);
  algid[0] = (1 & (dibit >> 1)) + 48;   // bit 1
  algid[1] = (1 & dibit) + 48;  // bit 0
  skipDibit (opts, state, 1);
  dibit = getDibit (opts, state);
  algid[2] = (1 & (dibit >> 1)) + 48;   // bit 1
  algid[3] = (1 & dibit) + 48;  // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  algid[4] = (1 & (dibit >> 1)) + 48;   // bit 1
  algid[5] = (1 & dibit) + 48;  // bit 0
  dibit = getDibit (opts, state);
  algid[6] = (1 & (dibit >> 1)) + 48;   // bit 1
  algid[7] = (1 & dibit) + 48;  // bit 0
  dibit = getDibit (opts, state);
  kid[0] = (1 & (dibit >> 1)) + 48;     // bit 1
  kid[1] = (1 & dibit) + 48;    // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  kid[2] = (1 & (dibit >> 1)) + 48;     // bit 1
  kid[3] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  kid[4] = (1 & (dibit >> 1)) + 48;     // bit 1
  kid[5] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  kid[6] = (1 & (dibit >> 1)) + 48;     // bit 1
  kid[7] = (1 & dibit) + 48;    // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  kid[8] = (1 & (dibit >> 1)) + 48;     // bit 1
  kid[9] = (1 & dibit) + 48;    // bit 0
  dibit = getDibit (opts, state);
  kid[10] = (1 & (dibit >> 1)) + 48;    // bit 1
  kid[11] = (1 & dibit) + 48;   // bit 0
  dibit = getDibit (opts, state);
  kid[12] = (1 & (dibit >> 1)) + 48;    // bit 1
  kid[13] = (1 & dibit) + 48;   // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  kid[14] = (1 & (dibit >> 1)) + 48;    // bit 1
  kid[15] = (1 & dibit) + 48;   // bit 0
  skipDibit (opts, state, 1);
  dibit = getDibit (opts, state);
  tgid[0] = (1 & (dibit >> 1)) + 48;    // bit 1
  tgid[1] = (1 & dibit) + 48;   // bit 0
  dibit = getDibit (opts, state);
  tgid[2] = (1 & (dibit >> 1)) + 48;    // bit 1
  tgid[3] = (1 & dibit) + 48;   // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  tgid[4] = (1 & (dibit >> 1)) + 48;    // bit 1
  tgid[5] = (1 & dibit) + 48;   // bit 0
  dibit = getDibit (opts, state);
  tgid[6] = (1 & (dibit >> 1)) + 48;    // bit 1
  tgid[7] = (1 & dibit) + 48;   // bit 0
  dibit = getDibit (opts, state);
  tgid[8] = (1 & (dibit >> 1)) + 48;    // bit 1
  tgid[9] = (1 & dibit) + 48;   // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  tgid[10] = (1 & (dibit >> 1)) + 48;   // bit 1
  tgid[11] = (1 & dibit) + 48;  // bit 0
  dibit = getDibit (opts, state);
  tgid[12] = (1 & (dibit >> 1)) + 48;   // bit 1
  tgid[13] = (1 & dibit) + 48;  // bit 0
  dibit = getDibit (opts, state);
  tgid[14] = (1 & (dibit >> 1)) + 48;   // bit 1
  tgid[15] = (1 & dibit) + 48;  // bit 0

  skipDibit (opts, state, 160);

  state->p25kid = strtol(kid, NULL, 2);

  if (opts->p25enc == 1)
    {
      algidhex = strtol (algid, NULL, 2);
      kidhex = strtol (kid, NULL, 2);
      printf ("mi: %s algid: $%x kid: $%x\n", mi, algidhex, kidhex);
    }
  if (opts->p25lc == 1)
    {
      printf ("mfid: %s tgid: %s ", mfid, tgid);
      if (opts->p25tg == 0)
        {
          printf ("\n");
        }
    }

  j = 0;
  if (strcmp (mfid, "10010000") == 0)
    {
      for (i = 4; i < 16; i++)
        {
          if (state->tgcount < 24)
            {
              state->tg[state->tgcount][j] = tgid[i];
            }
          tmpstr[j] = tgid[i];
          j++;
        }
      tmpstr[12] = 48;
      tmpstr[13] = 48;
      tmpstr[14] = 48;
      tmpstr[15] = 48;
    }
  else
    {
      for (i = 0; i < 16; i++)
        {
          if (state->tgcount < 24)
            {
              state->tg[state->tgcount][j] = tgid[i];
            }
          tmpstr[j] = tgid[i];
          j++;
        }
    }
  tmpstr[16] = 0;
  talkgroup = strtol (tmpstr, NULL, 2);
  state->lasttg = talkgroup;
  if (state->tgcount < 24)
    {
      state->tgcount = state->tgcount + 1;
    }
  if (opts->p25tg == 1)
    {
      printf ("tg: %li\n", talkgroup);
    }
}
