// This file is part of LeanSDR Copyright (C) 2016-2019 <pabr@pabr.org>.
// See the toplevel README for more information.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef LEANSDR_DVBS2_H
#define LEANSDR_DVBS2_H

/*
#include "leansdr/bch.h"
#include "leansdr/crc.h"
#include "leansdr/dvb.h"
#include "leansdr/ldpc.h"
#include "leansdr/sdr.h"
#include "leansdr/softword.h"
*/

#include "bch.h"

#include "crc.h"

#include "dvb.h"
#include "softword.h"
#include "ldpc.h"
#include "sdr.h"

namespace leansdr
{

// S2 THRESHOLDS (for comparing demodulators)

static const int S2_MAX_ERR_SOF_INITIAL = 1; // 26 bits
static const int S2_MAX_ERR_SOF = 13;        // 26 bits
static const int S2_MAX_ERR_PLSCODE = 8;     // 64 bits, dmin=32
static const int S2_MAX_ERR_PILOT = 10;      // 36 bits

static const int pilot_length = 36;

// S2 SOF
// EN 302 307-1 section 5.5.2.1 SOF field

template <typename T>
struct s2_sof
{
    static const uint32_t VALUE = 0x18d2e82;
    static const uint32_t MASK = 0x3ffffff;
    static const int LENGTH = 26;
    complex<T> symbols[LENGTH];
    s2_sof()
    {
        for (int s = 0; s < LENGTH; ++s)
        {
            int angle = ((VALUE >> (LENGTH - 1 - s)) & 1) * 2 + (s & 1); // pi/2-BPSK
            symbols[s].re = cstln_amp * cosf(M_PI / 4 + 2 * M_PI * angle / 4);
            symbols[s].im = cstln_amp * sinf(M_PI / 4 + 2 * M_PI * angle / 4);
        }
    }
}; // s2_sof

// S2 PLS CODES
// Precomputes the PLS code sequences.
// EN 302 307-1 section 5.5.2.4 PLS code

template <typename T>
struct s2_plscodes
{
    // PLS index format MODCOD[4:0]|SHORTFRAME|PILOTS
    static const int COUNT = 128;
    static const int LENGTH = 64;
    uint64_t codewords[COUNT];
    complex<T> symbols[COUNT][LENGTH];
    s2_plscodes()
    {
        uint32_t G[6] = {0x55555555,
                         0x33333333,
                         0x0f0f0f0f,
                         0x00ff00ff,
                         0x0000ffff,
                         0xffffffff};
        for (int index = 0; index < COUNT; ++index)
        {
            uint32_t y = 0;
            for (int row = 0; row < 6; ++row)
                if ((index >> (6 - row)) & 1)
                    y ^= G[row];
            uint64_t code = 0;
            for (int bit = 31; bit >= 0; --bit)
            {
                int yi = (y >> bit) & 1;
                if (index & 1)
                    code = (code << 2) | (yi << 1) | (yi ^ 1);
                else
                    code = (code << 2) | (yi << 1) | yi;
            }
            // Scrambling
            code ^= SCRAMBLING;
            // Store precomputed codeword.
            codewords[index] = code;
            // Also store as symbols.
            for (int i = 0; i < LENGTH; ++i)
            {
                int yi = (code >> (LENGTH - 1 - i)) & 1;
                int nyi = yi ^ (i & 1);
                symbols[index][i].re = cstln_amp * (1 - 2 * nyi) / sqrtf(2);
                symbols[index][i].im = cstln_amp * (1 - 2 * yi) / sqrtf(2);
            }
        }
    }
    static const uint64_t SCRAMBLING = 0x719d83c953422dfa;
}; // s2_plscodes

// S2 SCRAMBLING
// Precomputes the symbol rotations for PL scrambling.
// EN 302 307-1 section 5.5.4 Physical layer scrambling

struct s2_scrambling
{
    uint8_t Rn[131072]; // 0..3  (* 2pi/4)
    s2_scrambling(int codenum = 0)
    {
        uint32_t stx = 0x00001, sty = 0x3ffff;
        // x starts at codenum, wraps at index 2^18-1 by design
        for (int i = 0; i < codenum; ++i)
            stx = lfsr_x(stx);
        // First half of sequence is LSB of scrambling angle
        for (int i = 0; i < 131072; ++i)
        {
            int zn = (stx ^ sty) & 1;
            Rn[i] = zn;
            stx = lfsr_x(stx);
            sty = lfsr_y(sty);
        }
        // Second half is MSB
        for (int i = 0; i < 131072; ++i)
        {
            int zn = (stx ^ sty) & 1;
            Rn[i] |= zn << 1;
            stx = lfsr_x(stx);
            sty = lfsr_y(sty);
        }
    }
    uint32_t lfsr_x(uint32_t X)
    {
        int bit = ((X >> 7) ^ X) & 1;
        return ((bit << 18) | X) >> 1;
    }
    uint32_t lfsr_y(uint32_t Y)
    {
        int bit = ((Y >> 10) ^ (Y >> 7) ^ (Y >> 5) ^ Y) & 1;
        return ((bit << 18) | Y) >> 1;
    }
}; // s2_scrambling

// S2 BBSCRAMBLING
// Precomputes the xor pattern for baseband scrambling.
// EN 302 307-1 section 5.2.2 BB scrambling

struct s2_bbscrambling
{
    s2_bbscrambling()
    {
        uint16_t st = 0x00a9; // 000 0000 1010 1001  (Fig 5 reversed)
        for (int i = 0; i < sizeof(pattern); ++i)
        {
            uint8_t out = 0;
            for (int n = 8; n--;)
            {
                int bit = ((st >> 13) ^ (st >> 14)) & 1; // Taps
                out = (out << 1) | bit;                  // MSB first
                st = (st << 1) | bit;                    // Feedback
            }
            pattern[i] = out;
        }
    }
    void transform(const uint8_t *in, int bbsize, uint8_t *out)
    {
        for (int i = 0; i < bbsize; ++i)
            out[i] = in[i] ^ pattern[i];
    }

  private:
    uint8_t pattern[58192]; // Values 0..3
};                          // s2_bbscrambling

// S2 PHYSICAL LAYER SIGNALLING

struct s2_pls
{
    int modcod; // 0..31
    bool sf;
    bool pilots;
    int framebits() const { return sf ? 16200 : 64800; }
};

template <typename SOFTSYMB>
struct plslot
{
    static const int LENGTH = 90;
    bool is_pls;
    union {
        s2_pls pls;
        SOFTSYMB symbols[LENGTH];
    };
};

// EN 302 307-1 section 5.5.2.2 MODCOD field
// EN 302 307-1 section 6 Error performance

const struct modcod_info
{
    static const int MAX_SLOTS_PER_FRAME = 360;
    static const int MAX_SYMBOLS_PER_FRAME =
        (1 + MAX_SLOTS_PER_FRAME) * plslot<uint8_t>::LENGTH +
        ((MAX_SLOTS_PER_FRAME - 1) / 16) * pilot_length;
    int nslots_nf; // Number of 90-symbol slots per normal frame
    int nsymbols;  // Symbols in the constellation
    cstln_base::predef c;
    code_rate rate;
    // Ideal Es/N0 for normal frames
    // EN 302 307 section 6 Error performance
    float esn0_nf;
    // Radii for APSK
    // EN 302 307, section 5.4.3, Table 9
    // EN 302 307, section 5.4.4, Table 10
    float g1, g2, g3;
} modcod_infos[32] = {
    {
        0,
    },
    // 1 - 11
    {360, 4, cstln_base::QPSK, FEC14, -2.35},
    {360, 4, cstln_base::QPSK, FEC13, -1.24},
    {360, 4, cstln_base::QPSK, FEC25, -0.30},
    {360, 4, cstln_base::QPSK, FEC12, 1.00},
    {360, 4, cstln_base::QPSK, FEC35, 2.23},
    {360, 4, cstln_base::QPSK, FEC23, 3.10},
    {360, 4, cstln_base::QPSK, FEC34, 4.03},
    {360, 4, cstln_base::QPSK, FEC45, 4.68},
    {360, 4, cstln_base::QPSK, FEC56, 5.18},
    {360, 4, cstln_base::QPSK, FEC89, 6.20},
    {360, 4, cstln_base::QPSK, FEC910, 6.42},
    // 12 - 17
    {240, 8, cstln_base::PSK8, FEC35, 5.50},
    {240, 8, cstln_base::PSK8, FEC23, 6.62},
    {240, 8, cstln_base::PSK8, FEC34, 7.91},
    {240, 8, cstln_base::PSK8, FEC56, 9.35},
    {240, 8, cstln_base::PSK8, FEC89, 10.69},
    {240, 8, cstln_base::PSK8, FEC910, 10.98},
    // 18 - 23
    {180, 16, cstln_base::APSK16, FEC23, 8.97, 3.15},
    {180, 16, cstln_base::APSK16, FEC34, 10.21, 2.85},
    {180, 16, cstln_base::APSK16, FEC45, 11.03, 2.75},
    {180, 16, cstln_base::APSK16, FEC56, 11.61, 2.70},
    {180, 16, cstln_base::APSK16, FEC89, 12.89, 2.60},
    {180, 16, cstln_base::APSK16, FEC910, 13.13, 2.57},
    // 24 - 28
    {144, 32, cstln_base::APSK32, FEC34, 12.73, 2.84, 5.27},
    {144, 32, cstln_base::APSK32, FEC45, 13.64, 2.72, 4.87},
    {144, 32, cstln_base::APSK32, FEC56, 14.28, 2.64, 4.64},
    {144, 32, cstln_base::APSK32, FEC89, 15.69, 2.54, 4.33},
    {144, 32, cstln_base::APSK32, FEC910, 16.05, 2.53, 4.30},
    // 29 - 31
    {
        0,
    },
    {
        0,
    },
    {
        0,
    }};

// Assert that a MODCOD number is valid
const modcod_info *check_modcod(int m)
{
    if (m < 0 || m > 31)
        fail("Invalid MODCOD number");
    const modcod_info *r = &modcod_infos[m];
    if (!r->nslots_nf)
        fail("Unsupported MODCOD");
    return r;
}

// S2 FRAME TRANSMITTER

template <typename T>
struct s2_frame_transmitter : runnable
{
    s2_frame_transmitter(scheduler *sch,
                         pipebuf<plslot<hard_ss>> &_in,
                         pipebuf<complex<T>> &_out)
        : runnable(sch, "S2 frame transmitter"),
          in(_in), out(_out, modcod_info::MAX_SYMBOLS_PER_FRAME)
    {
        float amp = cstln_amp / sqrtf(2);
        qsymbols[0].re = +amp;
        qsymbols[0].im = +amp;
        qsymbols[1].re = +amp;
        qsymbols[1].im = -amp;
        qsymbols[2].re = -amp;
        qsymbols[2].im = +amp;
        qsymbols[3].re = -amp;
        qsymbols[3].im = -amp;
    }
    void run()
    {
        while (in.readable() >= 1)
        {
            plslot<hard_ss> *pin = in.rd();
            if (!pin->is_pls)
                fail("Expected PLS pseudo-slot");
            s2_pls *pls = &pin->pls;
            const modcod_info *mcinfo = check_modcod(pls->modcod);
            int nslots = (pls->sf ? mcinfo->nslots_nf / 4 : mcinfo->nslots_nf);
            if (in.readable() < 1 + nslots)
                break;
            // Require room for BBHEADER + slots + optional pilots.
            int nsymbols = ((1 + nslots) * plslot<hard_ss>::LENGTH +
                            (pls->pilots ? ((nslots - 1) / 16) * pilot_length : 0));
            if (out.writable() < nsymbols)
                break;
            update_cstln(mcinfo);
            int nw = run_frame(pls, mcinfo, pin + 1, nslots, out.wr());
            if (nw != nsymbols)
                fail("Bug: s2_frame_transmitter overflow");
            in.read(1 + nslots);
            out.written(nsymbols);
        }
    }
    int run_frame(s2_pls *pls, const modcod_info *mcinfo,
                  const plslot<hard_ss> *pin, int nslots,
                  complex<T> *pout)
    {
        complex<T> *pout0 = pout; // For sanity check
        // PLHEADER: SOF AND PLSCODE
        // EN 302 307-1 section 5.5.2 PL signalling
        memcpy(pout, sof.symbols, sof.LENGTH * sizeof(*pout));
        pout += sof.LENGTH;
        int pls_index = (pls->modcod << 2) | (pls->sf << 1) | pls->pilots;
        memcpy(pout, plscodes.symbols[pls_index], plscodes.LENGTH * sizeof(*pout));
        pout += plscodes.LENGTH;
        // Slots and pilots
        int till_next_pilot = pls->pilots ? 16 : nslots;
        uint8_t *scr = &scrambling.Rn[0];
        for (int S = 0; S < nslots; ++S, ++pin, --till_next_pilot)
        {
            if (till_next_pilot == 0)
            {
                // Send pilot
                for (int s = 0; s < pilot_length; ++s, ++scr, ++pout)
                    scramble(&qsymbols[0], *scr, pout);
                till_next_pilot = 16;
            }
            // Send slot
            if (pin->is_pls)
                fail("s2_frame_transmitter: bad input sequence");
            const hard_ss *ps = pin->symbols;
            for (int s = 0; s < pin->LENGTH; ++s, ++ps, ++scr, ++pout)
                scramble(&csymbols[*ps], *scr, pout);
        }
        return pout - pout0;
    }
    inline void scramble(const complex<T> *src, uint8_t r, complex<T> *dst)
    {
        switch (r)
        {
        case 3:
            dst->re = src->im;
            dst->im = -src->re;
            break;
        case 2:
            dst->re = -src->re;
            dst->im = -src->im;
            break;
        case 1:
            dst->re = -src->im;
            dst->im = src->re;
            break;
        default:
            *dst = *src;
        }
    }

  private:
    pipereader<plslot<hard_ss>> in;
    pipewriter<complex<T>> out;
    cstln_lut<hard_ss, 256> *cstln; // NULL initially
    complex<T> *csymbols;           // Valid iff cstln is valid. RMS cstln_amp.
    void update_cstln(const modcod_info *mcinfo)
    {
        if (!cstln || cstln->nsymbols != mcinfo->nsymbols)
        {
            if (cstln)
            {
                fprintf(stderr, "Warning: Variable MODCOD is inefficient\n");
                delete cstln;
                delete csymbols;
            }
            if (sch->debug)
                fprintf(stderr, "Building constellation %d\n", mcinfo->nsymbols);
            // TBD Different Es/N0 for short frames ?
            cstln = new cstln_lut<hard_ss, 256>(mcinfo->c, mcinfo->esn0_nf,
                                                mcinfo->g1, mcinfo->g2, mcinfo->g3);
            csymbols = new complex<T>[cstln->nsymbols];
            for (int s = 0; s < cstln->nsymbols; ++s)
            {
                csymbols[s].re = cstln->symbols[s].re;
                csymbols[s].im = cstln->symbols[s].im;
            }
        }
    }
    complex<T> qsymbols[4]; // RMS cstln_amp
    s2_sof<T> sof;
    s2_plscodes<T> plscodes;
    s2_scrambling scrambling;
}; // s2_frame_transmitter

// S2 FRAME RECEIVER

static int pl_errors = 0, pl_symbols = 0;

#define TEST_DIVERSITY 0

template <typename T, typename SOFTSYMB>
struct s2_frame_receiver : runnable
{
    sampler_interface<T> *sampler;
    int meas_decimation;
    float Ftune; // Tuning bias in cycles per symbol
    float Fm;    // Baud rate in Hz, for debug messages only. TBD remove.
    bool strongpls;
    static const int MAX_SYMBOLS_PER_FRAME =
        (1 + modcod_info::MAX_SLOTS_PER_FRAME) * plslot<hard_ss>::LENGTH +
        ((modcod_info::MAX_SLOTS_PER_FRAME - 1) / 16) * pilot_length;
    s2_frame_receiver(scheduler *sch,
                      sampler_interface<T> *_sampler,
                      pipebuf<complex<T>> &_in,
                      pipebuf<plslot<SOFTSYMB>> &_out,
                      pipebuf<float> *_freq_out = NULL,
                      pipebuf<float> *_ss_out = NULL,
                      pipebuf<float> *_mer_out = NULL,
                      pipebuf<complex<float>> *_cstln_out = NULL,
                      pipebuf<complex<float>> *_cstln_pls_out = NULL,
                      pipebuf<complex<float>> *_symbols_out = NULL,
                      pipebuf<int> *_state_out = NULL)
        : runnable(sch, "S2 frame receiver"),
          sampler(_sampler),
          meas_decimation(1048576),
          Ftune(0), Fm(0),
          strongpls(false),
          in_power(0), ev_power(0), agc_gain(1), agc_bw(1e-3),
          nsyncs(0),
          cstln(NULL),
          in(_in), out(_out, 1 + modcod_info::MAX_SLOTS_PER_FRAME),
          meas_count(0),
          freq_out(opt_writer(_freq_out)),
          ss_out(opt_writer(_ss_out)),
          mer_out(opt_writer(_mer_out)),
          cstln_out(opt_writer(_cstln_out, 1024)),
          cstln_pls_out(opt_writer(_cstln_pls_out, 1024)),
          symbols_out(opt_writer(_symbols_out, MAX_SYMBOLS_PER_FRAME)),
          state_out(opt_writer(_state_out)),
          report_state(false),
          scrambling(0),
          m_modcodType(-1),
          m_modcodRate(-1)
    {
        // Constellation for PLS
        qpsk = new cstln_lut<SOFTSYMB, 256>(cstln_base::QPSK);
        add_syncs(qpsk);

        init_coarse_freq();

#if TEST_DIVERSITY
        fprintf(stderr, "** DEBUG: Diversity test mode (slower)\n");
#endif
    }

    enum
    {
        COARSE_FREQ,
        FRAME_SEARCH,
        FRAME_LOCKED,
    } state;

    float min_freqw16, max_freqw16;

    // State during COARSE_FREQ
    complex<float> diffcorr;
    int coarse_count;

    // State during FRAME_SEARCH and FRAME_LOCKED
    float freqw16; // Carrier frequency initialized by COARSE_FREQ
    float phase16; // Estimated phase of carrier at next symbol

    float mu;    // Time to next symbol, in samples
    float omega; // Samples per symbol

    void run()
    {
        // Require enough samples to detect one plheader,
        // TBD margin ?
        int min_samples = (1 + MAX_SYMBOLS_PER_FRAME +
                           plslot<SOFTSYMB>::LENGTH) *
                          omega * 2;
        while (in.readable() >= min_samples + sampler->readahead() &&
               out.writable() >= 1 + modcod_info::MAX_SLOTS_PER_FRAME &&
               opt_writable(freq_out, 1) &&
               opt_writable(ss_out, 1) &&
               opt_writable(mer_out, 1) &&
               opt_writable(symbols_out, MAX_SYMBOLS_PER_FRAME) &&
               opt_writable(state_out, 1))
        {
            if (report_state)
            {
                // Report unlocked state on first invocation.
                opt_write(state_out, 0);
                report_state = false;
            }
            switch (state)
            {
            case COARSE_FREQ:
                run_frame_coarse();
                break;
            case FRAME_SEARCH:
                run_frame_search();
                break;
            case FRAME_LOCKED:
                run_frame_locked();
                break;
            }
        }
    }

    // Initial state
    void init_coarse_freq()
    {
        diffcorr = 0;
        coarse_count = 0;
        memset(hist, 0, sizeof(hist));
        state = COARSE_FREQ;
    }

    // State transtion
    void enter_coarse_freq()
    {
        opt_write(state_out, 0);
        init_coarse_freq();
    }

    void run_frame_coarse()
    {
        freqw16 = 65536 * Ftune;
        min_freqw16 = freqw16 - 65536.0 / 9;
        max_freqw16 = freqw16 + 65536.0 / 9;

        complex<T> *pin = in.rd();
        complex<T> p = *pin++;
        int nsamples = MAX_SYMBOLS_PER_FRAME * omega;
        for (int s = nsamples; s--; ++pin)
        {
            complex<T> n = *pin;
            diffcorr.re += p.re * n.re + p.im * n.im;
            diffcorr.im += p.re * n.im - p.im * n.re;
            p = n;
        }
        in.read(nsamples);
        ++coarse_count;
        if (coarse_count == 50)
        {
            float freqw = atan2f(diffcorr.im, diffcorr.re) * omega;
            fprintf(stderr, "COARSE(%d): %f rad/symb (%.0f Hz at %.0f baud)\n",
                    coarse_count, freqw, freqw * Fm / (2 * M_PI), Fm);
#if 0
	freqw16 = freqw * 65536 / (2*M_PI);
#else
            fprintf(stderr, "Ignoring coarse det, using %f\n", freqw16 * Fm / 65536);
#endif
            enter_frame_search();
        }
    }

    // State transtion
    void enter_frame_search()
    {
        opt_write(state_out, 0);
        mu = 0;
        phase16 = 0;
        if (sch->debug)
            fprintf(stderr, "ACQ\n");
        state = FRAME_SEARCH;
    }

    void run_frame_search()
    {
        complex<float> *psampled;
        if (cstln_out && cstln_out->writable() >= 1024)
            psampled = cstln_out->wr();
        else
            psampled = NULL;

        // Preserve float precision
        phase16 -= 65536 * floor(phase16 / 65536);

        int nsymbols = MAX_SYMBOLS_PER_FRAME; // TBD Adjust after PLS decoding

        sampler_state ss = {in.rd(), mu, phase16, freqw16};
        sampler->update_freq(ss.fw16 / omega);

        if (!in_power)
            init_agc(ss.p, 64);
        update_agc();

        for (int s = 0; s < nsymbols; ++s)
        {
            complex<float> p0 = interp_next(&ss);
            track_agc(p0);
            complex<float> p = p0 * agc_gain;

            // Constellation plot
            if (psampled && s < 1024)
                *psampled++ = p;

            // Demodulate everything as QPSK.
            // Occasionally it locks onto 8PSK at offet 2pi/16.
            uint8_t symb = track_symbol(&ss, p, qpsk, 1);

            // Feed symbol into all synchronizers.
            for (sync *ps = syncs; ps < syncs + nsyncs; ++ps)
            {
                ps->hist = (ps->hist << 1) | ((ps->tobpsk >> symb) & 1);
                int errors = hamming_weight((ps->hist & sof.MASK) ^ sof.VALUE);
                if (errors <= S2_MAX_ERR_SOF_INITIAL)
                {
                    if (sch->debug2)
                        fprintf(stderr, "Found SOF+%d at %d offset %f\n",
                                errors, s, ps->offset16);
                    ss.ph16 += ps->offset16;
                    in.read(ss.p - in.rd());
                    mu = ss.mu;
                    phase16 = ss.ph16;
                    freqw16 = ss.fw16;
                    if (psampled)
                        cstln_out->written(psampled - cstln_out->wr());
                    enter_frame_locked();
                    return;
                }
            }
            ss.normalize();
        }

        // Write back sampler progress
        in.read(ss.p - in.rd());
        mu = ss.mu;
        phase16 = ss.ph16;
        freqw16 = ss.fw16;
        if (psampled)
            cstln_out->written(psampled - cstln_out->wr());
    }

    // State transtion
    void enter_frame_locked()
    {
        opt_write(state_out, 1);
        if (sch->debug)
            fprintf(stderr, "LOCKED\n");
        state = FRAME_LOCKED;
    }

    // Note: Starts after SOF

    struct sampler_state
    {
        complex<T> *p; // Pointer to samples
        float mu;      // Time of next symbol, counted from p
        float ph16;    // Carrier phase at next symbol, cycles*65536
        float fw16;    // Carrier frequency, cycles per symbol * 65536
        uint8_t *scr;  // Position in scrambling sequeence
        void skip_symbols(int ns, float omega)
        {
            mu += omega * ns;
            ph16 += fw16 * ns;
            scr += ns;
        }
        void normalize()
        {
            ph16 = fmodf(ph16, 65536.0f); // Rounding direction irrelevant
        }
    };

#define xfprintf(...) \
    {                 \
    }
    //#define xfprintf fprintf

    void run_frame_locked()
    {
        complex<float> *psampled;
        if (cstln_out && cstln_out->writable() >= 1024)
            psampled = cstln_out->wr();
        else
            psampled = NULL;
        complex<float> *psampled_pls;
        if (cstln_pls_out && cstln_pls_out->writable() >= 1024)
            psampled_pls = cstln_pls_out->wr();
        else
            psampled_pls = NULL;

#if TEST_DIVERSITY
        complex<float> *psymbols = symbols_out ? symbols_out->wr() : NULL;
        float scale_symbols = 1.0 / cstln_amp;
#endif

        xfprintf(stderr, "lock0step fw= %f (%.0f Hz) mu=%f\n",
                 freqw16, freqw16 * Fm / 65536, mu);

        sampler_state ss = {in.rd(), mu, phase16, freqw16, scrambling.Rn};
        sampler->update_freq(ss.fw16 / omega);

        update_agc();

        // Read PLSCODE

        uint64_t plscode = 0;
        complex<float> pls_symbols[s2_plscodes<T>::LENGTH];
        for (int s = 0; s < plscodes.LENGTH; ++s)
        {
            complex<float> p = interp_next(&ss) * agc_gain;
#if TEST_DIVERSITY
            if (psymbols)
                *psymbols++ = p * scale_symbols;
#endif
            pls_symbols[s] = p;
            if (psampled_pls)
                *psampled_pls++ = p;
            int bit = (p.im < 1); // TBD suboptimal
            plscode = (plscode << 1) | bit;
        }
        int pls_index = -1;
        int pls_errors = S2_MAX_ERR_PLSCODE + 1; // dmin=32
        // TBD: Optimiser
        for (int i = 0; i < plscodes.COUNT; ++i)
        {
            int e = hamming_weight(plscode ^ plscodes.codewords[i]);
            if (e < pls_errors)
            {
                pls_errors = e;
                pls_index = i;
            }
        }
        if (pls_index < 0)
        {
            if (sch->debug2)
                fprintf(stderr, "Too many errors in plheader (%d)\n", pls_errors);
            in.read(ss.p - in.rd());
            enter_frame_search();
            return;
        }

        // Adjust phase with PLS
        complex<float> pls_corr = conjprod(plscodes.symbols[pls_index],
                                           pls_symbols, plscodes.LENGTH);
        ss.normalize();
        align_phase(&ss, pls_corr);

        s2_pls pls;
        pls.modcod = pls_index >> 2; // Guaranteed 0..31
        pls.sf = pls_index & 2;
        pls.pilots = pls_index & 1;
        xfprintf(stderr, "PLS: modcod %d, short=%d, pilots=%d (%d errors)\n",
                 pls.modcod, pls.sf, pls.pilots, pls_errors);
        const modcod_info *mcinfo = &modcod_infos[pls.modcod];
        if (!mcinfo->nslots_nf)
        {
            fprintf(stderr, "Unsupported or corrupted MODCOD\n");
            in.read(ss.p - in.rd());
            enter_frame_search();
            return;
        }
#if 1 // TBD use fec_infos
        if (pls.sf && mcinfo->rate == FEC910)
        {
            fprintf(stderr, "Unsupported or corrupted FEC\n");
            in.read(ss.p - in.rd());
            enter_frame_search();
            return;
        }
#endif
        // Store current MODCOD info
        if (mcinfo->c != m_modcodType) {
            m_modcodType = mcinfo->c;
        }
        if (mcinfo->rate != m_modcodRate) {
            m_modcodRate = mcinfo->rate;
        }

        // TBD Comparison of nsymbols is insufficient for DVB-S2X.
        if (!cstln || cstln->nsymbols != mcinfo->nsymbols)
        {
            if (cstln)
            {
                fprintf(stderr, "Warning: Variable MODCOD is inefficient\n");
                delete cstln;
            }
            fprintf(stderr, "Creating LUT for %s ratecode %d\n",
                    cstln_base::names[mcinfo->c], mcinfo->rate);
            cstln = new cstln_lut<SOFTSYMB, 256>(mcinfo->c, mcinfo->esn0_nf,
                                                 mcinfo->g1, mcinfo->g2, mcinfo->g3);
            cstln->m_rateCode = (int) mcinfo->rate;
            cstln->m_typeCode = (int) mcinfo->c;
            cstln->m_setByModcod = true;
#if 0
	fprintf(stderr, "Dumping constellation LUT to stdout.\n");
	cstln->dump(stdout);
#endif
        }

        int S = pls.sf ? mcinfo->nslots_nf / 4 : mcinfo->nslots_nf;

        plslot<SOFTSYMB> *pout = out.wr(), *pout0 = pout;

        // Output special slot with PLS information
        pout->is_pls = true;
        pout->pls = pls;
        ++pout;

        // Read slots and pilots

        int pilot_errors = 0;

        // Slots to skip until next PL slot (pilot or sof)
        int till_next_pls = pls.pilots ? 16 : S;

        for (int leansdr_slots = S; leansdr_slots--; ++pout, --till_next_pls)
        {
            if (till_next_pls == 0)
            {
                // Read pilot
                int errors = 0;
                complex<float> corr = 0;
                for (int s = 0; s < pilot_length; ++s)
                {
                    complex<float> p0 = interp_next(&ss);
                    track_agc(p0);
                    complex<float> p = p0 * agc_gain;
#if TEST_DIVERSITY
                    if (psymbols)
                        *psymbols++ = p * scale_symbols;
#endif
                    (void)track_symbol(&ss, p, qpsk, 1);
                    if (psampled_pls)
                        *psampled_pls++ = p;
                    complex<float> d = descramble(&ss, p);
                    if (d.im < 0 || d.re < 0)
                        ++errors;
                    corr.re += d.re + d.im;
                    corr.im += d.im - d.re;
                }
                if (errors > S2_MAX_ERR_PILOT)
                {
                    if (sch->debug2)
                        fprintf(stderr, "Too many errors in pilot (%d/36)\n", errors);
                    in.read(ss.p - in.rd());
                    enter_frame_search();
                    return;
                }
                pilot_errors += errors;
                ss.normalize();
                align_phase(&ss, corr);
                till_next_pls = 16;
            }
            // Read slot
            pout->is_pls = false;
            complex<float> p; // Export last symbols for cstln_out
            for (int s = 0; s < pout->LENGTH; ++s)
            {
                p = interp_next(&ss) * agc_gain;
#if TEST_DIVERSITY
                if (psymbols)
                    *psymbols++ = p * scale_symbols;
#endif
#if 1 || TEST_DIVERSITY
                (void)track_symbol(&ss, p, cstln, 0); // SLOW
#endif
                complex<float> d = descramble(&ss, p);
#if 0 // Slow
	  SOFTSYMB *symb = &cstln->lookup(d.re, d.im)->ss;
#else // Avoid scaling floats. May wrap at very low SNR.
                SOFTSYMB *symb = &cstln->lookup((int)d.re, (int)d.im)->ss;
#endif
                pout->symbols[s] = *symb;
            }
            if (psampled)
                *psampled++ = p;
        } // slots

        // Read SOF

        memset(hist, 0, sizeof(hist));
        complex<float> sof_corr = 0;
        uint32_t sofbits = 0;
        for (int s = 0; s < sof.LENGTH; ++s)
        {
            complex<float> p0 = interp_next(&ss);
            track_agc(p0);
            complex<float> p = p0 * agc_gain;
#if TEST_DIVERSITY
            if (psymbols)
                *psymbols++ = p * scale_symbols;
#endif
            uint8_t symb = track_symbol(&ss, p, qpsk, 1);
            if (psampled_pls)
                *psampled_pls++ = p;
            int bit = (p.im < 0); // suboptimal
            sofbits = (sofbits << 1) | bit;
            sof_corr += conjprod(sof.symbols[s], p);
        }
        int sof_errors = hamming_weight(sofbits ^ sof.VALUE);
        if (sof_errors >= S2_MAX_ERR_SOF)
        {
            if (sch->debug2)
                fprintf(stderr, "Too many errors in SOF (%d/26)\n", sof_errors);
            in.read(ss.p - in.rd());
            enter_coarse_freq();
            return;
        }
        ss.normalize();
        align_phase(&ss, sof_corr);

        // Commit whole frame after final SOF.
        out.written(pout - pout0);

        // Write back sampler progress
        meas_count += ss.p - in.rd();
        in.read(ss.p - in.rd());
        mu = ss.mu;
        phase16 = ss.ph16;
        freqw16 = ss.fw16;

        // Measurements
        if (psampled)
            cstln_out->written(psampled - cstln_out->wr());
        if (psampled_pls)
            cstln_pls_out->written(psampled_pls - cstln_pls_out->wr());
#if TEST_DIVERSITY
        if (psymbols)
            symbols_out->written(psymbols - symbols_out->wr());
#endif
        if (meas_count >= meas_decimation)
        {
            opt_write(freq_out, freqw16 / 65536 / omega);
            opt_write(ss_out, in_power);
            // TBD Adjust if cfg.strongpls
            float mer = ev_power ? (float)cstln_amp * cstln_amp / ev_power : 1;
            opt_write(mer_out, 10 * logf(mer) / logf(10));
            meas_count -= meas_decimation;
        }

        int all_errors = pls_errors + pilot_errors + sof_errors;
        int max_errors = plscodes.LENGTH + sof.LENGTH;
        if (pls.pilots)
            max_errors += ((S - 1) / 16) * pilot_length;

        xfprintf(stderr, "success   fw= %f (%.0f Hz) mu= %f "
                         "errors=%d/64+%d+%d/26 = %2d/%d\n",
                 freqw16, freqw16 * Fm / 65536, mu,
                 pls_errors, pilot_errors, sof_errors, all_errors, max_errors);
        pl_errors += all_errors;
        pl_symbols += max_errors;
    }

    void shutdown()
    {
        fprintf(stderr, "PL SER: %f ppm\n", pl_errors / (pl_symbols + 1e-6) * 1e6);
    }

    void init_agc(const complex<T> *buf, int n)
    {
        in_power = 0;
        for (int i = 0; i < n; ++i)
            in_power += cnorm2(buf[i]);
        in_power /= n;
    }
    void track_agc(const complex<float> &p)
    {
        float in_p = p.re * p.re + p.im * p.im;
        in_power = in_p * agc_bw + in_power * (1.0f - agc_bw);
    }

    void update_agc()
    {
        float in_amp = gen_sqrt(in_power);
        if (!in_amp)
            return;
        if (!strongpls || !cstln)
        {
            // Match RMS amplitude
            agc_gain = cstln_amp / in_amp;
        }
        else
        {
            // Match peak amplitude
            agc_gain = cstln_amp / cstln->amp_max / in_amp;
        }
    }

    complex<float> descramble(sampler_state *ss, const complex<float> &p)
    {
        int r = *ss->scr++;
        complex<float> res;
        switch (r)
        {
        case 3:
            res.re = -p.im;
            res.im = p.re;
            break;
        case 2:
            res.re = -p.re;
            res.im = -p.im;
            break;
        case 1:
            res.re = p.im;
            res.im = -p.re;
            break;
        default:
            res = p;
        }
        return res;
    }

    // Interpolator

    inline complex<float> interp_next(sampler_state *ss)
    {
        // Skip to next sample
        while (ss->mu >= 1)
        {
            ++ss->p;
            ss->mu -= 1.0f;
        }
        // Interpolate
#if 0
      // Interpolate linearly then derotate.
      // This will fail with large carrier offsets (e.g. --tune).
      float cmu = 1.0f - ss->mu;
      complex<float> s(ss->p[0].re*cmu + ss->p[1].re*ss->mu,
		       ss->p[0].im*cmu + ss->p[1].im*ss->mu);
      ss->mu += omega;
      // Derotate
      const complex<float> &rot = trig.expi(-ss->ph16);
      ss->ph16 += ss->fw16;
      return rot * s;
#else
        // Use generic interpolator
        complex<float> s = sampler->interp(ss->p, ss->mu, ss->ph16);
        ss->mu += omega;
        ss->ph16 += ss->fw16;
        return s;
#endif
    }

    void align_phase(sampler_state *ss, const complex<float> &c)
    {
#if 0
      // Reference implementation
      float err = atan2f(c.im,c.re) * (65536/(2*M_PI));
#else
        // Same performance as atan2f, faster
        if (!c.re)
            return;
        float err = c.im / c.re * (65536 / (2 * M_PI));
#endif
        ss->ph16 += err;
    }

    inline uint8_t track_symbol(sampler_state *ss, const complex<float> &p,
                                cstln_lut<SOFTSYMB, 256> *c, int mode)
    {
        static struct
        {
            float kph, kfw, kmu;
        } gains[2] = {
            {4e-2, 1e-4, (float) 0.001 / (cstln_amp * cstln_amp)},
            {4e-2, 1e-4, (float) 0.001 / (cstln_amp * cstln_amp)}};
        // Decision
        typename cstln_lut<SOFTSYMB, 256>::result *cr = c->lookup(p.re, p.im);
        // Carrier tracking
        ss->ph16 += cr->phase_error * gains[mode].kph;
        ss->fw16 += cr->phase_error * gains[mode].kfw;
        if (ss->fw16 < min_freqw16)
            ss->fw16 = min_freqw16;
        if (ss->fw16 > max_freqw16)
            ss->fw16 = max_freqw16;
        // Phase tracking
        hist[2] = hist[1];
        hist[1] = hist[0];
        hist[0].p = p;
        complex<int8_t> *cp = &c->symbols[cr->symbol];
        hist[0].c.re = cp->re;
        hist[0].c.im = cp->im;
        float muerr =
            ((hist[0].p.re - hist[2].p.re) * hist[1].c.re +
             (hist[0].p.im - hist[2].p.im) * hist[1].c.im) -
            ((hist[0].c.re - hist[2].c.re) * hist[1].p.re +
             (hist[0].c.im - hist[2].c.im) * hist[1].p.im);
        float mucorr = muerr * gains[mode].kmu;
        const float max_mucorr = 0.1;
        // TBD Optimize out statically
        if (mucorr < -max_mucorr)
            mucorr = -max_mucorr;
        if (mucorr > max_mucorr)
            mucorr = max_mucorr;
        ss->mu += mucorr;
        // Error vector for MER
        complex<float> ev(p.re - cp->re, p.im - cp->im);
        float ev_p = ev.re * ev.re + ev.im * ev.im;
        ev_power = ev_p * agc_bw + ev_power * (1.0f - agc_bw);
        return cr->symbol;
    }

    struct
    {
        complex<float> p; // Received symbol
        complex<float> c; // Matched constellation point
    } hist[3];

  public:
    float in_power, ev_power;
    float agc_gain;
    float agc_bw;
    cstln_lut<SOFTSYMB, 256> *qpsk;
    static const int MAXSYNCS = 8;
    struct sync
    {
        uint16_t nsmask; // bitmask of cstln orders for which this sync is used
        uint64_t tobpsk; // Bitmask from cstln symbols to pi/2-BPSK bits
        float offset16;  // Phase offset 0..65536
        uint32_t hist;   // For SOF detection
    } syncs[MAXSYNCS], *current_sync;
    int nsyncs;
    s2_plscodes<T> plscodes;
    cstln_lut<SOFTSYMB, 256> *cstln;
    // Initialize synchronizers for an arbitrary constellation.
    void add_syncs(cstln_lut<SOFTSYMB, 256> *c)
    {
        int random_decision = 0;
        int nrot = c->nrotations;
#if 0
      if ( nrot == 4 ) {
	fprintf(stderr, "Special case for 8PSK locking as QPSK pi/8\n");
	nrot = 8;
      }
#endif
        for (int r = 0; r < nrot; ++r)
        {
            if (nsyncs == MAXSYNCS)
                fail("Bug: too many syncs");
            sync *s = &syncs[nsyncs++];
            s->offset16 = 65536.0 * r / nrot;
            float angle = -2 * M_PI * r / nrot;
            s->tobpsk = 0;
            for (int i = c->nsymbols; i--;)
            {
                complex<int8_t> p = c->symbols[i];
                float re = p.re * cosf(angle) - p.im * sinf(angle);
                float im = p.re * sinf(angle) + p.im * cosf(angle);
                int bit;
                if (im > 1)
                    bit = 0;
                else if (im < -1)
                    bit = 1;
                else
                {
                    bit = random_decision;
                    random_decision ^= 1;
                } // Near 0
                s->tobpsk = (s->tobpsk << 1) | bit;
            }
            s->hist = 0;
        }
    }

    trig16 trig;
    modcod_info *mcinfo;
    pipereader<complex<T>> in;
    pipewriter<plslot<SOFTSYMB>> out;
    int meas_count;
    pipewriter<float> *freq_out, *ss_out, *mer_out;
    pipewriter<complex<float>> *cstln_out;
    pipewriter<complex<float>> *cstln_pls_out;
    pipewriter<complex<float>> *symbols_out;
    pipewriter<int> *state_out;
    bool report_state;
    // S2 constants
    s2_scrambling scrambling;
    s2_sof<T> sof;
    int m_modcodType;
    int m_modcodRate;
    // Max size of one frame
    //    static const int MAX_SLOTS = 360;
    static const int MAX_SLOTS = 240; // DEBUG match test signal
    static const int MAX_SYMBOLS =
        (1 + MAX_SLOTS) * plslot<SOFTSYMB>::LENGTH + ((MAX_SLOTS - 1) / 16) * pilot_length;
}; // s2_frame_receiver

template <typename SOFTBYTE>
struct fecframe
{
    s2_pls pls;
    SOFTBYTE bytes[64800 / 8]; // Contains 16200/8 or 64800/8 bytes.
};

// S2 INTERLEAVER
// EN 302 307-1 section 5.3.3 Bit Interleaver

struct s2_interleaver : runnable
{
    s2_interleaver(scheduler *sch,
                   pipebuf<fecframe<hard_sb>> &_in,
                   pipebuf<plslot<hard_ss>> &_out)
        : runnable(sch, "S2 interleaver"),
          in(_in), out(_out, 1 + 360)
    {
    }
    void run()
    {
        while (in.readable() >= 1)
        {
            const s2_pls *pls = &in.rd()->pls;
            const modcod_info *mcinfo = check_modcod(pls->modcod);
            int nslots = pls->sf ? mcinfo->nslots_nf / 4 : mcinfo->nslots_nf;
            if (out.writable() < 1 + nslots)
                return;
            const hard_sb *pbytes = in.rd()->bytes;
            // Output pseudo slot with PLS.
            plslot<hard_ss> *ppls = out.wr();
            ppls->is_pls = true;
            ppls->pls = *pls;
            out.written(1);
            // Interleave
            plslot<hard_ss> *pout = out.wr();
            if (mcinfo->nsymbols == 4)
                serialize_qpsk(pbytes, nslots, pout);
            else
            {
                int bps = log2(mcinfo->nsymbols);
                int rows = pls->framebits() / bps;
                if (mcinfo->nsymbols == 8 && mcinfo->rate == FEC35)
                    interleave(bps, rows, pbytes, nslots, false, pout);
                else
                    interleave(bps, rows, pbytes, nslots, true, pout);
            }
            in.read(1);
            out.written(nslots);
        }
    }

  private:
    // Fill slots with serialized QPSK symbols, MSB first.
    static void serialize_qpsk(const hard_sb *pin, int nslots,
                               plslot<hard_ss> *pout)
    {
#if 0 // For reference
      hard_sb acc;
      int nacc = 0;
      for ( ; nslots; --nslots,++pout ) {
	pout->is_pls = false;
	hard_ss *ps = pout->symbols;
	for ( int ns=pout->LENGTH; ns--; ++ps ) {
	  if ( nacc < 2 ) { acc=*pin++; nacc=8; }
	  *ps = acc>>6;
	  acc <<= 2;
	  nacc -= 2;
	}
      }
      if ( nacc ) fail("Bug: s2_interleaver");
#else
        if (nslots % 2)
            fatal("Bug: Truncated byte");
        for (; nslots; nslots -= 2)
        {
            hard_sb b;
            hard_ss *ps;
            // Slot 0 (mod 2)
            pout->is_pls = false;
            ps = pout->symbols;
            for (int i = 0; i < 22; ++i)
            {
                b = *pin++;
                *ps++ = (b >> 6);
                *ps++ = (b >> 4) & 3;
                *ps++ = (b >> 2) & 3;
                *ps++ = (b)&3;
            }
            b = *pin++;
            *ps++ = (b >> 6);
            *ps++ = (b >> 4) & 3;
            // Slot 1 (mod 2)
            ++pout;
            pout->is_pls = false;
            ps = pout->symbols;
            *ps++ = (b >> 2) & 3;
            *ps++ = (b)&3;
            for (int i = 0; i < 22; ++i)
            {
                b = *pin++;
                *ps++ = (b >> 6);
                *ps++ = (b >> 4) & 3;
                *ps++ = (b >> 2) & 3;
                *ps++ = (b)&3;
            }
            ++pout;
        }
#endif
    }

    // Fill slots with interleaved symbols.
    // EN 302 307-1 figures 7 and 8
#if 0  // For reference
    static void interleave(int bps, int rows,
			   const hard_sb *pin, int nslots,
			   bool msb_first, plslot<hard_ss> *pout) {
      if ( bps==4 && rows==4050 && msb_first )
	return interleave4050(pin, nslots, pout);
      if ( rows % 8 ) fatal("modcod/framesize combination not supported\n");
      int stride = rows/8;  // Offset to next column, in bytes
      hard_sb accs[bps];    // One accumulator per column
      int nacc = 0;         // Bits in each column accumulator
      for ( ; nslots; --nslots,++pout ) {
	pout->is_pls = false;
	hard_ss *ps = pout->symbols;
	for ( int ns=pout->LENGTH; ns--; ++ps ) {
	  if ( ! nacc ) {
	    const hard_sb *pi = pin;
	    for ( int b=0; b<bps; ++b,pi+=stride ) accs[b] = *pi;
	    ++pin;
	    nacc = 8;
	  }
	  hard_ss symb = 0;
	  if ( msb_first )
	    for ( int b=0; b<bps; ++b ) {
	      symb = (symb<<1) | (accs[b]>>7);
	      accs[b] <<= 1;
	    }
	  else
	    for ( int b=bps; b--; ) {
	      symb = (symb<<1) | (accs[b]>>7);
	      accs[b] <<= 1;
	    }
	  --nacc;
	  *ps = symb;
	}
      }
      if ( nacc ) fail("Bug: s2_interleaver");
    }
#else  // reference
    static void interleave(int bps, int rows,
                           const hard_sb *pin, int nslots,
                           bool msb_first, plslot<hard_ss> *pout)
    {
        void (*func)(int rows, const hard_sb *pin, int nslots,
                     plslot<hard_ss> *pout) = 0;
        if (msb_first)
            switch (bps)
            {
            case 2:
                func = interleave<1, 2>;
                break;
            case 3:
                func = interleave<1, 3>;
                break;
            case 4:
                func = interleave<1, 4>;
                break;
            case 5:
                func = interleave<1, 5>;
                break;
            default:
                fail("Bad bps");
            }
        else
            switch (bps)
            {
            case 2:
                func = interleave<0, 2>;
                break;
            case 3:
                func = interleave<0, 3>;
                break;
            case 4:
                func = interleave<0, 4>;
                break;
            case 5:
                func = interleave<0, 5>;
                break;
            default:
                fail("Bad bps");
            }
        (*func)(rows, pin, nslots, pout);
    }
    template <int MSB_FIRST, int BPS>
    static void interleave(int rows, const hard_sb *pin, int nslots,
                           plslot<hard_ss> *pout)
    {
        if (BPS == 4 && rows == 4050 && MSB_FIRST)
            return interleave4050(pin, nslots, pout);
        if (rows % 8)
            fatal("modcod/framesize combination not supported\n");
        int stride = rows / 8; // Offset to next column, in bytes
        if (nslots % 4)
            fatal("Bug: Truncated byte");
        // plslot::symbols[] are not packed across slots,
        // so we need tos split bytes at boundaries.
        for (; nslots; nslots -= 4)
        {
            const hard_sb *pi;
            hard_sb accs[BPS]; // One accumulator per column
            hard_ss *ps;
            // Slot 0 (mod 4): 88+2
            pout->is_pls = false;
            ps = pout->symbols;
            for (int i = 0; i < 11; ++i)
            {
                split_byte<BPS>(pin++, stride, accs);
                pop_symbols<MSB_FIRST, BPS>(accs, &ps, 8);
            }
            split_byte<BPS>(pin++, stride, accs);
            pop_symbols<MSB_FIRST, BPS>(accs, &ps, 2);
            ++pout;
            // Slot 1 (mod 4): 6+80+4
            pout->is_pls = false;
            ps = pout->symbols;
            pop_symbols<MSB_FIRST, BPS>(accs, &ps, 6);
            for (int i = 0; i < 10; ++i)
            {
                split_byte<BPS>(pin++, stride, accs);
                pop_symbols<MSB_FIRST, BPS>(accs, &ps, 8);
            }
            split_byte<BPS>(pin++, stride, accs);
            pop_symbols<MSB_FIRST, BPS>(accs, &ps, 4);
            ++pout;
            // Slot 2 (mod 4): 4+80+6
            pout->is_pls = false;
            ps = pout->symbols;
            pop_symbols<MSB_FIRST, BPS>(accs, &ps, 4);
            for (int i = 0; i < 10; ++i)
            {
                split_byte<BPS>(pin++, stride, accs);
                pop_symbols<MSB_FIRST, BPS>(accs, &ps, 8);
            }
            split_byte<BPS>(pin++, stride, accs);
            pop_symbols<MSB_FIRST, BPS>(accs, &ps, 6);
            ++pout;
            // Slot 3 (mod 4): 2+88
            pout->is_pls = false;
            ps = pout->symbols;
            pop_symbols<MSB_FIRST, BPS>(accs, &ps, 2);
            for (int i = 0; i < 11; ++i)
            {
                split_byte<BPS>(pin++, stride, accs);
                pop_symbols<MSB_FIRST, BPS>(accs, &ps, 8);
            }
            ++pout;
        }
    }
    template <int BPS>
    static inline void split_byte(const hard_sb *pi, int stride,
                                  hard_sb accs[BPS])
    {
        // TBD Pass stride as template parameter.
        for (int b = 0; b < BPS; ++b, pi += stride)
            accs[b] = *pi;
    }
    template <int MSB_FIRST, int BPS>
    static void pop_symbols(hard_sb accs[BPS], hard_ss **ps, int ns)
    {
        for (int i = 0; i < ns; ++i)
        {
            hard_ss symb = 0;
            // Check unrolling and constant propagation.
            for (int b = 0; b < BPS; ++b)
                if (MSB_FIRST)
                    symb = (symb << 1) | (accs[b] >> 7);
                else
                    symb = (symb << 1) | (accs[BPS - 1 - b] >> 7);
            for (int b = 0; b < BPS; ++b)
                accs[b] <<= 1;
            *(*ps)++ = symb;
        }
    }
#endif // reference

    // Special case for 16APSK short frames.
    // 4050 rows is not a multiple of 8.
    static void interleave4050(const hard_sb *pin, int nslots,
                               plslot<hard_ss> *pout)
    {
        const hard_sb *pin0 = pin;
        int rows = 4050;
        hard_sb accs[4]; // One accumulator per column
        int nacc = 0;    // Bits in each column accumulator
        for (; nslots; --nslots, ++pout)
        {
            pout->is_pls = false;
            hard_ss *ps = pout->symbols;
            for (int ns = pout->LENGTH; ns--; ++ps)
            {
                if (!nacc)
                {
                    if (nslots == 1 && ns == 1)
                    {
                        // Special case just to avoid reading beyond end of buffer
                        accs[0] = pin[0];
                        accs[1] = (pin[506] << 2) | (pin[507] >> 6);
                        accs[2] = (pin[1012] << 4) | (pin[1013] >> 4);
                        accs[3] = (pin[1518] << 6);
                    }
                    else
                    {
                        accs[0] = pin[0];
                        accs[1] = (pin[506] << 2) | (pin[507] >> 6);
                        accs[2] = (pin[1012] << 4) | (pin[1013] >> 4);
                        accs[3] = (pin[1518] << 6) | (pin[1519] >> 2);
                    }
                    ++pin;
                    nacc = 8;
                }
                hard_ss symb = 0;
                for (int b = 0; b < 4; ++b)
                {
                    symb = (symb << 1) | (accs[b] >> 7);
                    accs[b] <<= 1;
                }
                --nacc;
                *ps = symb;
            }
        }
    }

    pipereader<fecframe<hard_sb>> in;
    pipewriter<plslot<hard_ss>> out;
}; // s2_interleaver

// S2 DEINTERLEAVER
// EN 302 307-1 section 5.3.3 Bit Interleaver

template <typename SOFTSYMB, typename SOFTBYTE>
struct s2_deinterleaver : runnable
{
    s2_deinterleaver(scheduler *sch,
                     pipebuf<plslot<SOFTSYMB>> &_in,
                     pipebuf<fecframe<SOFTBYTE>> &_out)
        : runnable(sch, "S2 deinterleaver"),
          in(_in), out(_out)
    {
    }
    void run()
    {
        while (in.readable() >= 1 && out.writable() >= 1)
        {
            plslot<SOFTSYMB> *pin = in.rd();
            if (!pin->is_pls)
                fail("s2_deinterleaver: bad input sequence");
            s2_pls *pls = &pin->pls;
            const modcod_info *mcinfo = check_modcod(pls->modcod);
            int nslots = pls->sf ? mcinfo->nslots_nf / 4 : mcinfo->nslots_nf;
            if (in.readable() < 1 + nslots)
                return;
            fecframe<SOFTBYTE> *pout = out.wr();
            pout->pls = *pls;
            SOFTBYTE *pbytes = pout->bytes;
            if (mcinfo->nsymbols == 4)
                deserialize_qpsk(pin + 1, nslots, pbytes);
            else
            {
                int bps = log2(mcinfo->nsymbols);
                int rows = pls->framebits() / bps;
                if (mcinfo->nsymbols == 8 && mcinfo->rate == FEC35)
                    deinterleave(bps, rows, pin + 1, nslots, false, pbytes);
                else
                    deinterleave(bps, rows, pin + 1, nslots, true, pbytes);
            }
            in.read(1 + nslots);
            out.written(1);
        }
    }

  private:
    // Deserialize slots of QPSK symbols, MSB first.
    static void deserialize_qpsk(plslot<SOFTSYMB> *pin, int nslots,
                                 SOFTBYTE *pout)
    {
        SOFTBYTE acc;
        softword_clear(&acc); // gcc warning
        int nacc = 0;
        for (; nslots; --nslots, ++pin)
        {
            SOFTSYMB *ps = pin->symbols;
            for (int ns = pin->LENGTH; ns--; ++ps)
            {
                pack_qpsk_symbol(*ps, &acc, nacc);
                nacc += 2;
                if (nacc == 8)
                { // TBD unroll
                    *pout++ = acc;
                    nacc = 0;
                }
            }
        }
    }

    // Deinterleave slots of symbols.
    // EN 302 307-1 figures 7 and 8
#if 0  // For reference
    static void deinterleave(int bps, int rows,
			     const plslot<SOFTSYMB> *pin, int nslots,
			     bool msb_first, SOFTBYTE *pout) {
      if ( bps==4 && rows==4050 && msb_first )
	return deinterleave4050(pin, nslots, pout);
      if ( rows % 8 ) fatal("modcod/framesize combination not supported\n");
      int stride = rows/8;  // Offset to next column, in bytes
      SOFTBYTE accs[bps];
      for ( int b=0; b<bps; ++b ) softword_clear(&accs[b]);  // gcc warning
      int nacc = 0;
      for ( ; nslots; --nslots,++pin ) {
	const SOFTSYMB *ps = pin->symbols;
	for ( int ns=pin->LENGTH; ns--; ++ps ) {
	  split_symbol(*ps, bps, accs, nacc, msb_first);
	  ++nacc;
	  if ( nacc == 8 ) {
	    SOFTBYTE *po = pout;
	    for ( int b=0; b<bps; ++b,po+=stride ) *po = accs[b];
	    ++pout;
	    nacc = 0;
	  }
	}
      }
      if ( nacc ) fail("Bug: s2_deinterleaver");
    }
#else  // reference
    static void deinterleave(int bps, int rows,
                             const plslot<SOFTSYMB> *pin, int nslots,
                             bool msb_first, SOFTBYTE *pout)
    {
        void (*func)(int rows, const plslot<SOFTSYMB> *pin, int nslots,
                     SOFTBYTE *pout) = 0;
        if (msb_first)
            switch (bps)
            {
            case 2:
                func = deinterleave<1, 2>;
                break;
            case 3:
                func = deinterleave<1, 3>;
                break;
            case 4:
                func = deinterleave<1, 4>;
                break;
            case 5:
                func = deinterleave<1, 5>;
                break;
            default:
                fail("Bad bps");
            }
        else
            switch (bps)
            {
            case 2:
                func = deinterleave<0, 2>;
                break;
            case 3:
                func = deinterleave<0, 3>;
                break;
            case 4:
                func = deinterleave<0, 4>;
                break;
            case 5:
                func = deinterleave<0, 5>;
                break;
            default:
                fail("Bad bps");
            }
        (*func)(rows, pin, nslots, pout);
    }

    template <int MSB_FIRST, int BPS>
    static void deinterleave(int rows, const plslot<SOFTSYMB> *pin, int nslots,
                             SOFTBYTE *pout)
    {
        if (BPS == 4 && rows == 4050 && MSB_FIRST)
            return deinterleave4050(pin, nslots, pout);
        if (rows % 8)
            fatal("modcod/framesize combination not supported\n");
        int stride = rows / 8; // Offset to next column, in bytes
        SOFTBYTE accs[BPS];
        for (int b = 0; b < BPS; ++b)
            softword_clear(&accs[b]); // gcc warning
        int nacc = 0;
        for (; nslots; --nslots, ++pin)
        {
            const SOFTSYMB *ps = pin->symbols;
            for (int ns = pin->LENGTH; ns--; ++ps)
            {
                split_symbol(*ps, BPS, accs, nacc, MSB_FIRST);
                ++nacc;
                if (nacc == 8)
                { // TBD Unroll, same as interleave()
                    SOFTBYTE *po = pout;
                    // TBD Pass stride as template parameter.
                    for (int b = 0; b < BPS; ++b, po += stride)
                        *po = accs[b];
                    ++pout;
                    nacc = 0;
                }
            }
        }
        if (nacc)
            fail("Bug: s2_deinterleaver");
    }
#endif // reference

    // Special case for 16APSK short frames.
    // 4050 rows is not a multiple of 8
    // so we process rows one at a time rather than in chunks of 8.
    static void deinterleave4050(const plslot<SOFTSYMB> *pin, int nslots,
                                 SOFTBYTE *pout)
    {
        const int rows = 4050;
        SOFTBYTE accs[4];
        for (int b = 0; b < 4; ++b)
            softword_clear(&accs[b]); // gcc warning
        int nacc = 0;
        for (; nslots; --nslots, ++pin)
        {
            const SOFTSYMB *ps = pin->symbols;
            for (int ns = pin->LENGTH; ns--; ++ps)
            {
                split_symbol(*ps, 4, accs, nacc, true);
                ++nacc;
                if (nacc == 8)
                {
                    for (int b = 0; b < 8; ++b)
                    {
                        softwords_set(pout, rows * 0 + b, softword_get(accs[0], b));
                        softwords_set(pout, rows * 1 + b, softword_get(accs[1], b));
                        softwords_set(pout, rows * 2 + b, softword_get(accs[2], b));
                        softwords_set(pout, rows * 3 + b, softword_get(accs[3], b));
                    }
                    ++pout;
                    nacc = 0;
                }
            }
        }
        if (nacc != 2)
            fatal("Bug: Expected 2 leftover rows\n");
        // Pad with random symbol so that we can use accs[].
        for (int b = nacc; b < 8; ++b)
            split_symbol(pin->symbols[0], 4, accs, b, true);
        for (int b = 0; b < nacc; ++b)
        {
            softwords_set(pout, rows * 0 + b, softword_get(accs[0], b));
            softwords_set(pout, rows * 1 + b, softword_get(accs[1], b));
            softwords_set(pout, rows * 2 + b, softword_get(accs[2], b));
            softwords_set(pout, rows * 3 + b, softword_get(accs[3], b));
        }
    }

    // Spread LLR symbol across hard columns.
    // Must call 8 times before using result because we use bit shifts.
    static inline void split_symbol(const llr_ss &ps, int bps,
                                    hard_sb accs[/*bps*/], int nacc,
                                    bool msb_first)
    {
        if (msb_first)
        {
            for (int b = 0; b < bps; ++b)
                accs[b] = (accs[b] << 1) | llr_harden(ps.bits[bps - 1 - b]);
        }
        else
        {
            for (int b = 0; b < bps; ++b)
                accs[b] = (accs[b] << 1) | llr_harden(ps.bits[b]);
        }
    }
    // Fast variant
    template <int MSB_FIRST, int BPS>
    static inline void split_symbol(const llr_ss &ps,
                                    hard_sb accs[/*bps*/], int nacc)
    {
        if (MSB_FIRST)
        {
            for (int b = 0; b < BPS; ++b)
                accs[b] = (accs[b] << 1) | llr_harden(ps.bits[BPS - 1 - b]);
        }
        else
        {
            for (int b = 0; b < BPS; ++b)
                accs[b] = (accs[b] << 1) | llr_harden(ps.bits[b]);
        }
    }

    // Spread LLR symbol across LLR columns.
    static inline void split_symbol(const llr_ss &ps, int bps,
                                    llr_sb accs[/*bps*/], int nacc,
                                    bool msb_first)
    {
        if (msb_first)
        {
            for (int b = 0; b < bps; ++b)
                accs[b].bits[nacc] = ps.bits[bps - 1 - b];
        }
        else
        {
            for (int b = 0; b < bps; ++b)
                accs[b].bits[nacc] = ps.bits[b];
        }
    }
    // Fast variant
    template <int MSB_FIRST, int BPS>
    static inline void split_symbol(const llr_ss &ps,
                                    llr_sb accs[/*bps*/], int nacc)
    {
        if (MSB_FIRST)
        {
            for (int b = 0; b < BPS; ++b)
                accs[b].bits[nacc] = ps.bits[BPS - 1 - b];
        }
        else
        {
            for (int b = 0; b < BPS; ++b)
                accs[b].bits[nacc] = ps.bits[b];
        }
    }

    // Merge QPSK LLR symbol into hard byte.
    static inline void pack_qpsk_symbol(const llr_ss &ps,
                                        hard_sb *acc, int nacc)
    {
        // TBD Must match LLR law, see softsymb_harden.
        uint8_t s = llr_harden(ps.bits[0]) | (llr_harden(ps.bits[1]) << 1);
        *acc = (*acc << 2) | s;
    }

    // Merge QPSK LLR symbol into LLR byte.
    static inline void pack_qpsk_symbol(const llr_ss &ps,
                                        llr_sb *acc, int nacc)
    {
        acc->bits[nacc] = ps.bits[1];
        acc->bits[nacc + 1] = ps.bits[0];
    }

    pipereader<plslot<SOFTSYMB>> in;
    pipewriter<fecframe<SOFTBYTE>> out;
}; // s2_deinterleaver

typedef ldpc_table<uint16_t> s2_ldpc_table;
typedef ldpc_engine<bool, hard_sb, 8, uint16_t> s2_ldpc_engine;

#include "dvbs2_data.h"

static const struct fec_info
{
    static const int KBCH_MAX = 58192;
    int Kbch;  // BCH message size (bits)
    int kldpc; // LDPC message size (= BCH codeword size) (bits)
    int t;     // BCH error correction
    const s2_ldpc_table *ldpc;
} fec_infos[2][FEC_COUNT] = {
    {
        // Normal frames - must respect enum code_rate order
        {32208, 32400, 12, &ldpc_nf_fec12},  // FEC12 (was [FEC12] = {...} and so on. Does not compile with MSVC)
        {43040, 43200, 10, &ldpc_nf_fec23},  // FEC23
        {0},                                 // FEC46
        {48408, 48600, 12, &ldpc_nf_fec34},  // FEC34
        {53840, 54000, 10, &ldpc_nf_fec56},  // FEC56
        {0},                                 // FEC78
        {51648, 51840, 12, &ldpc_nf_fec45},  // FEC45
        {57472, 57600, 8, &ldpc_nf_fec89},   // FEC89
        {58192, 58320, 8, &ldpc_nf_fec910},  // FEC910
        {16008, 16200, 12, &ldpc_nf_fec14},  // FEC14
        {21408, 21600, 12, &ldpc_nf_fec13},  // FEC13
        {25728, 25920, 12, &ldpc_nf_fec25},  // FEC25
        {38688, 38880, 12, &ldpc_nf_fec35},  // FEC35
    },
    {
        // Short frames - must respect enum code_rate order
        {7032, 7200, 12, &ldpc_sf_fec12},    // FEC12 (was [FEC12] = {...} and so on. Does not compile with MSVC)
        {10632, 10800, 12, &ldpc_sf_fec23},  // FEC23
        {},                                  // FEC46
        {11712, 11880, 12, &ldpc_sf_fec34},  // FEC34
        {13152, 13320, 12, &ldpc_sf_fec56},  // FEC56
        {},                                  // FEC78
        {12432, 12600, 12, &ldpc_sf_fec45},  // FEC45
        {14232, 14400, 12, &ldpc_sf_fec89},  // FEC89
        {},                                  // FEC910
        {3072, 3240, 12, &ldpc_sf_fec14},    // FEC14
        {5232, 5400, 12, &ldpc_sf_fec13},    // FEC13
        {6312, 6480, 12, &ldpc_sf_fec25},    // FEC25
        {9552, 9720, 12, &ldpc_sf_fec35},    // FEC35
    },
};

struct bbframe
{
    s2_pls pls;
    uint8_t bytes[58192 / 8]; // Kbch/8 max
};

// S2_LDPC_ENGINES
// Initializes LDPC engines for all DVB-S2 FEC settings.

template <typename SOFTBIT, typename SOFTBYTE>
struct s2_ldpc_engines
{
    typedef ldpc_engine<SOFTBIT, SOFTBYTE, 8, uint16_t> s2_ldpc_engine;
    s2_ldpc_engine *ldpcs[2][FEC_COUNT]; // [shortframes][fec]
    s2_ldpc_engines()
    {
        memset(ldpcs, 0, sizeof(ldpcs));
        for (int sf = 0; sf <= 1; ++sf)
        {
            for (int fec = 0; fec < FEC_COUNT; ++fec)
            {
                const fec_info *fi = &fec_infos[sf][fec];
                if (!fi->ldpc)
                {
                    ldpcs[sf][fec] = NULL;
                }
                else
                {
                    int n = (sf ? 64800 / 4 : 64800);
                    int k = fi->kldpc;
                    ldpcs[sf][fec] = new s2_ldpc_engine(fi->ldpc, k, n);
                }
            }
        }
    }
    void print_node_stats()
    {
        for (int sf = 0; sf <= 1; ++sf)
            for (int fec = 0; fec < FEC_COUNT; ++fec)
            {
                s2_ldpc_engine *ldpc = ldpcs[sf][fec];
                if (ldpc)
                    ldpc->print_node_stats();
            }
    }
}; // s2_ldpc_engines

// S2_BCH_ENGINES
// Initializes BCH engines for all DVB-S2 FEC settings.

struct s2_bch_engines
{
    bch_interface *bchs[2][FEC_COUNT];
    // N=t*m
    // The generator of GF(2^m) is always g1.
    // Normal frames with 8, 10 or 12 polynomials.
    typedef bch_engine<uint32_t, 192, 17, 16, uint16_t, 0x002d> s2_bch_engine_nf12;
    typedef bch_engine<uint32_t, 160, 17, 16, uint16_t, 0x002d> s2_bch_engine_nf10;
    typedef bch_engine<uint32_t, 128, 17, 16, uint16_t, 0x002d> s2_bch_engine_nf8;
    // Short frames with 12 polynomials.
    typedef bch_engine<uint32_t, 168, 17, 14, uint16_t, 0x002b> s2_bch_engine_sf12;
    s2_bch_engines()
    {
        bitvect<uint32_t, 17> bch_polys[2][12]; // [shortframes][polyindex]
        // EN 302 307-1 5.3.1 Table 6a (polynomials for normal frames)
        bch_polys[0][0] = bitvect<uint32_t, 17>(0x1002d);  // g1
        bch_polys[0][1] = bitvect<uint32_t, 17>(0x10173);  // g2
        bch_polys[0][2] = bitvect<uint32_t, 17>(0x10fbd);  // g3
        bch_polys[0][3] = bitvect<uint32_t, 17>(0x15a55);  // g4
        bch_polys[0][4] = bitvect<uint32_t, 17>(0x11f2f);  // g5
        bch_polys[0][5] = bitvect<uint32_t, 17>(0x1f7b5);  // g6
        bch_polys[0][6] = bitvect<uint32_t, 17>(0x1af65);  // g7
        bch_polys[0][7] = bitvect<uint32_t, 17>(0x17367);  // g8
        bch_polys[0][8] = bitvect<uint32_t, 17>(0x10ea1);  // g9
        bch_polys[0][9] = bitvect<uint32_t, 17>(0x175a7);  // g10
        bch_polys[0][10] = bitvect<uint32_t, 17>(0x13a2d); // g11
        bch_polys[0][11] = bitvect<uint32_t, 17>(0x11ae3); // g12
        // EN 302 307-1 5.3.1 Table 6b (polynomials for short frames)
        bch_polys[1][0] = bitvect<uint32_t, 17>(0x402b);  // g1
        bch_polys[1][1] = bitvect<uint32_t, 17>(0x4941);  // g2
        bch_polys[1][2] = bitvect<uint32_t, 17>(0x4647);  // g3
        bch_polys[1][3] = bitvect<uint32_t, 17>(0x5591);  // g4
        bch_polys[1][4] = bitvect<uint32_t, 17>(0x6b55);  // g5
        bch_polys[1][5] = bitvect<uint32_t, 17>(0x6389);  // g6
        bch_polys[1][6] = bitvect<uint32_t, 17>(0x6ce5);  // g7
        bch_polys[1][7] = bitvect<uint32_t, 17>(0x4f21);  // g8
        bch_polys[1][8] = bitvect<uint32_t, 17>(0x460f);  // g9
        bch_polys[1][9] = bitvect<uint32_t, 17>(0x5a49);  // g10
        bch_polys[1][10] = bitvect<uint32_t, 17>(0x5811); // g11
        bch_polys[1][11] = bitvect<uint32_t, 17>(0x65ef); // g12
        // Redundant with fec_infos[], but needs static template argument.
        memset(bchs, 0, sizeof(bchs));
        bchs[0][FEC12] = new s2_bch_engine_nf12(bch_polys[0], 12);
        bchs[0][FEC23] = new s2_bch_engine_nf10(bch_polys[0], 10);
        bchs[0][FEC34] = new s2_bch_engine_nf12(bch_polys[0], 12);
        bchs[0][FEC56] = new s2_bch_engine_nf10(bch_polys[0], 10);
        bchs[0][FEC45] = new s2_bch_engine_nf12(bch_polys[0], 12);
        bchs[0][FEC89] = new s2_bch_engine_nf8(bch_polys[0], 8);
        bchs[0][FEC910] = new s2_bch_engine_nf8(bch_polys[0], 8);
        bchs[0][FEC14] = new s2_bch_engine_nf12(bch_polys[0], 12);
        bchs[0][FEC13] = new s2_bch_engine_nf12(bch_polys[0], 12);
        bchs[0][FEC25] = new s2_bch_engine_nf12(bch_polys[0], 12);
        bchs[0][FEC35] = new s2_bch_engine_nf12(bch_polys[0], 12);
        bchs[1][FEC12] = new s2_bch_engine_sf12(bch_polys[1], 12);
        bchs[1][FEC23] = new s2_bch_engine_sf12(bch_polys[1], 12);
        bchs[1][FEC34] = new s2_bch_engine_sf12(bch_polys[1], 12);
        bchs[1][FEC56] = new s2_bch_engine_sf12(bch_polys[1], 12);
        bchs[1][FEC45] = new s2_bch_engine_sf12(bch_polys[1], 12);
        bchs[1][FEC89] = new s2_bch_engine_sf12(bch_polys[1], 12);
        bchs[1][FEC14] = new s2_bch_engine_sf12(bch_polys[1], 12);
        bchs[1][FEC13] = new s2_bch_engine_sf12(bch_polys[1], 12);
        bchs[1][FEC25] = new s2_bch_engine_sf12(bch_polys[1], 12);
        bchs[1][FEC35] = new s2_bch_engine_sf12(bch_polys[1], 12);
    }
}; // s2_bch_engines

// S2 BASEBAND DESCRAMBLER AND FEC ENCODER
// EN 302 307-1 section 5.2.2
// EN 302 307-1 section 5.3

struct s2_fecenc : runnable
{
    typedef ldpc_engine<bool, hard_sb, 8, uint16_t> s2_ldpc_engine;
    s2_fecenc(scheduler *sch,
              pipebuf<bbframe> &_in, pipebuf<fecframe<hard_sb>> &_out)
        : runnable(sch, "S2 fecenc"),
          in(_in), out(_out)
    {
        if (sch->debug)
            s2ldpc.print_node_stats();
    }
    void run()
    {
        while (in.readable() >= 1 && out.writable() >= 1)
        {
            bbframe *pin = in.rd();
            fecframe<hard_sb> *pout = out.wr();
            run_frame(in.rd(), out.wr());
            in.read(1);
            out.written(1);
        }
    }

  private:
    void run_frame(const bbframe *pin, fecframe<hard_sb> *pout)
    {
        const modcod_info *mcinfo = check_modcod(pin->pls.modcod);
        const fec_info *fi = &fec_infos[pin->pls.sf][mcinfo->rate];
        pout->pls = pin->pls;
        hard_sb *pbytes = pout->bytes;
        bbscrambling.transform(pin->bytes, fi->Kbch / 8, pbytes);
        { // BCH
            size_t msgbytes = fi->Kbch / 8;
            size_t cwbytes = fi->kldpc / 8;
            bch_interface *bch = s2bch.bchs[pin->pls.sf][mcinfo->rate];
            bch->encode(pbytes, msgbytes, pbytes + msgbytes);
        }
        { // LDPC
            size_t msgbits = fi->kldpc;
            size_t cwbits = pin->pls.framebits();
            s2_ldpc_engine *ldpc = s2ldpc.ldpcs[pin->pls.sf][mcinfo->rate];
            ldpc->encode(fi->ldpc, pbytes, msgbits, cwbits, pbytes + msgbits / 8);
        }
    }
    pipereader<bbframe> in;
    pipewriter<fecframe<hard_sb>> out;
    s2_bbscrambling bbscrambling;
    s2_bch_engines s2bch;
    s2_ldpc_engines<bool, hard_sb> s2ldpc;
}; // s2_fecenc

// S2 FEC DECODER AND BASEBAND DESCRAMBLER
// EN 302 307-1 section 5.3
// EN 302 307-1 section 5.2.2

template <typename SOFTBIT, typename SOFTBYTE>
struct s2_fecdec : runnable
{
    int bitflips;
    s2_fecdec(scheduler *sch,
              pipebuf<fecframe<SOFTBYTE>> &_in, pipebuf<bbframe> &_out,
              pipebuf<int> *_bitcount = NULL,
              pipebuf<int> *_errcount = NULL)
        : runnable(sch, "S2 fecdec"),
          bitflips(0),
          in(_in), out(_out),
          bitcount(opt_writer(_bitcount, 1)),
          errcount(opt_writer(_errcount, 1))
    {
        if (sch->debug)
            s2ldpc.print_node_stats();
    }
    void run()
    {
        while (in.readable() >= 1 && out.writable() >= 1 &&
               opt_writable(bitcount, 1) && opt_writable(errcount, 1))
        {
            fecframe<SOFTBYTE> *pin = in.rd();
            const modcod_info *mcinfo = check_modcod(pin->pls.modcod);
            const fec_info *fi = &fec_infos[pin->pls.sf][mcinfo->rate];
            bool corrupted = false;
            bool residual_errors;
            if (true)
            {
                // LDPC decode
                size_t cwbits = pin->pls.framebits();
                size_t msgbits = fi->kldpc;
                size_t chkbits = cwbits - msgbits;
                s2_ldpc_engine *ldpc = s2ldpc.ldpcs[pin->pls.sf][mcinfo->rate];
                int ncorr = ldpc->decode_bitflip(fi->ldpc, pin->bytes, msgbits, cwbits, bitflips);
                if (sch->debug2)
                    fprintf(stderr, "LDPCCORR = %d\n", ncorr);
            }
            uint8_t *hardbytes = softbytes_harden(pin->bytes, fi->kldpc / 8, bch_buf);
            if (true)
            {
                // BCH decode
                size_t cwbytes = fi->kldpc / 8;
                size_t msgbytes = fi->Kbch / 8;
                size_t chkbytes = cwbytes - msgbytes;
                // Decode with suitable BCH decoder for this MODCOD
                bch_interface *bch = s2bch.bchs[pin->pls.sf][mcinfo->rate];
                int ncorr = bch->decode(hardbytes, cwbytes);
                if (sch->debug2)
                    fprintf(stderr, "BCHCORR = %d\n", ncorr);
                corrupted = (ncorr < 0);
                residual_errors = (ncorr != 0);
                // Report VER
                opt_write(bitcount, fi->Kbch);
                opt_write(errcount, (ncorr >= 0) ? ncorr : fi->Kbch);
            }
            int bbsize = fi->Kbch / 8;
            // TBD Some decoders want the bad packets.
#if 0
	if ( corrupted ) {
	  fprintf(stderr, "Passing bad frame\n");
	  corrupted = false;
	}
#endif
            if (!corrupted)
            {
                // Descramble and output
                bbframe *pout = out.wr();
                pout->pls = pin->pls;
                bbscrambling.transform(hardbytes, bbsize, pout->bytes);
                out.written(1);
            }
            if (sch->debug)
                fprintf(stderr, "%c", corrupted ? ':' : residual_errors ? '.' : '_');
            in.read(1);
        }
    }

  private:
    s2_ldpc_engines<SOFTBIT, SOFTBYTE> s2ldpc;
    uint8_t bch_buf[64800 / 8]; // Temp storage for hardening before BCH
    s2_bch_engines s2bch;
    s2_bbscrambling bbscrambling;
    pipereader<fecframe<SOFTBYTE>> in;
    pipewriter<bbframe> out;
    pipewriter<int> *bitcount, *errcount;
}; // s2_fecdec

// External LDPC decoder
// Spawns a user-specified command, FEC frames on stdin/stdout.

template <typename T, int _SIZE>
struct simplequeue
{
    static const int SIZE = _SIZE;
    simplequeue() { rd = wr = count = 0; }
    bool full() { return count == SIZE; }
    T *put()
    {
        T *res = &q[wr];
        wr = (wr + 1) % SIZE;
        ++count;
        return res;
    }
    bool empty() { return count == 0; }
    const T *peek() { return &q[rd]; }
    const T *get()
    {
        const T *res = &q[rd];
        rd = (rd + 1) % SIZE;
        --count;
        return res;
    }
    //  private:
    int rd, wr, count;
    T q[SIZE];
};

template <typename SOFTBIT, typename SOFTBYTE>
struct s2_fecdec_helper : runnable
{
    int batch_size;
    int nhelpers;
    bool must_buffer;
    s2_fecdec_helper(scheduler *sch,
                     pipebuf<fecframe<SOFTBYTE>> &_in,
                     pipebuf<bbframe> &_out,
                     const char *_command,
                     pipebuf<int> *_bitcount = NULL,
                     pipebuf<int> *_errcount = NULL)
        : runnable(sch, "S2 fecdec io"),
          batch_size(32),
          nhelpers(1),
          must_buffer(false),
          in(_in), out(_out),
          command(_command),
          bitcount(opt_writer(_bitcount, 1)),
          errcount(opt_writer(_errcount, 1))
    {
        for (int mc = 0; mc < 32; ++mc)
            for (int sf = 0; sf < 2; ++sf)
                pools[mc][sf].procs = NULL;
    }
    void run()
    {
        bool work_done = false;
        // Send work until all helpers block.
        bool all_blocked = false;
        while (in.readable() >= 1 && !jobs.full())
        {
            if (!send_frame(in.rd()))
            {
                all_blocked = true;
                break;
            }
            in.read(1);
            work_done = true;
        }
        // Risk blocking on read() only when we have nothing else to do
        // and we know a result is coming.
        while ((all_blocked || !work_done || jobs.full()) &&
               !jobs.empty() &&
               jobs.peek()->h->b_out &&
               out.writable() >= 1 &&
               opt_writable(bitcount, 1) && opt_writable(errcount, 1))
        {
            receive_frame(jobs.get());
        }
    }

  private:
    struct helper_instance
    {
        int fd_tx;      // To helper
        int fd_rx;      // From helper
        int batch_size; // Latency
        int b_in;       // Jobs in input queue
        int b_out;      // Jobs in output queue
    };
    struct pool
    {
        helper_instance *procs; // NULL or [nprocs]
        int nprocs;
    } pools[32][2]; // [modcod][sf]
    struct helper_job
    {
        s2_pls pls;
        helper_instance *h;
    };
    simplequeue<helper_job, 1024> jobs;
    // Try to send a frame. Return false if helper was busy.
    bool send_frame(fecframe<SOFTBYTE> *pin)
    {
        pool *p = get_pool(&pin->pls);
        for (int i = 0; i < p->nprocs; ++i)
        {
            helper_instance *h = &p->procs[i];
            size_t iosize = (pin->pls.framebits() / 8) * sizeof(SOFTBYTE);
            // fprintf(stderr, "Writing %lu to fd %d\n", iosize, h->fd_tx);
            int nw = write(h->fd_tx, pin->bytes, iosize);
            if (nw < 0 && errno == EWOULDBLOCK)
                continue;
            if (nw < 0)
                fatal("write(LDPC helper");
            if (nw != iosize)
                fatal("partial write(LDPC helper)");
            helper_job *job = jobs.put();
            job->pls = pin->pls;
            job->h = h;
            ++h->b_in;
            if (h->b_in >= h->batch_size)
            {
                h->b_in -= h->batch_size;
                h->b_out += h->batch_size;
            }
            return true;
        }
        return false;
    }
    // Return a pool of running helpers for a given modcod.
    pool *get_pool(const s2_pls *pls)
    {
        pool *p = &pools[pls->modcod][pls->sf];
        if (!p->procs)
        {
            p->procs = new helper_instance[nhelpers];
            for (int i = 0; i < nhelpers; ++i)
                spawn_helper(&p->procs[i], pls);
            p->nprocs = nhelpers;
        }
        return p;
    }
    // Spawn a helper process.
    void spawn_helper(helper_instance *h, const s2_pls *pls)
    {
        if (sch->debug)
            fprintf(stderr, "Spawning LDPC helper: modcod=%d sf=%d\n",
                    pls->modcod, pls->sf);
        int tx[2], rx[2];
        if (pipe(tx) || pipe(rx))
            fatal("pipe");
        // Size the pipes so that the helper never runs out of work to do.
        int pipesize = 64800 * batch_size;
// macOS does not have F_SETPIPE_SZ and there
// is no way to change the buffer size
#ifndef __APPLE__
        if (fcntl(tx[0], F_SETPIPE_SZ, pipesize) < 0 ||
            fcntl(rx[0], F_SETPIPE_SZ, pipesize) < 0 ||
            fcntl(tx[1], F_SETPIPE_SZ, pipesize) < 0 ||
            fcntl(rx[1], F_SETPIPE_SZ, pipesize) < 0)
        {
            fprintf(stderr,
                    "*** Failed to increase pipe size.\n"
                    "*** Try echo %d > /proc/sys/fs/pipe-max-size\n",
                    pipesize);
            if (must_buffer)
                fatal("F_SETPIPE_SZ");
            else
                fprintf(stderr, "*** Throughput will be suboptimal.\n");
        }
#endif
        int child = vfork();
        if (!child)
        {
            // Child process
            close(tx[1]);
            dup2(tx[0], 0);
            close(rx[0]);
            dup2(rx[1], 1);
            char mc_arg[16];
            sprintf(mc_arg, "%d", pls->modcod);
            const char *sf_arg = pls->sf ? "--shortframes" : NULL;
            const char *argv[] = {command, "--modcod", mc_arg, sf_arg, NULL};
            execve(command, (char *const *)argv, NULL);
            fatal(command);
        }
        h->fd_tx = tx[1];
        close(tx[0]);
        h->fd_rx = rx[0];
        close(rx[1]);
        h->batch_size = 32; // TBD
        h->b_in = h->b_out = 0;
        int flags = fcntl(h->fd_tx, F_GETFL);
        if (fcntl(h->fd_tx, F_SETFL, flags | O_NONBLOCK))
            fatal("fcntl(helper)");
    }

    // Receive a finished job.
    void receive_frame(const helper_job *job)
    {
        // Read corrected frame from helper
        const s2_pls *pls = &job->pls;
        size_t iosize = (pls->framebits() / 8) * sizeof(ldpc_buf[0]);
        int nr = read(job->h->fd_rx, ldpc_buf, iosize);
        if (nr < 0)
            fatal("read(LDPC helper)");
        if (nr != iosize)
            fatal("partial read(LDPC helper)");
        --job->h->b_out;
        // Decode BCH.
        const modcod_info *mcinfo = check_modcod(job->pls.modcod);
        const fec_info *fi = &fec_infos[job->pls.sf][mcinfo->rate];
        uint8_t *hardbytes = softbytes_harden(ldpc_buf, fi->kldpc / 8, bch_buf);
        size_t cwbytes = fi->kldpc / 8;
        size_t msgbytes = fi->Kbch / 8;
        size_t chkbytes = cwbytes - msgbytes;
        bch_interface *bch = s2bch.bchs[job->pls.sf][mcinfo->rate];
        int ncorr = bch->decode(hardbytes, cwbytes);
        if (sch->debug2)
            fprintf(stderr, "BCHCORR = %d\n", ncorr);
        bool corrupted = (ncorr < 0);
        // Report VBER
        opt_write(bitcount, fi->Kbch);
        opt_write(errcount, (ncorr >= 0) ? ncorr : fi->Kbch);
#if 0
      // TBD Some decoders want the bad packets.
	if ( corrupted ) {
	  fprintf(stderr, "Passing bad frame\n");
	  corrupted = false;
	}
#endif
        if (!corrupted)
        {
            // Descramble and output
            bbframe *pout = out.wr();
            pout->pls = job->pls;
            bbscrambling.transform(hardbytes, fi->Kbch / 8, pout->bytes);
            out.written(1);
        }
        if (sch->debug)
            fprintf(stderr, "%c", corrupted ? '!' : ncorr ? '.' : '_');
    }
    pipereader<fecframe<SOFTBYTE>> in;
    pipewriter<bbframe> out;
    const char *command;
    SOFTBYTE ldpc_buf[64800 / 8];
    uint8_t bch_buf[64800 / 8]; // Temp storage for hardening before BCH
    s2_bch_engines s2bch;
    s2_bbscrambling bbscrambling;
    pipewriter<int> *bitcount, *errcount;
}; // s2_fecdec_helper

// S2 FRAMER
// EN 302 307-1 section 5.1 Mode adaptation

struct s2_framer : runnable
{
    uint8_t rolloff_code; // 0=0.35, 1=0.25, 2=0.20, 3=reserved
    s2_pls pls;
    s2_framer(scheduler *sch, pipebuf<tspacket> &_in, pipebuf<bbframe> &_out)
        : runnable(sch, "S2 framer"),
          in(_in), out(_out)
    {
        pls.modcod = 4;
        pls.sf = false;
        pls.pilots = true;
        nremain = 0;
        remcrc = 0; // CRC for nonexistent previous packet
    }
    void run()
    {
        while (out.writable() >= 1)
        {
            const modcod_info *mcinfo = check_modcod(pls.modcod);
            const fec_info *fi = &fec_infos[pls.sf][mcinfo->rate];
            int framebytes = fi->Kbch / 8;
            if (!framebytes)
                fail("MODCOD/framesize combination not allowed");
            if (10 + nremain + 188 * in.readable() < framebytes)
                break; // Not enough data to fill a frame
            bbframe *pout = out.wr();
            pout->pls = pls;
            uint8_t *buf = pout->bytes;
            uint8_t *end = buf + framebytes;
            // EN 302 307-1 section 5.1.6 Base-Band Header insertion
            uint8_t *bbheader = buf;
            *buf++ = 0x30 | rolloff_code; // MATYPE-1: SIS, CCM
            *buf++ = 0;                   // MATYPE-2
            uint16_t upl = 188 * 8;
            *buf++ = upl >> 8; // UPL MSB
            *buf++ = upl;      // UPL LSB
            uint16_t dfl = (framebytes - 10) * 8;
            *buf++ = dfl >> 8; // DFL MSB
            *buf++ = dfl;      // DFL LSB
            *buf++ = 0x47;     // SYNC
            uint16_t syncd = nremain * 8;
            *buf++ = syncd >> 8; // SYNCD MSB
            *buf++ = syncd;      // SYNCD LSB
            *buf++ = crc8.compute(bbheader, 9);
            // Data field
            memcpy(buf, rembuf, nremain); // Leftover from previous runs
            buf += nremain;
            while (buf < end)
            {
                tspacket *tsp = in.rd();
                if (tsp->data[0] != MPEG_SYNC)
                    fail("Invalid TS");
                *buf++ = remcrc; // Replace SYNC with CRC of previous.
                remcrc = crc8.compute(tsp->data + 1, tspacket::SIZE - 1);
                int nused = end - buf;
                if (nused > tspacket::SIZE - 1)
                    nused = tspacket::SIZE - 1;
                memcpy(buf, tsp->data + 1, nused);
                buf += nused;
                if (buf == end)
                {
                    nremain = (tspacket::SIZE - 1) - nused;
                    memcpy(rembuf, tsp->data + 1 + nused, nremain);
                }
                in.read(1);
            }
            if (buf != end)
                fail("Bug: s2_framer");
            out.written(1);
        }
    }

  private:
    pipereader<tspacket> in;
    pipewriter<bbframe> out;
    crc8_engine crc8;
    int nremain;
    uint8_t rembuf[tspacket::SIZE];
    uint8_t remcrc;
}; // s2_framer

// S2 DEFRAMER
// EN 302 307-1 section 5.1 Mode adaptation

struct s2_deframer : runnable
{
    s2_deframer(scheduler *sch, pipebuf<bbframe> &_in, pipebuf<tspacket> &_out,
                pipebuf<int> *_state_out = NULL,
                pipebuf<unsigned long> *_locktime_out = NULL)
        : runnable(sch, "S2 deframer"),
          missing(-1),
          in(_in), out(_out, MAX_TS_PER_BBFRAME),
          current_state(false),
          state_out(opt_writer(_state_out, 2)),
          report_state(true),
          locktime(0),
          locktime_out(opt_writer(_locktime_out, MAX_TS_PER_BBFRAME))
    {
    }
    void run()
    {
        while (in.readable() >= 1 && out.writable() >= MAX_TS_PER_BBFRAME &&
               opt_writable(state_out, 2) &&
               opt_writable(locktime_out, MAX_TS_PER_BBFRAME))
        {
            if (report_state)
            {
                // Report unlocked state on first invocation.
                opt_write(state_out, 0);
                report_state = false;
            }
            run_bbframe(in.rd());
            in.read(1);
        }
    }

  private:
    void run_bbframe(bbframe *pin)
    {
        uint8_t *bbh = pin->bytes;
        uint16_t upl = (bbh[2] << 8) | bbh[3];
        uint16_t dfl = (bbh[4] << 8) | bbh[5];
        uint8_t sync = bbh[6];
        uint16_t syncd = (bbh[7] << 8) | bbh[8];
        uint8_t crcexp = crc8.compute(bbh, 9);
        uint8_t crc = bbh[9];
        uint8_t *data = bbh + 10;
        int ro_code = bbh[0] & 3;
        if (sch->debug2)
        {
            static float ro_values[] = {0.35, 0.25, 0.20, 0};
            fprintf(stderr, "BBH: crc %02x/%02x %s ma=%02x%02x ro=%.2f"
                            " upl=%d dfl=%d sync=%02x syncd=%d\n",
                    crc, crcexp, (crc == crcexp) ? "OK" : "KO",
                    bbh[0], bbh[1], ro_values[ro_code], upl, dfl, sync, syncd);
        }
        if (crc != crcexp || upl != 188 * 8 || sync != 0x47 || dfl > fec_info::KBCH_MAX ||
            syncd > dfl || (dfl & 7) || (syncd & 7))
        {
            // Note: Maybe accept syncd=65535
            fprintf(stderr, "Bad bbframe\n");
            missing = -1;
            info_unlocked();
            return;
        }
        // TBD Handle packets as payload+finalCRC and do crc8 before pout
        int pos; // Start of useful data in this bbframe
        if (missing < 0)
        {
            // Skip unusable data at beginning of bbframe
            pos = syncd / 8;
            fprintf(stderr, "Start TS at %d\n", pos);
            missing = 0;
        }
        else
        {
            // Sanity check
            if (syncd / 8 != missing)
            {
                fprintf(stderr, "Lost a bbframe ?\n");
                missing = -1;
                info_unlocked();
                return;
            }
            pos = 0;
        }
        if (missing)
        {
            // Complete and output the partial TS packet in leftover[].
            tspacket *pout = out.wr();
            memcpy(pout->data, leftover, 188 - missing);
            memcpy(pout->data + (188 - missing), data + pos, missing);
            out.written(1);
            info_good_packet();
            ++pout;
            // Skip to beginning of next TS packet
            pos += missing;
            missing = 0;
        }
        while (pos + 188 <= dfl / 8)
        {
            tspacket *pout = out.wr();
            memcpy(pout->data, data + pos, 188);
            pout->data[0] = sync; // Replace CRC
            out.written(1);
            info_good_packet();
            pos += 188;
        }
        int remain = dfl / 8 - pos;
        if (remain)
        {
            memcpy(leftover, data + pos, remain);
            leftover[0] = sync; // Replace CRC
            missing = 188 - remain;
        }
    }

    void info_unlocked()
    {
        info_is_locked(false);
        locktime = 0;
    }
    void info_good_packet()
    {
        info_is_locked(true);
        ++locktime;
        opt_write(locktime_out, locktime);
    }
    void info_is_locked(bool newstate)
    {
        if (newstate != current_state)
        {
            opt_write(state_out, newstate ? 1 : 0);
            current_state = newstate;
        }
    }

    crc8_engine crc8;
    int missing; // Bytes needed to complete leftover[],
                 // or 0 if no leftover data,
                 // or -1 if not synced.
    uint8_t leftover[188];
    static const int MAX_TS_PER_BBFRAME = fec_info::KBCH_MAX / 8 / 188 + 1;
    bool locked;
    pipereader<bbframe> in;
    pipewriter<tspacket> out;
    int current_state;
    pipewriter<int> *state_out;
    bool report_state;
    unsigned long locktime;
    pipewriter<unsigned long> *locktime_out;
}; // s2_deframer

} // namespace leansdr

#endif // LEANSDR_DVBS2_H
