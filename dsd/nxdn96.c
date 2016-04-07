#include "dsd.h"
#include "nxdn96_const.h"

void
processNXDN96 (dsd_opts * opts, dsd_state * state)
{
  int i, j, k, dibit;

  char ambe_fr[4][24];
  const int *w, *x, *y, *z;

  if (opts->errorbars == 1)
    {
      printf ("VOICE e:");
    }

#ifdef NXDN_DUMP
  printf ("\n");
#endif

  for (k = 0; k < 4; k++)
    {
      for (i = 0; i < 222; i++)
        {
          dibit = getDibit (opts, state);
#ifdef NXDN_DUMP
          printf ("%c", dibit + 48);
#endif
        }
#ifdef NXDN_DUMP
      printf (" ");
#endif

      if (k < 3)
        {
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
                  ambe_fr[*w][*x] = (1 & (dibit >> 1)); // bit 1
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
        }
      else
        {
          for (j = 0; j < 3; j++)       // we skip the last voice frame until frame sync can work with < 24 symbols
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
                  ambe_fr[*w][*x] = (1 & (dibit >> 1)); // bit 1
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
        }

      if (k < 3)
        {
          for (i = 0; i < 18; i++)
            {
              dibit = getDibit (opts, state);
#ifdef NXDN_DUMP
              printf ("%c", dibit + 48);
#endif
            }
#ifdef NXDN_DUMP
          printf (" ");
#endif

        }
      else
        {
          for (i = 0; i < 30; i++)
            {
              dibit = getDibit (opts, state);
#ifdef NXDN_DUMP
              printf ("%c", dibit + 48);
#endif
            }
        }
    }

#ifdef NXDN_DUMP
  printf ("\n");
#endif

  if (opts->errorbars == 1)
    {
      printf ("\n");
    }

}
