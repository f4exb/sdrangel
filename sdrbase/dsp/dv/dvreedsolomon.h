///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_DSP_DV_DVREEDSOLOMON_H
#define INCLUDE_DSP_DV_DVREEDSOLOMON_H

#include <cstdint>
#include <vector>

#include "export.h"

/**
 * GF(2^m), m <= 8, with the given primitive polynomial (including the x^m term).
 * alpha = 2 is the primitive element.
 */
class SDRBASE_API DVGaloisField
{
public:
    DVGaloisField(int m, uint32_t primitivePolynomial);

    int getM() const { return m_m; }
    int getSize() const { return m_size; }       //!< 2^m
    int getOrder() const { return m_size - 1; }  //!< 2^m - 1

    uint8_t mul(uint8_t a, uint8_t b) const;
    uint8_t inv(uint8_t a) const;
    uint8_t exp(int i) const;   //!< alpha^i for any integer i
    int log(uint8_t a) const;   //!< log_alpha(a), a != 0

    static const DVGaloisField& gf64();   //!< x^6 + x + 1 (P25)
    static const DVGaloisField& gf256();  //!< x^8 + x^4 + x^3 + x^2 + 1 (DMR)

private:
    int m_m;
    int m_size;
    std::vector<uint8_t> m_exp;
    std::vector<uint8_t> m_log;
};

/**
 * Reed-Solomon code over GF(2^m) with 2t = nbParity consecutive roots alpha^fcr .. alpha^(fcr + 2t - 1).
 * Systematic: the k data symbols are followed by the parity symbols, data symbol 0 being the
 * highest degree coefficient. Codes may be shortened: n is given per call.
 *
 * Decoding is Berlekamp-Massey, Chien search and Forney; it corrects up to t symbol errors
 * and reports failure otherwise.
 */
class SDRBASE_API DVReedSolomon
{
public:
    DVReedSolomon(const DVGaloisField& field, int nbParity, int firstRoot = 1);

    int getNbParity() const { return m_nbParity; }
    //! k data symbols -> nbParity parity symbols (parity[0] transmitted first after the data)
    void encodeParity(const uint8_t *data, int k, uint8_t *parity) const;
    //! k data symbols -> k + nbParity codeword symbols
    void encode(const uint8_t *data, int k, uint8_t *codeword) const;
    //! Correct a codeword of n symbols in place. Returns the number of corrected symbols or -1.
    int decode(uint8_t *codeword, int n) const;
    //! Errors and erasures: the symbols at the nbErasures positions listed are unknown (punctured
    //! parity, lost symbols) and count once against the 2t budget, errors twice. Returns the
    //! number of symbols changed (erasures filled in and errors corrected) or -1.
    int decode(uint8_t *codeword, int n, const int *erasures, int nbErasures) const;

    static const DVReedSolomon& p25_36_20();  //!< P25 header, 16 parity hexbits
    static const DVReedSolomon& p25_24_12();  //!< P25 link control, 12 parity hexbits
    static const DVReedSolomon& p25_24_16();  //!< P25 encryption sync, 8 parity hexbits
    static const DVReedSolomon& dmr_12_9();   //!< DMR full link control, 3 parity bytes
    static const DVReedSolomon& p25_63_35();  //!< P25 Phase 2 SACCH, FACCH and encryption sync, 28 parity hexbits

private:
    const DVGaloisField& m_field;
    int m_nbParity;
    int m_firstRoot;
    std::vector<uint8_t> m_generator; //!< g[j] is the x^j coefficient

    friend class DVBCH;
    //! Berlekamp-Massey / Chien / Forney over symbols[n] with syndromes at alpha^fcr .. alpha^(fcr + nbSyndromes - 1).
    //! binary: bits rather than symbols, corrected by inversion.
    static int decodeGeneric(const DVGaloisField& field, uint8_t *symbols, int n, int nbSyndromes, int firstRoot, bool binary,
        const int *erasures = nullptr, int nbErasures = 0);
};

/**
 * Binary BCH code over GF(2^m): a bit codeword of n = 2^m - 1 bits (first transmitted first)
 * with the generator polynomial being the product of the minimal polynomials of
 * alpha^1 .. alpha^(2t). Decoding uses the Reed-Solomon machinery in binary mode.
 */
class SDRBASE_API DVBCH
{
public:
    //! generator: g(x) with its leading term, degree n - k
    DVBCH(const DVGaloisField& field, int k, uint64_t generator, int nbSyndromes);

    int getN() const { return m_n; }
    int getK() const { return m_k; }
    //! k data bits (word, MSB first) -> n codeword bits as a word (bit n-1 first on air)
    uint64_t encodeWord(uint64_t data) const;
    //! Correct in place (one bit per byte). Returns the number of corrected bits or -1.
    int decode(uint8_t *bits) const;

    static const DVBCH& p25_63_16(); //!< P25 network identifier, corrects 11 errors

private:
    const DVGaloisField& m_field;
    int m_n;
    int m_k;
    uint64_t m_generator;
    int m_nbSyndromes;
};

#endif // INCLUDE_DSP_DV_DVREEDSOLOMON_H
