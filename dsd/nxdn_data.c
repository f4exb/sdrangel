#include "dsd.h"

void
processNXDNData (dsd_opts * opts, dsd_state * state)
{
  int i, dibit;

  if (opts->errorbars == 1)
    {
      fprintf(stderr, "DATA    ");
    }

  for (i = 0; i < 30; i++)
    {
      dibit = getDibit (opts, state);
#ifdef NXDN_DUMP
      fprintf(stderr, "%c", dibit + 48);
#endif
    }
#ifdef NXDN_DUMP
  fprintf(stderr, " ");
#endif

  for (i = 0; i < 144; i++)
    {
      dibit = getDibit (opts, state);
#ifdef NXDN_DUMP
      fprintf(stderr, "%c", dibit + 48);
#endif
    }

  if (opts->errorbars == 1)
    {
      fprintf(stderr, "\n");
    }
}
