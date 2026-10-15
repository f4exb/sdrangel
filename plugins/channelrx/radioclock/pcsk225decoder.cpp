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

#include "pcsk225decoder.h"

namespace {

constexpr quint8 scrambler[5] = {0x0a, 0x47, 0x55, 0x4d, 0x2b};

}

quint8 PCSK225Decoder::gfMultiply(quint8 a, quint8 b)
{
    quint8 value = 0;

    while (b != 0)
    {
        if (b & 1) {
            value ^= a;
        }

        b >>= 1;
        a <<= 1;

        if (a & 0x10) {
            a ^= 0x13; // x^4 + x + 1
        }
    }

    return value & 0x0f;
}

quint8 PCSK225Decoder::gfPower(int power)
{
    quint8 value = 1;

    for (int i = 0; i < (power % 15); i++) {
        value = gfMultiply(value, 2);
    }

    return value;
}

void PCSK225Decoder::syndromes(const std::array<quint8, 15>& codeword, std::array<quint8, 6>& result)
{
    // The on-air shortened RS(15,9) code uses roots alpha^9 through alpha^14.
    for (int rootIndex = 0; rootIndex < 6; rootIndex++)
    {
        const int root = 9 + rootIndex;
        quint8 syndrome = 0;

        for (int position = 0; position < 15; position++)
        {
            int degree = 14 - position;
            syndrome ^= gfMultiply(codeword[position], gfPower(degree * root));
        }

        result[rootIndex] = syndrome;
    }
}

bool PCSK225Decoder::allZero(const std::array<quint8, 6>& values)
{
    quint8 combined = 0;

    for (quint8 value : values) {
        combined |= value;
    }

    return combined == 0;
}

int PCSK225Decoder::correct(std::array<quint8, 15>& codeword)
{
    std::array<quint8, 6> receivedSyndromes;
    syndromes(codeword, receivedSyndromes);

    if (allZero(receivedSyndromes)) {
        return 0;
    }

    // There are only 15 symbols. Precomputing each possible error's syndrome
    // makes an exhaustive search through the code's three-symbol correction
    // radius small, deterministic and considerably simpler than a general RS
    // implementation.
    quint8 contribution[15][16][6]{};

    for (int position = 0; position < 15; position++)
    {
        int degree = 14 - position;

        for (int errorValue = 1; errorValue < 16; errorValue++)
        {
            for (int rootIndex = 0; rootIndex < 6; rootIndex++)
            {
                contribution[position][errorValue][rootIndex] = gfMultiply(
                    errorValue,
                    gfPower(degree * (9 + rootIndex))
                );
            }
        }
    }

    auto matches = [&receivedSyndromes, &contribution](int p1, int e1, int p2, int e2, int p3, int e3) {
        for (int i = 0; i < 6; i++)
        {
            quint8 value = contribution[p1][e1][i];
            if (p2 >= 0) {
                value ^= contribution[p2][e2][i];
            }
            if (p3 >= 0) {
                value ^= contribution[p3][e3][i];
            }
            if (value != receivedSyndromes[i]) {
                return false;
            }
        }
        return true;
    };

    for (int p1 = 0; p1 < 15; p1++)
    {
        for (int e1 = 1; e1 < 16; e1++)
        {
            if (matches(p1, e1, -1, 0, -1, 0))
            {
                codeword[p1] ^= e1;
                return 1;
            }
        }
    }

    for (int p1 = 0; p1 < 14; p1++)
    {
        for (int p2 = p1 + 1; p2 < 15; p2++)
        {
            for (int e1 = 1; e1 < 16; e1++)
            {
                for (int e2 = 1; e2 < 16; e2++)
                {
                    if (matches(p1, e1, p2, e2, -1, 0))
                    {
                        codeword[p1] ^= e1;
                        codeword[p2] ^= e2;
                        return 2;
                    }
                }
            }
        }
    }

    for (int p1 = 0; p1 < 13; p1++)
    {
        for (int p2 = p1 + 1; p2 < 14; p2++)
        {
            for (int p3 = p2 + 1; p3 < 15; p3++)
            {
                for (int e1 = 1; e1 < 16; e1++)
                {
                    for (int e2 = 1; e2 < 16; e2++)
                    {
                        for (int e3 = 1; e3 < 16; e3++)
                        {
                            if (matches(p1, e1, p2, e2, p3, e3))
                            {
                                codeword[p1] ^= e1;
                                codeword[p2] ^= e2;
                                codeword[p3] ^= e3;
                                return 3;
                            }
                        }
                    }
                }
            }
        }
    }

    return -1;
}

quint8 PCSK225Decoder::crc8(const quint8 *data, int count)
{
    quint8 crc = 0;

    for (int i = 0; i < count; i++)
    {
        crc ^= data[i];

        for (int bitIndex = 0; bitIndex < 8; bitIndex++) {
            crc = crc & 0x80 ? static_cast<quint8>((crc << 1) ^ 0x07) : static_cast<quint8>(crc << 1);
        }
    }

    return crc;
}

int PCSK225Decoder::bit(const std::array<quint8, 5>& data, int index)
{
    return (data[index / 8] >> (7 - index % 8)) & 1;
}

bool PCSK225Decoder::decode(std::array<quint8, FrameBytes>& frame, Result& result, QString& error)
{
    if ((frame[0] != 0x55) || (frame[1] != 0x55) || (frame[2] != 0x60))
    {
        error = "Invalid PCSK-225 header";
        return false;
    }

    std::array<quint8, 15> codeword{};
    int codeBit = 0;

    // The protected data begins after the three timing bits in byte 4 and
    // ends at SK0. It is protected in its transmitted (scrambled) form.
    for (int frameBit = 3; frameBit < 39; frameBit += 4)
    {
        quint8 symbol = 0;
        for (int i = 0; i < 4; i++, codeBit++) {
            symbol = static_cast<quint8>((symbol << 1) | ((frame[3 + (frameBit + i) / 8] >> (7 - (frameBit + i) % 8)) & 1));
        }
        codeword[codeBit / 4 - 1] = symbol;
    }

    for (int i = 0; i < 3; i++)
    {
        codeword[9 + 2*i] = frame[8 + i] >> 4;
        codeword[10 + 2*i] = frame[8 + i] & 0x0f;
    }

    int correctedSymbols = correct(codeword);

    if (correctedSymbols < 0)
    {
        error = "Uncorrectable Reed-Solomon error";
        return false;
    }

    codeBit = 0;
    for (int frameBit = 3; frameBit < 39; frameBit++)
    {
        int symbolIndex = codeBit / 4;
        int symbolBit = 3 - codeBit % 4;
        quint8 value = (codeword[symbolIndex] >> symbolBit) & 1;
        quint8 mask = static_cast<quint8>(1U << (7 - frameBit % 8));
        quint8& byte = frame[3 + frameBit / 8];
        byte = value ? static_cast<quint8>(byte | mask) : static_cast<quint8>(byte & ~mask);
        codeBit++;
    }

    for (int i = 0; i < 3; i++) {
        frame[8 + i] = static_cast<quint8>((codeword[9 + 2*i] << 4) | codeword[10 + 2*i]);
    }

    if (crc8(&frame[3], 5) != frame[11])
    {
        error = "CRC error";
        return false;
    }

    std::array<quint8, 5> data{};
    for (int i = 0; i < 5; i++) {
        data[i] = frame[3 + i] ^ scrambler[i];
    }

    if ((data[0] & 0xe0) != 0xa0)
    {
        error = "Invalid PCSK-225 timing marker";
        return false;
    }

    quint32 periods = 0;
    for (int i = 3; i < 33; i++) {
        periods = (periods << 1) | bit(data, i);
    }

    const int utcOffsetHours = bit(data, 33) + 2 * bit(data, 34);
    const QDateTime epoch(QDate(2000, 1, 1), QTime(0, 0), Qt::UTC);

    result.m_dateTimeUtc = epoch.addSecs(static_cast<qint64>(periods) * 3);
    result.m_utcOffsetHours = utcOffsetHours;
    result.m_leapSecondPending = bit(data, 35);
    result.m_leapSecondDelete = bit(data, 36);
    result.m_timeChangePending = bit(data, 37);
    result.m_transmitterStatus = bit(data, 38) | (bit(data, 39) << 1);
    result.m_correctedSymbols = correctedSymbols;

    error.clear();
    return true;
}
