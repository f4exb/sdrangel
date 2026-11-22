///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef MODEMRADIOPAGING_RADIOPAGINGRECODE_H
#define MODEMRADIOPAGING_RADIOPAGINGRECODE_H

#include <QString>
#include <QStringList>

#include "export.h"
#include "util/messagerecord.h"
#include "radiopagingcharset.h"

// A received message decoded again from what its record keeps: the message bits of POCSAG and GSC (with another
// codepage, or as the other type, alphanumeric or numeric, when the content guess was wrong), the 7-bit characters
// of FLEX alphanumeric messages (alpha_codes, with another codepage; the type of FLEX is in the message).
class MODEMRADIOPAGING_API RadioPagingRecode
{
public:
    //!< The types a record can be decoded as (alphanumeric, numeric; FLEX alphanumeric only), none if it cannot be
    static QStringList types(const MessageRecord& record);
    //!< The record decoded with the codepage (none: the one of the record) as the type (empty: the one of the record).
    //!< The text, the type, the codepage and the other candidate; redecoded is set. False and an error if it cannot be
    static bool redecode(const MessageRecord& record, const RadioPagingCharset::Key& charset, const QString& type,
        MessageRecord& out, QString& error);
    //!< Longest numeric alternative reading of a message that is given, characters
    static const int m_maxAltNumeric = 18;
    //!< Whether the other reading of a message (type and text) is given with it: not a long numeric one
    static bool alternativeGiven(const QString& type, const QString& text);
    //!< The text of a record: the codes below 0x20 and 0x7f as <0xNN>, other bytes kept (UTF-8 passes)
    static QString escapeText(const std::string& text);
    //!< Right-to-left: the characters of the text in the reverse order, with their status (one for each character,
    //!< or empty). A <0xNN> code, the <...> of a lost fragment and a character of two UTF-16 units stay as they are
    static void reverseText(QString& text, QString& charStatus);
    //!< The text of codes as they were received: the codes received (status . or c) decoded with the codepage (none:
    //!< the codes are the characters), a lost code (X) as '?', a lost fragment of unknown length (G) as <...>, a code
    //!< of a word that could not be corrected (u) as received, alone, an unprintable one as '?'.
    //!< charStatus: one for each character of the text, . received, c corrected, u uncertain, X lost; empty, and the codes decoded
    //!< as one, when codeStatus is not one for each code
    static QString decodeWithStatus(const std::string& codes, const std::string& codeStatus,
        const RadioPagingCharset::Codepage *codepage, QString& charStatus);
    //!< POCSAG: the status of each code of a message (bitsPerCode 7 alphanumeric, 4 numeric), the worst of the codewords
    //!< its bits are in (wordStatus: one for each 20 bits). Empty if wordStatus does not cover the codes
    static std::string pocsagCodeStatus(const std::string& wordStatus, size_t codes, int bitsPerCode);
    //!< POCSAG: the text of the message bits as the type (alphanumeric, numeric) with the status of its characters
    static QString pocsagText(const std::vector<uint8_t>& raw, int rawBits, const std::string& type,
        const RadioPagingCharset::Codepage *codepage, const std::vector<uint8_t>& fill, const std::string& wordStatus,
        QString& charStatus);
};

#endif // MODEMRADIOPAGING_RADIOPAGINGRECODE_H
