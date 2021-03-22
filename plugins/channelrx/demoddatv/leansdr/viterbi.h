// This file is part of LeanSDR Copyright (C) 2016-2018 <pabr@pabr.org>.
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

#ifndef LEANSDR_VITERBI_H
#define LEANSDR_VITERBI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This is a generic implementation of Viterbi with explicit
// representation of the trellis.  There is special support for
// convolutional coding, but the code can handle other schemes.
// TBD This is very inefficient. For a specific trellis all loops
// can be be unrolled.

namespace leansdr
{

// TS is an integer type for a least NSTATES+1 states.
// NSTATES is the number of states (e.g. 2^(K-1)).
// TUS is an integer type for uncoded symbols (branch identifiers).
// NUS is the number of uncoded symbols.
// TCS is an integer type for coded symbols (branch labels).
// NCS is the number of coded symbols.
// TP is a type for representing paths.
// TPM, TBM are unsigned integer types for path/branch metrics.
// TPM is at least as wide as TBM.

template <typename TS, int NSTATES, typename TUS, int NUS, int NCS>
struct trellis
{
    static const int NOSTATE = NSTATES + 1;

    struct state
    {
        struct branch
        {
            TS pred;     // Predecessor state or NOSTATE
            TUS us;      // Uncoded symbol
        } branches[NCS]; // Incoming branches indexed by coded symbol
    } states[NSTATES];

    trellis()
    {
        for (TS s = 0; s < NSTATES; ++s)
        {
            for (int cs = 0; cs < NCS; ++cs) {
                states[s].branches[cs].pred = NOSTATE;
            }
        }
    }

    // TBD Polynomial width should be a template parameter ?
    void init_convolutional(const uint16_t G[])
    {
        if (NCS & (NCS - 1)) {
            fprintf(stderr, "NCS must be a power of 2\n");
        }

        // Derive number of polynomials from NCS.
        int nG = log2i(NCS);

        for (TS s = 0; s < NSTATES; ++s)
        {
            for (TUS us = 0; us < NUS; ++us)
            {
                // Run the convolutional encoder from state s with input us
                uint64_t shiftreg = s; // TBD type
                // Reverse bits
                TUS us_rev = 0;

                for (int b = 1; b < NUS; b *= 2)
                {
                    if (us & b) {
                        us_rev |= (NUS / 2 / b);
                    }
                }

                shiftreg |= us_rev * NSTATES;
                uint32_t cs = 0; // TBD type

                for (int g = 0; g < nG; ++g) {
                    cs = (cs << 1) | parity(shiftreg & G[g]);
                }

                shiftreg /= NUS; // Shift bits for 1 uncoded symbol
                // [us] at state [s] emits [cs] and leads to state [shiftreg].
                typename state::branch *b = &states[shiftreg].branches[cs];

                if (b->pred != NOSTATE) {
                    fprintf(stderr, "Invalid convolutional code\n");
                }

                b->pred = s;
                b->us = us;
            }
        }
    }

    void dump()
    {
        for (int s = 0; s < NSTATES; ++s)
        {
            fprintf(stderr, "State %02x:", s);

            for (int cs = 0; cs < NCS; ++cs)
            {
                typename state::branch *b = &states[s].branches[cs];

                if (b->pred == NOSTATE) {
                    fprintf(stderr, "     - ");
                } else {
                    fprintf(stderr, "   %02x+%x", b->pred, b->us);
                }
            }

            fprintf(stderr, "\n");
        }
    }
};

// Interface that hides the templated internals.
template <typename TUS,
          typename TCS,
          typename TBM,
          typename TPM>
struct viterbi_dec_interface
{
    virtual ~viterbi_dec_interface() {}
    virtual TUS update(TBM *costs, TPM *quality = nullptr) = 0;
    virtual TUS update(TCS s, TBM cost, TPM *quality = nullptr) = 0;
};

template <typename TS, int NSTATES,
          typename TUS, int NUS,
          typename TCS, int NCS,
          typename TBM, typename TPM,
          typename TP>
struct viterbi_dec : viterbi_dec_interface<TUS, TCS, TBM, TPM>
{

    trellis<TS, NSTATES, TUS, NUS, NCS> *trell;

    struct state
    {
        TPM cost; // Metric of best path leading to this state
        TP path;  // Best path leading to this state
    };

    typedef state statebank[NSTATES];
    state statebanks[2][NSTATES];
    statebank *states, *newstates; // Alternate between banks

    viterbi_dec(trellis<TS, NSTATES, TUS, NUS, NCS> *_trellis) : trell(_trellis)
    {
        states = &statebanks[0];
        newstates = &statebanks[1];

        for (TS s = 0; s < NSTATES; ++s) {
            (*states)[s].cost = 0;
        }

        // Determine max value that can fit in TPM
        max_tpm = (TPM)0 - 1;

        if (max_tpm < 0)
        {
            // TPM is signed
            for (max_tpm = 0; max_tpm * 2 + 1 > max_tpm; max_tpm = max_tpm * 2 + 1);
        }
    }

    // Update with full metric

    TUS update(TBM costs[NCS], TPM *quality = nullptr)
    {
        TPM best_tpm = max_tpm, best2_tpm = max_tpm;
        TS best_state = 0;

        // Update all states
        for (int s = 0; s < NSTATES; ++s)
        {
            TPM best_m = max_tpm;
            typename trellis<TS, NSTATES, TUS, NUS, NCS>::state::branch *best_b = nullptr;

            // Select best branch
            for (int cs = 0; cs < NCS; ++cs)
            {
                typename trellis<TS, NSTATES, TUS, NUS, NCS>::state::branch *b =
                    &trell->states[s].branches[cs];

                if (b->pred == trell->NOSTATE) {
                    continue;
                }

                TPM m = (*states)[b->pred].cost + costs[cs];

                if (m <= best_m)
                { // <= guarantees one match
                    best_m = m;
                    best_b = b;
                }
            }

            (*newstates)[s].path = (*states)[best_b->pred].path;
            (*newstates)[s].path.append(best_b->us);
            (*newstates)[s].cost = best_m;

            // Select best and second-best states
            if (best_m < best_tpm)
            {
                best_state = s;
                best2_tpm = best_tpm;
                best_tpm = best_m;
            }
            else if (best_m < best2_tpm)
            {
                best2_tpm = best_m;
            }
        }
        // Swap banks
        {
            statebank *tmp = states;
            states = newstates;
            newstates = tmp;
        }

        // Prevent overflow of path metrics
        for (TS s = 0; s < NSTATES; ++s) {
            (*states)[s].cost -= best_tpm;
        }
#if 0
      // Observe that the min-max range remains bounded
      fprintf(stderr,"-%2d = [", best_tpm);
      for ( TS s=0; s<NSTATES; ++s ) fprintf(stderr," %d", (*states)[s].cost);
      fprintf(stderr," ]\n");
#endif
        // Return difference between best and second-best as quality metric.
        if (quality) {
            *quality = best2_tpm - best_tpm;
        }
        // Return uncoded symbol of best path
        return (*states)[best_state].path.read();
    }

    // Update with partial metrics.
    // The costs provided must be negative.
    // The other symbols will be assigned a cost of 0.

    TUS update(int nm, TCS cs[], TBM costs[], TPM *quality = nullptr)
    {
        TPM best_tpm = max_tpm, best2_tpm = max_tpm;
        TS best_state = 0;

        // Update all states
        for (int s = 0; s < NSTATES; ++s)
        {
            // Select best branch among those for with metrics are provided
            TPM best_m = max_tpm;
            typename trellis<TS, NSTATES, TUS, NUS, NCS>::state::branch *best_b = nullptr;

            for (int im = 0; im < nm; ++im)
            {
                typename trellis<TS, NSTATES, TUS, NUS, NCS>::state::branch *b =
                    &trell->states[s].branches[cs[im]];

                if (b->pred == trell->NOSTATE) {
                    continue;
                }

                TPM m = (*states)[b->pred].cost + costs[im];

                if (m <= best_m)
                { // <= guarantees one match
                    best_m = m;
                    best_b = b;
                }
            }
            if (nm != NCS)
            {
                // Also scan the other branches.
                // We actually rescan the branches with metrics.
                // This works because costs are negative.
                for (int cs = 0; cs < NCS; ++cs)
                {
                    typename trellis<TS, NSTATES, TUS, NUS, NCS>::state::branch *b =
                        &trell->states[s].branches[cs];

                    if (b->pred == trell->NOSTATE) {
                        continue;
                    }

                    TPM m = (*states)[b->pred].cost;

                    if (m <= best_m)
                    {
                        best_m = m;
                        best_b = b;
                    }
                }
            }

            (*newstates)[s].path = (*states)[best_b->pred].path;
            (*newstates)[s].path.append(best_b->us);
            (*newstates)[s].cost = best_m;

            // Select best states
            if (best_m < best_tpm)
            {
                best_state = s;
                best2_tpm = best_tpm;
                best_tpm = best_m;
            }
            else if (best_m < best2_tpm)
            {
                best2_tpm = best_m;
            }
        }
        // Swap banks
        {
            statebank *tmp = states;
            states = newstates;
            newstates = tmp;
        }
        // Prevent overflow of path metrics
        for (TS s = 0; s < NSTATES; ++s) {
            (*states)[s].cost -= best_tpm;
        }
#if 0
      // Observe that the min-max range remains bounded
      fprintf(stderr,"-%2d = [", best_tpm);
      for ( TS s=0; s<NSTATES; ++s ) fprintf(stderr," %d", (*states)[s].cost);
      fprintf(stderr," ]\n");
#endif
        // Return difference between best and second-best as quality metric.
        if (quality) {
            *quality = best2_tpm - best_tpm;
        }

        // Return uncoded symbol of best path
        return (*states)[best_state].path.read();
    }

    // Update with single-symbol metric.
    // cost must be negative.

    TUS update(TCS cs, TBM cost, TPM *quality = nullptr) {
        return update(1, &cs, &cost, quality);
    }

    void dump()
    {
        fprintf(stderr, "[");

        for (TS s = 0; s < NSTATES; ++s)
        {
            if (states[s].cost) {
                fprintf(stderr, " %02x:%d", s, states[s].cost);
            }
        }

        fprintf(stderr, "\n");
    }

  private:
    TPM max_tpm;
};

// Paths (sequences of uncoded symbols) represented as bitstreams.
// NBITS is the number of bits per symbol.
// DEPTH is the number of symbols stored in the path.
// T is an unsigned integer type wider than NBITS*DEPTH.

template <typename T, typename TUS, int NBITS, int DEPTH>
struct bitpath
{
    T val;
    bitpath() : val(0) {}
    void append(TUS us) { val = (val << NBITS) | us; }
    TUS read() { return (val >> (DEPTH - 1) * NBITS) & ((1 << NBITS) - 1); }
};

} // namespace leansdr

#endif // LEANSDR_VITERBI_H
