/*---------------------------------------------------------------------------*\

  FILE........: interldpc.c
  AUTHOR......: David Rowe
  DATE CREATED: April 2018

  Helper functions for interleaved LDPC waveforms.

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2018 David Rowe

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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "interldpc.h"
#include "codec2_ofdm.h"
#include "mpdecode_core.h"
#include "gp_interleaver.h"
#include "HRA_112_112.h"

namespace FreeDV
{

/* CRC type function, used to compare QPSK vectors when debugging */

COMP test_acc(COMP v[], int n) {
    COMP acc = {0.0f, 0.0f};
    int i;

    for (i = 0; i < n; i++) {
        acc.real += roundf(v[i].real);
        acc.imag += roundf(v[i].imag);
    }

    return acc;
}

void printf_n(COMP v[], int n) {
    int i;

    for (i = 0; i < n; i++) {
        fprintf(stderr, "%d %10f %10f\n", i, round(v[i].real), round(v[i].imag));
    }
}

void set_up_hra_112_112(struct LDPC *ldpc, struct OFDM_CONFIG *config) {
    ldpc->max_iter = HRA_112_112_MAX_ITER;
    ldpc->dec_type = 0;
    ldpc->q_scale_factor = 1;
    ldpc->r_scale_factor = 1;
    ldpc->CodeLength = HRA_112_112_CODELENGTH;
    ldpc->NumberParityBits = HRA_112_112_NUMBERPARITYBITS;
    ldpc->NumberRowsHcols = HRA_112_112_NUMBERROWSHCOLS;
    ldpc->max_row_weight = HRA_112_112_MAX_ROW_WEIGHT;
    ldpc->max_col_weight = HRA_112_112_MAX_COL_WEIGHT;
    ldpc->H_rows = (uint16_t *) HRA_112_112_H_rows;
    ldpc->H_cols = (uint16_t *) HRA_112_112_H_cols;

    /* provided for convenience and to match Octave vaiable names */

    ldpc->data_bits_per_frame = HRA_112_112_CODELENGTH - HRA_112_112_NUMBERPARITYBITS;
    ldpc->coded_bits_per_frame = HRA_112_112_CODELENGTH;
    ldpc->coded_syms_per_frame = ldpc->coded_bits_per_frame / config->bps;
}

void ldpc_encode_frame(struct LDPC *ldpc, int codeword[], unsigned char tx_bits_char[]) {
    unsigned char *pbits = new unsigned char[ldpc->NumberParityBits];
    int i, j;

    encode(ldpc, tx_bits_char, pbits);

    for (i = 0; i < ldpc->data_bits_per_frame; i++) {
        codeword[i] = tx_bits_char[i];
    }

    for (j = 0; i < ldpc->coded_bits_per_frame; i++, j++) {
        codeword[i] = pbits[j];
    }

    delete[] pbits;
}

void qpsk_modulate_frame(COMP tx_symbols[], int codeword[], int n) {
    int s, i;
    int dibit[2];
    std::complex<float> qpsk_symb;

    for (s = 0, i = 0; i < n; s += 2, i++) {
        dibit[0] = codeword[s + 1] & 0x1;
        dibit[1] = codeword[s] & 0x1;
        qpsk_symb = qpsk_mod(dibit);
        tx_symbols[i].real = std::real(qpsk_symb);
        tx_symbols[i].imag = std::imag(qpsk_symb);
    }
}

void interleaver_sync_state_machine(struct OFDM *ofdm,
        struct LDPC *ldpc,
        struct OFDM_CONFIG *config,
        COMP codeword_symbols_de[],
        float codeword_amps_de[],
        float EsNo, int interleave_frames,
        int *iter, int *parityCheckCount, int *Nerrs_coded)
{
    (void) config;
    int coded_syms_per_frame = ldpc->coded_syms_per_frame;
    int coded_bits_per_frame = ldpc->coded_bits_per_frame;
    int data_bits_per_frame = ldpc->data_bits_per_frame;
    float *llr = new float[coded_bits_per_frame];
    uint8_t *out_char = new uint8_t[coded_bits_per_frame];
    State next_sync_state_interleaver;

    next_sync_state_interleaver = ofdm->sync_state_interleaver;

    if ((ofdm->sync_state_interleaver == search) && (ofdm->frame_count >= (interleave_frames - 1))) {
        symbols_to_llrs(llr, codeword_symbols_de, codeword_amps_de, EsNo, ofdm->mean_amp, coded_syms_per_frame);
        iter[0] = run_ldpc_decoder(ldpc, out_char, llr, parityCheckCount);
        Nerrs_coded[0] = data_bits_per_frame - parityCheckCount[0];

        if ((Nerrs_coded[0] == 0) || (interleave_frames == 1)) {
            /* sucessful decode! */
            next_sync_state_interleaver = synced;
            ofdm->frame_count_interleaver = interleave_frames;
        }
    }

    ofdm->sync_state_interleaver = next_sync_state_interleaver;
    delete[] out_char;
    delete[] llr;
}

/* measure uncoded (raw) bit errors over interleaver frame, note we
   don't include txt bits as this is done after we dissassemmble the
   frame */

int count_uncoded_errors(struct LDPC *ldpc, struct OFDM_CONFIG *config, int Nerrs_raw[], int interleave_frames, COMP codeword_symbols_de[]) {
    int i, j, Nerrs, Terrs;

    int coded_syms_per_frame = ldpc->coded_syms_per_frame;
    int coded_bits_per_frame = ldpc->coded_bits_per_frame;
    int data_bits_per_frame = ldpc->data_bits_per_frame;
    int *rx_bits_raw = new int[coded_bits_per_frame];

    /* generate test codeword from known payload data bits */

    int *test_codeword = new int[coded_bits_per_frame];
    uint16_t *r = new uint16_t[data_bits_per_frame];
    uint8_t *tx_bits = new uint8_t[data_bits_per_frame];

    ofdm_rand(r, data_bits_per_frame);

    for (i = 0; i < data_bits_per_frame; i++) {
        tx_bits[i] = r[i] > 16384;
    }

    ldpc_encode_frame(ldpc, test_codeword, tx_bits);

    Terrs = 0;
    for (j = 0; j < interleave_frames; j++) {
        for (i = 0; i < coded_syms_per_frame; i++) {
            int bits[2];
            std::complex<float> s = std::complex<float>{codeword_symbols_de[j * coded_syms_per_frame + i].real, codeword_symbols_de[j * coded_syms_per_frame + i].imag};
            qpsk_demod(s, bits);
            rx_bits_raw[config->bps * i] = bits[1];
            rx_bits_raw[config->bps * i + 1] = bits[0];
        }

        Nerrs = 0;

        for (i = 0; i < coded_bits_per_frame; i++) {
            if (test_codeword[i] != rx_bits_raw[i]) {
                Nerrs++;
            }
        }

        Nerrs_raw[j] = Nerrs;
        Terrs += Nerrs;
    }

    delete[] tx_bits;
    delete[] r;
    delete[] test_codeword;
    delete[] rx_bits_raw;

    return Terrs;
}

int count_errors(uint8_t tx_bits[], uint8_t rx_bits[], int n) {
    int i;
    int Nerrs = 0;

    for (i = 0; i < n; i++) {
        if (tx_bits[i] != rx_bits[i]) {
            Nerrs++;
        }
    }

    return Nerrs;
}

/*
   Given an array of tx_bits, LDPC encodes, interleaves, and OFDM
   modulates.

   Note this could be refactored to save memory, e.g. for embedded
   applications we could call ofdm_txframe on a frame by frame
   basis
 */

void ofdm_ldpc_interleave_tx(struct OFDM *ofdm, struct LDPC *ldpc, std::complex<float> tx_sams[], uint8_t tx_bits[], uint8_t txt_bits[], int interleave_frames, struct OFDM_CONFIG *config) {
    int coded_syms_per_frame = ldpc->coded_syms_per_frame;
    int coded_bits_per_frame = ldpc->coded_bits_per_frame;
    int data_bits_per_frame = ldpc->data_bits_per_frame;
    int ofdm_bitsperframe = ofdm_get_bits_per_frame();
    int *codeword = new int[coded_bits_per_frame];
    COMP *coded_symbols = new COMP[interleave_frames * coded_syms_per_frame];
    COMP *coded_symbols_inter = new COMP[interleave_frames * coded_syms_per_frame];
    int Nsamperframe = ofdm_get_samples_per_frame();
    std::complex<float> *tx_symbols = new std::complex<float>[ofdm_bitsperframe / config->bps];
    int j;

    for (j = 0; j < interleave_frames; j++) {
        ldpc_encode_frame(ldpc, codeword, &tx_bits[j * data_bits_per_frame]);
        qpsk_modulate_frame(&coded_symbols[j * coded_syms_per_frame], codeword, coded_syms_per_frame);
    }

    gp_interleave_comp(coded_symbols_inter, coded_symbols, interleave_frames * coded_syms_per_frame);

    for (j = 0; j < interleave_frames; j++) {
        ofdm_assemble_modem_frame_symbols(tx_symbols, &coded_symbols_inter[j * coded_syms_per_frame], &txt_bits[config->txtbits * j]);
        ofdm_txframe(ofdm, &tx_sams[j * Nsamperframe], tx_symbols);
    }

    delete[] tx_symbols;
    delete[] coded_symbols_inter;
    delete[] coded_symbols;
    delete[] codeword;
}

} // FreeDV
