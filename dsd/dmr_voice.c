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
#include "dmr_const.h"

void
processDMRvoice (dsd_opts * opts, dsd_state * state)
{
  // extracts AMBE frames from DMR frame
  int i, j, dibit;
  int *dibit_p;
  char ambe_fr[4][24];
  char ambe_fr2[4][24];
  char ambe_fr3[4][24];
  const int *w, *x, *y, *z;
  char sync[25];
  char syncdata[25];
  char cachdata[13];
  int mutecurrentslot;
  int msMode;

#ifdef DMR_DUMP
  int k;
  char syncbits[49];
  char cachbits[25];
#endif

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
              if (opts->inverted_dmr == 1)
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
              if (opts->inverted_dmr == 1)
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


#ifdef DMR_DUMP
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
      w = rW;
      x = rX;
      y = rY;
      z = rZ;
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
              if (opts->inverted_dmr == 1)
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
      w = rW;
      x = rX;
      y = rY;
      z = rZ;
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
              if (opts->inverted_dmr == 1)
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
              if (opts->inverted_dmr == 1)
                {
                  dibit = (dibit ^ 2);
                }
            }
          syncdata[i] = dibit;
          sync[i] = (dibit | 1) + 48;
        }
      sync[24] = 0;
      syncdata[24] = 0;

      if ((strcmp (sync, DMR_BS_DATA_SYNC) == 0) || (strcmp (sync, DMR_MS_DATA_SYNC) == 0))
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
      else if ((strcmp (sync, DMR_BS_VOICE_SYNC) == 0) || (strcmp (sync, DMR_MS_VOICE_SYNC) == 0))
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
      if ((strcmp (sync, DMR_MS_VOICE_SYNC) == 0) || (strcmp (sync, DMR_MS_DATA_SYNC) == 0))
        {
          msMode = 1;
        }

      if ((j == 0) && (opts->errorbars == 1))
        {
          printf ("%s %s  VOICE e:", state->slot0light, state->slot1light);
        }

#ifdef DMR_DUMP
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
      w = rW;
      x = rX;
      y = rY;
      z = rZ;
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

#ifdef DMR_DUMP
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

      if ((strcmp (sync, DMR_BS_DATA_SYNC) == 0) || (msMode == 1))
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
      else if (strcmp (sync, DMR_BS_VOICE_SYNC) == 0)
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

#ifdef DMR_DUMP
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

}
