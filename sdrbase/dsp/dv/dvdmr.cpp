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

// Implemented from ETSI TS 102 361-1 V2.5.1 Annex B.

#include <algorithm>
#include <cstring>

#include "dvbits.h"
#include "dvblockcode.h"
#include "dvcrc.h"
#include "dvdmr.h"
#include "dvreedsolomon.h"

// On-air bit k of the CACH carries TACT/payload bit m_cachInterleave[k] (figure B.9)
// where bits 0..3 = AT, TC, LCSS(1), LCSS(0), 4..6 = Hamming parity, 7..23 = payload P(16)..P(0)
const int DVDMR::m_cachInterleave[24] = {
    0, 7, 8, 9, 1, 10, 11, 12, 2, 13, 14, 15, 3, 16, 4, 17, 18, 19, 5, 20, 21, 22, 6, 23
};

// ---- BPTC(196,96) ------------------------------------------------------------
// Encoder matrix: 13 rows x 15 columns. Index 0 is the pad bit R(3), then the
// matrix row by row (index = 1 + 15*row + column). Rows 0..8 hold the
// information (row 0: R(2), R(1), R(0), I(95)..I(88); rows 1..8: 11 bits each)
// with Hamming(15,11) parity in columns 11..14. Rows 9..12 hold the
// Hamming(13,9) column parity. Transmit position = index * 181 mod 196.

void DVDMR::bptcFill(const uint8_t *info, uint8_t m[13][15])
{
    std::memset(m, 0, 13 * 15);

    for (int c = 3; c < 11; c++) {
        m[0][c] = info[c - 3];
    }

    for (int r = 1; r < 9; r++)
    {
        for (int c = 0; c < 11; c++) {
            m[r][c] = info[8 + 11 * (r - 1) + c];
        }
    }
}

void DVDMR::bptcExtract(const uint8_t m[13][15], uint8_t *info)
{
    for (int c = 3; c < 11; c++) {
        info[c - 3] = m[0][c];
    }

    for (int r = 1; r < 9; r++)
    {
        for (int c = 0; c < 11; c++) {
            info[8 + 11 * (r - 1) + c] = m[r][c];
        }
    }
}

void DVDMR::bptcEncode(const uint8_t *info, uint8_t *tx)
{
    uint8_t m[13][15];
    bptcFill(info, m);

    for (int r = 0; r < 9; r++)
    {
        uint8_t row[15];
        DVBlockCode::hamming1511().encode(m[r], row);
        std::copy(row + 11, row + 15, &m[r][11]);
    }

    for (int c = 0; c < 15; c++)
    {
        uint8_t data[9], col[13];

        for (int r = 0; r < 9; r++) {
            data[r] = m[r][c];
        }

        DVBlockCode::hamming139().encode(data, col);

        for (int r = 9; r < 13; r++) {
            m[r][c] = col[r];
        }
    }

    tx[0] = 0; // R(3) at index 0 maps to position 0

    for (int r = 0; r < 13; r++)
    {
        for (int c = 0; c < 15; c++)
        {
            int index = 1 + 15 * r + c;
            tx[(index * 181) % 196] = m[r][c];
        }
    }
}

int DVDMR::bptcDecode(const uint8_t *tx, uint8_t *info)
{
    uint8_t m[13][15];

    for (int r = 0; r < 13; r++)
    {
        for (int c = 0; c < 15; c++)
        {
            int index = 1 + 15 * r + c;
            m[r][c] = tx[(index * 181) % 196] & 1;
        }
    }

    // Two passes of row then column correction: a single error in a row is fixed by the row
    // code; an error the row code cannot fix may be fixed by the column code and vice versa.
    int corrected = 0;
    bool ok = false;

    for (int pass = 0; pass < 2 && !ok; pass++)
    {
        ok = true;

        for (int r = 0; r < 13; r++)
        {
            int e = DVBlockCode::hamming1511().decode(m[r]);

            if (e < 0) {
                ok = false;
            } else {
                corrected += e;
            }
        }

        for (int c = 0; c < 15; c++)
        {
            uint8_t col[13];

            for (int r = 0; r < 13; r++) {
                col[r] = m[r][c];
            }

            int e = DVBlockCode::hamming139().decode(col);

            if (e < 0)
            {
                ok = false;
            }
            else if (e > 0)
            {
                corrected += e;

                for (int r = 0; r < 13; r++) {
                    m[r][c] = col[r];
                }
            }
        }
    }

    bptcExtract(m, info);
    return ok ? corrected : -1;
}

// ---- Full link control ------------------------------------------------------

void DVDMR::fullLCEncode(const uint8_t *lc, uint8_t mask, uint8_t *info)
{
    uint8_t bytes[12];
    std::copy(lc, lc + 9, bytes);
    DVReedSolomon::dmr_12_9().encodeParity(lc, 9, &bytes[9]);
    bytes[9] ^= mask;
    bytes[10] ^= mask;
    bytes[11] ^= mask;
    DVBits::bytesToBitsMSB(bytes, 12, info);
}

int DVDMR::fullLCDecode(const uint8_t *info, uint8_t mask, uint8_t *lc)
{
    uint8_t bytes[12];
    DVBits::bitsToBytesMSB(info, 96, bytes);
    bytes[9] ^= mask;
    bytes[10] ^= mask;
    bytes[11] ^= mask;
    int errors = DVReedSolomon::dmr_12_9().decode(bytes, 12);
    std::copy(bytes, bytes + 9, lc);
    return errors;
}

// ---- Embedded signalling ----------------------------------------------------

// The 72 LC bits and the 5 bit checksum in the 7 rows of 11 information bits (TS 102 361-1 B.2.1,
// as MMDVMHost's CDMREmbeddedData lays them): rows 0 and 1 hold 11 LC bits each, rows 2 to 6 ten,
// and column 10 of rows 2 to 6 holds CS(4) to CS(0). (Before 30 September 2026 all 77 ran on in
// order, the checksum in row 6: the embedded LC of real radios failed its checksum here, and
// theirs ours)
static void embeddedInfoRows(const uint8_t *lcBits, uint8_t cs, uint8_t info[7][11])
{
    int b = 0;

    for (int r = 0; r < 7; r++)
    {
        const int n = r < 2 ? 11 : 10;

        for (int c = 0; c < n; c++) {
            info[r][c] = lcBits[b++];
        }

        if (r >= 2) {
            info[r][10] = (cs >> (6 - r)) & 1;
        }
    }
}

void DVDMR::embeddedLCEncode(const uint8_t *lc, uint8_t *tx)
{
    uint8_t lcBits[72];
    DVBits::bytesToBitsMSB(lc, 9, lcBits);
    uint8_t info[7][11];
    embeddedInfoRows(lcBits, DVCRC::dmrLcChecksum(lc), info);
    uint8_t m[8][16];

    for (int r = 0; r < 7; r++) {
        DVBlockCode::hamming16114().encode(info[r], m[r]);
    }

    for (int c = 0; c < 16; c++)
    {
        uint8_t p = 0;

        for (int r = 0; r < 7; r++) {
            p ^= m[r][c];
        }

        m[7][c] = p; // even column parity
    }

    // Transmit by reading the matrix columns top to bottom, left to right
    for (int j = 0; j < 128; j++) {
        tx[j] = m[j % 8][j / 8];
    }
}

int DVDMR::embeddedLCDecode(const uint8_t *tx, uint8_t *lc)
{
    uint8_t m[8][16];

    for (int j = 0; j < 128; j++) {
        m[j % 8][j / 8] = tx[j] & 1;
    }

    int corrected = 0;

    for (int r = 0; r < 7; r++)
    {
        int e = DVBlockCode::hamming16114().decode(m[r]);

        if (e < 0) {
            return -1;
        }

        corrected += e;
    }

    for (int c = 0; c < 16; c++)
    {
        uint8_t p = 0;

        for (int r = 0; r < 8; r++) {
            p ^= m[r][c];
        }

        if (p) {
            return -1;
        }
    }

    uint8_t lcBits[72];
    uint8_t cs = 0;
    int b = 0;

    for (int r = 0; r < 7; r++)
    {
        const int n = r < 2 ? 11 : 10;

        for (int c = 0; c < n; c++) {
            lcBits[b++] = m[r][c];
        }

        if (r >= 2) {
            cs = (uint8_t) ((cs << 1) | m[r][10]);
        }
    }

    DVBits::bitsToBytesMSB(lcBits, 72, lc);

    if (cs != DVCRC::dmrLcChecksum(lc)) {
        return -1;
    }

    return corrected;
}

// ---- Burst fields ------------------------------------------------------------

void DVDMR::cachEncode(bool at, int tc, int lcss, const uint8_t *payload, uint8_t *tx)
{
    uint8_t tact[4] = { (uint8_t) (at ? 1 : 0), (uint8_t) (tc & 1), (uint8_t) ((lcss >> 1) & 1), (uint8_t) (lcss & 1) };
    uint8_t bits[24];
    DVBlockCode::hamming74().encode(tact, bits);

    for (int i = 0; i < 17; i++) {
        bits[7 + i] = payload ? payload[i] : 0;
    }

    for (int k = 0; k < 24; k++) {
        tx[k] = bits[m_cachInterleave[k]];
    }
}

int DVDMR::cachDecode(const uint8_t *rx, bool& at, int& tc, int& lcss, uint8_t *payload)
{
    uint8_t bits[24];

    for (int k = 0; k < 24; k++) {
        bits[m_cachInterleave[k]] = rx[k] & 1;
    }

    int errors = DVBlockCode::hamming74().decode(bits);
    at = bits[0] != 0;
    tc = bits[1];
    lcss = (bits[2] << 1) | bits[3];

    if (payload) {
        std::copy(bits + 7, bits + 24, payload);
    }

    return errors;
}

void DVDMR::slotTypeEncode(int colorCode, int dataType, uint8_t *tx)
{
    uint8_t data[8];
    DVBits::putBits(data, 0, colorCode & 0xF, 4);
    DVBits::putBits(data, 4, dataType & 0xF, 4);
    DVBlockCode::golay208().encode(data, tx);
}

int DVDMR::slotTypeDecode(const uint8_t *rx, int& colorCode, int& dataType)
{
    uint8_t bits[20];
    std::copy(rx, rx + 20, bits);
    int errors = DVBlockCode::golay208().decode(bits);
    colorCode = (int) DVBits::getBits(bits, 0, 4);
    dataType = (int) DVBits::getBits(bits, 4, 4);
    return errors;
}

void DVDMR::embEncode(int colorCode, bool pi, int lcss, uint8_t *tx)
{
    uint8_t data[7];
    DVBits::putBits(data, 0, colorCode & 0xF, 4);
    data[4] = pi ? 1 : 0;
    data[5] = (lcss >> 1) & 1;
    data[6] = lcss & 1;
    DVBlockCode::qr1676().encode(data, tx);
}

int DVDMR::embDecodeSoft(const float *llr, int& colorCode, bool& pi, int& lcss)
{
    uint8_t bits[7];
    int differences = DVBlockCode::qr1676().decodeSoft(llr, bits);
    colorCode = (int) DVBits::getBits(bits, 0, 4);
    pi = bits[4] != 0;
    lcss = (bits[5] << 1) | bits[6];
    return differences;
}

int DVDMR::embDecode(const uint8_t *rx, int& colorCode, bool& pi, int& lcss)
{
    uint8_t bits[16];
    std::copy(rx, rx + 16, bits);
    int errors = DVBlockCode::qr1676().decode(bits);
    colorCode = (int) DVBits::getBits(bits, 0, 4);
    pi = bits[4] != 0;
    lcss = (bits[5] << 1) | bits[6];
    return errors;
}
