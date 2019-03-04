/*---------------------------------------------------------------------------*\

  FILE........: codec2_fdmdv.h
  AUTHOR......: David Rowe
  DATE CREATED: April 14 2012

  A 1400 bit/s (nominal) Frequency Division Multiplexed Digital Voice
  (FDMDV) modem.  Used for digital audio over HF SSB. See
  README_fdmdv.txt for more information, and fdmdv_mod.c and
  fdmdv_demod.c for example usage.

  The name codec2_fdmdv.h is used to make it unique when "make
  installed".

  References:

    [1] http://n1su.com/fdmdv/FDMDV_Docs_Rel_1_4b.pdf

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2012 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __FDMDV__
#define __FDMDV__

/* set up the calling convention for DLL function import/export for
   WIN32 cross compiling */

#ifdef __CODEC2_WIN32__
#ifdef __CODEC2_BUILDING_DLL__
#define CODEC2_WIN32SUPPORT __declspec(dllexport) __stdcall
#else
#define CODEC2_WIN32SUPPORT __declspec(dllimport) __stdcall
#endif
#else
#define CODEC2_WIN32SUPPORT
#endif

#include "codec2/comp.h"
#include "modem_stats.h"

#define FDMDV_NC                      14  /* default number of data carriers                                */
#define FDMDV_NC_MAX                  20  /* maximum number of data carriers                                */
#define FDMDV_BITS_PER_FRAME          28  /* 20ms frames, for nominal 1400 bit/s                            */
#define FDMDV_NOM_SAMPLES_PER_FRAME  160  /* modulator output samples/frame and nominal demod samples/frame */
                                          /* at 8000 Hz sample rate                                         */
#define FDMDV_MAX_SAMPLES_PER_FRAME  200  /* max demod samples/frame, use this to allocate storage          */
#define FDMDV_SCALE                 1000  /* suggested scaling for 16 bit shorts                            */
#define FDMDV_FCENTRE               1500  /* Centre frequency, Nc/2 carriers below this, Nc/2 carriers above (Hz) */

/* 8 to 48 kHz sample rate conversion */

#define FDMDV_OS                 2                            /* oversampling rate                   */
#define FDMDV_OS_TAPS_16K       48                            /* number of OS filter taps at 16kHz   */
#define FDMDV_OS_TAPS_8K        (FDMDV_OS_TAPS_16K/FDMDV_OS)  /* number of OS filter taps at 8kHz    */

namespace FreeDV
{

/* FDMDV states and stats structures */

struct FDMDV;

struct FDMDV * fdmdv_create(int Nc);
void           fdmdv_destroy(struct FDMDV *fdmdv_state);
void           fdmdv_use_old_qpsk_mapping(struct FDMDV *fdmdv_state);
int            fdmdv_bits_per_frame(struct FDMDV *fdmdv_state);
float          fdmdv_get_fsep(struct FDMDV *fdmdv_state);
void           fdmdv_set_fsep(struct FDMDV *fdmdv_state, float fsep);

void           fdmdv_mod(struct FDMDV *fdmdv_state, COMP tx_fdm[], int tx_bits[], int *sync_bit);
void           fdmdv_demod(struct FDMDV *fdmdv_state, int rx_bits[], int *reliable_sync_bit, COMP rx_fdm[], int *nin);

void           fdmdv_get_test_bits(struct FDMDV *fdmdv_state, int tx_bits[]);
int            fdmdv_error_pattern_size(struct FDMDV *fdmdv_state);
void           fdmdv_put_test_bits(struct FDMDV *f, int *sync, short error_pattern[], int *bit_errors, int *ntest_bits, int rx_bits[]);

void           fdmdv_get_demod_stats(struct FDMDV *fdmdv_state, struct MODEM_STATS *stats);

void           fdmdv_8_to_16(float out16k[], float in8k[], int n);
void           fdmdv_8_to_16_short(short out16k[], short in8k[], int n);
void           fdmdv_16_to_8(float out8k[], float in16k[], int n);
void           fdmdv_16_to_8_short(short out8k[], short in16k[], int n);

void           fdmdv_freq_shift(COMP rx_fdm_fcorr[], COMP rx_fdm[], float foff, COMP *foff_phase_rect, int nin);

/* debug/development function(s) */

void fdmdv_dump_osc_mags(struct FDMDV *f);
void fdmdv_simulate_channel(float *sig_pwr_av, COMP samples[], int nin, float target_snr);

} // FreeDV

#endif

