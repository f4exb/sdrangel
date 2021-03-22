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

#ifndef LEANSDR_BCH_H
#define LEANSDR_BCH_H

#include "leansdr/discrmath.h"

namespace leansdr
{

// Interface to hide the template parameters

struct bch_interface
{
    virtual ~bch_interface() {}
    virtual void encode(const uint8_t *msg, size_t msgbytes, uint8_t *out) = 0;
    virtual int decode(uint8_t *cw, size_t cwbytes) = 0;
}; // bch_interface

// BCH error correction.
// T: Unsigned type for packing binary polynomials.
// N: Number of parity bits.
// NP: Width of the polynomials supplied.
// DP: Actual degree of the minimum polynomials (all must be same).
// TGF: Unsigned type for syndromes (must be wider than DP).
// GFTRUNCGEN: Generator polynomial for GF(2^DP), with X^DP omitted.

template <typename T, int N, int NP, int DP, typename TGF, int GFTRUNCGEN>
struct bch_engine : bch_interface
{
    bch_engine(
        const bitvect<T, NP> *polys,
        int _npolys
    ) :
        npolys(_npolys)
    {
        // Build the generator polynomial (product of polys[]).
        g = 1;

        for (int i = 0; i < npolys; ++i) {
            g = g * polys[i];
        }

        // Convert the polynomials to truncated representation
        // (with X^DP omitted) for use with divmod().
        truncpolys = new bitvect<T, DP>[npolys];

        for (int i = 0; i < npolys; ++i) {
            truncpolys[i].copy(polys[i]);
        }

        // Check which polynomial contains each root.
        // Note: The DVB-S2 polynomials are numbered so that
        // syndpoly[2*i]==i, but we don't use that property.
        syndpolys = new int[2 * npolys];

        for (int i = 0; i < 2 * npolys; ++i)
        {
            int j;

            for (j = 0; j < npolys; ++j)
            {
                if (!eval_poly(truncpolys[j], true, 1 + i)) {
                    break;
                }
            }

            if (j == npolys) {
                fail("Bad polynomials/root");
            }

            syndpolys[i] = j;
        }
    }

    virtual ~bch_engine()
    {
        delete[] truncpolys;
        delete[] syndpolys;
    }

    // Generate BCH parity bits.

    void encode(const uint8_t *msg, size_t msgbytes, uint8_t *out)
    {
        bitvect<T, N> parity = shiftdivmod(msg, msgbytes, g);

        // Output as bytes, coefficient of highest degree first
        for (int i = N / 8; i--; ++out) {
            *out = parity.v[i / sizeof(T)] >> ((i & (sizeof(T) - 1)) * 8);
        }
    }

    // Decode BCH.
    // Return number of bits corrected, or -1 on failure.

    int decode(uint8_t *cw, size_t cwbytes)
    {
        //again:
        bool corrupted = false;
        // Divide by individual polynomials.
        // TBD Maybe do in parallel, scanning cw only once.
        bitvect<T, DP> *rem = new bitvect<T, DP>[npolys]; // npolys is not static hence 'bitvect<T, DP> rem[npolys]' does not compile in all compilers

        for (int j = 0; j < npolys; ++j)
        {
            rem[j] = divmod(cw, cwbytes, truncpolys[j]);
        }

        // Compute syndromes.
        TGF *S = new TGF[2 * npolys]; // npolys is not static hence 'TGF S[2 * npolys]' does not compile in all compilers

        for (int i = 0; i < 2 * npolys; ++i)
        {
            // Compute R(alpha^(1+i)), exploiting the fact that
            // R(x)=Q(x)g_j(X)+rem_j(X) and g_j(alpha^(1+i))=0
            // for some j that we already determined.
            // TBD Compute even exponents using conjugates.
            S[i] = eval_poly(rem[syndpolys[i]], false, 1 + i);

            if (S[i]) {
                corrupted = true;
            }
        }

        if (!corrupted)
        {
            delete[] S;
            delete[] rem;
            return 0;
        }
#if 0
      fprintf(stderr, "synd:");
      for ( int i=0; i<2*npolys; ++i ) fprintf(stderr, " %04x", S[i]);
      fprintf(stderr, "\n");
#endif

        // S_j = R(alpha_j) = 0+E(alpha_j) = sum(l=1..L)((alpha^j)^i_l)
        // where i_1 .. i_L are the degrees of the non-zero coefficients of E.
        // S_j = sum(l=1..L)((alpha^i_l)^j) = sum(l=1..L)(X_l^j)

        // Berlekamp - Massey
        // http://en.wikipedia.org/wiki/Berlekamp%E2%80%93Massey_algorithm
        // TBD More efficient to work with logs of syndromes ?

        int NN = 2 * npolys;
        TGF *C = new TGF[NN];
        C[0] = 1;
        TGF *B = new TGF[NN];
        B[0] = 1;
        // TGF C[NN] = { crap code
        //     1,
        // },
        //     B[NN] = {
        //         1,
        //     };
        int L = 0, m = 1;
        TGF b = 1;

        for (int n = 0; n < NN; ++n)
        {
            TGF d = S[n];

            for (int i = 1; i <= L; ++i) {
                d = GF.add(d, GF.mul(C[i], S[n - i]));
            }

            if (d == 0)
            {
                ++m;
            }
            else
            {
                TGF d_div_b = GF.mul(d, GF.inv(b));

                if (2 * L <= n)
                {
                    TGF *tmp = new TGF[NN]; // replaced crap code
                    std::copy(C, C+NN, tmp);   //memcpy(tmp, C, sizeof(tmp));

                    for (int i = 0; i < NN - m; ++i) {
                        C[m + i] = GF.sub(C[m + i], GF.mul(d_div_b, B[i]));
                    }

                    L = n + 1 - L;
                    std::copy(tmp, tmp+NN, B); //memcpy(B, tmp, sizeof(B));
                    b = d;
                    m = 1;

                    delete[] tmp;
                }
                else
                {
                    for (int i = 0; i < NN - m; ++i) {
                        C[m + i] = GF.sub(C[m + i], GF.mul(d_div_b, B[i]));
                    }

                    ++m;
                }
            }
        }
        // L is the number of errors.
        // C of degree L is the error locator polynomial (Lambda).
        // C(X) = sum(l=1..L)(1-X_l*X).
#if 0
      fprintf(stderr, "C[%d]=", L);
      for ( int i=0; i<NN; ++i ) fprintf(stderr, " %04x", C[i]);
      fprintf(stderr, "\n");
#endif

        // Forney
        // http://en.wikipedia.org/wiki/Forney_algorithm
        // Simplified because coefficients are in GF(2).

        // Find zeroes of C by exhaustive search.
        // TODO Chien method
        int roots_found = 0;

        for (int i = 0; i < (1 << DP) - 1; ++i)
        {
            // Candidate root ALPHA^i
            TGF v = eval_poly(C, L, i);

            if (!v)
            {
                // ALPHA^i is a root of C, i.e. the inverse of an X_l.
                int loc = (i ? (1 << DP) - 1 - i : 0); // exponent of inverse
                // Reverse because cw[0..cwbytes-1] is stored MSB first
                int rloc = cwbytes * 8 - 1 - loc;

                if (rloc < 0)
                {
                    // This may happen if the code is used truncated.
                    delete[] C;
                    delete[] B;
                    delete[] S;
                    delete[] rem;
                    return -1;
                }

                cw[rloc / 8] ^= 128 >> (rloc & 7);
                ++roots_found;

                if (roots_found == L) {
                    break;
                }
            }
        }

        delete[] C;
        delete[] B;
        delete[] S;
        delete[] rem;

        if (roots_found != L) {
            return -1;
        }

        return L;
    }

  private:
    // Eval a GF(2)[X] polynomial at a power of ALPHA.

    TGF eval_poly(const bitvect<T, DP> &poly, bool is_trunc, int rootexp)
    {
        TGF acc = 0;
        int re = 0;

        for (int i = 0; i < DP; ++i)
        {
            if (poly[i]) {
                acc = GF.add(acc, GF.exp(re));
            }

            re += rootexp;

            if (re >= (1 << DP) - 1) {
                re -= (1 << DP) - 1; // mod 2^DP-1 incrementally
            }
        }

        if (is_trunc) {
            acc = GF.add(acc, GF.exp(re));
        }

        return acc;
    }

    // Eval a GF(2^16)[X] polynomial at a power of ALPHA.

    TGF eval_poly(const TGF *poly, int deg, int rootexp)
    {
        TGF acc = 0;
        int re = 0;

        for (int i = 0; i <= deg; ++i)
        {
            acc = GF.add(acc, GF.mul(poly[i], GF.exp(re)));
            re += rootexp;

            if (re >= (1 << DP) - 1) {
                re -= (1 << DP) - 1; // mod 2^DP-1 incrementally
            }
        }

        return acc;
    }

    bitvect<T, DP> *truncpolys;
    int npolys;
    int *syndpolys;
    bitvect<T, N> g;
    // Finite group for syndrome calculations
    gf2n<TGF, DP, 2, GFTRUNCGEN> GF;
}; // bch_engine

} // namespace leansdr

#endif // LEANSDR_BCH_H
