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

#ifndef LEANSDR_LDPC_H
#define LEANSDR_LDPC_H

#define lfprintf(...) \
    {                 \
    }

namespace leansdr
{

// LDPC sparse matrix specified like in the DVB-S2 standard
// Taddr must be wide enough to index message bits and address bits.

template <typename Taddr>
struct ldpc_table
{
    // TBD Save space
    static const int MAX_ROWS = 162; // 64800 * (9/10) / 360
    static const int MAX_COLS = 13;
    int q;
    int nrows;
    struct row
    {
        int ncols;
        Taddr cols[MAX_COLS];
    } rows[MAX_ROWS];
};

// LDPC ENGINE

// SOFTBITs can be hard (e.g. bool) or soft (e.g. llr_t).
// They are stored as SOFTWORDs containing SWSIZE SOFTBITs.
// See interface in softword.h.

template <typename SOFTBIT, typename SOFTWORD, int SWSIZE, typename Taddr>
struct ldpc_engine
{
    // vnodes: Value/variable nodes (message bits)
    // cnodes: Check nodes (parity bits)

    int k; // Message size in bits
    int n; // Codeword size in bits

    struct node
    {
        Taddr *edges;
        int nedges;
        static const int CHUNK = 4; // Grow edges[] in steps of CHUNK.

        void append(Taddr a)
        {
            if (nedges % CHUNK == 0)
            { // Full ?
                edges = (Taddr *)realloc(edges, (nedges + CHUNK) * sizeof(Taddr));

                if (!edges) {
                    fatal("realloc");
                }
            }

            edges[nedges++] = a;
        }
    };

    node *vnodes; // [k]
    node *cnodes; // [n-k]

    ldpc_engine() :
        vnodes(nullptr),
        cnodes(nullptr)
    {
    }

    // Initialize from a S2-style table.

    ldpc_engine(
        const ldpc_table<Taddr> *table,
        int _k,
        int _n
    ) :
        k(_k),
        n(_n)
    {
        // Sanity checks
        if (360 % SWSIZE) {
            fatal("Bad LDPC word size");
        }

        if (k % SWSIZE) {
            fatal("Bad LDPC k");
        }

        if (n % SWSIZE) {
            fatal("Bad LDPC n");
        }

        if (k != table->nrows * 360) {
            fatal("Bad table");
        }

        int n_k = n - k;

        if (table->q * 360 != n_k) {
            fatal("Bad q");
        }

        vnodes = new node[k];
        memset(vnodes, 0, sizeof(node) * k);
        cnodes = new node[n_k];
        memset(cnodes, 0, sizeof(node) * n_k);

        // Expand the graph.

        int m = 0;
        // Iterate over rows
        for (const typename ldpc_table<Taddr>::row *prow = table->rows;
             prow < table->rows + table->nrows;
             ++prow)
        {
            // Process 360 bits per row.
            int q = table->q;
            int qoffs = 0;

            for (int mw = 360; mw--; ++m, qoffs += q)
            {
                const Taddr *pa = prow->cols;

                for (int nc = prow->ncols; nc--; ++pa)
                {
                    int a = (int)*pa + qoffs;

                    if (a >= n_k) {
                        a -= n_k; // Modulo n-k. Note qoffs<360*q.
                    }

                    if (a >= n_k) {
                        fail("Invalid LDPC table");
                    }

                    vnodes[m].append(a);
                    cnodes[a].append(m);
                }
            }
        }
    }

    ~ldpc_engine()
    {
        if (vnodes) {
            delete[] vnodes;
        }
        if (cnodes) {
            delete[] cnodes;
        }
    }

    void print_node_stats()
    {
        int nedges = count_edges(vnodes, k);
        fprintf(stderr, "LDPC(%5d,%5d)(%.2f)"
                        " %5.2f edges/vnode, %5.2f edges/cnode\n",
                k, n - k, (float)k / n, (float)nedges / k, (float)nedges / (n - k));
    }

    int count_edges(node *nodes, int nnodes)
    {
        int c = 0;

        for (int i = 0; i < nnodes; ++i) {
            c += nodes[i].nedges;
        }

        return c;
    }

    // k: Message size in bits
    // n: Codeword size in bits
    // integrate: Optional S2-style post-processing

#if 0
    void encode_hard(const ldpc_table<Taddr> *table, const uint8_t *msg,
		     int k, int n, uint8_t *parity, bool integrate=true) {
      // Sanity checks
      if ( 360 % SWSIZE ) fatal("Bad LDPC word size");
      if ( k % SWSIZE ) fatal("Bad LDPC k");
      if ( n % SWSIZE ) fatal("Bad LDPC n");
      if ( k != table->nrows*360 ) fatal("Bad table");
      int n_k = n - k;
      if ( table->q*360 != n_k ) fatal("Bad q");

      for ( int i=0; i<n_k/SWSIZE; ++i ) softword_zero(&parity[i]);

      // Iterate over rows
      for ( const typename ldpc_table<Taddr>::row *prow = table->rows; // quirk
	    prow < table->rows+table->nrows;
	    ++prow ) {
	// Process 360 bits per row, in words of SWSIZE bits
	int q = table->q;
	int qoffs = 0;
	for ( int mw=360/SWSIZE; mw--; ++msg ) {
	  SOFTWORD msgword = *msg;
	  for ( int wbit=0; wbit<SWSIZE; ++wbit,qoffs+=q ) {
	    SOFTBIT msgbit = softword_get(msgword, wbit);
	    if ( ! msgbit ) continue;  // TBD Generic soft version
	    const Taddr *pa = prow->cols;
	    for ( int nc=prow->ncols; nc--; ++pa ) {
	      // Don't wrap modulo range of Taddr
	      int a = (int)*pa + qoffs;
	      // Note: qoffs < 360*q=n-k
	      if ( a >= n_k ) a -= n_k;  // TBD not predictable
	      softwords_flip(parity, a);
	    }
	  }
	}
      }

      if ( integrate )
	integrate_bits(parity, parity, n_k/SWSIZE);
    }
#endif

    void encode(
        const ldpc_table<Taddr> *table,
        const SOFTWORD *msg,
        int k,
        int n,
        SOFTWORD *parity,
        int integrate = true
    )
    {
        // Sanity checks
        if (360 % SWSIZE) {
            fatal("Bad LDPC word size");
        }

        if (k % SWSIZE) {
            fatal("Bad LDPC k");
        }

        if (n % SWSIZE) {
            fatal("Bad LDPC n");
        }

        if (k != table->nrows * 360) {
            fatal("Bad table");
        }

        int n_k = n - k;

        if (table->q * 360 != n_k) {
            fatal("Bad q");
        }

        for (int i = 0; i < n_k / SWSIZE; ++i) {
            softword_zero(&parity[i]);
        }

        // Iterate over rows
        for (const typename ldpc_table<Taddr>::row *prow = table->rows; // quirk
             prow < table->rows + table->nrows;
             ++prow)
        {
            // Process 360 bits per row, in words of SWSIZE bits
            int q = table->q;
            int qoffs = 0;

            for (int mw = 360 / SWSIZE; mw--; ++msg)
            {
                SOFTWORD msgword = *msg;

                for (int wbit = 0; wbit < SWSIZE; ++wbit, qoffs += q)
                {
                    SOFTBIT msgbit = softword_get(msgword, wbit);

                    if (!softbit_harden(msgbit)) {
                        continue;
                    }

                    const Taddr *pa = prow->cols;

                    for (int nc = prow->ncols; nc--; ++pa)
                    {
                        int a = (int)*pa + qoffs;
                        // Note: qoffs < 360*q=n-k

                        if (a >= n_k) {
                            a -= n_k; // TBD not predictable
                        }

                        softwords_flip(parity, a);
                    }
                }
            }
        }

        if (integrate) {
            integrate_bits(parity, parity, n_k / SWSIZE);
        }
    }

    // Flip bits connected to parity errors, one at a time,
    // as long as things improve and max_bitflips is not exceeded.

    // cw: codeword (k value bits followed by n-k check bits)

    static const int PPCM = 39;

    typedef int64_t score_t;

    score_t compute_scores(
        SOFTWORD *m,
        SOFTWORD *p,
        SOFTWORD *q,
        int nc,
        score_t *score,
        int k)
    {
        int total = 0;
        memset(score, 0, k * sizeof(*score));

        for (int c = 0; c < nc; ++c)
        {
            SOFTBIT err = softwords_xor(p, q, c);

            if (softbit_harden(err))
            {
                Taddr *pe = cnodes[c].edges;

                for (int e = cnodes[c].nedges; e--; ++pe)
                {
                    int v = *pe;
                    int s = err * softwords_weight<SOFTBIT, SOFTWORD>(m, v) * PPCM / vnodes[v].nedges;
                    //fprintf(stderr, "c[%d] bad => v[%d] += %d  (%d*%d)\n",
                    ///c, v, s, err, softwords_weight<SOFTBIT,SOFTWORD>(m,*pe));
                    score[v] += s;
                    total += s;
                }
            }
        }

        return total;
    }

    int decode_bitflip(
        const ldpc_table<Taddr> *table,
        SOFTWORD *cw,
        int k,
        int n,
        int max_bitflips)
    {
        if (!vnodes) {
            fail("LDPC graph not initialized");
        }

        int n_k = n - k;

        // Compute the expected check bits (without the final mixing)
        SOFTWORD *expected = new SOFTWORD[n_k / SWSIZE]; // Forbidden to statically allocate with non constant size
        encode(table, cw, k, n, expected, false);
        // Reverse the integrator mixing from the received check bits
        SOFTWORD *received = new SOFTWORD[n_k / SWSIZE]; // Forbidden to statically allocate with non constant size
        diff_bits(cw + k / SWSIZE, received, n_k / SWSIZE);

        // Compute initial scores
        score_t *score = new score_t[k]; // Forbidden to statically allocate with non constant size
        score_t tots = compute_scores(cw, expected, received, n_k, score, k);
        lfprintf(stderr, "Initial score %d\n", (int)tots);

        int nflipped = 0;

        score_t score_threshold;
        {
            SOFTBIT one;
            softbit_set(&one, true);
            score_threshold = (int)one * 2;
        }

        bool progress = true;

        while (progress && nflipped < max_bitflips)
        {
            progress = false;
            // Try to flip parity bits.
            // Due to differential decoding, they appear as consecutive errors.
            SOFTBIT prev_err = softwords_xor(expected, received, 0);

            for (int b = 0; b < n - k - 1; ++b)
            {
                prev_err = softwords_xor(expected, received, b); //TBD
                SOFTBIT err = softwords_xor(expected, received, b + 1);

                if (softbit_harden(prev_err) && softbit_harden(err))
                {
                    lfprintf(stderr, "flip parity %d\n", b);
                    softwords_flip(received, b);
                    softwords_flip(received, b + 1);
                    ++nflipped; // Counts as one flip before differential decoding.
                    progress = true;
                    int dtot = 0;

                    // Depenalize adjacent message bits.
                    {
                        Taddr *pe = cnodes[b].edges;

                        for (int e = cnodes[b].nedges; e--; ++pe)
                        {
                            int d = prev_err * softwords_weight<SOFTBIT, SOFTWORD>(cw, *pe) * PPCM / vnodes[*pe].nedges;
                            score[*pe] -= d;
                            dtot -= d;
                        }
                    }

                    {
                        Taddr *pe = cnodes[b + 1].edges;

                        for (int e = cnodes[b + 1].nedges; e--; ++pe)
                        {
                            int d = err * softwords_weight<SOFTBIT, SOFTWORD>(cw, *pe) * PPCM / vnodes[*pe].nedges;
                            score[*pe] -= d;
                            dtot -= d;
                        }
                    }

                    tots += dtot;
#if 1
                    // Also update the codeword in-place.
                    // TBD Useful for debugging only.
                    softwords_flip(cw, k + b);
#endif
                    // TBD version soft. err = ! err;
                }
                prev_err = err;
            } // c nodes

            score_t maxs = -(1 << 30);

            for (int v = 0; v < k; ++v)
            {
                if (score[v] > maxs) {
                    maxs = score[v];
                }
            }

            if (!maxs) {
                break;
            }

            lfprintf(stderr, "maxs %d\n", (int)maxs);
            // Try to flip each message bits with maximal score

            for (int v = 0; v < k; ++v)
            {
                if (score[v] < score_threshold) {
                    continue;
                }

                //	  if ( score[v] < maxs*9/10 ) continue;
                if (score[v] < maxs - 4) {
                    continue;
                }

                lfprintf(stderr, "  flip %d score=%d\n", (int)v, (int)score[v]);
                // Update expected parities and scores that depend on them.
                score_t dtot = 0;

                for (int commit = 0; commit <= 1; ++commit)
                {
                    Taddr *pe = vnodes[v].edges;

                    for (int e = vnodes[v].nedges; e--; ++pe)
                    {
                        Taddr c = *pe;
                        SOFTBIT was_bad = softwords_xor(expected, received, c);

                        if (softbit_harden(was_bad))
                        {
                            Taddr *pe = cnodes[c].edges;

                            for (int e = cnodes[c].nedges; e--; ++pe)
                            {
                                int d = was_bad * softwords_weight<SOFTBIT, SOFTWORD>(cw, *pe) * PPCM / vnodes[*pe].nedges;

                                if (commit) {
                                    score[*pe] -= d;
                                } else {
                                    dtot -= d;
                                }
                            }
                        }

                        softwords_flip(expected, c);
                        SOFTBIT is_bad = softwords_xor(expected, received, c);

                        if (softbit_harden(is_bad))
                        {
                            Taddr *pe = cnodes[c].edges;

                            for (int e = cnodes[c].nedges; e--; ++pe)
                            {
                                int d = is_bad * softwords_weight<SOFTBIT, SOFTWORD>(cw, *pe) * PPCM / vnodes[*pe].nedges;

                                if (commit) {
                                    score[*pe] += d;
                                } else {
                                    dtot += d;
                                }
                            }
                        }

                        if (!commit) {
                            softwords_flip(expected, c);
                        }
                    }

                    if (!commit)
                    {
                        if (dtot >= 0)
                        {
                            lfprintf(stderr, "    abort %d\n", v);
                            break; // Next v
                        }
                    }
                    else
                    {
                        softwords_flip(cw, v);
                        ++nflipped;
                        tots += dtot;
                        progress = true;
                        v = k - 1; // Force exit to update maxs ?
                    }
                } // commit
            }     // v
            lfprintf(stderr, "progress %d\n", progress);
#if 0
	fprintf(stderr, "CHECKING TOTS INCREMENT (slow) %d\n", tots);
	score_t tots2 = compute_scores(cw, expected, received, n_k, score, k);
	if ( tots2 != tots ) fail("bad tots update");
#endif
        }

        delete[] score;
        delete[] received;
        delete[] expected;

        return nflipped;
    }

    // EN 302 307-1 5.3.2.1 post-processing of parity bits.
    // In-place allowed.

#if 1
    static void integrate_bits(const SOFTWORD *in, SOFTWORD *out, int nwords)
    {
        SOFTBIT sum;
        softbit_clear(&sum);

        for (int i = 0; i < nwords; ++i)
        {
            SOFTWORD w = in[i];

            for (int b = 0; b < SWSIZE; ++b)
            {
                sum = softbit_xor(sum, softword_get(w, b));
                softword_write(w, b, sum);
            }

            out[i] = w;
        }
    }
#else
    // Optimized for hard_sb
    static void integrate_bits(const uint8_t *in, uint8_t *out, int nwords)
    {
        // TBD Optimize
        uint8_t prev = 0;
        for (int i = 0; i < nwords; ++i)
        {
            uint8_t c = in[i];
            for (int j = SWSIZE; j--;)
            {
                c ^= prev << j;
                prev = (c >> j) & 1;
            }
            out[i] = c;
        }
    }
#endif

    // Undo EN 302 307-1 5.3.2.1, post-processing of parity bits.
    // In-place allowed.

#if 1
    static void diff_bits(const SOFTWORD *in, SOFTWORD *out, int nwords)
    {
        SOFTBIT prev;
        softbit_clear(&prev);

        for (int i = 0; i < nwords; ++i, ++in, ++out)
        {
            SOFTWORD w = *in;

            for (int b = 0; b < SWSIZE; ++b)
            {
                SOFTBIT n = softword_get(w, b);
                softword_write(w, b, softbit_xor(prev, n));
                prev = n;
            }

            *out = w;
        }
    }
#else
    // Optimized for hard_sb
    static void diff_bits(const uint8_t *in, uint8_t *out, int nwords)
    {
        uint8_t prev = 0;
        for (int i = 0; i < nwords; ++i)
        {
            uint8_t c = in[i];
            out[i] = c ^ (prev | (c >> 1));
            prev = (c & 1) << (SWSIZE - 1);
        }
    }
#endif

}; // ldpc_engine

} // namespace leansdr

#endif // LEANSDR_LDPC_H
