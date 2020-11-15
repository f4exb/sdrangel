// This file is part of LeanSDR Copyright (C) 2018 <pabr@pabr.org>.
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

#ifndef LEANSDR_DISCRMATH_H
#define LEANSDR_DISCRMATH_H

#pragma GCC diagnostic ignored "-Wshift-negative-value"

#include  <cstddef>

namespace leansdr
{

// Polynomials with N binary coefficients.
// This is designed to compile into trivial inline code.

// Bits are packed into words of type T.
// Unused most-significant bits of the last word are 0.
// T must be an unsigned type.

template <typename T, int N>
struct bitvect
{
    static const size_t WSIZE = sizeof(T) * 8;
    static const size_t NW = (N + WSIZE - 1) / WSIZE;
    T v[NW];

    bitvect() {}
    bitvect(T val)
    {
        v[0] = val;
        for (unsigned int i = 1; i < NW; ++i)
            v[i] = 0;
    }

    // Assign from another bitvect, with truncation or padding
    template <int M>
    bitvect<T, N> &copy(const bitvect<T, M> &a)
    {
        int nw = (a.NW < NW) ? a.NW : NW;
        for (int i = 0; i < nw; ++i)
            v[i] = a.v[i];
        if (M < N)
            for (int i = a.NW; i < NW; ++i)
                v[i] = 0;
        if (M > N)
            truncate_to_N();
        return *this;
    }

    bool operator[](unsigned int i) const
    {
        return v[i / WSIZE] & ((T)1 << (i & (WSIZE - 1)));
    }

    // Add (in GF(2))

    template <int M>
    bitvect<T, N> &operator+=(const bitvect<T, M> &a)
    {
        int nw = (a.NW < NW) ? a.NW : NW;
        for (int i = 0; i < nw; ++i)
            v[i] ^= a.v[i];
        if (M > N)
            truncate_to_N();
        return *this;
    }

    // Multiply by X^n

    bitvect<T, N> &operator<<=(unsigned int n)
    {
        if (n >= WSIZE)
            fail("shift exceeds word width");
        T mask = ~(((T)-1) << n);
        for (int i = NW - 1; i > 0; --i)
            v[i] = (v[i] << n) | ((v[i - 1] >> (WSIZE - n)) & mask);
        v[0] <<= n;
        if (N != NW * WSIZE)
            truncate_to_N();
        return *this;
    }

  private:
    // Call this whenever bits N .. NW*WSIZE-1 may have been set.
    inline void truncate_to_N()
    {
        v[NW - 1] &= ((T)-1) >> (NW * WSIZE - N);
    }

}; // bitvect

// Return m modulo (X^N+p).
// p is typically a generator polynomial of degree N
// with the highest-coefficient monomial omitted.
// m is packed into nm words of type Tm.
// The MSB of m[0] is the highest-order coefficient of m.

template <typename T, int N, typename Tm>
bitvect<T, N> divmod(const Tm *m, size_t nm, const bitvect<T, N> &p)
{
    bitvect<T, N> res = 0;
    const Tm bitmask = (Tm)1 << (sizeof(Tm) * 8 - 1);
    for (; nm--; ++m)
    {
        Tm mi = *m;
        for (int bit = sizeof(Tm) * 8; bit--; mi <<= 1)
        {
            // Multiply by X, save outgoing coeff of degree N
            bool resN = res[N - 1];
            res <<= 1;
            // Add m[i]
            if (mi & bitmask)
                res.v[0] ^= 1;
            // Modulo X^N+p
            if (resN)
                res += p;
        }
    }
    return res;
}

// Return (m*X^N) modulo (X^N+p).
// Same as divmod(), slightly faster than appending N zeroes.

template <typename T, int N, typename Tm>
bitvect<T, N> shiftdivmod(const Tm *m, size_t nm, const bitvect<T, N> &p,
                          T init = 0)
{
    bitvect<T, N> res;
    for (unsigned int i = 0; i < res.NW; ++i)
        res.v[i] = init;
    const Tm bitmask = (Tm)1 << (sizeof(Tm) * 8 - 1);
    for (; nm--; ++m)
    {
        Tm mi = *m;
        for (int bit = sizeof(Tm) * 8; bit--; mi <<= 1)
        {
            // Multiply by X, save outgoing coeff of degree N
            bool resN = res[N - 1];
            res <<= 1;
            // Add m[i]*X^N
            resN ^= (bool)(mi & bitmask);
            // Modulo X^N+p
            if (resN)
                res += p;
        }
    }
    return res;
}

template <typename T, int N>
bool operator==(const bitvect<T, N> &a, const bitvect<T, N> &b)
{
    for (int i = 0; i < a.NW; ++i)
        if (a.v[i] != b.v[i])
            return false;
    return true;
}

// Add (in GF(2))

template <typename T, int N>
bitvect<T, N> operator+(const bitvect<T, N> &a, const bitvect<T, N> &b)
{
    bitvect<T, N> res;
    for (int i = 0; i < a.NW; ++i)
        res.v[i] = a.v[i] ^ b.v[i];
    return res;
}

// Polynomial multiplication.

template <typename T, int N, int NB>
bitvect<T, N> operator*(bitvect<T, N> a, const bitvect<T, NB> &b)
{
    bitvect<T, N> res = 0;
    for (int i = 0; i < NB; ++i, a <<= 1)
        if (b[i])
            res += a;
    // TBD If truncation is needed, do it only once at the end.
    return res;
}

// Finite group GF(2^N), for small N.

// GF(2) is the ring ({0,1},+,*).
// GF(2)[X] is the ring of polynomials with coefficients in GF(2).
// P(X) is an irreducible polynomial of GF(2)[X].
// N is the degree of P(x).
// GF(2)[X]/(P) is GF(2)[X] modulo P(X).
// (GF(2)[X]/(P), +) is a group with 2^N elements.
// (GF(2)[X]/(P)*, *) is a group with 2^N-1 elements.
// (GF(2)[X]/(P), +, *) is a field with 2^N elements, noted GF(2^N).

// Te is a C++ integer type for encoding elements of GF(2^N).
// Binary coefficients are packed, with degree 0 at LSB.
//   (Te)0 is 0
//   (Te)1 is 1
//   (Te)2 is X
//   (Te)3 is X+1
//   (Te)4 is X^2
// TRUNCP is the encoding of the generator, with highest monomial omitted.
// ALPHA is a primitive element of GF(2^N). Usually "2"=[X] is chosen.

template <typename Te, int N, Te ALPHA, Te TRUNCP>
struct gf2n
{
    typedef Te element;
    static const Te alpha = ALPHA;
    gf2n()
    {
        if (ALPHA != 2)
            fail("alpha!=2 not implemented");
        // Precompute log and exp tables.
        Te alpha_i = 1; // ALPHA^0
        for (int i = 0; i < (1 << N); ++i)
        {
            lut_exp[i] = alpha_i;                  // ALPHA^i
            lut_exp[((1 << N) - 1) + i] = alpha_i; // Wrap to avoid modulo 2^N-1
            lut_log[alpha_i] = i;
            bool overflow = alpha_i & (1 << (N - 1));
            alpha_i *= 2;                // Multiply by alpha=[X] i.e. increase degrees
            alpha_i &= ~((~(Te)0) << N); // In case Te is wider than N bits
            if (overflow)
                alpha_i ^= TRUNCP; // Modulo P iteratively
        }
    }
    inline Te add(Te x, Te y) { return x ^ y; } // Addition modulo 2
    inline Te sub(Te x, Te y) { return x ^ y; } // Subtraction modulo 2
    inline Te mul(Te x, Te y)
    {
        if (!x || !y)
            return 0;
        return lut_exp[lut_log[x] + lut_log[y]];
    }
    inline Te div(Te x, Te y)
    {
        if (!x)
            return 0;
        return lut_exp[lut_log[x] + ((1 << N) - 1) - lut_log[y]];
    }
    inline Te inv(Te x)
    {
        return lut_exp[((1 << N) - 1) - lut_log[x]];
    }
    inline Te exp(Te x) { return lut_exp[x]; }
    inline Te log(Te x) { return lut_log[x]; }

  private:
    Te lut_exp[(1 << N) * 2]; // Extra room for wrapping modulo 2^N-1
    Te lut_log[1 << N];
}; // gf2n

} // namespace leansdr

#endif // LEANSDR_DISCRMATH_H
