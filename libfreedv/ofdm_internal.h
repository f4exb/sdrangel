/*---------------------------------------------------------------------------*\

  FILE........: ofdm_internal.h
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: June 2017

  OFDM Internal definitions.

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2017 David Rowe

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

#ifndef OFDM_INTERNAL_H
#define OFDM_INTERNAL_H

#include <complex.h>
#include <stdbool.h>
#include <stdint.h>

#include "codec2_ofdm.h"
#include "freedv_filter.h"
#include "fdv_arm_math.h"

#ifndef M_PI
#define M_PI        3.14159265358979323846f
#endif

#define TAU         (2.0f * M_PI)
#define ROT45       (M_PI / 4.0f)

#define cmplx(value) (std::complex<float>{cosf(value), sinf(value)})
#define cmplxconj(value) (std::complex<float>{cosf(value), -sinf(value)})

namespace FreeDV
{

/*
 * Contains user configuration for OFDM modem
 */

struct OFDM_CONFIG {
    float tx_centre; /* TX Centre Audio Frequency */
    float rx_centre; /* RX Centre Audio Frequency */
    float fs;  /* Sample Frequency */
    float rs;  /* Modulation Symbol Rate */
    float ts;  /* symbol duration */
    float tcp; /* Cyclic Prefix duration */
    float ofdm_timing_mx_thresh;

    int nc;  /* Number of carriers */
    int ns;  /* Number of Symbol frames */
    int bps;   /* Bits per Symbol */
    int txtbits; /* number of auxiliary data bits */
    int ftwindowwidth;
};

struct OFDM {
    std::complex<float> *pilot_samples;
    std::complex<float> *rxbuf;
    std::complex<float> *pilots;
    std::complex<float> **rx_sym;
    std::complex<float> *rx_np;

    float *rx_amp;
    float *aphase_est_pilot_log;

    uint8_t *tx_uw;

    State sync_state;
    State last_sync_state;
    State sync_state_interleaver;
    State last_sync_state_interleaver;

    Sync sync_mode;

    struct quisk_cfFilter *ofdm_tx_bpf;

    std::complex<float> foff_metric;

    float foff_est_gain;
    float foff_est_hz;
    float timing_mx;
    float coarse_foff_est_hz;
    float timing_norm;
    float sig_var;
    float noise_var;
    float mean_amp;

    int clock_offset_counter;
    int verbose;
    int sample_point;
    int timing_est;
    int timing_valid;
    int nin;
    int uw_errors;
    int sync_counter;
    int frame_count;

    int frame_count_interleaver;

    bool sync_start;
    bool sync_end;
    bool timing_en;
    bool foff_est_en;
    bool phase_est_en;
    bool tx_bpf_en;
};

/* function headers exposed for LDPC work */

std::complex<float> qpsk_mod(int *);
void qpsk_demod(std::complex<float>, int *);
void ofdm_txframe(struct OFDM *, std::complex<float> *, std::complex<float> []);
void ofdm_assemble_modem_frame(struct OFDM *, uint8_t [], uint8_t [], uint8_t []);
void ofdm_assemble_modem_frame_symbols(std::complex<float> [], COMP [], uint8_t []);
void ofdm_disassemble_modem_frame(struct OFDM *, uint8_t [], COMP [], float [], short []);
void ofdm_rand(uint16_t [], int);
void ofdm_generate_payload_data_bits(uint8_t payload_data_bits[], int data_bits_per_frame);

} // FreeDV

#endif
