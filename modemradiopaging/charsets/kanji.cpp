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

// FLEX Japanese Text Service: Shift JIS characters of a HEX/binary message (STD-43A 3.10.2.3). Byte 1 of a 2-byte
// code is 0x81-0x9F or 0xE0-0xFC, byte 2 0x40-0xFC; the other bytes are 1-byte characters (JIS X 0201)

#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <vector>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringDecoder>
#include <QStringEncoder>
#else
#include <QTextCodec>
#endif

#include "radiopagingkanji.h"
#include "radiopagingcharsettables.h"

namespace {

const char hexDigits[17] = "0123456789ABCDEF";

bool leadByte(unsigned char b)
{
    return ((b >= 0x81) && (b <= 0x9f)) || ((b >= 0xe0) && (b <= 0xfc));
}

// Unicode to Shift JIS; false if a character has no code
bool toShiftJis(const QString& text, QByteArray& bytes)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QStringEncoder encoder("Shift_JIS");

    if (!encoder.isValid()) {
        return false;
    }

    bytes = encoder.encode(text);
    return !encoder.hasError();
#else
    QTextCodec *codec = QTextCodec::codecForName("Shift_JIS");

    if (!codec) {
        return false;
    }

    QTextCodec::ConverterState state;
    bytes = codec->fromUnicode(text.constData(), text.size(), &state);
    return state.invalidChars == 0;
#endif
}

QString fromShiftJis(const QByteArray& bytes)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QStringDecoder decoder("Shift_JIS");
    return decoder.isValid() ? QString(decoder.decode(bytes)) : QString::fromLatin1(bytes);
#else
    QTextCodec *codec = QTextCodec::codecForName("Shift_JIS");
    return codec ? codec->toUnicode(bytes) : QString::fromLatin1(bytes);
#endif
}

int digitValue(char c)
{
    return ((c >= '0') && (c <= '9')) ? c - '0' : ((c >= 'A') && (c <= 'F')) ? c - 'A' + 10
        : ((c >= 'a') && (c <= 'f')) ? c - 'a' + 10 : -1;
}

// The Kanji codepage of a FLEX channel: its characters are those of the HEX/binary messages (RadioPagingKanji), the
// alphanumeric messages keep ASCII
bool encode(const std::string& text, std::string& out, std::string& error, std::string&)
{
    return RadioPagingKanji::encode(text, 16, out, error);
}

std::string decode(const std::string& hex)
{
    return RadioPagingKanji::decode(hex, 16);
}

} // namespace

bool RadioPagingKanji::encode(const std::string& text, int blocking, std::string& hex, std::string& error)
{
    QByteArray bytes;

    if ((blocking != 8) && (blocking != 16))
    {
        error = "Kanji: a blocking length of 8 or 16 bits";
        return false;
    }
    if (!toShiftJis(QString::fromUtf8(text.c_str(), (int) text.size()), bytes))
    {
        error = "Kanji: a character has no Shift JIS code";
        return false;
    }

    hex.clear();

    for (int i = 0; i < bytes.size(); i++)
    {
        unsigned value = (unsigned char) bytes[i];

        if (leadByte((unsigned char) value) && (i + 1 < bytes.size()))
        {
            if (blocking == 8)
            {
                error = "Kanji: a 2-byte character in 8-bit characters";
                return false;
            }

            value = (value << 8) | (unsigned char) bytes[++i];
        }

        for (int d = 0; d < blocking / 4; d++) {
            hex += hexDigits[(value >> (4 * d)) & 0xf];
        }
    }

    return true;
}

std::string RadioPagingKanji::decode(const std::string& hex, int blocking, bool rightToLeft)
{
    const int digits = (blocking == 8) ? 2 : 4;
    std::vector<QString> characters;
    size_t pos = 0;

    while (pos < hex.size())
    {
        if (hex[pos] == '|')
        {
            characters.push_back(QString(' '));
            pos++;
            continue;
        }

        unsigned value = 0;
        bool lost = false;
        int d = 0;

        for (; (d < digits) && (pos < hex.size()) && (hex[pos] != '|'); d++, pos++)
        {
            int v = digitValue(hex[pos]);
            lost = lost || (v < 0);
            value |= (unsigned) std::max(0, v) << (4 * d);
        }

        if (lost || (d < digits))
        {
            characters.push_back(QString('?'));
        }
        else if (value > 0xff)
        {
            QByteArray bytes;
            bytes.append((char) (value >> 8));
            bytes.append((char) (value & 0xff));
            characters.push_back(fromShiftJis(bytes));
        }
        else if (value != 0)
        {
            characters.push_back(fromShiftJis(QByteArray(1, (char) value)));
        }
    }

    // Display direction D = 1 (3.10.1.2): the first character is shown at the right
    if (rightToLeft) {
        std::reverse(characters.begin(), characters.end());
    }

    QString text;

    for (const QString& c : characters) {
        text += c;
    }

    return text.toStdString();
}

const RadioPagingCharset::Transcoder RadioPagingCharsetTables::kanji = {
    "Kanji: the text is sent as a HEX/binary message of 16-bit Shift JIS characters (STD-43A Japanese Text Service); "
    "the HEX/binary messages of 8 or 16-bit characters received are shown as text",
    encode,
    decode,
    true
};
