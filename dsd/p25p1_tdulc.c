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
processTDULC (dsd_opts * opts, dsd_state * state)
{

  char lcinfo[57], lcformat[9], mfid[9];
  int dibit, count;

  lcformat[8] = 0;
  mfid[8] = 0;
  lcinfo[56] = 0;

  skipDibit (opts, state, 25);
  count = 57;

  dibit = getDibit (opts, state);
  lcformat[0] = (1 & (dibit >> 1)) + 48;        // bit 1
  lcformat[1] = (1 & dibit) + 48;       // bit 0
  dibit = getDibit (opts, state);
  lcformat[2] = (1 & (dibit >> 1)) + 48;        // bit 1
  lcformat[3] = (1 & dibit) + 48;       // bit 0
  dibit = getDibit (opts, state);
  lcformat[4] = (1 & (dibit >> 1)) + 48;        // bit 1
  lcformat[5] = (1 & dibit) + 48;       // bit 0
  dibit = getDibit (opts, state);
  lcformat[6] = (1 & (dibit >> 1)) + 48;        // bit 1
  lcformat[7] = (1 & dibit) + 48;       // bit 0
  dibit = getDibit (opts, state);
  mfid[0] = (1 & (dibit >> 1)) + 48;    // bit 1
  mfid[1] = (1 & dibit) + 48;   // bit 0
  dibit = getDibit (opts, state);
  mfid[2] = (1 & (dibit >> 1)) + 48;    // bit 1
  mfid[3] = (1 & dibit) + 48;   // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  mfid[4] = (1 & (dibit >> 1)) + 48;    // bit 1
  mfid[5] = (1 & dibit) + 48;   // bit 0
  dibit = getDibit (opts, state);
  mfid[6] = (1 & (dibit >> 1)) + 48;    // bit 1
  mfid[7] = (1 & dibit) + 48;   // bit 0
  skipDibit (opts, state, 1);
  dibit = getDibit (opts, state);
  lcinfo[0] = (1 & (dibit >> 1)) + 48;  // bit 1
  lcinfo[1] = (1 & dibit) + 48; // bit 0
  dibit = getDibit (opts, state);
  lcinfo[2] = (1 & (dibit >> 1)) + 48;  // bit 1
  lcinfo[3] = (1 & dibit) + 48; // bit 0
  dibit = getDibit (opts, state);
  lcinfo[4] = (1 & (dibit >> 1)) + 48;  // bit 1
  lcinfo[5] = (1 & dibit) + 48; // bit 0
  dibit = getDibit (opts, state);
  lcinfo[6] = (1 & (dibit >> 1)) + 48;  // bit 1
  lcinfo[7] = (1 & dibit) + 48; // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  lcinfo[8] = (1 & (dibit >> 1)) + 48;  // bit 1
  lcinfo[9] = (1 & dibit) + 48; // bit 0
  dibit = getDibit (opts, state);
  lcinfo[10] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[11] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[12] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[13] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[14] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[15] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[16] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[17] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[18] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[19] = (1 & dibit) + 48;        // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  lcinfo[20] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[21] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[22] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[23] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[24] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[25] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[26] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[27] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[28] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[29] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[30] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[31] = (1 & dibit) + 48;        // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  lcinfo[32] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[33] = (1 & dibit) + 48;        // bit 0
  skipDibit (opts, state, 1);
  dibit = getDibit (opts, state);
  lcinfo[34] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[35] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[36] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[37] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[38] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[39] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[40] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[41] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[42] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[43] = (1 & dibit) + 48;        // bit 0
  skipDibit (opts, state, 6);

  dibit = getDibit (opts, state);
  lcinfo[44] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[45] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[46] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[47] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[48] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[49] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[50] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[51] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[52] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[53] = (1 & dibit) + 48;        // bit 0
  dibit = getDibit (opts, state);
  lcinfo[54] = (1 & (dibit >> 1)) + 48; // bit 1
  lcinfo[55] = (1 & dibit) + 48;        // bit 0

  skipDibit (opts, state, 91);

  processP25lcw (opts, state, lcformat, mfid, lcinfo);
}
