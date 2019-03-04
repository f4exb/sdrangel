/*---------------------------------------------------------------------------*\

  FILE........: codec2_ofdm.h
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: June 2017

  External user references to the modem library.

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

#ifndef CODEC2_OFDM_H
#define CODEC2_OFDM_H

/* Includes */

#include <complex.h>
#include <stdbool.h>
#include <stdint.h>

#include "codec2/comp.h"
#include "modem_stats.h"

/* Defines */

#define OFDM_AMP_SCALE (2E5*1.1491/1.06)   /* use to scale to 16 bit short */
#define OFDM_CLIP (32767*0.35)             /* experimentally derived constant to reduce PAPR to about 8dB */

namespace FreeDV
{

struct OFDM_CONFIG;
struct OFDM;

typedef enum {
    search,
    trial,
    synced
} State;

typedef enum {
    unsync,             /* force sync state machine to lose sync, and search for new sync */
    autosync,           /* falls out of sync automatically */
    manualsync          /* fall out of sync only under operator control */
} Sync;

/* create and destroy modem states */

struct OFDM *ofdm_create(const struct OFDM_CONFIG * config);
void ofdm_destroy(struct OFDM *);

/* signal processing */

void ofdm_mod(struct OFDM *, COMP *, const int *);
void ofdm_demod(struct OFDM *, int *, COMP *);
void ofdm_demod_shorts(struct OFDM *, int *, short *, float);
int  ofdm_sync_search(struct OFDM *, COMP *);
int  ofdm_sync_search_shorts(struct OFDM *, short *, float);
void ofdm_sync_state_machine(struct OFDM *, uint8_t *);

/* getters */

struct OFDM_CONFIG *ofdm_get_config_param(void);
int ofdm_get_nin(struct OFDM *);
int ofdm_get_samples_per_frame(void);
int ofdm_get_max_samples_per_frame(void);
int ofdm_get_bits_per_frame(void);
void ofdm_get_demod_stats(struct OFDM *ofdm, struct MODEM_STATS *stats);

/* option setters */

void ofdm_set_verbose(struct OFDM *, int);
void ofdm_set_timing_enable(struct OFDM *, bool);
void ofdm_set_foff_est_enable(struct OFDM *, bool);
void ofdm_set_phase_est_enable(struct OFDM *, bool);
void ofdm_set_off_est_hz(struct OFDM *, float);
void ofdm_set_sync(struct OFDM *, Sync);
void ofdm_set_tx_bpf(struct OFDM *, bool);

void ofdm_print_info(struct OFDM *);

} // FreeDV

#endif

