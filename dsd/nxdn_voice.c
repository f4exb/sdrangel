#include "dsd.h"
#include "nxdn_const.h"

void
processNXDNVoice (dsd_opts * opts, dsd_state * state)
{
  int i, j, dibit;
  char ambe_fr[4][24];
  const int *w, *x, *y, *z;
  const char *pr;

  if (opts->errorbars == 1)
    {
      printf ("VOICE e:");
    }

  for (i = 0; i < 30; i++)
    {
      dibit = getDibit (opts, state);
#ifdef NXDN_DUMP
      printf ("%c", dibit + 48);
#endif
    }
#ifdef NXDN_DUMP
  printf (" ");
#endif

  pr = nxdnpr;
  for (j = 0; j < 4; j++)
    {
      w = nW;
      x = nX;
      y = nY;
      z = nZ;
      for (i = 0; i < 36; i++)
        {
          dibit = getDibit (opts, state);
#ifdef NXDN_DUMP
          printf ("%c", dibit + 48);
#endif
          ambe_fr[*w][*x] = *pr ^ (1 & (dibit >> 1));   // bit 1
          pr++;
          ambe_fr[*y][*z] = (1 & dibit);        // bit 0
          w++;
          x++;
          y++;
          z++;
        }
      processMbeFrame (opts, state, NULL, ambe_fr, NULL);
#ifdef NXDN_DUMP
      printf (" ");
#endif
    }

  if (opts->errorbars == 1)
    {
      printf ("\n");
    }
}
