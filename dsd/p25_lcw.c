#include "dsd.h"

void
processP25lcw (dsd_opts * opts, dsd_state * state, char *lcformat, char *mfid, char *lcinfo)
{

  char tgid[17], tmpstr[255];
  long talkgroup, source;
  int i, j;

  tgid[16] = 0;

  if (opts->p25lc == 1)
    {
      printf ("lcformat: %s mfid: %s lcinfo: %s ", lcformat, mfid, lcinfo);
      if (opts->p25tg == 0)
        {
          printf ("\n");
        }
    }

  if (strcmp (lcformat, "00000100") == 0)
    {

      // first tg is the active channel
      j = 0;
      for (i = 40; i < 52; i++)
        {
          if (state->tgcount < 24)
            {
              state->tg[state->tgcount][j] = lcinfo[i];
            }
          tmpstr[j] = lcinfo[i];
          j++;
        }
      tmpstr[12] = 48;
      tmpstr[13] = 48;
      tmpstr[14] = 48;
      tmpstr[15] = 48;
      tmpstr[16] = 0;
      talkgroup = strtol (tmpstr, NULL, 2);
      state->lasttg = talkgroup;
      if (state->tgcount < 24)
        {
          state->tgcount = state->tgcount + 1;
        }
      if (opts->p25tg == 1)
        {
          printf ("tg: %li ", talkgroup);
        }

      if (opts->p25tg == 1)
        {
          printf ("tg: %li ", talkgroup);

          // the remaining 3 appear to be other active tg's on the system
          j = 0;
          for (i = 28; i < 40; i++)
            {
              tmpstr[j] = lcinfo[i];
              j++;
            }
          tmpstr[12] = 48;
          tmpstr[13] = 48;
          tmpstr[14] = 48;
          tmpstr[15] = 48;
          tmpstr[16] = 0;
          talkgroup = strtol (tmpstr, NULL, 2);
          printf ("%li ", talkgroup);
          j = 0;
          for (i = 16; i < 28; i++)
            {
              tmpstr[j] = lcinfo[i];
              j++;
            }
          tmpstr[12] = 48;
          tmpstr[13] = 48;
          tmpstr[14] = 48;
          tmpstr[15] = 48;
          tmpstr[16] = 0;
          talkgroup = strtol (tmpstr, NULL, 2);
          printf ("%li ", talkgroup);
          j = 0;
          for (i = 4; i < 16; i++)
            {
              tmpstr[j] = lcinfo[i];
              j++;
            }
          tmpstr[12] = 48;
          tmpstr[13] = 48;
          tmpstr[14] = 48;
          tmpstr[15] = 48;
          tmpstr[16] = 0;
          talkgroup = strtol (tmpstr, NULL, 2);
          printf ("%li\n", talkgroup);
        }
    }

  else if (strcmp (lcformat, "00000000") == 0)
    {
      j = 0;
      if (strcmp (mfid, "10010000") == 0)
        {
          for (i = 20; i < 32; i++)
            {
              if (state->tgcount < 24)
                {
                  state->tg[state->tgcount][j] = lcinfo[i];
                }
              tmpstr[j] = lcinfo[i];
              j++;
            }
          tmpstr[12] = 48;
          tmpstr[13] = 48;
          tmpstr[14] = 48;
          tmpstr[15] = 48;
        }
      else
        {
          for (i = 16; i < 32; i++)
            {
              if (state->tgcount < 24)
                {
                  state->tg[state->tgcount][j] = lcinfo[i];
                }
              tmpstr[j] = lcinfo[i];
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
          printf ("tg: %li ", talkgroup);
        }

      j = 0;
      for (i = 32; i < 56; i++)
        {
          tmpstr[j] = lcinfo[i];
          j++;
        }
      tmpstr[24] = 0;
      source = strtol (tmpstr, NULL, 2);
      state->lastsrc = source;
      if (opts->p25tg == 1)
        {
          printf ("src: %li emr: %c\n", source, lcinfo[0]);
        }
    }
  else if ((opts->p25tg == 1) && (opts->p25lc == 1))
    {
      printf ("\n");
    }
}
