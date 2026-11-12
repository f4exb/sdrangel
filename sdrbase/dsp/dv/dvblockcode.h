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

#ifndef INCLUDE_DSP_DV_DVBLOCKCODE_H
#define INCLUDE_DSP_DV_DVBLOCKCODE_H

#include <cstdint>
#include <vector>

#include "export.h"

/**
 * Binary systematic linear block code of length n = k + p, n <= 32.
 *
 * A codeword is the k data bits followed by the p parity bits, first
 * transmitted bit first. Codes are defined either by the parity part of their
 * generator matrix (one row per data bit, as the ETSI and TIA standards print
 * them) or by the generator polynomial of a cyclic code.
 *
 * Hard decoding is by syndrome look-up and corrects every pattern of up to
 * t = floor((dmin - 1) / 2) errors; anything else is reported as uncorrectable.
 * Soft decoding is maximum likelihood over all 2^k codewords (k <= 16).
 *
 * The codes of the digital voice protocols are available as shared instances,
 * see the static accessors.
 */
class SDRBASE_API DVBlockCode
{
public:
    //! parityRows[i] is a string of p '0'/'1' characters: the parity bits of data bit i
    DVBlockCode(int k, int nbParity, const char *const *parityRows);
    //! Cyclic code: parity = data(x) x^p mod g(x), generator with its leading x^p term.
    //! With overallParity an even parity bit over the cyclic codeword is appended (p + 1 parity bits).
    DVBlockCode(int k, int nbParity, uint32_t generator, bool overallParity);

    int getN() const { return m_n; }
    int getK() const { return m_k; }
    int getNbParity() const { return m_p; }
    int getMinDistance() const { return m_dmin; }
    int getCorrectable() const { return m_t; }

    //! k data bits -> n codeword bits
    void encode(const uint8_t *data, uint8_t *codeword) const;
    //! Correct in place. Returns the number of corrected bits or -1 if uncorrectable.
    int decode(uint8_t *codeword) const;
    //! Correct in place and copy the data bits out. Returns as decode().
    int decode(uint8_t *codeword, uint8_t *data) const;
    //! Maximum likelihood decoding from n LLRs (positive favours 0). Writes the k data bits
    //! and optionally the n codeword bits. Returns the number of codeword bits that differ from
    //! the hard decisions.
    int decodeSoft(const float *llr, uint8_t *data, uint8_t *codeword = nullptr) const;

    //! Word forms: bit (n-1) of the word is the first transmitted bit
    uint32_t encodeWord(uint32_t data) const;
    int decodeWord(uint32_t& codeword) const;
    uint32_t getSyndrome(uint32_t codeword) const;

    // ---- Shared instances -----------------------------------------------------------------
    static const DVBlockCode& hamming74();       //!< DMR CACH TACT (ETSI TS 102 361-1 B.3.5)
    static const DVBlockCode& hamming139();      //!< DMR BPTC columns (B.3.2)
    static const DVBlockCode& hamming1511();     //!< DMR BPTC rows (B.3.3)
    static const DVBlockCode& hamming16114();    //!< DMR embedded signalling (B.3.4)
    static const DVBlockCode& golay208();        //!< DMR slot type (B.3.1)
    static const DVBlockCode& qr1676();          //!< DMR EMB (B.3.7)
    static const DVBlockCode& hamming128();      //!< dPMR (ETSI TS 102 490 table 7.1)
    static const DVBlockCode& hamming106();      //!< P25 link control (TIA-102.BAAA)
    static const DVBlockCode& golay2312();       //!< AMBE C1, generator 0xC75
    static const DVBlockCode& golay2412();       //!< AMBE C0, YSF FICH, P25: Golay(23,12) + even parity
    static const DVBlockCode& golay186();        //!< P25 header: Golay(24,12) with 6 leading data bits zero
    static const DVBlockCode& cyclic168();       //!< P25 low speed data, generator x^8 + x^5 + x^4 + x^3 + 1

private:
    int m_k;
    int m_p;
    int m_n;
    int m_dmin;
    int m_t;
    std::vector<uint32_t> m_parityOfData;   //!< Parity word (p bits) contributed by each data bit
    std::vector<uint32_t> m_syndromeTable;  //!< 2^p entries: error pattern (n bit word) or m_noPattern
    std::vector<uint32_t> m_codewords;      //!< 2^k codewords for soft decoding
    static constexpr uint32_t m_noPattern = 0xFFFFFFFFu;

    void build();
    uint32_t parityOf(uint32_t data) const;
};

#endif // INCLUDE_DSP_DV_DVBLOCKCODE_H
