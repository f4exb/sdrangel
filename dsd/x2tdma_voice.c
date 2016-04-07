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
#include "x2tdma_const.h"

void
processX2TDMAvoice (dsd_opts * opts, dsd_state * state)
{
  // extracts AMBE frames from X2TDMA frame
  int i, j, dibit;
  int *dibit_p;
  char ambe_fr[4][24];
  char ambe_fr2[4][24];
  char ambe_fr3[4][24];
  const int *w, *x, *y, *z;
  char sync[25];
  char syncdata[25];
  char lcformat[9], mfid[9], lcinfo[57];
  char cachdata[13];
  char parity;
  int eeei, aiei;
  char mi[73];
  int burstd;
  int mutecurrentslot;
  int algidhex, kidhex;
  int msMode;

#ifdef X2TDMA_DUMP
  int k;
  char cachbits[25];
  char syncbits[49];
#endif

  lcformat[8] = 0;
  mfid[8] = 0;
  lcinfo[56] = 0;
  sprintf (mi, "________________________________________________________________________");
  eeei = 0;
  aiei = 0;
  burstd = 0;
  mutecurrentslot = 0;
  msMode = 0;

  dibit_p = state->dibit_buf_p - 144;
  for (j = 0; j < 6; j++)
    {
      // 2nd half of previous slot
      for (i = 0; i < 54; i++)
        {
          if (j > 0)
            {
              dibit = getDibit (opts, state);
            }
          else
            {
              dibit = *dibit_p;
              dibit_p++;
              if (opts->inverted_x2tdma == 1)
                {
                  dibit = (dibit ^ 2);
                }
            }
        }

      // CACH
      for (i = 0; i < 12; i++)
        {
          if (j > 0)
            {
              dibit = getDibit (opts, state);
            }
          else
            {
              dibit = *dibit_p;
              dibit_p++;
              if (opts->inverted_x2tdma == 1)
                {
                  dibit = (dibit ^ 2);
                }
            }
          cachdata[i] = dibit;
          if (i == 2)
            {
              state->currentslot = (1 & (dibit >> 1));  // bit 1
              if (state->currentslot == 0)
                {
                  state->slot0light[0] = '[';
                  state->slot0light[6] = ']';
                  state->slot1light[0] = ' ';
                  state->slot1light[6] = ' ';
                }
              else
                {
                  state->slot1light[0] = '[';
                  state->slot1light[6] = ']';
                  state->slot0light[0] = ' ';
                  state->slot0light[6] = ' ';
                }
            }
        }
      cachdata[12] = 0;


#ifdef X2TDMA_DUMP
      k = 0;
      for (i = 0; i < 12; i++)
        {
          dibit = cachdata[i];
          cachbits[k] = (1 & (dibit >> 1)) + 48;        // bit 1
          k++;
          cachbits[k] = (1 & dibit) + 48;       // bit 0
          k++;
        }
      cachbits[24] = 0;
      printf ("%s ", cachbits);
#endif

      // current slot frame 1
      w = aW;
      x = aX;
      y = aY;
      z = aZ;
      for (i = 0; i < 36; i++)
        {
          if (j > 0)
            {
              dibit = getDibit (opts, state);
            }
          else
            {
              dibit = *dibit_p;
              dibit_p++;
              if (opts->inverted_x2tdma == 1)
                {
                  dibit = (dibit ^ 2);
                }
            }
          ambe_fr[*w][*x] = (1 & (dibit >> 1)); // bit 1
          ambe_fr[*y][*z] = (1 & dibit);        // bit 0
          w++;
          x++;
          y++;
          z++;
        }

      // current slot frame 2 first half
      w = aW;
      x = aX;
      y = aY;
      z = aZ;
      for (i = 0; i < 18; i++)
        {
          if (j > 0)
            {
              dibit = getDibit (opts, state);
            }
          else
            {
              dibit = *dibit_p;
              dibit_p++;
              if (opts->inverted_x2tdma == 1)
                {
                  dibit = (dibit ^ 2);
                }
            }
          ambe_fr2[*w][*x] = (1 & (dibit >> 1));        // bit 1
          ambe_fr2[*y][*z] = (1 & dibit);       // bit 0
          w++;
          x++;
          y++;
          z++;
        }

      // signaling data or sync
      for (i = 0; i < 24; i++)
        {
          if (j > 0)
            {
              dibit = getDibit (opts, state);
            }
          else
            {
              dibit = *dibit_p;
              dibit_p++;
              if (opts->inverted_x2tdma == 1)
                {
                  dibit = (dibit ^ 2);
                }
            }
          syncdata[i] = dibit;
          sync[i] = (dibit | 1) + 48;
        }
      sync[24] = 0;
      syncdata[24] = 0;

      if ((strcmp (sync, X2TDMA_BS_DATA_SYNC) == 0) || (strcmp (sync, X2TDMA_MS_DATA_SYNC) == 0))
        {
          mutecurrentslot = 1;
          if (state->currentslot == 0)
            {
              sprintf (state->slot0light, "[slot0]");
            }
          else
            {
              sprintf (state->slot1light, "[slot1]");
            }
        }
      else if ((strcmp (sync, X2TDMA_BS_VOICE_SYNC) == 0) || (strcmp (sync, X2TDMA_MS_VOICE_SYNC) == 0))
        {
          mutecurrentslot = 0;
          if (state->currentslot == 0)
            {
              sprintf (state->slot0light, "[SLOT0]");
            }
          else
            {
              sprintf (state->slot1light, "[SLOT1]");
            }
        }

      if ((strcmp (sync, X2TDMA_MS_VOICE_SYNC) == 0) || (strcmp (sync, X2TDMA_MS_DATA_SYNC) == 0))
        {
          msMode = 1;
        }

      if ((j == 0) && (opts->errorbars == 1))
        {
          printf ("%s %s  VOICE e:", state->slot0light, state->slot1light);
        }

#ifdef X2TDMA_DUMP
      k = 0;
      for (i = 0; i < 24; i++)
        {
          dibit = syncdata[i];
          syncbits[k] = (1 & (dibit >> 1)) + 48;        // bit 1
          k++;
          syncbits[k] = (1 & dibit) + 48;       // bit 0
          k++;
        }
      syncbits[48] = 0;
      printf ("%s ", syncbits);
#endif

      if (j == 1)
        {
          eeei = (1 & syncdata[1]);     // bit 0
          aiei = (1 & (syncdata[2] >> 1));      // bit 1

          if ((eeei == 0) && (aiei == 0))
            {
              lcformat[0] = (1 & (syncdata[4] >> 1)) + 48;      // bit 1
              mfid[3] = (1 & syncdata[4]) + 48; // bit 0
              lcinfo[6] = (1 & (syncdata[5] >> 1)) + 48;        // bit 1
              lcinfo[16] = (1 & syncdata[5]) + 48;      // bit 0
              lcinfo[26] = (1 & (syncdata[6] >> 1)) + 48;       // bit 1
              lcinfo[36] = (1 & syncdata[6]) + 48;      // bit 0
              lcinfo[46] = (1 & (syncdata[7] >> 1)) + 48;       // bit 1
              parity = (1 & syncdata[7]) + 48;  // bit 0
              lcformat[1] = (1 & (syncdata[8] >> 1)) + 48;      // bit 1
              mfid[4] = (1 & syncdata[8]) + 48; // bit 0
              lcinfo[7] = (1 & (syncdata[9] >> 1)) + 48;        // bit 1
              lcinfo[17] = (1 & syncdata[9]) + 48;      // bit 0
              lcinfo[27] = (1 & (syncdata[10] >> 1)) + 48;      // bit 1
              lcinfo[37] = (1 & syncdata[10]) + 48;     // bit 0
              lcinfo[47] = (1 & (syncdata[11] >> 1)) + 48;      // bit 1
              parity = (1 & syncdata[11]) + 48; // bit 0
              lcformat[2] = (1 & (syncdata[12] >> 1)) + 48;     // bit 1
              mfid[5] = (1 & syncdata[12]) + 48;        // bit 0
              lcinfo[8] = (1 & (syncdata[13] >> 1)) + 48;       // bit 1
              lcinfo[18] = (1 & syncdata[13]) + 48;     // bit 0
              lcinfo[28] = (1 & (syncdata[14] >> 1)) + 48;      // bit 1
              lcinfo[38] = (1 & syncdata[14]) + 48;     // bit 0
              lcinfo[48] = (1 & (syncdata[15] >> 1)) + 48;      // bit 1
              parity = (1 & syncdata[15]) + 48; // bit 0
              lcformat[3] = (1 & (syncdata[16] >> 1)) + 48;     // bit 1
              mfid[6] = (1 & syncdata[16]) + 48;        // bit 0
              lcinfo[9] = (1 & (syncdata[17] >> 1)) + 48;       // bit 1
              lcinfo[19] = (1 & syncdata[17]) + 48;     // bit 0
              lcinfo[29] = (1 & (syncdata[18] >> 1)) + 48;      // bit 1
              lcinfo[39] = (1 & syncdata[18]) + 48;     // bit 0
              lcinfo[49] = (1 & (syncdata[19] >> 1)) + 48;      // bit 1
              parity = (1 & syncdata[19]) + 48; // bit 0
            }
          else
            {
              mi[0] = (1 & (syncdata[4] >> 1)) + 48;    // bit 1
              mi[11] = (1 & syncdata[4]) + 48;  // bit 0
              mi[22] = (1 & (syncdata[5] >> 1)) + 48;   // bit 1
              mi[32] = (1 & syncdata[5]) + 48;  // bit 0
              mi[42] = (1 & (syncdata[6] >> 1)) + 48;   // bit 1
              mi[52] = (1 & syncdata[6]) + 48;  // bit 0
              mi[62] = (1 & (syncdata[7] >> 1)) + 48;   // bit 1
              parity = (1 & syncdata[7]) + 48;  // bit 0
              mi[1] = (1 & (syncdata[8] >> 1)) + 48;    // bit 1
              mi[12] = (1 & syncdata[8]) + 48;  // bit 0
              mi[23] = (1 & (syncdata[9] >> 1)) + 48;   // bit 1
              mi[33] = (1 & syncdata[9]) + 48;  // bit 0
              mi[43] = (1 & (syncdata[10] >> 1)) + 48;  // bit 1
              mi[53] = (1 & syncdata[10]) + 48; // bit 0
              mi[63] = (1 & (syncdata[11] >> 1)) + 48;  // bit 1
              parity = (1 & syncdata[11]) + 48; // bit 0
              mi[2] = (1 & (syncdata[12] >> 1)) + 48;   // bit 1
              mi[13] = (1 & syncdata[12]) + 48; // bit 0
              mi[24] = (1 & (syncdata[13] >> 1)) + 48;  // bit 1
              mi[34] = (1 & syncdata[13]) + 48; // bit 0
              mi[44] = (1 & (syncdata[14] >> 1)) + 48;  // bit 1
              mi[54] = (1 & syncdata[14]) + 48; // bit 0
              mi[64] = (1 & (syncdata[15] >> 1)) + 48;  // bit 1
              parity = (1 & syncdata[15]) + 48; // bit 0
              mi[3] = (1 & (syncdata[16] >> 1)) + 48;   // bit 1
              mi[14] = (1 & syncdata[16]) + 48; // bit 0
              mi[25] = (1 & (syncdata[17] >> 1)) + 48;  // bit 1
              mi[35] = (1 & syncdata[17]) + 48; // bit 0
              mi[45] = (1 & (syncdata[18] >> 1)) + 48;  // bit 1
              mi[55] = (1 & syncdata[18]) + 48; // bit 0
              mi[65] = (1 & (syncdata[19] >> 1)) + 48;  // bit 1
              parity = (1 & syncdata[19]) + 48; // bit 0
            }
        }
      else if (j == 2)
        {
          if ((eeei == 0) && (aiei == 0))
            {
              lcformat[4] = (1 & (syncdata[4] >> 1)) + 48;      // bit 1
              mfid[7] = (1 & syncdata[4]) + 48; // bit 0
              lcinfo[10] = (1 & (syncdata[5] >> 1)) + 48;       // bit 1
              lcinfo[20] = (1 & syncdata[5]) + 48;      // bit 0
              lcinfo[30] = (1 & (syncdata[6] >> 1)) + 48;       // bit 1
              lcinfo[40] = (1 & syncdata[6]) + 48;      // bit 0
              lcinfo[50] = (1 & (syncdata[7] >> 1)) + 48;       // bit 1
              parity = (1 & syncdata[7]) + 48;  // bit 0
              lcformat[5] = (1 & (syncdata[8] >> 1)) + 48;      // bit 1
              lcinfo[0] = (1 & syncdata[8]) + 48;       // bit 0
              lcinfo[11] = (1 & (syncdata[9] >> 1)) + 48;       // bit 1
              lcinfo[21] = (1 & syncdata[9]) + 48;      // bit 0
              lcinfo[31] = (1 & (syncdata[10] >> 1)) + 48;      // bit 1
              lcinfo[41] = (1 & syncdata[10]) + 48;     // bit 0
              lcinfo[51] = (1 & (syncdata[11] >> 1)) + 48;      // bit 1
              parity = (1 & syncdata[11]) + 48; // bit 0
              lcformat[6] = (1 & (syncdata[12] >> 1)) + 48;     // bit 1
              lcinfo[1] = (1 & syncdata[12]) + 48;      // bit 0
              lcinfo[12] = (1 & (syncdata[13] >> 1)) + 48;      // bit 1
              lcinfo[22] = (1 & syncdata[13]) + 48;     // bit 0
              lcinfo[32] = (1 & (syncdata[14] >> 1)) + 48;      // bit 1
              lcinfo[42] = (1 & syncdata[14]) + 48;     // bit 0
              lcinfo[52] = (1 & (syncdata[15] >> 1)) + 48;      // bit 1
              parity = (1 & syncdata[15]) + 48; // bit 0
              lcformat[7] = (1 & (syncdata[16] >> 1)) + 48;     // bit 1
              lcinfo[2] = (1 & syncdata[16]) + 48;      // bit 0
              lcinfo[13] = (1 & (syncdata[17] >> 1)) + 48;      // bit 1
              lcinfo[23] = (1 & syncdata[17]) + 48;     // bit 0
              lcinfo[33] = (1 & (syncdata[18] >> 1)) + 48;      // bit 1
              lcinfo[43] = (1 & syncdata[18]) + 48;     // bit 0
              lcinfo[53] = (1 & (syncdata[19] >> 1)) + 48;      // bit 1
              parity = (1 & syncdata[19]) + 48; // bit 0
            }
          else
            {
              mi[4] = (1 & (syncdata[4] >> 1)) + 48;    // bit 1
              mi[15] = (1 & syncdata[4]) + 48;  // bit 0
              mi[26] = (1 & (syncdata[5] >> 1)) + 48;   // bit 1
              mi[36] = (1 & syncdata[5]) + 48;  // bit 0
              mi[46] = (1 & (syncdata[6] >> 1)) + 48;   // bit 1
              mi[56] = (1 & syncdata[6]) + 48;  // bit 0
              mi[66] = (1 & (syncdata[7] >> 1)) + 48;   // bit 1
              parity = (1 & syncdata[7]) + 48;  // bit 0
              mi[5] = (1 & (syncdata[8] >> 1)) + 48;    // bit 1
              mi[16] = (1 & syncdata[8]) + 48;  // bit 0
              mi[27] = (1 & (syncdata[9] >> 1)) + 48;   // bit 1
              mi[37] = (1 & syncdata[9]) + 48;  // bit 0
              mi[47] = (1 & (syncdata[10] >> 1)) + 48;  // bit 1
              mi[57] = (1 & syncdata[10]) + 48; // bit 0
              mi[67] = (1 & (syncdata[11] >> 1)) + 48;  // bit 1
              parity = (1 & syncdata[11]) + 48; // bit 0
              mi[6] = (1 & (syncdata[12] >> 1)) + 48;   // bit 1
              mi[17] = (1 & syncdata[12]) + 48; // bit 0
              mi[28] = (1 & (syncdata[13] >> 1)) + 48;  // bit 1
              mi[38] = (1 & syncdata[13]) + 48; // bit 0
              mi[48] = (1 & (syncdata[14] >> 1)) + 48;  // bit 1
              mi[58] = (1 & syncdata[14]) + 48; // bit 0
              mi[68] = (1 & (syncdata[15] >> 1)) + 48;  // bit 1
              parity = (1 & syncdata[15]) + 48; // bit 0
              mi[7] = (1 & (syncdata[16] >> 1)) + 48;   // bit 1
              mi[18] = (1 & syncdata[16]) + 48; // bit 0
              mi[29] = (1 & (syncdata[17] >> 1)) + 48;  // bit 1
              mi[39] = (1 & syncdata[17]) + 48; // bit 0
              mi[49] = (1 & (syncdata[18] >> 1)) + 48;  // bit 1
              mi[59] = (1 & syncdata[18]) + 48; // bit 0
              mi[69] = (1 & (syncdata[19] >> 1)) + 48;  // bit 1
              parity = (1 & syncdata[19]) + 48; // bit 0
            }
        }
      else if (j == 3)
        {
          burstd = (1 & syncdata[1]);   // bit 0

          state->algid[0] = (1 & (syncdata[4] >> 1)) + 48;      // bit 1
          state->algid[1] = (1 & syncdata[4]) + 48;     // bit 0
          state->algid[2] = (1 & (syncdata[5] >> 1)) + 48;      // bit 1
          state->algid[3] = (1 & syncdata[5]) + 48;     // bit 0
          if (burstd == 0)
            {
              state->algid[4] = (1 & (syncdata[8] >> 1)) + 48;  // bit 1
              state->algid[5] = (1 & syncdata[8]) + 48; // bit 0
              state->algid[6] = (1 & (syncdata[9] >> 1)) + 48;  // bit 1
              state->algid[7] = (1 & syncdata[9]) + 48; // bit 0

              state->keyid[0] = (1 & (syncdata[10] >> 1)) + 48; // bit 1
              state->keyid[1] = (1 & syncdata[10]) + 48;        // bit 0
              state->keyid[2] = (1 & (syncdata[11] >> 1)) + 48; // bit 1
              state->keyid[3] = (1 & syncdata[11]) + 48;        // bit 0
              state->keyid[4] = (1 & (syncdata[12] >> 1)) + 48; // bit 1
              state->keyid[5] = (1 & syncdata[12]) + 48;        // bit 0
              state->keyid[6] = (1 & (syncdata[13] >> 1)) + 48; // bit 1
              state->keyid[7] = (1 & syncdata[13]) + 48;        // bit 0
              state->keyid[8] = (1 & (syncdata[14] >> 1)) + 48; // bit 1
              state->keyid[9] = (1 & syncdata[14]) + 48;        // bit 0
              state->keyid[10] = (1 & (syncdata[15] >> 1)) + 48;        // bit 1
              state->keyid[11] = (1 & syncdata[15]) + 48;       // bit 0
              state->keyid[12] = (1 & (syncdata[16] >> 1)) + 48;        // bit 1
              state->keyid[13] = (1 & syncdata[16]) + 48;       // bit 0
              state->keyid[14] = (1 & (syncdata[17] >> 1)) + 48;        // bit 1
              state->keyid[15] = (1 & syncdata[17]) + 48;       // bit 0
            }
          else
            {
              sprintf (state->algid, "________");
              sprintf (state->keyid, "________________");
            }
        }
      else if (j == 4)
        {
          if ((eeei == 0) && (aiei == 0))
            {
              mfid[0] = (1 & (syncdata[4] >> 1)) + 48;  // bit 1
              lcinfo[3] = (1 & syncdata[4]) + 48;       // bit 0
              lcinfo[14] = (1 & (syncdata[5] >> 1)) + 48;       // bit 1
              lcinfo[24] = (1 & syncdata[5]) + 48;      // bit 0
              lcinfo[34] = (1 & (syncdata[6] >> 1)) + 48;       // bit 1
              lcinfo[44] = (1 & syncdata[6]) + 48;      // bit 0
              lcinfo[54] = (1 & (syncdata[7] >> 1)) + 48;       // bit 1
              parity = (1 & syncdata[7]) + 48;  // bit 0
              mfid[1] = (1 & (syncdata[8] >> 1)) + 48;  // bit 1
              lcinfo[4] = (1 & syncdata[8]) + 48;       // bit 0
              lcinfo[15] = (1 & (syncdata[9] >> 1)) + 48;       // bit 1
              lcinfo[25] = (1 & syncdata[9]) + 48;      // bit 0
              lcinfo[35] = (1 & (syncdata[10] >> 1)) + 48;      // bit 1
              lcinfo[45] = (1 & syncdata[10]) + 48;     // bit 0
              lcinfo[55] = (1 & (syncdata[11] >> 1)) + 48;      // bit 1
              parity = (1 & syncdata[11]) + 48; // bit 0
              mfid[2] = (1 & (syncdata[12] >> 1)) + 48; // bit 1
              lcinfo[5] = (1 & syncdata[12]) + 48;      // bit 0
            }
          else
            {
              mi[8] = (1 & (syncdata[4] >> 1)) + 48;    // bit 1
              mi[19] = (1 & syncdata[4]) + 48;  // bit 0
              mi[30] = (1 & (syncdata[5] >> 1)) + 48;   // bit 1
              mi[40] = (1 & syncdata[5]) + 48;  // bit 0
              mi[50] = (1 & (syncdata[6] >> 1)) + 48;   // bit 1
              mi[60] = (1 & syncdata[6]) + 48;  // bit 0
              mi[70] = (1 & (syncdata[7] >> 1)) + 48;   // bit 1
              parity = (1 & syncdata[7]) + 48;  // bit 0
              mi[9] = (1 & (syncdata[8] >> 1)) + 48;    // bit 1
              mi[20] = (1 & syncdata[8]) + 48;  // bit 0
              mi[31] = (1 & (syncdata[9] >> 1)) + 48;   // bit 1
              mi[41] = (1 & syncdata[9]) + 48;  // bit 0
              mi[51] = (1 & (syncdata[10] >> 1)) + 48;  // bit 1
              mi[61] = (1 & syncdata[10]) + 48; // bit 0
              mi[71] = (1 & (syncdata[11] >> 1)) + 48;  // bit 1
              parity = (1 & syncdata[11]) + 48; // bit 0
              mi[10] = (1 & (syncdata[12] >> 1)) + 48;  // bit 1
              mi[21] = (1 & syncdata[12]) + 48; // bit 0
            }
        }

      // current slot frame 2 second half
      for (i = 0; i < 18; i++)
        {
          dibit = getDibit (opts, state);
          ambe_fr2[*w][*x] = (1 & (dibit >> 1));        // bit 1
          ambe_fr2[*y][*z] = (1 & dibit);       // bit 0
          w++;
          x++;
          y++;
          z++;
        }

      if (mutecurrentslot == 0)
        {
          if (state->firstframe == 1)
            {                   // we don't know if anything received before the first sync after no carrier is valid
              state->firstframe = 0;
            }
          else
            {
              processMbeFrame (opts, state, NULL, ambe_fr, NULL);
              processMbeFrame (opts, state, NULL, ambe_fr2, NULL);
            }
        }

      // current slot frame 3
      w = aW;
      x = aX;
      y = aY;
      z = aZ;
      for (i = 0; i < 36; i++)
        {
          dibit = getDibit (opts, state);
          ambe_fr3[*w][*x] = (1 & (dibit >> 1));        // bit 1
          ambe_fr3[*y][*z] = (1 & dibit);       // bit 0
          w++;
          x++;
          y++;
          z++;
        }
      if (mutecurrentslot == 0)
        {
          processMbeFrame (opts, state, NULL, ambe_fr3, NULL);
        }

      // CACH
      for (i = 0; i < 12; i++)
        {
          dibit = getDibit (opts, state);
          cachdata[i] = dibit;
        }
      cachdata[12] = 0;

#ifdef X2TDMA_DUMP
      k = 0;
      for (i = 0; i < 12; i++)
        {
          dibit = cachdata[i];
          cachbits[k] = (1 & (dibit >> 1)) + 48;        // bit 1
          k++;
          cachbits[k] = (1 & dibit) + 48;       // bit 0
          k++;
        }
      cachbits[24] = 0;
      printf ("%s ", cachbits);
#endif


      // next slot
      skipDibit (opts, state, 54);

      // signaling data or sync
      for (i = 0; i < 24; i++)
        {
          dibit = getDibit (opts, state);
          syncdata[i] = dibit;
          sync[i] = (dibit | 1) + 48;
        }
      sync[24] = 0;
      syncdata[24] = 0;

      if ((strcmp (sync, X2TDMA_BS_DATA_SYNC) == 0) || (msMode == 1))
        {
          if (state->currentslot == 0)
            {
              sprintf (state->slot1light, " slot1 ");
            }
          else
            {
              sprintf (state->slot0light, " slot0 ");
            }
        }
      else if (strcmp (sync, X2TDMA_BS_VOICE_SYNC) == 0)
        {
          if (state->currentslot == 0)
            {
              sprintf (state->slot1light, " SLOT1 ");
            }
          else
            {
              sprintf (state->slot0light, " SLOT0 ");
            }
        }

#ifdef X2TDMA_DUMP
      k = 0;
      for (i = 0; i < 24; i++)
        {
          dibit = syncdata[i];
          syncbits[k] = (1 & (dibit >> 1)) + 48;        // bit 1
          k++;
          syncbits[k] = (1 & dibit) + 48;       // bit 0
          k++;
        }
      syncbits[48] = 0;
      printf ("%s ", syncbits);
#endif

      if (j == 5)
        {
          // 2nd half next slot
          skipDibit (opts, state, 54);

          // CACH
          skipDibit (opts, state, 12);

          // first half current slot
          skipDibit (opts, state, 54);
        }
    }

  if (opts->errorbars == 1)
    {
      printf ("\n");
    }

  if (mutecurrentslot == 0)
    {
      if ((eeei == 0) && (aiei == 0))
        {
          processP25lcw (opts, state, lcformat, mfid, lcinfo);
        }
      if (opts->p25enc == 1)
        {
          algidhex = strtol (state->algid, NULL, 2);
          kidhex = strtol (state->keyid, NULL, 2);
          printf ("mi: %s algid: $%x kid: $%x\n", mi, algidhex, kidhex);
        }
    }
}
