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
saveImbe4400Data (dsd_opts * opts, dsd_state * state, char *imbe_d)
{
  int i, j, k;
  unsigned char b;
  unsigned char err;

  err = (unsigned char) state->errs2;
  fputc (err, opts->mbe_out_f);

  k = 0;
  for (i = 0; i < 11; i++)
    {
      b = 0;
      for (j = 0; j < 8; j++)
        {
          b = b << 1;
          b = b + imbe_d[k];
          k++;
        }
      fputc (b, opts->mbe_out_f);
    }
  fflush (opts->mbe_out_f);
}

void
saveAmbe2450Data (dsd_opts * opts, dsd_state * state, char *ambe_d)
{
  int i, j, k;
  unsigned char b;
  unsigned char err;

  err = (unsigned char) state->errs2;
  fputc (err, opts->mbe_out_f);

  k = 0;
  for (i = 0; i < 6; i++)
    {
      b = 0;
      for (j = 0; j < 8; j++)
        {
          b = b << 1;
          b = b + ambe_d[k];
          k++;
        }
      fputc (b, opts->mbe_out_f);
    }
  b = ambe_d[48];
  fputc (b, opts->mbe_out_f);
  fflush (opts->mbe_out_f);
}

int
readImbe4400Data (dsd_opts * opts, dsd_state * state, char *imbe_d)
{

  int i, j, k;
  unsigned char b;

  state->errs2 = fgetc (opts->mbe_in_f);
  state->errs = state->errs2;

  k = 0;
  for (i = 0; i < 11; i++)
    {
      b = fgetc (opts->mbe_in_f);
      if (feof (opts->mbe_in_f))
        {
          return (1);
        }
      for (j = 0; j < 8; j++)
        {
          imbe_d[k] = (b & 128) >> 7;
          b = b << 1;
          b = b & 255;
          k++;
        }
    }
  return (0);
}

int
readAmbe2450Data (dsd_opts * opts, dsd_state * state, char *ambe_d)
{

  int i, j, k;
  unsigned char b;

  state->errs2 = fgetc (opts->mbe_in_f);
  state->errs = state->errs2;

  k = 0;
  for (i = 0; i < 6; i++)
    {
      b = fgetc (opts->mbe_in_f);
      if (feof (opts->mbe_in_f))
        {
          return (1);
        }
      for (j = 0; j < 8; j++)
        {
          ambe_d[k] = (b & 128) >> 7;
          b = b << 1;
          b = b & 255;
          k++;
        }
    }
  b = fgetc (opts->mbe_in_f);
  ambe_d[48] = (b & 1);

  return (0);
}

void
openMbeInFile (dsd_opts * opts, dsd_state * state)
{

  char cookie[5];

  opts->mbe_in_f = fopen (opts->mbe_in_file, "ro");
  if (opts->mbe_in_f == NULL)
    {
      printf ("Error: could not open %s\n", opts->mbe_in_file);
    }

  // read cookie
  cookie[0] = fgetc (opts->mbe_in_f);
  cookie[1] = fgetc (opts->mbe_in_f);
  cookie[2] = fgetc (opts->mbe_in_f);
  cookie[3] = fgetc (opts->mbe_in_f);
  cookie[4] = 0;
  if (strstr (cookie, ".amb") != NULL)
    {
      state->mbe_file_type = 1;
    }
  else if (strstr (cookie, ".imb") != NULL)
    {
      state->mbe_file_type = 0;
    }
  else
    {
      state->mbe_file_type = -1;
      printf ("Error - unrecognized file type\n");
    }

}

void
closeMbeOutFile (dsd_opts * opts, dsd_state * state)
{

  char shell[255], newfilename[64], ext[5], datestr[32];
  char tgid[17];
  int sum, i, j;
  int talkgroup;
  struct tm timep;
  int result;

  if (opts->mbe_out_f != NULL)
    {
      if ((state->synctype == 0) || (state->synctype == 1) || (state->synctype == 14) || (state->synctype == 15))
        {
          sprintf (ext, ".imb");
          strptime (opts->mbe_out_file, "%s.imb", &timep);
        }
      else
        {
          sprintf (ext, ".amb");
          strptime (opts->mbe_out_file, "%s.amb", &timep);
        }

      if (state->tgcount > 0)
        {
          for (i = 0; i < 16; i++)
            {
              sum = 0;
              for (j = 0; j < state->tgcount; j++)
                {
                  sum = sum + state->tg[j][i] - 48;
                }
              tgid[i] = (char) (((float) sum / (float) state->tgcount) + 48.5);
            }
          tgid[16] = 0;
          talkgroup = (int) strtol (tgid, NULL, 2);
        }
      else
        {
          talkgroup = 0;
        }

      fflush (opts->mbe_out_f);
      fclose (opts->mbe_out_f);
      opts->mbe_out_f = NULL;
      strftime (datestr, 31, "%Y-%m-%d-%H%M%S", &timep);
      sprintf (newfilename, "nac%X-%s-tg%i%s", state->nac, datestr, talkgroup, ext);
      sprintf (shell, "mv %s %s", opts->mbe_out_file, newfilename);
      result = system (shell);

      state->tgcount = 0;
      for (i = 0; i < 25; i++)
        {
          for (j = 0; j < 16; j++)
            {
              state->tg[i][j] = 48;
            }
        }
    }
}

void
openMbeOutFile (dsd_opts * opts, dsd_state * state)
{

  struct timeval tv;
  int i, j;
  char ext[5];

  if ((state->synctype == 0) || (state->synctype == 1) || (state->synctype == 14) || (state->synctype == 15))
    {
      sprintf (ext, ".imb");
    }
  else
    {
      sprintf (ext, ".amb");
    }

  //  reset talkgroup id buffer
  for (i = 0; i < 12; i++)
    {
      for (j = 0; j < 25; j++)
        {
          state->tg[j][i] = 0;
        }
    }

  state->tgcount = 0;

  gettimeofday (&tv, NULL);
  sprintf (opts->mbe_out_file, "%i%s", (int) tv.tv_sec, ext);
  opts->mbe_out_f = fopen (opts->mbe_out_file, "w");
  if (opts->mbe_out_f == NULL)
    {
      printf ("Error, couldn't open %s\n", opts->mbe_out_file);
    }

  // write magic
  fprintf (opts->mbe_out_f, "%s", ext);

  fflush (opts->mbe_out_f);
}

void
openWavOutFile (dsd_opts * opts, dsd_state * state)
{

 // opts->wav_out_f = fopen (opts->wav_out_file, "w");
  
  SF_INFO info;
  info.samplerate = 8000;
  info.channels = 1;
  info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
  opts->wav_out_f = sf_open (opts->wav_out_file, SFM_WRITE, &info);

  if (opts->wav_out_f == NULL)
  {
    printf ("Error - could not open wav output file %s\n", opts->wav_out_file);
    return;
  }

//  state->wav_out_bytes = 0;
  
}

void
closeWavOutFile (dsd_opts * opts, dsd_state * state)
{
  sf_close(opts->wav_out_f);

/*
  int length;

  if (opts->wav_out_f != NULL)
    {
      rewind (opts->wav_out_f);
      length = state->wav_out_bytes;

      fprintf (opts->wav_out_f, "RIFF");
      // total length
      fputc (((36 + length) & 0xff), opts->≈≈);
      fputc ((((36 + length) >> 8) & 0xff), opts->wav_out_f);
      fputc ((((36 + length) >> 16) & 0xff), opts->wav_out_f);
      fputc ((((36 + length) >> 24) & 0xff), opts->wav_out_f);

      fprintf (opts->wav_out_f, "WAVE");
      fprintf (opts->wav_out_f, "fmt ");

      // length of format chunk
      fputc (16, opts->wav_out_f);
      fputc (0, opts->wav_out_f);
      fputc (0, opts->wav_out_f);
      fputc (0, opts->wav_out_f);

      // always 0x1
      fputc (1, opts->wav_out_f);
      fputc (0, opts->wav_out_f);

      // channels
      fputc (1, opts->wav_out_f);
      fputc (0, opts->wav_out_f);

      // sample rate
      fputc (64, opts->wav_out_f);
      fputc (31, opts->wav_out_f);
      fputc (0, opts->wav_out_f);
      fputc (0, opts->wav_out_f);

      // bytes per second
      fputc (128, opts->wav_out_f);
      fputc (62, opts->wav_out_f);
      fputc (0, opts->wav_out_f);
      fputc (0, opts->wav_out_f);

      // block align
      fputc (2, opts->wav_out_f);
      fputc (0, opts->wav_out_f);

      // bits/sample
      fputc (16, opts->wav_out_f);
      fputc (0, opts->wav_out_f);

      // data chunk header
      fprintf (opts->wav_out_f, "data");

      // length of data
      fputc ((length & 0xff), opts->wav_out_f);
      fputc (((length >> 8) & 0xff), opts->wav_out_f);
      fputc (((length >> 16) & 0xff), opts->wav_out_f);
      fputc (((length >> 24) & 0xff), opts->wav_out_f);

      fflush (opts->wav_out_f);
    }
    */
}
