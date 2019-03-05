/*---------------------------------------------------------------------------*\

  FILE........: cohpsk_internal.h
  AUTHOR......: David Rowe
  DATE CREATED: March 2015

  Functions that implement a coherent PSK FDM modem.

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2015 David Rowe

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

#ifndef __COHPSK_INTERNAL__
#define __COHPSK_INTERNAL__

#define NCT_SYMB_BUF      (2*NSYMROWPILOT+2)
#define ND                2                           /* diversity factor ND 1 is no diveristy, ND we have orginal plus
                                                         one copy */
#define NSW               4                           /* number of sync window frames */
#define COHPSK_ND         2                           /* diversity factor   */
#define COHPSK_M          100                         /* oversampling rate */
#define COHPSK_NSYM       6
#define COHPSK_NFILTER    (COHPSK_NSYM*COHPSK_M)
#define COHPSK_EXCESS_BW  0.5                         /* excess BW factor of root nyq filter */
#define COHPSK_NT         5                           /* number of symbols we estimate timing over */

#include "fdmdv_internal.h"
#include "kiss_fft.h"

namespace FreeDV
{

struct COHPSK {
    COMP         ch_fdm_frame_buf[NSW*NSYMROWPILOT*COHPSK_M];  /* buffer of several frames of symbols from channel      */
    float        pilot2[2*NPILOTSFRAME][COHPSK_NC];
    float        phi_[NSYMROWPILOT][COHPSK_NC*ND];      /* phase estimates for this frame of rx data symbols     */
    float        amp_[NSYMROW][COHPSK_NC*ND];           /* amplitude estimates for this frame of rx data symbols */
    COMP         rx_symb[NSYMROWPILOT][COHPSK_NC*ND];   /* demodulated symbols                                   */
    float        f_est;
    COMP         rx_filter_memory[COHPSK_NC*ND][COHPSK_NFILTER];
    COMP         ct_symb_buf[NCT_SYMB_BUF][COHPSK_NC*ND];
    int          ct;                                    /* coarse timing offset in symbols                       */
    float        rx_timing;                             /* fine timing for last symbol in frame                  */
    int          nin;                                   /* number of samples to input for next symbol            */
    float        f_fine_est;
    COMP         ff_rect;
    COMP         ff_phase;
    COMP         ct_symb_ff_buf[NSYMROWPILOT+2][COHPSK_NC*ND];
    int          sync;
    int          sync_timer;

    int          frame;
    float        ratio;

    float        sig_rms;
    float        noise_rms;

    struct FDMDV *fdmdv;

    int           verbose;

    int          *ptest_bits_coh_tx;
    int          *ptest_bits_coh_rx[2];
    int          *ptest_bits_coh_end;

    /* counting bit errors using pilots */

    int           npilotbits;
    int           npilotbiterrors;

    /* optional log variables used for testing Octave to C port */

    COMP          *rx_baseband_log;
    int            rx_baseband_log_col_index;
    int            rx_baseband_log_col_sz;

    COMP          *rx_filt_log;
    int            rx_filt_log_col_index;
    int            rx_filt_log_col_sz;

    COMP          *ch_symb_log;
    int            ch_symb_log_r;
    int            ch_symb_log_col_sz;

    float         *rx_timing_log;
    int            rx_timing_log_index;

    /* demodulated bits before diversity combination for test/instrumentation purposes */

    float          rx_bits_lower[COHPSK_BITS_PER_FRAME];
    float          rx_bits_upper[COHPSK_BITS_PER_FRAME];

    /* tx amplitude weights for each carrier for test/instrumentation */

    float          carrier_ampl[COHPSK_NC*ND];

    /* Flag enabling simple freq est mode */
    int            freq_est_mode_reduced;
};

void bits_to_qpsk_symbols(COMP tx_symb[][COHPSK_NC*COHPSK_ND], int tx_bits[], int nbits);
void qpsk_symbols_to_bits(struct COHPSK *coh, float rx_bits[], COMP ct_symb_buf[][COHPSK_NC*COHPSK_ND]);
void tx_filter_and_upconvert_coh(COMP tx_fdm[], int Nc, const COMP tx_symbols[],
                                 COMP tx_filter_memory[COHPSK_NC][COHPSK_NSYM],
                                 COMP phase_tx[], COMP freq[],
                                 COMP *fbb_phase, COMP fbb_rect);
void fdm_downconvert_coh(COMP rx_baseband[COHPSK_NC][COHPSK_M+COHPSK_M/P], int Nc, COMP rx_fdm[], COMP phase_rx[], COMP freq[], int nin);
void rx_filter_coh(COMP rx_filt[COHPSK_NC+1][P+1], int Nc, COMP rx_baseband[COHPSK_NC+1][COHPSK_M+COHPSK_M/P], COMP rx_filter_memory[COHPSK_NC+1][COHPSK_NFILTER], int nin);
void frame_sync_fine_freq_est(struct COHPSK *coh, COMP ch_symb[][COHPSK_NC*COHPSK_ND], int sync, int *next_sync);
void fine_freq_correct(struct COHPSK *coh, int sync, int next_sync);
int sync_state_machine(struct COHPSK *coh, int sync, int next_sync);
int cohpsk_fs_offset(COMP out[], COMP in[], int n, float sample_rate_ppm);

} // FreeDV

#endif
