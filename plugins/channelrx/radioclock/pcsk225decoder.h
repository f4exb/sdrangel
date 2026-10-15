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

#ifndef INCLUDE_PCSK225DECODER_H
#define INCLUDE_PCSK225DECODER_H

#include <array>

#include <QDateTime>
#include <QString>
#include <QtGlobal>

class PCSK225Decoder
{
public:
    static constexpr int FrameBytes = 12;

    struct Result
    {
        QDateTime m_dateTimeUtc;
        int m_utcOffsetHours;
        bool m_leapSecondPending;
        bool m_leapSecondDelete;
        bool m_timeChangePending;
        int m_transmitterStatus;
        int m_correctedSymbols;
    };

    static bool decode(std::array<quint8, FrameBytes>& frame, Result& result, QString& error);

private:
    static quint8 gfMultiply(quint8 a, quint8 b);
    static quint8 gfPower(int power);
    static void syndromes(const std::array<quint8, 15>& codeword, std::array<quint8, 6>& result);
    static bool allZero(const std::array<quint8, 6>& values);
    static int correct(std::array<quint8, 15>& codeword);
    static quint8 crc8(const quint8 *data, int count);
    static int bit(const std::array<quint8, 5>& data, int index);
};

#endif // INCLUDE_PCSK225DECODER_H
