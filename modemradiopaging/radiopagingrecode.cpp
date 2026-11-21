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

#include <cstdio>
#include <memory>

#include <QRegularExpression>

#include "radiopagingpage.h"
#include "radiopagingpocsag.h"
#include "radiopaginggsc.h"
#include "radiopagingkanji.h"
#include "radiopagingrecode.h"

namespace {

bool characters(const MessageRecord& record)
{
    return (record.m_msgType == "alphanumeric") || (record.m_msgType == "numeric");
}

// POCSAG and GSC: the interpretation, the text and the other candidate as the type asked for
template <typename Interpretation>
void setInterpretation(const Interpretation& interpretation, const QString& type, MessageRecord& out)
{
    const bool first = (type.toStdString() == interpretation.m_type);

    out.m_msgType = type;
    out.m_text = RadioPagingRecode::escapeText(first ? interpretation.m_text : interpretation.m_altText);
    out.m_protocolData.insert("score", first ? interpretation.m_score : interpretation.m_altScore);
    QString altType = QString::fromStdString(first ? interpretation.m_altType : interpretation.m_type);
    QString altText = RadioPagingRecode::escapeText(first ? interpretation.m_altText : interpretation.m_text);

    if (!RadioPagingRecode::alternativeGiven(altType, altText))
    {
        altType.clear();
        altText.clear();
    }

    out.m_protocolData.insert("alt_type", altType);
    out.m_protocolData.insert("alt_text", altText);
    out.m_protocolData.insert("alt_score", first ? interpretation.m_altScore : interpretation.m_score);
}

// The status of each character of the text, none when it is not known (the text is coloured as a whole)
void setCharStatus(MessageRecord& out, const QString& charStatus)
{
    if (charStatus.isEmpty()) {
        out.m_protocolData.remove("char_status");
    } else {
        out.m_protocolData.insert("char_status", charStatus);
    }
}

} // namespace

bool RadioPagingRecode::alternativeGiven(const QString& type, const QString& text)
{
    return !(type.contains("numeric") && !type.contains("alpha") && (text.size() > m_maxAltNumeric));
}

QStringList RadioPagingRecode::types(const MessageRecord& record)
{
    if (record.m_direction != MessageRecord::DirectionRX) {
        return QStringList();
    }
    if (((record.m_protocol == "pocsag") || (record.m_protocol == "gsc")) && characters(record) && (record.m_rawBits > 0)) {
        return {"alphanumeric", "numeric"};
    }
    if ((record.m_protocol == "flex") && record.m_protocolData.contains("alpha_codes")) {
        return {"alphanumeric"};
    }
    if ((record.m_protocol == "flex") && (record.m_msgType == "binary") && record.m_protocolData.contains("hex")) {
        return {"binary"};
    }

    return QStringList();
}

bool RadioPagingRecode::redecode(const MessageRecord& record, const RadioPagingCharset::Key& charset, const QString& type,
    MessageRecord& out, QString& error)
{
    const QStringList can = types(record);
    const QString as = type.isEmpty() ? record.m_msgType : type;
    const std::string protocol = record.m_protocol.toStdString();
    const RadioPagingCharset::Key key = charset.empty() ? RadioPagingPage::charsetKey(record.m_protocolData.value("charset")) : charset;
    const RadioPagingCharset::Codepage *codepage = RadioPagingCharset::find(protocol, key);
    // The custom codepage: the one the record was decoded with (its codes are kept with it)
    std::unique_ptr<RadioPagingCustomCodepage> custom;

    if (RadioPagingCustomCodepage::isCustom(key))
    {
        if (!record.m_protocolData.contains("custom_codes"))
        {
            error = QString("The record keeps no custom codepage");
            return false;
        }

        custom.reset(new RadioPagingCustomCodepage(protocol, record.m_protocolData.value("custom_codes").toString().toStdString()));
        codepage = &custom->codepage();
    }

    if (!can.contains(as))
    {
        error = can.isEmpty() ? QString("The record keeps nothing to decode again")
            : QString("A %1 record decodes as %2").arg(record.m_protocol).arg(can.join(" or "));
        return false;
    }
    if (!codepage)
    {
        if (!charset.empty())
        {
            error = RadioPagingPage::checkCharset(record.m_protocol, charset);
            return false;
        }

        codepage = RadioPagingCharset::defaultFor(protocol);
    }

    out = record;
    const std::vector<uint8_t> raw(record.m_rawData.begin(), record.m_rawData.end());

    if (record.m_protocol == "pocsag")
    {
        std::vector<uint8_t> fill;

        for (const QVariant& code : record.m_protocolData.value("fill_codes").toList()) {
            fill.push_back((uint8_t) code.toUInt());
        }

        setInterpretation(RadioPagingPocsag::interpret(raw, record.m_rawBits, codepage,
            record.m_protocolData.contains("fill_codes") ? fill : RadioPagingPocsag::fillMinimal()), as, out);

        // The characters of the codewords that could not be corrected are '?', with the status of each character
        QString charStatus;
        out.m_text = pocsagText(raw, record.m_rawBits, as.toStdString(), codepage,
            record.m_protocolData.contains("fill_codes") ? fill : RadioPagingPocsag::fillMinimal(),
            record.m_protocolData.value("word_status").toString().toStdString(), charStatus);

        if (codepage->m_rtl && (as == "alphanumeric")) {
            reverseText(out.m_text, charStatus);
        }

        setCharStatus(out, charStatus);
    }
    else if (record.m_protocol == "gsc")
    {
        // The characters of the codewords that could not be corrected stay '?'
        const QByteArray lostBytes = QByteArray::fromHex(record.m_protocolData.value("lost_bits").toString().toLatin1());
        const std::vector<uint8_t> lost(lostBytes.begin(), lostBytes.end());
        setInterpretation(RadioPagingGsc::interpret(raw, record.m_rawBits, codepage, lost.empty() ? nullptr : &lost), as, out);

        if (codepage->m_rtl && (out.m_msgType == "alphanumeric"))
        {
            QString noStatus;
            reverseText(out.m_text, noStatus);
        }
    }
    else if (as == "binary")
    {
        // HEX/binary of 8 or 16-bit characters: Shift JIS text with the Kanji codepage (STD-43A 1.3.1), the hex
        // digits with any other
        const QString hex = record.m_protocolData.value("hex").toString();

        if (!(codepage->m_transcoder && codepage->m_transcoder->m_binary))
        {
            out.m_text = hex;
            out.m_protocolData.remove("charset");
            out.m_protocolData.insert("redecoded", true);
            return true;
        }

        out.m_text = escapeText(RadioPagingKanji::decode(hex.toStdString(), record.m_protocolData.value("blocking", 16).toInt(),
            record.m_protocolData.value("display_direction").toString() == "rtl"));
    }
    else
    {
        // The 7-bit characters of an alphanumeric message: not the Kanji codepage (HEX/binary messages)
        if (codepage->m_transcoder && codepage->m_transcoder->m_binary)
        {
            error = QString("Kanji is the codepage of HEX/binary messages");
            return false;
        }

        const QByteArray codes = QByteArray::fromHex(record.m_protocolData.value("alpha_codes").toString().toLatin1());
        QString charStatus;
        out.m_text = decodeWithStatus(std::string(codes.begin(), codes.end()), record.m_protocolData.value("code_status").toString().toStdString(),
            codepage, charStatus);

        if (codepage->m_rtl) {
            reverseText(out.m_text, charStatus);
        }

        setCharStatus(out, charStatus);
    }

    out.m_protocolData.insert("charset", RadioPagingPage::charsetData(codepage->key()));
    out.m_protocolData.insert("redecoded", true);
    return true;
}

void RadioPagingRecode::reverseText(QString& text, QString& charStatus)
{
    static const QRegularExpression code("^<0x[0-9A-F]{2}>");
    const bool withStatus = (charStatus.size() == text.size());
    QStringList units, statuses;
    int i = 0;

    while (i < text.size())
    {
        int length = 1;

        if (text.at(i) == '<')
        {
            if (text.mid(i, 5) == "<...>") {
                length = 5;
            } else if (code.match(text.mid(i, 6)).hasMatch()) {
                length = 6;
            }
        }
        else if (text.at(i).isHighSurrogate() && (i + 1 < text.size()) && text.at(i + 1).isLowSurrogate())
        {
            length = 2;
        }

        units.prepend(text.mid(i, length));

        if (withStatus) {
            statuses.prepend(charStatus.mid(i, length));
        }

        i += length;
    }

    text = units.join(QString());

    if (withStatus) {
        charStatus = statuses.join(QString());
    }
}

QString RadioPagingRecode::escapeText(const std::string& text)
{
    std::string out;

    for (char c : text)
    {
        unsigned char u = (unsigned char) c;

        if ((u < 0x20) || (u == 0x7f))
        {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "<0x%02X>", u);
            out += buf;
        }
        else
        {
            out.push_back(c);
        }
    }

    return QString::fromUtf8(out.c_str(), (int) out.size());
}

namespace {

char worse(char a, char b)
{
    auto rank = [](char s) { return (s == 'X') ? 3 : (s == 'u') ? 2 : (s == 'c') ? 1 : 0; };
    return (rank(b) > rank(a)) ? b : a;
}

} // namespace

QString RadioPagingRecode::decodeWithStatus(const std::string& codes, const std::string& codeStatus,
    const RadioPagingCharset::Codepage *codepage, QString& charStatus)
{
    auto decode = [codepage](const std::string& run) {
        return RadioPagingRecode::escapeText(codepage ? RadioPagingCharset::decode(*codepage, std::vector<uint8_t>(run.begin(), run.end())) : run);
    };

    charStatus.clear();

    if (codeStatus.size() != codes.size()) {
        return decode(codes);
    }

    QString text;
    std::string run;
    std::string runStatus;

    // A run of codes received: decoded together (a codepage may join codes); its characters take the status of their
    // code when there is one character a code, else the worst of the run
    auto flush = [&]() {
        if (run.empty()) {
            return;
        }

        QString piece = decode(run);
        char worst = '.';

        for (char s : runStatus) {
            worst = worse(worst, s);
        }
        for (int i = 0; i < piece.size(); i++) {
            charStatus += QChar((piece.size() == (int) runStatus.size()) ? runStatus[(size_t) i] : worst);
        }

        text += piece;
        run.clear();
        runStatus.clear();
    };

    for (size_t i = 0; i < codes.size(); i++)
    {
        const char s = codeStatus[i];

        if (s == 'X')
        {
            flush();
            text += '?';
            charStatus += 'X';
        }
        else if (s == 'u')
        {
            // A code of a word that could not be corrected, as received: shown alone, an unprintable one as '?',
            // so the text keeps one character a code
            flush();
            const unsigned char u = (unsigned char) codes[i];
            QString piece = ((u < 0x20) || (u == 0x7f)) ? QString() : decode(std::string(1, codes[i]));

            if (piece.isEmpty() || piece.contains("<0x")) {
                piece = "?";
            }

            text += piece;
            charStatus += QString(piece.size(), QChar('u'));
        }
        else if (s == 'G')
        {
            flush();
            text += "<...>";
            charStatus += "XXXXX";
        }
        else
        {
            run += codes[i];
            runStatus += (s == 'c') ? 'c' : '.';
        }
    }

    flush();
    return text;
}

std::string RadioPagingRecode::pocsagCodeStatus(const std::string& wordStatus, size_t codes, int bitsPerCode)
{
    std::string status;

    for (size_t i = 0; i < codes; i++)
    {
        size_t first = (i * bitsPerCode) / 20;
        size_t last = (i * bitsPerCode + bitsPerCode - 1) / 20;

        if (last >= wordStatus.size()) {
            return std::string();
        }

        char s = wordStatus[first];

        for (size_t w = first + 1; w <= last; w++) {
            s = worse(s, wordStatus[w]);
        }

        status += s;
    }

    return status;
}

QString RadioPagingRecode::pocsagText(const std::vector<uint8_t>& raw, int rawBits, const std::string& type,
    const RadioPagingCharset::Codepage *codepage, const std::vector<uint8_t>& fill, const std::string& wordStatus,
    QString& charStatus)
{
    const bool numeric = (type == "numeric");
    const std::string codes = numeric ? RadioPagingPocsag::decodeNumeric(raw, rawBits)
        : RadioPagingPocsag::decodeAlphanumeric(raw, rawBits, fill);

    return decodeWithStatus(codes, pocsagCodeStatus(wordStatus, codes.size(), numeric ? 4 : 7), numeric ? nullptr : codepage,
        charStatus);
}
