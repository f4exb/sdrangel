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
processDMRdata (dsd_opts * opts, dsd_state * state)
{

  int i, dibit;
  int *dibit_p;
  char sync[25];
  char syncdata[25];
  char cachdata[13];
  char cc[5];
  char bursttype[5];

#ifdef DMR_DUMP
  int k;
  char syncbits[49];
  char cachbits[25];
#endif

  cc[4] = 0;
  bursttype[4] = 0;

  dibit_p = state->dibit_buf_p - 90;

  // CACH
  for (i = 0; i < 12; i++)
    {
      dibit = *dibit_p;
      dibit_p++;
      if (opts->inverted_dmr == 1)
        {
          dibit = (dibit ^ 2);
        }
      cachdata[i] = dibit;
      if (i == 2)
        {
          state->currentslot = (1 & (dibit >> 1));      // bit 1
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
      cachbits[k] = (1 & (dibit >> 1)) + 48;    // bit 1
      k++;
      cachbits[k] = (1 & dibit) + 48;   // bit 0
      k++;
    }
  cachbits[24] = 0;
  printf ("%s ", cachbits);
#endif

  // current slot
  dibit_p += 49;

  // slot type
  dibit = *dibit_p;
  dibit_p++;
  if (opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
  cc[0] = (1 & (dibit >> 1)) + 48;      // bit 1
  cc[1] = (1 & dibit) + 48;     // bit 0

  dibit = *dibit_p;
  dibit_p++;
  if (opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
  cc[2] = (1 & (dibit >> 1)) + 48;      // bit 1
  cc[3] = (1 & dibit) + 48;     // bit 0

  dibit = *dibit_p;
  dibit_p++;
  if (opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
  bursttype[0] = (1 & (dibit >> 1)) + 48;       // bit 1
  bursttype[1] = (1 & dibit) + 48;      // bit 0

  dibit = *dibit_p;
  dibit_p++;
  if (opts->inverted_dmr == 1)
    {
      dibit = (dibit ^ 2);
    }
  bursttype[2] = (1 & (dibit >> 1)) + 48;       // bit 1
  bursttype[3] = (1 & dibit) + 48;      // bit 0

  // parity bit
  dibit_p++;

  if (strcmp (bursttype, "0000") == 0)
    {
      sprintf (state->fsubtype, " PI Header    ");
    }
  else if (strcmp (bursttype, "0001") == 0)
    {
      sprintf (state->fsubtype, " VOICE Header ");
    }
  else if (strcmp (bursttype, "0010") == 0)
    {
      sprintf (state->fsubtype, " TLC          ");
    }
  else if (strcmp (bursttype, "0011") == 0)
    {
      sprintf (state->fsubtype, " CSBK         ");
    }
  else if (strcmp (bursttype, "0100") == 0)
    {
      sprintf (state->fsubtype, " MBC Header   ");
    }
  else if (strcmp (bursttype, "0101") == 0)
    {
      sprintf (state->fsubtype, " MBC          ");
    }
  else if (strcmp (bursttype, "0110") == 0)
    {
      sprintf (state->fsubtype, " DATA Header  ");
    }
  else if (strcmp (bursttype, "0111") == 0)
    {
      sprintf (state->fsubtype, " RATE 1/2 DATA");
    }
  else if (strcmp (bursttype, "1000") == 0)
    {
      sprintf (state->fsubtype, " RATE 3/4 DATA");
    }
  else if (strcmp (bursttype, "1001") == 0)
    {
      sprintf (state->fsubtype, " Slot idle    ");
    }
  else if (strcmp (bursttype, "1010") == 0)
    {
      sprintf (state->fsubtype, " Rate 1 DATA  ");
    }
  else
    {
      sprintf (state->fsubtype, "              ");
    }

  // signaling data or sync
  for (i = 0; i < 24; i++)
    {
      dibit = *dibit_p;
      dibit_p++;
      if (opts->inverted_dmr == 1)
        {
          dibit = (dibit ^ 2);
        }
      syncdata[i] = dibit;
      sync[i] = (dibit | 1) + 48;
    }
  sync[24] = 0;
  syncdata[24] = 0;

#ifdef DMR_DUMP
  k = 0;
  for (i = 0; i < 24; i++)
    {
      dibit = syncdata[i];
      syncbits[k] = (1 & (dibit >> 1)) + 48;    // bit 1
      k++;
      syncbits[k] = (1 & dibit) + 48;   // bit 0
      k++;
    }
  syncbits[48] = 0;
  printf ("%s ", syncbits);
#endif

  if ((strcmp (sync, DMR_BS_DATA_SYNC) == 0) || (strcmp (sync, DMR_MS_DATA_SYNC) == 0))
    {
      if (state->currentslot == 0)
        {
          sprintf (state->slot0light, "[slot0]");
        }
      else
        {
          sprintf (state->slot1light, "[slot1]");
        }
    }

  if (opts->errorbars == 1)
    {
      printf ("%s %s ", state->slot0light, state->slot1light);
    }

  // current slot second half, cach, next slot 1st half
  skipDibit (opts, state, 120);

  if (opts->errorbars == 1)
    {
      if (strcmp (state->fsubtype, "              ") == 0)
        {
          printf (" Unknown burst type: %s\n", bursttype);
        }
      else
        {
          printf ("%s\n", state->fsubtype);
        }
    }
}
