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

#define _MAIN

#include "dsd.h"
#include "p25p1_const.h"
#include "x2tdma_const.h"
#include "dstar_const.h"
#include "nxdn_const.h"
#include "dmr_const.h"
#include "provoice_const.h"
#include "git_ver.h"

int
comp (const void *a, const void *b)
{
  if (*((const int *) a) == *((const int *) b))
    return 0;
  else if (*((const int *) a) < *((const int *) b))
    return -1;
  else
    return 1;
}

void
noCarrier (dsd_opts * opts, dsd_state * state)
{
  state->dibit_buf_p = state->dibit_buf + 200;
  memset (state->dibit_buf, 0, sizeof (int) * 200);
  if (opts->mbe_out_f != NULL)
    {
      closeMbeOutFile (opts, state);
    }
  state->jitter = -1;
  state->lastsynctype = -1;
  state->carrier = 0;
  state->max = 15000;
  state->min = -15000;
  state->center = 0;
  state->err_str[0] = 0;
  sprintf (state->fsubtype, "              ");
  sprintf (state->ftype, "             ");
  state->errs = 0;
  state->errs2 = 0;
  state->lasttg = 0;
  state->lastsrc = 0;
  state->lastp25type = 0;
  state->repeat = 0;
  state->nac = 0;
  state->numtdulc = 0;
  sprintf (state->slot0light, " slot0 ");
  sprintf (state->slot1light, " slot1 ");
  state->firstframe = 0;
  if (opts->audio_gain == (float) 0)
    {
      state->aout_gain = 25;
    }
  memset (state->aout_max_buf, 0, sizeof (float) * 200);
  state->aout_max_buf_p = state->aout_max_buf;
  state->aout_max_buf_idx = 0;
  sprintf (state->algid, "________");
  sprintf (state->keyid, "________________");
  mbe_initMbeParms (state->cur_mp, state->prev_mp, state->prev_mp_enhanced);
}

void
initOpts (dsd_opts * opts)
{

  opts->onesymbol = 10;
  opts->mbe_in_file[0] = 0;
  opts->mbe_in_f = NULL;
  opts->errorbars = 1;
  opts->datascope = 0;
  opts->symboltiming = 0;
  opts->verbose = 2;
  opts->p25enc = 0;
  opts->p25lc = 0;
  opts->p25status = 0;
  opts->p25tg = 0;
  opts->scoperate = 15;
  sprintf (opts->audio_in_dev, "/dev/audio");
  opts->audio_in_fd = -1;
  sprintf (opts->audio_out_dev, "/dev/audio");
  opts->audio_out_fd = -1;
  opts->split = 0;
  opts->playoffset = 0;
  opts->mbe_out_dir[0] = 0;
  opts->mbe_out_file[0] = 0;
  opts->mbe_out_f = NULL;
  opts->audio_gain = 0;
  opts->audio_out = 1;
  opts->wav_out_file[0] = 0;
  opts->wav_out_f = NULL;
  //opts->wav_out_fd = -1;
  opts->serial_baud = 115200;
  sprintf (opts->serial_dev, "/dev/ttyUSB0");
  opts->resume = 0;
  opts->frame_dstar = 0;
  opts->frame_x2tdma = 1;
  opts->frame_p25p1 = 1;
  opts->frame_nxdn48 = 0;
  opts->frame_nxdn96 = 1;
  opts->frame_dmr = 1;
  opts->frame_provoice = 0;
  opts->mod_c4fm = 1;
  opts->mod_qpsk = 1;
  opts->mod_gfsk = 1;
  opts->uvquality = 3;
  opts->inverted_x2tdma = 1;    // most transmitter + scanner + sound card combinations show inverted signals for this
  opts->inverted_dmr = 0;       // most transmitter + scanner + sound card combinations show non-inverted signals for this
  opts->mod_threshold = 26;
  opts->ssize = 36;
  opts->msize = 15;
  opts->playfiles = 0;
  opts->delay = 0;
  opts->use_cosine_filter = 1;
  opts->unmute_encrypted_p25 = 0;
}

void
initState (dsd_state * state)
{

  int i, j;

  state->dibit_buf = malloc (sizeof (int) * 1000000);
  state->dibit_buf_p = state->dibit_buf + 200;
  memset (state->dibit_buf, 0, sizeof (int) * 200);
  state->repeat = 0;
  state->audio_out_buf = malloc (sizeof (short) * 1000000);
  memset (state->audio_out_buf, 0, 100 * sizeof (short));
  state->audio_out_buf_p = state->audio_out_buf + 100;
  state->audio_out_float_buf = malloc (sizeof (float) * 1000000);
  memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
  state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
  state->audio_out_idx = 0;
  state->audio_out_idx2 = 0;
  state->audio_out_temp_buf_p = state->audio_out_temp_buf;
  //state->wav_out_bytes = 0;
  state->center = 0;
  state->jitter = -1;
  state->synctype = -1;
  state->min = -15000;
  state->max = 15000;
  state->lmid = 0;
  state->umid = 0;
  state->minref = -12000;
  state->maxref = 12000;
  state->lastsample = 0;
  for (i = 0; i < 128; i++)
    {
      state->sbuf[i] = 0;
    }
  state->sidx = 0;
  for (i = 0; i < 1024; i++)
    {
      state->maxbuf[i] = 15000;
    }
  for (i = 0; i < 1024; i++)
    {
      state->minbuf[i] = -15000;
    }
  state->midx = 0;
  state->err_str[0] = 0;
  sprintf (state->fsubtype, "              ");
  sprintf (state->ftype, "             ");
  state->symbolcnt = 0;
  state->rf_mod = 0;
  state->numflips = 0;
  state->lastsynctype = -1;
  state->lastp25type = 0;
  state->offset = 0;
  state->carrier = 0;
  for (i = 0; i < 25; i++)
    {
      for (j = 0; j < 16; j++)
        {
          state->tg[i][j] = 48;
        }
    }
  state->tgcount = 0;
  state->lasttg = 0;
  state->lastsrc = 0;
  state->nac = 0;
  state->errs = 0;
  state->errs2 = 0;
  state->mbe_file_type = -1;
  state->optind = 0;
  state->numtdulc = 0;
  state->firstframe = 0;
  sprintf (state->slot0light, " slot0 ");
  sprintf (state->slot1light, " slot1 ");
  state->aout_gain = 25;
  memset (state->aout_max_buf, 0, sizeof (float) * 200);
  state->aout_max_buf_p = state->aout_max_buf;
  state->aout_max_buf_idx = 0;
  state->samplesPerSymbol = 10;
  state->symbolCenter = 4;
  sprintf (state->algid, "________");
  sprintf (state->keyid, "________________");
  state->currentslot = 0;
  state->cur_mp = malloc (sizeof (mbe_parms));
  state->prev_mp = malloc (sizeof (mbe_parms));
  state->prev_mp_enhanced = malloc (sizeof (mbe_parms));
  mbe_initMbeParms (state->cur_mp, state->prev_mp, state->prev_mp_enhanced);
  state->p25kid = 0;
}

void
usage ()
{
  printf ("\n");
  printf ("Usage:\n");
  printf ("  dsd [options]            Live scanner mode\n");
  printf ("  dsd [options] -r <files> Read/Play saved mbe data from file(s)\n");
  printf ("  dsd -h                   Show help\n");
  printf ("\n");
  printf ("Display Options:\n");
  printf ("  -e            Show Frame Info and errorbars (default)\n");
  printf ("  -pe           Show P25 encryption sync bits\n");
  printf ("  -pl           Show P25 link control bits\n");
  printf ("  -ps           Show P25 status bits and low speed data\n");
  printf ("  -pt           Show P25 talkgroup info\n");
  printf ("  -q            Don't show Frame Info/errorbars\n");
  printf ("  -s            Datascope (disables other display options)\n");
  printf ("  -t            Show symbol timing during sync\n");
  printf ("  -v <num>      Frame information Verbosity\n");
  printf ("  -z <num>      Frame rate for datascope\n");
  printf ("\n");
  printf ("Input/Output options:\n");
  printf ("  -i <device>   Audio input device (default is /dev/audio)\n");
  printf ("  -o <device>   Audio output device (default is /dev/audio)\n");
  printf ("  -d <dir>      Create mbe data files, use this directory\n");
  printf ("  -r <files>    Read/Play saved mbe data from file(s)\n");
  printf ("  -g <num>      Audio output gain (default = 0 = auto, disable = -1)\n");
  printf ("  -n            Do not send synthesized speech to audio output device\n");
  printf ("  -w <file>     Output synthesized speech to a .wav file\n");
  printf ("\n");
  printf ("Scanner control options:\n");
  printf ("  -B <num>      Serial port baud rate (default=115200)\n");
  printf ("  -C <device>   Serial port for scanner control (default=/dev/ttyUSB0)\n");
  printf ("  -R <num>      Resume scan after <num> TDULC frames or any PDU or TSDU\n");
  printf ("\n");
  printf ("Decoder options:\n");
  printf ("  -fa           Auto-detect frame type (default)\n");
  printf ("  -f1           Decode only P25 Phase 1\n");
  printf ("  -fd           Decode only D-STAR\n");
  printf ("  -fi           Decode only NXDN48* (6.25 kHz) / IDAS*\n");
  printf ("  -fn           Decode only NXDN96 (12.5 kHz)\n");
  printf ("  -fp           Decode only ProVoice*\n");
  printf ("  -fr           Decode only DMR/MOTOTRBO\n");
  printf ("  -fx           Decode only X2-TDMA\n");
  printf ("  -l            Disable DMR/MOTOTRBO and NXDN input filtering\n");
  printf ("  -ma           Auto-select modulation optimizations (default)\n");
  printf ("  -mc           Use only C4FM modulation optimizations\n");
  printf ("  -mg           Use only GFSK modulation optimizations\n");
  printf ("  -mq           Use only QPSK modulation optimizations\n");
  printf ("  -pu           Unmute Encrypted P25\n");
  printf ("  -u <num>      Unvoiced speech quality (default=3)\n");
  printf ("  -xx           Expect non-inverted X2-TDMA signal\n");
  printf ("  -xr           Expect inverted DMR/MOTOTRBO signal\n");
  printf ("\n");
  printf ("  * denotes frame types that cannot be auto-detected.\n");
  printf ("\n");
  printf ("Advanced decoder options:\n");
  printf ("  -A <num>      QPSK modulation auto detection threshold (default=26)\n");
  printf ("  -S <num>      Symbol buffer size for QPSK decision point tracking\n");
  printf ("                 (default=36)\n");
  printf ("  -M <num>      Min/Max buffer size for QPSK decision point tracking\n");
  printf ("                 (default=15)\n");
  exit (0);
}

void
liveScanner (dsd_opts * opts, dsd_state * state)
{
  if (opts->audio_in_fd == -1)
    {
      if (pthread_mutex_lock(&state->input_mutex))
        {
          printf("liveScanner -> Unable to lock mutex\n");
        }
    }
  while (1)
    {
      noCarrier (opts, state);
      state->synctype = getFrameSync (opts, state);
      // recalibrate center/umid/lmid
      state->center = ((state->max) + (state->min)) / 2;
      state->umid = (((state->max) - state->center) * 5 / 8) + state->center;
      state->lmid = (((state->min) - state->center) * 5 / 8) + state->center;
      while (state->synctype != -1)
        {
          processFrame (opts, state);
          state->synctype = getFrameSync (opts, state);
          // recalibrate center/umid/lmid
          state->center = ((state->max) + (state->min)) / 2;
          state->umid = (((state->max) - state->center) * 5 / 8) + state->center;
          state->lmid = (((state->min) - state->center) * 5 / 8) + state->center;
        }
    }
}

void
cleanupAndExit (dsd_opts * opts, dsd_state * state)
{
  noCarrier (opts, state);
  if (opts->wav_out_f != NULL)
    {
      closeWavOutFile (opts, state);
    }
  printf ("Exiting.\n");
  exit (0);
}

void
sigfun (int sig)
{
  exitflag = 1;
  signal (SIGINT, SIG_DFL);
}

int
main (int argc, char **argv)
{

  int c;
  extern char *optarg;
  extern int optind, opterr, optopt;
  dsd_opts opts;
  dsd_state state;
  char versionstr[25];
  mbe_printVersion (versionstr);

  printf ("Digital Speech Decoder 1.7.0-dev (build:%s)\n", GIT_TAG);
  printf ("mbelib version %s\n", versionstr);

  initOpts (&opts);
  initState (&state);

  exitflag = 0;
  signal (SIGINT, sigfun);

  while ((c = getopt (argc, argv, "hep:qstv:z:i:o:d:g:nw:B:C:R:f:m:u:x:A:S:M:rl")) != -1)
    {
      opterr = 0;
      switch (c)
        {
        case 'h':
          usage ();
          exit (0);
        case 'e':
          opts.errorbars = 1;
          opts.datascope = 0;
          break;
        case 'p':
          if (optarg[0] == 'e')
            {
              opts.p25enc = 1;
            }
          else if (optarg[0] == 'l')
            {
              opts.p25lc = 1;
            }
          else if (optarg[0] == 's')
            {
              opts.p25status = 1;
            }
          else if (optarg[0] == 't')
            {
              opts.p25tg = 1;
            }
          else if (optarg[0] == 'u')
            {
        	  opts.unmute_encrypted_p25 = 1;
            }
          break;
        case 'q':
          opts.errorbars = 0;
          opts.verbose = 0;
          break;
        case 's':
          opts.errorbars = 0;
          opts.p25enc = 0;
          opts.p25lc = 0;
          opts.p25status = 0;
          opts.p25tg = 0;
          opts.datascope = 1;
          opts.symboltiming = 0;
          break;
        case 't':
          opts.symboltiming = 1;
          opts.errorbars = 1;
          opts.datascope = 0;
          break;
        case 'v':
          sscanf (optarg, "%d", &opts.verbose);
          break;
        case 'z':
          sscanf (optarg, "%d", &opts.scoperate);
          opts.errorbars = 0;
          opts.p25enc = 0;
          opts.p25lc = 0;
          opts.p25status = 0;
          opts.p25tg = 0;
          opts.datascope = 1;
          opts.symboltiming = 0;
          printf ("Setting datascope frame rate to %i frame per second.\n", opts.scoperate);
          break;
        case 'i':
          strncpy(opts.audio_in_dev, optarg, 1023);
          opts.audio_in_dev[1023] = '\0';
          break;
        case 'o':
          strncpy(opts.audio_out_dev, optarg, 1023);
          opts.audio_out_dev[1023] = '\0';
          break;
        case 'd':
          strncpy(opts.mbe_out_dir, optarg, 1023);
          opts.mbe_out_dir[1023] = '\0';
          printf ("Writing mbe data files to directory %s\n", opts.mbe_out_dir);
          break;
        case 'g':
          sscanf (optarg, "%f", &opts.audio_gain);
          if (opts.audio_gain < (float) 0 )
            {
              printf ("Disabling audio out gain setting\n");
            }
          else if (opts.audio_gain == (float) 0)
            {
              opts.audio_gain = (float) 0;
              printf ("Enabling audio out auto-gain\n");
            }
          else
            {
              printf ("Setting audio out gain to %f\n", opts.audio_gain);
              state.aout_gain = opts.audio_gain;
            }
          break;
        case 'n':
          opts.audio_out = 0;
          printf ("Disabling audio output to soundcard.\n");
          break;
        case 'w':
          strncpy(opts.wav_out_file, optarg, 1023);
          opts.wav_out_file[1023] = '\0';
          printf ("Writing audio to file %s\n", opts.wav_out_file);
          openWavOutFile (&opts, &state);
          break;
        case 'B':
          sscanf (optarg, "%d", &opts.serial_baud);
          break;
        case 'C':
          strncpy(opts.serial_dev, optarg, 1023);
          opts.serial_dev[1023] = '\0';
          break;
        case 'R':
          sscanf (optarg, "%d", &opts.resume);
          printf ("Enabling scan resume after %i TDULC frames\n", opts.resume);
          break;
        case 'f':
          if (optarg[0] == 'a')
            {
              opts.frame_dstar = 1;
              opts.frame_x2tdma = 1;
              opts.frame_p25p1 = 1;
              opts.frame_nxdn48 = 0;
              opts.frame_nxdn96 = 1;
              opts.frame_dmr = 1;
              opts.frame_provoice = 0;
            }
          else if (optarg[0] == 'd')
            {
              opts.frame_dstar = 1;
              opts.frame_x2tdma = 0;
              opts.frame_p25p1 = 0;
              opts.frame_nxdn48 = 0;
              opts.frame_nxdn96 = 0;
              opts.frame_dmr = 0;
              opts.frame_provoice = 0;
              printf ("Decoding only D-STAR frames.\n");
            }
          else if (optarg[0] == 'x')
            {
              opts.frame_dstar = 0;
              opts.frame_x2tdma = 1;
              opts.frame_p25p1 = 0;
              opts.frame_nxdn48 = 0;
              opts.frame_nxdn96 = 0;
              opts.frame_dmr = 0;
              opts.frame_provoice = 0;
              printf ("Decoding only X2-TDMA frames.\n");
            }
          else if (optarg[0] == 'p')
            {
              opts.frame_dstar = 0;
              opts.frame_x2tdma = 0;
              opts.frame_p25p1 = 0;
              opts.frame_nxdn48 = 0;
              opts.frame_nxdn96 = 0;
              opts.frame_dmr = 0;
              opts.frame_provoice = 1;
              state.samplesPerSymbol = 5;
              state.symbolCenter = 2;
              opts.mod_c4fm = 0;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 1;
              state.rf_mod = 2;
              printf ("Setting symbol rate to 9600 / second\n");
              printf ("Enabling only GFSK modulation optimizations.\n");
              printf ("Decoding only ProVoice frames.\n");
            }
          else if (optarg[0] == '1')
            {
              opts.frame_dstar = 0;
              opts.frame_x2tdma = 0;
              opts.frame_p25p1 = 1;
              opts.frame_nxdn48 = 0;
              opts.frame_nxdn96 = 0;
              opts.frame_dmr = 0;
              opts.frame_provoice = 0;
              printf ("Decoding only P25 Phase 1 frames.\n");
            }
          else if (optarg[0] == 'i')
            {
              opts.frame_dstar = 0;
              opts.frame_x2tdma = 0;
              opts.frame_p25p1 = 0;
              opts.frame_nxdn48 = 1;
              opts.frame_nxdn96 = 0;
              opts.frame_dmr = 0;
              opts.frame_provoice = 0;
              state.samplesPerSymbol = 20;
              state.symbolCenter = 10;
              opts.mod_c4fm = 0;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 1;
              state.rf_mod = 2;
              printf ("Setting symbol rate to 2400 / second\n");
              printf ("Enabling only GFSK modulation optimizations.\n");
              printf ("Decoding only NXDN 4800 baud frames.\n");
            }
          else if (optarg[0] == 'n')
            {
              opts.frame_dstar = 0;
              opts.frame_x2tdma = 0;
              opts.frame_p25p1 = 0;
              opts.frame_nxdn48 = 0;
              opts.frame_nxdn96 = 1;
              opts.frame_dmr = 0;
              opts.frame_provoice = 0;
              opts.mod_c4fm = 0;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 1;
              state.rf_mod = 2;
              printf ("Enabling only GFSK modulation optimizations.\n");
              printf ("Decoding only NXDN 9600 baud frames.\n");
            }
          else if (optarg[0] == 'r')
            {
              opts.frame_dstar = 0;
              opts.frame_x2tdma = 0;
              opts.frame_p25p1 = 0;
              opts.frame_nxdn48 = 0;
              opts.frame_nxdn96 = 0;
              opts.frame_dmr = 1;
              opts.frame_provoice = 0;
              printf ("Decoding only DMR/MOTOTRBO frames.\n");
            }
          break;
        case 'm':
          if (optarg[0] == 'a')
            {
              opts.mod_c4fm = 1;
              opts.mod_qpsk = 1;
              opts.mod_gfsk = 1;
              state.rf_mod = 0;
            }
          else if (optarg[0] == 'c')
            {
              opts.mod_c4fm = 1;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 0;
              state.rf_mod = 0;
              printf ("Enabling only C4FM modulation optimizations.\n");
            }
          else if (optarg[0] == 'g')
            {
              opts.mod_c4fm = 0;
              opts.mod_qpsk = 0;
              opts.mod_gfsk = 1;
              state.rf_mod = 2;
              printf ("Enabling only GFSK modulation optimizations.\n");
            }
          else if (optarg[0] == 'q')
            {
              opts.mod_c4fm = 0;
              opts.mod_qpsk = 1;
              opts.mod_gfsk = 0;
              state.rf_mod = 1;
              printf ("Enabling only QPSK modulation optimizations.\n");
            }
          break;
        case 'u':
          sscanf (optarg, "%i", &opts.uvquality);
          if (opts.uvquality < 1)
            {
              opts.uvquality = 1;
            }
          else if (opts.uvquality > 64)
            {
              opts.uvquality = 64;
            }
          printf ("Setting unvoice speech quality to %i waves per band.\n", opts.uvquality);
          break;
        case 'x':
          if (optarg[0] == 'x')
            {
              opts.inverted_x2tdma = 0;
              printf ("Expecting non-inverted X2-TDMA signals.\n");
            }
          else if (optarg[0] == 'r')
            {
              opts.inverted_dmr = 1;
              printf ("Expecting inverted DMR/MOTOTRBO signals.\n");
            }
          break;
        case 'A':
          sscanf (optarg, "%i", &opts.mod_threshold);
          printf ("Setting C4FM/QPSK auto detection threshold to %i\n", opts.mod_threshold);
          break;
        case 'S':
          sscanf (optarg, "%i", &opts.ssize);
          if (opts.ssize > 128)
            {
              opts.ssize = 128;
            }
          else if (opts.ssize < 1)
            {
              opts.ssize = 1;
            }
          printf ("Setting QPSK symbol buffer to %i\n", opts.ssize);
          break;
        case 'M':
          sscanf (optarg, "%i", &opts.msize);
          if (opts.msize > 1024)
            {
              opts.msize = 1024;
            }
          else if (opts.msize < 1)
            {
              opts.msize = 1;
            }
          printf ("Setting QPSK Min/Max buffer to %i\n", opts.msize);
          break;
        case 'r':
          opts.playfiles = 1;
          opts.errorbars = 0;
          opts.datascope = 0;
          state.optind = optind;
          break;
        case 'l':
          opts.use_cosine_filter = 0;
          break;
        default:
          usage ();
          exit (0);
        }
    }


  if (opts.resume > 0)
    {
      openSerial (&opts, &state);
    }

  if (opts.playfiles == 1)
    {
      opts.split = 1;
      opts.playoffset = 0;
      opts.delay = 0;
      if(strlen(opts.wav_out_file) > 0) {
        openWavOutFile (&opts, &state);
      }
      else {
        openAudioOutDevice (&opts, 8000);
      }
    }
  else if (strcmp (opts.audio_in_dev, opts.audio_out_dev) != 0)
    {
      opts.split = 1;
      opts.playoffset = 0;
      opts.delay = 0;
      if(strlen(opts.wav_out_file) > 0) {
        openWavOutFile (&opts, &state);
      }
      else {
        openAudioOutDevice (&opts, 8000);
      }
      openAudioInDevice (&opts);
    }
  else
    {
      opts.split = 0;
      opts.playoffset = 25;     // 38
      opts.delay = 0;
      openAudioInDevice (&opts);
      opts.audio_out_fd = opts.audio_in_fd;
    }

  if (opts.playfiles == 1)
    {
      playMbeFiles (&opts, &state, argc, argv);
    }
  else
    {
      liveScanner (&opts, &state);
    }
  cleanupAndExit (&opts, &state);
  return (0);
}
