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

#include <algorithm>

#include "dvreedsolomon.h"

// ---- Galois field ---------------------------------------------------------------------

DVGaloisField::DVGaloisField(int m, uint32_t primitivePolynomial) :
    m_m(m),
    m_size(1 << m)
{
    m_exp.resize(2 * m_size);
    m_log.assign(m_size, 0);
    uint32_t v = 1;

    for (int i = 0; i < m_size - 1; i++)
    {
        m_exp[i] = (uint8_t) v;
        m_log[v] = (uint8_t) i;
        v <<= 1;

        if (v & m_size) {
            v ^= primitivePolynomial;
        }
    }

    for (int i = m_size - 1; i < 2 * m_size; i++) {
        m_exp[i] = m_exp[i - (m_size - 1)];
    }
}

uint8_t DVGaloisField::mul(uint8_t a, uint8_t b) const
{
    if ((a == 0) || (b == 0)) {
        return 0;
    }

    return m_exp[m_log[a] + m_log[b]];
}

uint8_t DVGaloisField::inv(uint8_t a) const
{
    return a ? m_exp[(m_size - 1) - m_log[a]] : 0;
}

uint8_t DVGaloisField::exp(int i) const
{
    int order = m_size - 1;
    i %= order;

    if (i < 0) {
        i += order;
    }

    return m_exp[i];
}

int DVGaloisField::log(uint8_t a) const
{
    return m_log[a];
}

const DVGaloisField& DVGaloisField::gf64()
{
    static const DVGaloisField field(6, 0x43);
    return field;
}

const DVGaloisField& DVGaloisField::gf256()
{
    static const DVGaloisField field(8, 0x11D);
    return field;
}

// ---- Reed-Solomon ---------------------------------------------------------------------

DVReedSolomon::DVReedSolomon(const DVGaloisField& field, int nbParity, int firstRoot) :
    m_field(field),
    m_nbParity(nbParity),
    m_firstRoot(firstRoot)
{
    // g(x) = prod (x + alpha^(firstRoot + i)), i = 0 .. nbParity - 1
    m_generator.assign(nbParity + 1, 0);
    m_generator[0] = 1;

    for (int i = 0; i < nbParity; i++)
    {
        uint8_t root = m_field.exp(firstRoot + i);

        for (int j = i + 1; j > 0; j--) {
            m_generator[j] = m_generator[j - 1] ^ m_field.mul(m_generator[j], root);
        }

        m_generator[0] = m_field.mul(m_generator[0], root);
    }
}

void DVReedSolomon::encodeParity(const uint8_t *data, int k, uint8_t *parity) const
{
    // Remainder of m(x) x^nbParity divided by g(x); parity[0] is the highest degree coefficient
    std::fill(parity, parity + m_nbParity, 0);

    for (int i = 0; i < k; i++)
    {
        uint8_t feedback = data[i] ^ parity[0];

        for (int j = 0; j < m_nbParity - 1; j++) {
            parity[j] = parity[j + 1] ^ m_field.mul(feedback, m_generator[m_nbParity - 1 - j]);
        }

        parity[m_nbParity - 1] = m_field.mul(feedback, m_generator[0]);
    }
}

void DVReedSolomon::encode(const uint8_t *data, int k, uint8_t *codeword) const
{
    std::copy(data, data + k, codeword);
    encodeParity(data, k, codeword + k);
}

int DVReedSolomon::decode(uint8_t *codeword, int n) const
{
    return decodeGeneric(m_field, codeword, n, m_nbParity, m_firstRoot, false);
}

int DVReedSolomon::decode(uint8_t *codeword, int n, const int *erasures, int nbErasures) const
{
    return decodeGeneric(m_field, codeword, n, m_nbParity, m_firstRoot, false, erasures, nbErasures);
}

int DVReedSolomon::decodeGeneric(const DVGaloisField& field, uint8_t *symbols, int n, int nbSyndromes, int firstRoot, bool binary,
    const int *erasures, int nbErasures)
{
    const int maxT = 16;

    if ((nbSyndromes > 2 * maxT) || (nbErasures > nbSyndromes)) {
        return -1;
    }

    // Erased symbols are taken as zero; their locator gamma(x) = prod (1 + X_j x) is known
    uint8_t gamma[2 * maxT + 2];
    std::fill(gamma, gamma + 2 * maxT + 2, 0);
    gamma[0] = 1;

    for (int e = 0; e < nbErasures; e++)
    {
        int j = erasures[e];

        if ((j < 0) || (j >= n)) {
            return -1;
        }

        symbols[j] = 0;
        uint8_t x = field.exp(n - 1 - j);

        for (int i = e + 1; i >= 1; i--) {
            gamma[i] ^= field.mul(gamma[i - 1], x);
        }
    }

    // Syndromes S[i] = r(alpha^(firstRoot + i - 1)), i = 1 .. nbSyndromes
    uint8_t S[2 * maxT + 1];
    bool anyError = false;

    for (int i = 1; i <= nbSyndromes; i++)
    {
        uint8_t s = 0;
        int rootPower = firstRoot + i - 1;

        for (int j = 0; j < n; j++)
        {
            if (symbols[j]) {
                s ^= field.mul(symbols[j], field.exp(rootPower * (n - 1 - j)));
            }
        }

        S[i] = s;
        anyError = anyError || (s != 0);
    }

    if (!anyError) {
        return 0;
    }

    // With erasures the Berlekamp-Massey step runs on the modified syndromes
    // Xi(x) = S(x) gamma(x) mod x^2t, for the 2t - e remaining unknowns
    uint8_t Xi[2 * maxT + 1];
    std::fill(Xi, Xi + 2 * maxT + 1, 0);

    for (int i = 0; i < nbSyndromes; i++)
    {
        for (int j = 0; (j <= nbErasures) && (i + j < nbSyndromes); j++) {
            Xi[i + j + 1] ^= field.mul(S[i + 1], gamma[j]);
        }
    }

    // Berlekamp-Massey: error locator lambda(x)
    uint8_t lambda[maxT + 2], B[maxT + 2], T[maxT + 2];
    std::fill(lambda, lambda + maxT + 2, 0);
    std::fill(B, B + maxT + 2, 0);
    lambda[0] = 1;
    B[0] = 1;
    int L = 0;
    int m = 1;
    uint8_t b = 1;

    for (int r = nbErasures; r < nbSyndromes; r++)
    {
        uint8_t delta = Xi[r + 1];

        for (int i = 1; i <= L; i++) {
            delta ^= field.mul(lambda[i], Xi[r + 1 - i]);
        }

        if (delta == 0)
        {
            m++;
        }
        else if (2 * L <= r - nbErasures)
        {
            std::copy(lambda, lambda + maxT + 2, T);
            uint8_t coeff = field.mul(delta, field.inv(b));

            for (int i = 0; i + m < maxT + 2; i++) {
                lambda[i + m] ^= field.mul(coeff, B[i]);
            }

            L = r - nbErasures + 1 - L;
            std::copy(T, T + maxT + 2, B);
            b = delta;
            m = 1;
        }
        else
        {
            uint8_t coeff = field.mul(delta, field.inv(b));

            for (int i = 0; i + m < maxT + 2; i++) {
                lambda[i + m] ^= field.mul(coeff, B[i]);
            }

            m++;
        }
    }

    if (2 * L > nbSyndromes - nbErasures) {
        return -1;
    }

    // Errata locator psi(x) = lambda(x) gamma(x), of degree L + e
    uint8_t psi[2 * maxT + 2];
    std::fill(psi, psi + 2 * maxT + 2, 0);

    for (int i = 0; i <= L; i++)
    {
        for (int j = 0; j <= nbErasures; j++) {
            psi[i + j] ^= field.mul(lambda[i], gamma[j]);
        }
    }

    int degree = L + nbErasures;

    // Chien search: symbol j has locator X = alpha^(n-1-j); psi(X^-1) = 0 at an erratum
    int positions[2 * maxT + 1];
    int nbRoots = 0;

    for (int j = 0; j < n; j++)
    {
        int power = n - 1 - j;
        uint8_t sum = 0;

        for (int i = 0; i <= degree; i++)
        {
            if (psi[i]) {
                sum ^= field.mul(psi[i], field.exp(-power * i));
            }
        }

        if (sum == 0)
        {
            if (nbRoots >= degree) {
                return -1;
            }

            positions[nbRoots++] = j;
        }
    }

    if (nbRoots != degree) {
        return -1;
    }

    if (binary)
    {
        for (int i = 0; i < nbRoots; i++) {
            symbols[positions[i]] ^= 1;
        }
    }
    else
    {
        // Forney: omega(x) = S(x) psi(x) mod x^nbSyndromes with S(x) = S1 + S2 x + ...
        uint8_t omega[2 * maxT];
        std::fill(omega, omega + 2 * maxT, 0);

        for (int i = 0; i < nbSyndromes; i++)
        {
            for (int j = 0; j <= degree; j++)
            {
                if (i + j < nbSyndromes) {
                    omega[i + j] ^= field.mul(S[i + 1], psi[j]);
                }
            }
        }

        for (int e = 0; e < nbRoots; e++)
        {
            int power = n - 1 - positions[e];
            uint8_t xInv = field.exp(-power);
            uint8_t num = 0, den = 0, xp = 1;

            for (int i = 0; i < nbSyndromes; i++)
            {
                num ^= field.mul(omega[i], xp);
                xp = field.mul(xp, xInv);
            }

            // psi'(x) keeps the odd powers only in characteristic 2
            xp = 1;

            for (int i = 1; i <= degree; i += 2)
            {
                den ^= field.mul(psi[i], xp);
                xp = field.mul(xp, field.mul(xInv, xInv));
            }

            if (den == 0) {
                return -1;
            }

            // e_j = X^(1 - firstRoot) omega(X^-1) / lambda'(X^-1)
            uint8_t magnitude = field.mul(num, field.inv(den));

            if (firstRoot != 1) {
                magnitude = field.mul(magnitude, field.exp(power * (1 - firstRoot)));
            }

            symbols[positions[e]] ^= magnitude;
        }
    }

    // Verify
    for (int i = 1; i <= nbSyndromes; i++)
    {
        uint8_t s = 0;
        int rootPower = firstRoot + i - 1;

        for (int j = 0; j < n; j++)
        {
            if (symbols[j]) {
                s ^= field.mul(symbols[j], field.exp(rootPower * (n - 1 - j)));
            }
        }

        if (s != 0) {
            return -1;
        }
    }

    return nbRoots;
}

const DVReedSolomon& DVReedSolomon::p25_36_20() { static const DVReedSolomon code(DVGaloisField::gf64(), 16); return code; }
const DVReedSolomon& DVReedSolomon::p25_24_12() { static const DVReedSolomon code(DVGaloisField::gf64(), 12); return code; }
const DVReedSolomon& DVReedSolomon::p25_24_16() { static const DVReedSolomon code(DVGaloisField::gf64(), 8); return code; }
const DVReedSolomon& DVReedSolomon::dmr_12_9()  { static const DVReedSolomon code(DVGaloisField::gf256(), 3); return code; }
const DVReedSolomon& DVReedSolomon::p25_63_35() { static const DVReedSolomon code(DVGaloisField::gf64(), 28); return code; }

// ---- BCH ------------------------------------------------------------------------------

DVBCH::DVBCH(const DVGaloisField& field, int k, uint64_t generator, int nbSyndromes) :
    m_field(field),
    m_n(field.getOrder()),
    m_k(k),
    m_generator(generator),
    m_nbSyndromes(nbSyndromes)
{
}

uint64_t DVBCH::encodeWord(uint64_t data) const
{
    int p = m_n - m_k;
    uint64_t r = data << p;

    for (int i = m_n - 1; i >= p; i--)
    {
        if (r & (1ULL << i)) {
            r ^= m_generator << (i - p);
        }
    }

    return (data << p) | (r & ((1ULL << p) - 1));
}

int DVBCH::decode(uint8_t *bits) const
{
    return DVReedSolomon::decodeGeneric(m_field, bits, m_n, m_nbSyndromes, 1, true);
}

const DVBCH& DVBCH::p25_63_16()
{
    // g(x) = product of the minimal polynomials of alpha^1 .. alpha^22 over GF(64): degree 47
    static const DVBCH code(DVGaloisField::gf64(), 16, 0xCD930BDD3B2BULL, 22);
    return code;
}
