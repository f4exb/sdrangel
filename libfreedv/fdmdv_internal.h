/*---------------------------------------------------------------------------*\

  FILE........: fdmdv_internal.h
  AUTHOR......: David Rowe
  DATE CREATED: April 16 2012

  Header file for FDMDV internal functions, exposed via this header
  file for testing.

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

#ifndef __FDMDV_INTERNAL__
#define __FDMDV_INTERNAL__

#include "codec2/comp.h"
#include "codec2_fdmdv.h"
#include "codec2_fft.h"

/*---------------------------------------------------------------------------*\

                               DEFINES

\*---------------------------------------------------------------------------*/

#ifndef PI
#define PI             3.141592654
#endif
#define FS                    8000  /* sample rate in Hz                                                    */
#define T                 (1.0/FS)  /* sample period in seconds                                             */
#define RS                      50  /* symbol rate in Hz                                                    */
#define NC                      20  /* max number of data carriers (plus one pilot in the centre)           */
#define NB                       2  /* Bits/symbol for QPSK modulation                                      */
#define RB              (NC*RS*NB)  /* bit rate                                                             */
#define M_FAC                  (FS/RS)  /* oversampling factor                                                  */
#define NSYM                     6  /* number of symbols to filter over                                     */
#define NFILTER            (NSYM*M_FAC) /* size of tx/rx filters at sample rate M                               */

#define FSEP                    75  /* Default separation between carriers (Hz)                             */

#define NT                       5  /* number of symbols we estimate timing over                            */
#define P                        4  /* oversample factor used for initial rx symbol filtering output        */
#define Q                     (M_FAC/4) /* oversample factor used for initial rx symbol filtering input         */
#define NRXDEC                  31  /* number of taps in the rx decimation filter                           */

#define NPILOT_LUT                 (4*M_FAC)    /* number of pilot look up table samples                 */
#define NPILOTCOEFF                   30    /* number of FIR filter coeffs in LP filter              */
#define NPILOTBASEBAND (NPILOTCOEFF+M_FAC+M_FAC/P)  /* number of pilot baseband samples reqd for pilot LPF   */
#define NPILOTLPF                  (4*M_FAC)    /* number of samples we DFT pilot over, pilot est window */
#define MPILOTFFT                    256

#define NSYNC_MEM                6

#define NRX_FDM_MEM (NFILTER+M_FAC+M_FAC/P)           /* size of rx filter memory            */
#define NRXDECMEM   (NRXDEC+M_FAC+M_FAC/P)            /* size of rx decimation filter memory */

/* averaging filter coeffs */

#define TRACK_COEFF              0.5
#define SNR_COEFF                0.9       /* SNR est averaging filter coeff */

namespace FreeDV
{

/*---------------------------------------------------------------------------*\

                               STRUCT for States

\*---------------------------------------------------------------------------*/

struct FDMDV {

    int   Nc;
    float fsep;

    /* test data (test frame) states */

    int  ntest_bits;
    int  current_test_bit;
    int *rx_test_bits_mem;

    /* Modulator */

    int   old_qpsk_mapping;
    int   tx_pilot_bit;
    COMP  prev_tx_symbols[NC+1];
    COMP  tx_filter_memory[NC+1][NSYM];
    COMP  phase_tx[NC+1];
    COMP  freq[NC+1];
    float freq_pol[NC+1];

    /* Pilot generation at demodulator */

    COMP pilot_lut[NPILOT_LUT];
    int  pilot_lut_index;
    int  prev_pilot_lut_index;

    /* freq offset estimation states */

    codec2_fft_cfg fft_pilot_cfg;
    COMP pilot_baseband1[NPILOTBASEBAND];
    COMP pilot_baseband2[NPILOTBASEBAND];
    COMP pilot_lpf1[NPILOTLPF];
    COMP pilot_lpf2[NPILOTLPF];
    COMP S1[MPILOTFFT];
    COMP S2[MPILOTFFT];

    /* baseband to low IF carrier states */

    COMP  fbb_rect;
    float fbb_pol;
    COMP  fbb_phase_tx;
    COMP  fbb_phase_rx;

    /* freq offset correction states */

    float foff;
    COMP foff_phase_rect;
    float foff_filt;

    /* Demodulator */

    COMP  rxdec_lpf_mem[NRXDECMEM];
    COMP  rx_fdm_mem[NRX_FDM_MEM];
    COMP  phase_rx[NC+1];
    COMP  rx_filter_mem_timing[NC+1][NT*P];
    float rx_timing;
    COMP  phase_difference[NC+1];
    COMP  prev_rx_symbols[NC+1];

    /* sync state machine */

    int  sync_mem[NSYNC_MEM];
    int  fest_state;
    int  sync;
    int  timer;

    /* SNR estimation states */

    float sig_est[NC+1];
    float noise_est[NC+1];

    /* channel simulation */

    float sig_pwr_av;
};

/*---------------------------------------------------------------------------*\

                              FUNCTION PROTOTYPES

\*---------------------------------------------------------------------------*/

void bits_to_dqpsk_symbols(COMP tx_symbols[], int Nc, COMP prev_tx_symbols[], int tx_bits[], int *pilot_bit, int old_qpsk_mapping);
void tx_filter(COMP tx_baseband[NC+1][M_FAC], int Nc, COMP tx_symbols[], COMP tx_filter_memory[NC+1][NSYM]);
void fdm_upconvert(COMP tx_fdm[], int Nc, COMP tx_baseband[NC+1][M_FAC], COMP phase_tx[], COMP freq_tx[],
                   COMP *fbb_phase, COMP fbb_rect);
void tx_filter_and_upconvert(COMP tx_fdm[], int Nc, COMP tx_symbols[],
                             COMP tx_filter_memory[NC+1][NSYM],
                             COMP phase_tx[], COMP freq[], COMP *fbb_phase, COMP fbb_rect);
void generate_pilot_fdm(COMP *pilot_fdm, int *bit, float *symbol, float *filter_mem, COMP *phase, COMP *freq);
void generate_pilot_lut(COMP pilot_lut[], COMP *pilot_freq);
float rx_est_freq_offset(struct FDMDV *f, COMP rx_fdm[], int nin, int do_fft);
void lpf_peak_pick(float *foff, float *max, COMP pilot_baseband[], COMP pilot_lpf[], codec2_fft_cfg fft_pilot_cfg, COMP S[], int nin, int do_fft);
void fdm_downconvert(COMP rx_baseband[NC+1][M_FAC+M_FAC/P], int Nc, COMP rx_fdm[], COMP phase_rx[], COMP freq[], int nin);
void rxdec_filter(COMP rx_fdm_filter[], COMP rx_fdm[], COMP rxdec_lpf_mem[], int nin);
void rx_filter(COMP rx_filt[NC+1][P+1], int Nc, COMP rx_baseband[NC+1][M_FAC+M_FAC/P], COMP rx_filter_memory[NC+1][NFILTER], int nin);
void down_convert_and_rx_filter(COMP rx_filt[NC+1][P+1], int Nc, COMP rx_fdm[],
                                COMP rx_fdm_mem[], COMP phase_rx[], COMP freq[],
                                float freq_pol[], int nin, int dec_rate);
float rx_est_timing(COMP  rx_symbols[], int Nc,
		    COMP  rx_filt[NC+1][P+1],
		    COMP  rx_filter_mem_timing[NC+1][NT*P],
		    float env[],
		    int   nin,
                    int   m);
float qpsk_to_bits(int rx_bits[], int *sync_bit, int Nc, COMP phase_difference[], COMP prev_rx_symbols[], COMP rx_symbols[], int old_qpsk_mapping);
void snr_update(float sig_est[], float noise_est[], int Nc, COMP phase_difference[]);
int freq_state(int *reliable_sync_bit, int sync_bit, int *state, int *timer, int *sync_mem);
float calc_snr(int Nc, float sig_est[], float noise_est[]);

} // FreeDV

#endif
