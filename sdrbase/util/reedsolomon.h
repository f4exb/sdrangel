/*
 * Reed-Solomon -- Reed-Solomon encoder / decoder library
 *
 * Copyright (c) 2014 Hard Consulting Corporation.
 * Copyright (c) 2006 Phil Karn, KA9Q
 *
 * It may be used under the terms of the GNU Lesser General Public License (LGPL).
 *
 * Simplified version of https://github.com/pjkundert/ezpwd-reed-solomon which
 * seems to be the fastest open-source decoder.
 *
 */

#ifndef REEDSOLOMON_H
#define REEDSOLOMON_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

// Preprocessor defines available:
//
// EZPWD_NO_MOD_TAB -- define to force no "modnn" Galois modulo table acceleration
//
//#define EZPWD_NO_MOD_TAB

namespace ReedSolomon {

//
// reed_solomon_base - Reed-Solomon codec generic base class
//
class reed_solomon_base {
public:
    virtual size_t datum() const = 0;   // a data element's bits
    virtual size_t symbol() const = 0;  // a symbol's bits
    virtual int size() const = 0;       // R-S block size (maximum total symbols)
    virtual int nroots() const = 0;     // R-S roots (parity symbols)
    virtual int load() const = 0;       // R-S net payload (data symbols)

    virtual ~reed_solomon_base() {}

    reed_solomon_base() {}

    //
    // {en,de}code -- Compute/Correct errors/erasures in a Reed-Solomon encoded container
    //
    ///     For decode, optionally specify some known erasure positions (up to nroots()).  If
    /// non-empty 'erasures' is provided, it contains the positions of each erasure.  If a
    /// non-zero pointer to a 'position' vector is provided, its capacity will be increased to
    /// be capable of storing up to 'nroots()' ints; the actual deduced error locations will be
    /// returned.
    ///
    /// RETURN VALUE
    ///
    ///     Return -1 on error.  The encode returns the number of parity symbols produced;
    /// decode returns the number of symbols corrected.  Both errors and erasures are included,
    /// so long as they are actually different than the deduced value.  In other words, if a
    /// symbol is marked as an erasure but it actually turns out to be correct, it's index will
    /// NOT be included in the returned count, nor the modified erasure vector!
    ///

    virtual int encode(const uint8_t *data, int len, uint8_t *parity) const = 0;

    virtual int decode1(uint8_t *data, int len, uint8_t *parity,
                       const std::vector<int> &erasure = std::vector<int>(), std::vector<int> *position = 0) const = 0;

    int decode(uint8_t *data,
               int len,
               int pad = 0,  // ignore 'pad' symbols at start of array
               const std::vector<int> &erasure = std::vector<int>(),
               std::vector<int> *position = 0) const
    {
        return decode1((uint8_t*)(data + pad), len, (uint8_t*)(data + len), erasure, position);
    }

};

//
// gfpoly - default field polynomial generator functor.
//
template <int PLY>
struct gfpoly {
    int operator()(int sr) const
    {
        if (sr == 0) {
            sr = 1;
        } else {
            sr <<= 1;
            if (sr & (1 << 8))
                sr ^= PLY;
            sr &= ((1 << 8) - 1);
        }
        return sr;
    }
};

//
// class reed_solomon_tabs -- R-S tables common to all RS(NN,*) with same SYM, PRM and PLY
//
template <int PRM, class PLY>
class reed_solomon_tabs : public reed_solomon_base {
public:
    typedef uint8_t symbol_t;
    static const size_t DATUM = 8;     // bits
    static const size_t SYMBOL = 8;  // bits / symbol
    static const int MM = 8;
    static const int SIZE = (1 << 8) - 1;  // maximum symbols in field
    static const int NN = SIZE;
    static const int A0 = SIZE;
    static const int MODS  // modulo table: 1/2 the symbol size squared, up to 4k
#if defined(EZPWD_NO_MOD_TAB)
        = 0;
#else
        = 8 > 8 ? (1 << 12) : (1 << 8 << 8 / 2);
#endif

    static int iprim;  // initialized to -1, below

protected:
    static std::array<uint8_t, NN + 1> alpha_to;
    static std::array<uint8_t, NN + 1> index_of;
    static std::array<uint8_t, MODS> mod_of;
    virtual ~reed_solomon_tabs() {}

    reed_solomon_tabs() : reed_solomon_base()
    {
        // Do init if not already done.  We check one value which is initialized to -1; this is
        // safe, 'cause the value will not be set 'til the initializing thread has completely
        // initialized the structure.  Worst case scenario: multiple threads will initialize
        // identically.  No mutex necessary.
        if (iprim >= 0)
            return;

        // Generate Galois field lookup tables
        index_of[0] = A0;  // log(zero) = -inf
        alpha_to[A0] = 0;  // alpha**-inf = 0
        PLY poly;
        int sr = poly(0);
        for (int i = 0; i < NN; i++) {
            index_of[sr] = i;
            alpha_to[i] = sr;
            sr = poly(sr);
        }
        // If it's not primitive, raise exception or abort
        if (sr != alpha_to[0]) {
            abort();
        }

        // Generate modulo table for some commonly used (non-trivial) values
        for (int x = NN; x < NN + MODS; ++x)
            mod_of[x - NN] = _modnn(x);
        // Find prim-th root of 1, index form, used in decoding.
        int iptmp = 1;
        while (iptmp % PRM != 0)
            iptmp += NN;
        iprim = iptmp / PRM;
    }

    //
    // modnn -- modulo replacement for galois field arithmetics, optionally w/ table acceleration
    //
    //  @x:         the value to reduce (will never be -'ve)
    //
    //  where
    //  MM = number of bits per symbol
    //  NN = (2^MM) - 1
    //
    //  Simple arithmetic modulo would return a wrong result for values >= 3 * NN
    //
    uint8_t _modnn(int x) const
    {
        while (x >= NN) {
            x -= NN;
            x = (x >> MM) + (x & NN);
        }
        return x;
    }

    uint8_t modnn(int x) const
    {
        while (x >= NN + MODS) {
            x -= NN;
            x = (x >> MM) + (x & NN);
        }
        if (MODS && x >= NN)
            x = mod_of[x - NN];
        return x;
    }
};

//
// class reed_solomon - Reed-Solomon codec
//
// @TYP:            A symbol datum; {en,de}code operates on arrays of these
// @DATUM:          Bits per datum (a TYP())
// @SYM{BOL}, MM:   Bits per symbol
// @NN:             Symbols per block (== (1<<MM)-1)
// @alpha_to:       log lookup table
// @index_of:       Antilog lookup table
// @genpoly:        Generator polynomial
// @NROOTS:         Number of generator roots = number of parity symbols
// @FCR:            First consecutive root, index form
// @PRM:            Primitive element, index form
// @iprim:          prim-th root of 1, index form
// @PLY:            The primitive generator polynominal functor
//
//     All reed_solomon<T, ...> instances with the same template type parameters share a common
// (static) set of alpha_to, index_of and genpoly tables.  The first instance to be constructed
// initializes the tables.
//
//     Each specialized type of reed_solomon implements a specific encode/decode method
// appropriate to its datum 'TYP'.  When accessed via a generic reed_solomon_base pointer, only
// access via "safe" (size specifying) containers or iterators is available.
//
template <int RTS, int FCR, int PRM, class PLY>
class reed_solomon : public reed_solomon_tabs<PRM, PLY> {
public:
    typedef reed_solomon_tabs<PRM, PLY> tabs_t;
    using tabs_t::A0;
    using tabs_t::DATUM;
    using tabs_t::MM;
    using tabs_t::NN;
    using tabs_t::SIZE;
    using tabs_t::SYMBOL;

    using tabs_t::iprim;

    using tabs_t::alpha_to;
    using tabs_t::index_of;

    using tabs_t::modnn;

    static const int NROOTS = RTS;
    static const int LOAD = SIZE - NROOTS;  // maximum non-parity symbol payload

protected:
    static std::array<uint8_t, NROOTS + 1> genpoly;

public:
    virtual size_t datum() const { return DATUM; }

    virtual size_t symbol() const { return SYMBOL; }

    virtual int size() const { return SIZE; }

    virtual int nroots() const { return NROOTS; }

    virtual int load() const { return LOAD; }

    using reed_solomon_base::decode;
    virtual int decode1(uint8_t *data, int len, uint8_t *parity,
                       const std::vector<int> &erasure = std::vector<int>(), std::vector<int> *position = 0) const
    {
        return decode_mask(data, len, parity, erasure, position);
    }

    //
    // decode_mask  -- mask INP data into valid SYMBOL data
    //
    ///     Incoming data may be in a variety of sizes, and may contain information beyond the
    /// R-S symbol capacity.  For example, we might use a 6-bit R-S symbol to correct the lower
    /// 6 bits of an 8-bit data character.  This would allow us to correct common substitution
    /// errors (such as '2' for '3', 'R' for 'T', 'n' for 'm').
    ///
    int decode_mask(uint8_t *data, int len,
                    uint8_t *parity = 0,  // either 0, or pointer to all parity symbols
                    const std::vector<int> &erasure = std::vector<int>(), std::vector<int> *position = 0) const
    {
        if (!parity) {
            len -= NROOTS;
            parity = data + len;
        }

        int corrects;
        if (!erasure.size() && !position) {
            // No erasures, and error position info not wanted.
            corrects = decode(data, len, parity);
        } else {
            // Either erasure location info specified, or resultant error position info wanted;
            // Prepare pos (a temporary, if no position vector provided), and copy any provided
            // erasure positions.  After number of corrections is known, resize the position
            // vector.  Thus, we use any supplied erasure info, and optionally return any
            // correction position info separately.
            std::vector<int> _pos;
            std::vector<int> &pos = position ? *position : _pos;
            pos.resize(std::max(size_t(NROOTS), erasure.size()));
            std::copy(erasure.begin(), erasure.end(), pos.begin());
            corrects = decode(data, len, parity, &pos.front(), erasure.size());
            if (corrects > int(pos.size())) {
                return -1;
            }
            pos.resize(std::max(0, corrects));
        }

        return corrects;
    }

    virtual ~reed_solomon()
    {
    }

    reed_solomon() : reed_solomon_tabs<PRM, PLY>()
    {
        // We check one element of the array; this is safe, 'cause the value will not be
        // initialized 'til the initializing thread has completely initialized the array.  Worst
        // case scenario: multiple threads will initialize identically.  No mutex necessary.
        if (genpoly[0])
            return;

        std::array<uint8_t, NROOTS + 1> tmppoly;  // uninitialized
        // Form RS code generator polynomial from its roots.  Only lower-index entries are
        // consulted, when computing subsequent entries; only index 0 needs initialization.
        tmppoly[0] = 1;
        for (int i = 0, root = FCR * PRM; i < NROOTS; i++, root += PRM) {
            tmppoly[i + 1] = 1;
            // Multiply tmppoly[] by  @**(root + x)
            for (int j = i; j > 0; j--) {
                if (tmppoly[j] != 0)
                    tmppoly[j] = tmppoly[j - 1] ^ alpha_to[modnn(index_of[tmppoly[j]] + root)];
                else
                    tmppoly[j] = tmppoly[j - 1];
            }
            // tmppoly[0] can never be zero
            tmppoly[0] = alpha_to[modnn(index_of[tmppoly[0]] + root)];
        }
        // convert NROOTS entries of tmppoly[] to genpoly[] in index form for quicker encoding,
        // in reverse order so genpoly[0] is last element initialized.
        for (int i = NROOTS; i >= 0; --i)
            genpoly[i] = index_of[tmppoly[i]];
    }

    virtual int encode(const uint8_t *data, int len, uint8_t *parity)  // at least nroots
        const
    {
        // Check length parameter for validity
        for (int i = 0; i < NROOTS; i++)
            parity[i] = 0;
        for (int i = 0; i < len; i++) {
            uint8_t feedback = index_of[data[i] ^ parity[0]];
            if (feedback != A0) {
                for (int j = 1; j < NROOTS; j++)
                    parity[j] ^= alpha_to[modnn(feedback + genpoly[NROOTS - j])];
            }

            std::rotate(parity, parity + 1, parity + NROOTS);
            if (feedback != A0)
                parity[NROOTS - 1] = alpha_to[modnn(feedback + genpoly[0])];
            else
                parity[NROOTS - 1] = 0;
        }
        return NROOTS;
    }

    int decode(uint8_t *data, int len,
               uint8_t *parity,    // Requires: at least NROOTS
               int *eras_pos = 0,  // Capacity: at least NROOTS
               int no_eras = 0,    // Maximum:  at most  NROOTS
               uint8_t *corr = 0)  // Capacity: at least NROOTS
        const
    {
        typedef std::array<uint8_t, NROOTS> typ_nroots;
        typedef std::array<uint8_t, NROOTS + 1> typ_nroots_1;
        typedef std::array<int, NROOTS> int_nroots;

        typ_nroots_1 lambda{{0}};
        typ_nroots syn;
        typ_nroots_1 b;
        typ_nroots_1 t;
        typ_nroots_1 omega;
        int_nroots root;
        typ_nroots_1 reg;
        int_nroots loc;
        int count = 0;

        // Check length parameter and erasures for validity
        int pad = NN - NROOTS - len;
        if (no_eras) {
            if (no_eras > NROOTS) {
                return -1;
            }
            for (int i = 0; i < no_eras; ++i) {
                if (eras_pos[i] < 0 || eras_pos[i] >= len + NROOTS) {
                    return -1;
                }
            }
        }

        // form the syndromes; i.e., evaluate data(x) at roots of g(x)
        for (int i = 0; i < NROOTS; i++)
            syn[i] = data[0];

        for (int j = 1; j < len; j++) {
            for (int i = 0; i < NROOTS; i++) {
                if (syn[i] == 0) {
                    syn[i] = data[j];
                } else {
                    syn[i] = data[j] ^ alpha_to[modnn(index_of[syn[i]] + (FCR + i) * PRM)];
                }
            }
        }

        for (int j = 0; j < NROOTS; j++) {
            for (int i = 0; i < NROOTS; i++) {
                if (syn[i] == 0) {
                    syn[i] = parity[j];
                } else {
                    syn[i] = parity[j] ^ alpha_to[modnn(index_of[syn[i]] + (FCR + i) * PRM)];
                }
            }
        }

        // Convert syndromes to index form, checking for nonzero condition
        uint8_t syn_error = 0;
        for (int i = 0; i < NROOTS; i++) {
            syn_error |= syn[i];
            syn[i] = index_of[syn[i]];
        }

        int deg_lambda = 0;
        int deg_omega = 0;
        int r = no_eras;
        int el = no_eras;
        if (!syn_error) {
            // if syndrome is zero, data[] is a codeword and there are no errors to correct.
            count = 0;
            goto finish;
        }

        lambda[0] = 1;
        if (no_eras > 0) {
            // Init lambda to be the erasure locator polynomial.  Convert erasure positions
            // from index into data, to index into Reed-Solomon block.
            lambda[1] = alpha_to[modnn(PRM * (NN - 1 - (eras_pos[0] + pad)))];
            for (int i = 1; i < no_eras; i++) {
                uint8_t u = modnn(PRM * (NN - 1 - (eras_pos[i] + pad)));
                for (int j = i + 1; j > 0; j--) {
                    uint8_t tmp = index_of[lambda[j - 1]];
                    if (tmp != A0) {
                        lambda[j] ^= alpha_to[modnn(u + tmp)];
                    }
                }
            }
        }

        for (int i = 0; i < NROOTS + 1; i++)
            b[i] = index_of[lambda[i]];

        //
        // Begin Berlekamp-Massey algorithm to determine error+erasure locator polynomial
        //
        while (++r <= NROOTS) {  // r is the step number
            // Compute discrepancy at the r-th step in poly-form
            uint8_t discr_r = 0;
            for (int i = 0; i < r; i++) {
                if ((lambda[i] != 0) && (syn[r - i - 1] != A0)) {
                    discr_r ^= alpha_to[modnn(index_of[lambda[i]] + syn[r - i - 1])];
                }
            }
            discr_r = index_of[discr_r];  // Index form
            if (discr_r == A0) {
                // 2 lines below: B(x) <-- x*B(x)
                // Rotate the last element of b[NROOTS+1] to b[0]
                std::rotate(b.begin(), b.begin() + NROOTS, b.end());
                b[0] = A0;
            } else {
                // 7 lines below: T(x) <-- lambda(x)-discr_r*x*b(x)
                t[0] = lambda[0];
                for (int i = 0; i < NROOTS; i++) {
                    if (b[i] != A0) {
                        t[i + 1] = lambda[i + 1] ^ alpha_to[modnn(discr_r + b[i])];
                    } else
                        t[i + 1] = lambda[i + 1];
                }
                if (2 * el <= r + no_eras - 1) {
                    el = r + no_eras - el;
                    // 2 lines below: B(x) <-- inv(discr_r) * lambda(x)
                    for (int i = 0; i <= NROOTS; i++) {
                        b[i] = ((lambda[i] == 0) ? A0 : modnn(index_of[lambda[i]] - discr_r + NN));
                    }
                } else {
                    // 2 lines below: B(x) <-- x*B(x)
                    std::rotate(b.begin(), b.begin() + NROOTS, b.end());
                    b[0] = A0;
                }
                lambda = t;
            }
        }

        // Convert lambda to index form and compute deg(lambda(x))
        for (int i = 0; i < NROOTS + 1; i++) {
            lambda[i] = index_of[lambda[i]];
            if (lambda[i] != NN)
                deg_lambda = i;
        }
        // Find roots of error+erasure locator polynomial by Chien search
        reg = lambda;
        count = 0;  // Number of roots of lambda(x)
        for (int i = 1, k = iprim - 1; i <= NN; i++, k = modnn(k + iprim)) {
            uint8_t q = 1;  // lambda[0] is always 0
            for (int j = deg_lambda; j > 0; j--) {
                if (reg[j] != A0) {
                    reg[j] = modnn(reg[j] + j);
                    q ^= alpha_to[reg[j]];
                }
            }
            if (q != 0)
                continue;  // Not a root
            // store root (index-form) and error location number
            root[count] = i;
            loc[count] = k;
            // If we've already found max possible roots, abort the search to save time
            if (++count == deg_lambda)
                break;
        }
        if (deg_lambda != count) {
            // deg(lambda) unequal to number of roots => uncorrectable error detected
            count = -1;
            goto finish;
        }
        //
        // Compute err+eras evaluator poly omega(x) = s(x)*lambda(x) (modulo x**NROOTS). in
        // index form. Also find deg(omega).
        //
        deg_omega = deg_lambda - 1;
        for (int i = 0; i <= deg_omega; i++) {
            uint8_t tmp = 0;
            for (int j = i; j >= 0; j--) {
                if ((syn[i - j] != A0) && (lambda[j] != A0))
                    tmp ^= alpha_to[modnn(syn[i - j] + lambda[j])];
            }
            omega[i] = index_of[tmp];
        }

        //
        // Compute error values in poly-form. num1 = omega(inv(X(l))), num2 = inv(X(l))**(fcr-1)
        // and den = lambda_pr(inv(X(l))) all in poly-form
        //
        for (int j = count - 1; j >= 0; j--) {
            uint8_t num1 = 0;
            for (int i = deg_omega; i >= 0; i--) {
                if (omega[i] != A0)
                    num1 ^= alpha_to[modnn(omega[i] + i * root[j])];
            }
            uint8_t num2 = alpha_to[modnn(root[j] * (FCR - 1) + NN)];
            uint8_t den = 0;

            // lambda[i+1] for i even is the formal derivative lambda_pr of lambda[i]
            for (int i = std::min(deg_lambda, NROOTS - 1) & ~1; i >= 0; i -= 2) {
                if (lambda[i + 1] != A0) {
                    den ^= alpha_to[modnn(lambda[i + 1] + i * root[j])];
                }
            }
            // Apply error to data.  Padding ('pad' unused symbols) begin at index 0.
            if (num1 != 0) {
                if (loc[j] < pad) {
                    // If the computed error position is in the 'pad' (the unused portion of the
                    // R-S data capacity), then our solution has failed -- we've computed a
                    // correction location outside of the data and parity we've been provided!
                    count = -1;
                    goto finish;
                }

                uint8_t cor = alpha_to[modnn(index_of[num1] + index_of[num2] + NN - index_of[den])];
                // Store the error correction pattern, if a correction buffer is available
                if (corr)
                    corr[j] = cor;
                // If a data/parity buffer is given and the error is inside the message or
                // parity data, correct it
                if (loc[j] < (NN - NROOTS)) {
                    if (data) {
                        data[loc[j] - pad] ^= cor;
                    }
                } else if (loc[j] < NN) {
                    if (parity)
                        parity[loc[j] - (NN - NROOTS)] ^= cor;
                }
            }
        }

    finish:
        if (eras_pos != NULL) {
            for (int i = 0; i < count; i++)
                eras_pos[i] = loc[i] - pad;
        }
        return count;
    }
};

//
// Define the static reed_solomon...<...> members; allowed in header for template types.
//
//     The reed_solomon_tags<...>::iprim < 0 is used to indicate to the first instance that the
// static tables require initialization.
//
template <int PRM, class PLY>
int reed_solomon_tabs<PRM, PLY>::iprim = -1;

template <int PRM, class PLY>
std::array<uint8_t, reed_solomon_tabs<PRM, PLY>::NN + 1> reed_solomon_tabs<PRM, PLY>::alpha_to;

template <int PRM, class PLY>
std::array<uint8_t, reed_solomon_tabs<PRM, PLY>::NN + 1> reed_solomon_tabs<PRM, PLY>::index_of;
template <int PRM, class PLY>
std::array<uint8_t, reed_solomon_tabs<PRM, PLY>::MODS> reed_solomon_tabs<PRM, PLY>::mod_of;

template <int RTS, int FCR, int PRM, class PLY>
std::array<uint8_t, reed_solomon<RTS, FCR, PRM, PLY>::NROOTS + 1> reed_solomon<RTS, FCR, PRM, PLY>::genpoly;

//
// RS( ... ) -- Define a reed-solomon codec
//
// @SYMBOLS:        Total number of symbols; must be a power of 2 minus 1, eg 2^8-1 == 255
// @PAYLOAD:        The maximum number of non-parity symbols, eg 253 ==> 2 parity symbols
// @POLY:           A primitive polynomial appropriate to the SYMBOLS size
// @FCR:            The first consecutive root of the Reed-Solomon generator polynomial
// @PRIM:           The primitive root of the generator polynomial
//

//
// RS<SYMBOLS, PAYLOAD> -- Standard partial specializations for Reed-Solomon codec type access
//
//     Normally, Reed-Solomon codecs are described with terms like RS(255,252).  Obtain various
// standard Reed-Solomon codecs using macros of a similar form, eg. RS<255, 252>. Standard PLY,
// FCR and PRM values are provided for various SYMBOL sizes, along with appropriate basic types
// capable of holding all internal Reed-Solomon tabular data.
//
//     In order to provide "default initialization" of const RS<...> types, a user-provided
// default constructor must be provided.
//
template <size_t SYMBOLS, size_t PAYLOAD>
struct RS;
template <size_t PAYLOAD>
struct RS<255, PAYLOAD> : public ReedSolomon::reed_solomon<(255) - (PAYLOAD), 0, 1, ReedSolomon::gfpoly<0x11d>>
{
    RS()
        : ReedSolomon::reed_solomon<(255) - (PAYLOAD), 0, 1, ReedSolomon::gfpoly<0x11d>>()
    {
    }
};

}  // namespace ReedSolomon

#endif  // REEDSOLOMON_H
