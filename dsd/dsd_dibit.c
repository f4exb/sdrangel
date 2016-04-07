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

int
getDibit (dsd_opts * opts, dsd_state * state)
{
  // returns one dibit value
  int i, j, o, symbol;
  int sbuf2[128];
  int spectrum[64];
  char modulation[8];
  int lmin, lmax, lsum;

  state->numflips = 0;

  symbol = getSymbol (opts, state, 1);
  state->sbuf[state->sidx] = symbol;

  for (i = 0; i < opts->ssize; i++)
    {
      sbuf2[i] = state->sbuf[i];
    }

  qsort (sbuf2, opts->ssize, sizeof (int), comp);
  // continuous update of min/max in rf_mod=1 (QPSK) mode
  // in c4fm min/max must only be updated during sync
  if (state->rf_mod == 1)
    {
      lmin = (sbuf2[0] + sbuf2[1]) / 2;
      lmax = (sbuf2[(opts->ssize - 1)] + sbuf2[(opts->ssize - 2)]) / 2;
      state->minbuf[state->midx] = lmin;
      state->maxbuf[state->midx] = lmax;
      if (state->midx == (opts->msize - 1))
        {
          state->midx = 0;
        }
      else
        {
          state->midx++;
        }
      lsum = 0;
      for (i = 0; i < opts->msize; i++)
        {
          lsum += state->minbuf[i];
        }
      state->min = lsum / opts->msize;
      lsum = 0;
      for (i = 0; i < opts->msize; i++)
        {
          lsum += state->maxbuf[i];
        }
      state->max = lsum / opts->msize;
      state->center = ((state->max) + (state->min)) / 2;
      state->umid = (((state->max) - state->center) * 5 / 8) + state->center;
      state->lmid = (((state->min) - state->center) * 5 / 8) + state->center;
      state->maxref = ((state->max) * 0.80);
      state->minref = ((state->min) * 0.80);
    }
  else
    {
      state->maxref = state->max;
      state->minref = state->min;
    }

  if (state->sidx == (opts->ssize - 1))
    {

      state->sidx = 0;

      if (opts->datascope == 1)
        {
          if (state->rf_mod == 0)
            {
              sprintf (modulation, "C4FM");
            }
          else if (state->rf_mod == 1)
            {
              sprintf (modulation, "QPSK");
            }
          else if (state->rf_mod == 2)
            {
              sprintf (modulation, "GFSK");
            }

          for (i = 0; i < 64; i++)
            {
              spectrum[i] = 0;
            }
          for (i = 0; i < opts->ssize; i++)
            {
              o = (sbuf2[i] + 32768) / 1024;
              spectrum[o]++;
            }
          if (state->symbolcnt > (4800 / opts->scoperate))
            {
              state->symbolcnt = 0;
              printf ("\n");
              printf ("Demod mode:     %s                Nac:                     %4X\n", modulation, state->nac);
              printf ("Frame Type:    %s        Talkgroup:            %7i\n", state->ftype, state->lasttg);
              printf ("Frame Subtype: %s       Source:          %12i\n", state->fsubtype, state->lastsrc);
              printf ("TDMA activity:  %s %s     Voice errors: %s\n", state->slot0light, state->slot1light, state->err_str);
              printf ("+----------------------------------------------------------------+\n");
              for (i = 0; i < 10; i++)
                {
                  printf ("|");
                  for (j = 0; j < 64; j++)
                    {
                      if (i == 0)
                        {
                          if ((j == ((state->min) + 32768) / 1024) || (j == ((state->max) + 32768) / 1024))
                            {
                              printf ("#");
                            }
                          else if ((j == ((state->lmid) + 32768) / 1024) || (j == ((state->umid) + 32768) / 1024))
                            {
                              printf ("^");
                            }
                          else if (j == (state->center + 32768) / 1024)
                            {
                              printf ("!");
                            }
                          else
                            {
                              if (j == 32)
                                {
                                  printf ("|");
                                }
                              else
                                {
                                  printf (" ");
                                }
                            }
                        }
                      else
                        {
                          if (spectrum[j] > 9 - i)
                            {
                              printf ("*");
                            }
                          else
                            {
                              if (j == 32)
                                {
                                  printf ("|");
                                }
                              else
                                {
                                  printf (" ");
                                }
                            }
                        }
                    }
                  printf ("|\n");
                }
              printf ("+----------------------------------------------------------------+\n");
            }
        }
    }
  else
    {
      state->sidx++;
    }

  if (state->dibit_buf_p > state->dibit_buf + 900000)
    {
	  state->dibit_buf_p = state->dibit_buf + 200;
    }

  // determine dibit state
  if ((state->synctype == 6) || (state->synctype == 14)|| (state->synctype == 18))
    {
      if (symbol > state->center)
        {
          *state->dibit_buf_p = 1;
          state->dibit_buf_p++;
          return (0);
        }
      else
        {
          *state->dibit_buf_p = 3;
          state->dibit_buf_p++;
          return (1);
        }
    }
  else if ((state->synctype == 7) || (state->synctype == 15)|| (state->synctype == 19))
    {
      if (symbol > state->center)
        {
          *state->dibit_buf_p = 1;
          state->dibit_buf_p++;
          return (1);
        }
      else
        {
          *state->dibit_buf_p = 3;
          state->dibit_buf_p++;
          return (0);
        }
    }
  else if ((state->synctype == 1) || (state->synctype == 3) || (state->synctype == 5) || (state->synctype == 9) || (state->synctype == 11) || (state->synctype == 13))
    {
      if (symbol > state->center)
        {
          if (symbol > state->umid)
            {
              *state->dibit_buf_p = 1;  // store non-inverted values in dibit_buf
              state->dibit_buf_p++;
              return (3);
            }
          else
            {
              *state->dibit_buf_p = 0;
              state->dibit_buf_p++;
              return (2);
            }
        }
      else
        {
          if (symbol < state->lmid)
            {
              *state->dibit_buf_p = 3;
              state->dibit_buf_p++;
              return (1);
            }
          else
            {
              *state->dibit_buf_p = 2;
              state->dibit_buf_p++;
              return (0);
            }
        }
    }
  else
    {
      if (symbol > state->center)
        {
          if (symbol > state->umid)
            {
              *state->dibit_buf_p = 1;
              state->dibit_buf_p++;
              return (1);
            }
          else
            {
              *state->dibit_buf_p = 0;
              state->dibit_buf_p++;
              return (0);
            }
        }
      else
        {
          if (symbol < state->lmid)
            {
              *state->dibit_buf_p = 3;
              state->dibit_buf_p++;
              return (3);
            }
          else
            {
              *state->dibit_buf_p = 2;
              state->dibit_buf_p++;
              return (2);
            }
        }
    }
}

void
skipDibit (dsd_opts * opts, dsd_state * state, int count)
{

  short sample;
  int i;

  for (i = 0; i < (count); i++)
    {
      sample = getDibit (opts, state);
    }
}
