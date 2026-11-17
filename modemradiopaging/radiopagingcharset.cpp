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

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "charsets/radiopagingcharsettables.h"
#include "radiopagingcharset.h"

using namespace RadioPagingCharsetTables;

const int RadioPagingCharset::m_maxGlyphs;

namespace {

std::string lower(const std::string& s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return (char) std::tolower(c); });
    return out;
}

int utf8Length(unsigned char c)
{
    if ((c & 0x80) == 0x00) {
        return 1;
    }
    if ((c & 0xe0) == 0xc0) {
        return 2;
    }
    if ((c & 0xf0) == 0xe0) {
        return 3;
    }
    if ((c & 0xf8) == 0xf0) {
        return 4;
    }
    return 1;
}

int hexValue(char c)
{
    return ((c >= '0') && (c <= '9')) ? c - '0' : ((c >= 'a') && (c <= 'f')) ? c - 'a' + 10
        : ((c >= 'A') && (c <= 'F')) ? c - 'A' + 10 : -1;
}

} // namespace

const char *RadioPagingCharset::Codepage::glyph(unsigned code) const
{
    return m_table->m_entries[code & (unsigned) (codes() - 1)][0];
}

std::vector<std::string> RadioPagingCharset::Codepage::glyphs(unsigned code) const
{
    std::vector<std::string> list;
    const Entry& entry = m_table->m_entries[code & (unsigned) (codes() - 1)];

    for (int i = 0; (i < m_maxGlyphs) && entry[i]; i++) {
        list.push_back(entry[i]);
    }

    return list;
}

const std::vector<RadioPagingCharset::Codepage>& RadioPagingCharset::codepages()
{
    static const std::vector<Codepage> list = {
        // POCSAG
        {"pocsag", "generic", "generic", "ascii", "Generic - ASCII", &pocsagAscii},
        {"pocsag", "motorola", "advisor", "ascii", "Motorola Advisor - ASCII", &pocsagAscii},
        {"pocsag", "generic", "generic", "ascii-uppercase", "Generic - ASCII (uppercase)", &pocsagAsciiUppercase},
        {"pocsag", "motorola", "bravo-alpha", "ascii-uppercase", "Motorola Bravo Alpha - ASCII (uppercase)", &pocsagAsciiUppercase},
        {"pocsag", "generic", "generic", "latin-british", "Generic - British", &pocsagLatinBritish},
        {"pocsag", "generic", "generic", "latin-danish-norwegian", "Generic - Danish/Norwegian", &pocsagLatinDanishNorwegian},
        {"pocsag", "generic", "generic", "latin-french", "Generic - French", &pocsagLatinFrench},
        {"pocsag", "generic", "generic", "latin-german", "Generic - German", &pocsagLatinGerman},
        {"pocsag", "generic", "generic", "latin-italian", "Generic - Italian", &pocsagLatinItalian},
        {"pocsag", "generic", "generic", "latin-portuguese", "Generic - Portuguese", &pocsagLatinPortuguese},
        {"pocsag", "generic", "generic", "latin-spanish", "Generic - Spanish", &pocsagLatinSpanish},
        {"pocsag", "generic", "generic", "latin-swedish", "Generic - Swedish", &pocsagLatinSwedish},
        {"pocsag", "generic", "generic", "latin-swiss", "Generic - Swiss", &pocsagLatinSwiss},
        {"pocsag", "generic", "generic", "hebrew", "Generic - Hebrew", &pocsagLatinHebrew},
        {"pocsag", "motorola", "advisor", "cyrillic", "Motorola Advisor - Cyrillic", &pocsagMotorolaAdvisorCyrillic},
        {"pocsag", "motorola", "advisor-linguist", "latin-cyrillic", "Motorola Advisor Linguist - Latin/Cyrillic (uppercase)",
            &pocsagMotorolaAdvisorLinguistLatinCyrillic},
        {"pocsag", "motorola", "scriptor-lx2-linguist", "latin-cyrillic", "Motorola Scriptor LX2 Linguist - Latin/Cyrillic (uppercase)",
            &pocsagMotorolaAdvisorLinguistLatinCyrillic},
        {"pocsag", "nec", "maxima", "latin-cyrillic", "NEC Maxima - Latin/Cyrillic (uppercase)", &pocsagMotorolaAdvisorLinguistLatinCyrillic},
        {"pocsag", "nec", "optima", "latin-cyrillic", "NEC Optima - Latin/Cyrillic (uppercase)", &pocsagMotorolaAdvisorLinguistLatinCyrillic},
        {"pocsag", "truly", "supervisor", "latin-cyrillic", "Truly Supervisor - Latin/Cyrillic (uppercase)",
            &pocsagMotorolaAdvisorLinguistLatinCyrillic},
        {"pocsag", "multitone", "generic", "ascii", "Multitone - ASCII (uppercase)", &pocsagMultitoneAscii},
        {"pocsag", "nec", "21a", "latin-cyrillic", "NEC 21A - Latin/Cyrillic (uppercase)", &pocsagNec21aLatinCyrillic},
        {"pocsag", "nec", "maxima-compact", "cyrillic", "NEC Maxima/Compact - Cyrillic", &pocsagNecMaximaCompactCyrillic},
        {"pocsag", "philips", "prg2220", "latin-cyrillic", "Philips PRG2220 - Latin/Cyrillic", &pocsagPhilipsPrg2220LatinCyrillic},
        {"pocsag", "swissphone", "de506", "ascii", "Swissphone DE506 - ASCII", &pocsagAscii},
        {"pocsag", "swissphone", "de506", "cyrillic", "Swissphone DE506 - Cyrillic", &pocsagSwissphoneDe506Cyrillic},
        {"pocsag", "partner", "generic", "cyrillic", "Partner - Cyrillic", &pocsagSwissphoneDe506Cyrillic},
        {"pocsag", "generic", "generic", "ascii-rot13", "Generic - ASCII, ROT-13", &pocsagAscii, &rot13},
        // FLEX
        {"flex", "generic", "generic", "ascii", "Generic - ASCII", &pocsagAscii},
        {"flex", "motorola", "linguist", "cyrillic", "Motorola Linguist - Cyrillic", &flexLinguistCyrillic},
        {"flex", "motorola", "linguist", "icelandic", "Motorola Linguist - Icelandic", &flexLinguistIcelandic},
        {"flex", "motorola", "linguist", "polish", "Motorola Linguist - Polish", &flexLinguistPolish},
        {"flex", "motorola", "linguist", "vietnamese", "Motorola Linguist - Vietnamese", &flexLinguistVietnamese},
        {"flex", "motorola", "linguist", "arabic", "Motorola Linguist - Arabic", &flexLinguistArabic},
        {"flex", "motorola", "linguist", "arabic-english", "Motorola Linguist - Arabic/English", &flexLinguistArabicEnglish},
        {"flex", "motorola", "linguist", "turkish", "Motorola Linguist - Turkish", &flexLinguistTurkish},
        {"flex", "motorola", "linguist", "hebrew", "Motorola Linguist - Hebrew", &flexLinguistHebrew},
        {"flex", "motorola", "linguist", "greek", "Motorola Linguist - Greek", &flexLinguistGreek},
        {"flex", "motorola", "linguist", "latin1", "Motorola Linguist - Latin-1 (ISO 8859-1)", &flexLinguistLatin1},
        {"flex", "motorola", "linguist", "latin2", "Motorola Linguist - Latin-2 (ISO 8859-2)", &flexLinguistLatin2},
        {"flex", "motorola", "linguist", "latin3", "Motorola Linguist - Latin-3 (ISO 8859-3)", &flexLinguistLatin3},
        {"flex", "motorola", "linguist", "latin4", "Motorola Linguist - Latin-4 (ISO 8859-4)", &flexLinguistLatin4},
        {"flex", "timex", "beepwear-pro", "alarm", "Timex BeepwearPRO - Alarm command (key=value)", &pocsagAscii, &beepwearProAlarm},
        {"flex", "generic", "generic", "ascii-rot13", "Generic - ASCII, ROT-13", &pocsagAscii, &rot13},
        {"flex", "generic", "generic", "kanji", "Generic - Kanji (Shift JIS, HEX/binary messages)", &pocsagAscii, &kanji},
        // GSC
        {"gsc", "generic", "generic", "ascii", "Generic - ASCII", &gscAscii},
    };

    return list;
}

std::vector<const RadioPagingCharset::Codepage*> RadioPagingCharset::codepages(const std::string& protocol)
{
    std::vector<const Codepage*> list;
    const std::string p = (lower(protocol) == "golay") ? std::string("gsc") : lower(protocol);

    for (const Codepage& codepage : codepages())
    {
        if (p == codepage.m_protocol) {
            list.push_back(&codepage);
        }
    }

    return list;
}

const RadioPagingCharset::Codepage *RadioPagingCharset::find(const std::string& protocol, const Key& key)
{
    const std::string vendor = lower(key.m_vendor);
    const std::string model = lower(key.m_model);
    const std::string page = lower(key.m_codepage);

    for (const Codepage *codepage : codepages(protocol))
    {
        if ((vendor == codepage->m_vendor) && (model == codepage->m_model) && (page == codepage->m_codepage)) {
            return codepage;
        }
    }

    return nullptr;
}

const RadioPagingCharset::Codepage *RadioPagingCharset::defaultFor(const std::string& protocol)
{
    std::vector<const Codepage*> list = codepages(protocol);
    return list.empty() ? nullptr : list.front();
}

std::string RadioPagingCharset::decode(const Codepage& codepage, const std::vector<uint8_t>& codes)
{
    std::string text;

    for (uint8_t code : codes)
    {
        const char *glyph = codepage.glyph(code);

        if (glyph)
        {
            text += glyph;
        }
        else
        {
            char hex[8];
            std::snprintf(hex, sizeof(hex), "<0x%02X>", code & (codepage.codes() - 1));
            text += hex;
        }
    }

    return (codepage.m_transcoder && !codepage.m_transcoder->m_binary) ? codepage.m_transcoder->m_decode(text) : text;
}

int RadioPagingCharset::encodeOne(const Codepage& codepage, const std::string& text, size_t& pos)
{
    // A code given as <0xNN>
    if ((pos + 6 <= text.size()) && (text[pos] == '<') && (text[pos + 1] == '0') && ((text[pos + 2] == 'x') || (text[pos + 2] == 'X'))
        && (hexValue(text[pos + 3]) >= 0) && (hexValue(text[pos + 4]) >= 0) && (text[pos + 5] == '>'))
    {
        int code = hexValue(text[pos + 3]) * 16 + hexValue(text[pos + 4]);

        if (code < codepage.codes())
        {
            pos += 6;
            return code;
        }
    }

    size_t length = std::min((size_t) utf8Length((unsigned char) text[pos]), text.size() - pos);

    // A glyph of several characters (Arabic lam-alef) first, the longest one
    int best = -1;
    size_t bestLength = length;

    for (int code = 0; code < codepage.codes(); code++)
    {
        const Entry& entry = codepage.m_table->m_entries[code];

        for (int i = 0; (i < m_maxGlyphs) && entry[i]; i++)
        {
            const std::string_view glyph(entry[i]);

            if ((glyph.size() > bestLength) && (text.compare(pos, glyph.size(), glyph) == 0))
            {
                best = code;
                bestLength = glyph.size();
            }
        }
    }

    if (best >= 0)
    {
        pos += bestLength;
        return best;
    }

    for (int code = 0; code < codepage.codes(); code++)
    {
        const Entry& entry = codepage.m_table->m_entries[code];

        for (int i = 0; (i < m_maxGlyphs) && entry[i]; i++)
        {
            const std::string_view glyph(entry[i]);

            if ((glyph.size() == length) && (text.compare(pos, length, glyph) == 0))
            {
                pos += length;
                return code;
            }
        }
    }

    pos += length;
    return -1;
}

bool RadioPagingCharset::encode(const Codepage& codepage, const std::string& userText, std::vector<uint8_t>& codes, std::string *error,
    std::string *warning)
{
    std::string text = userText;

    if (codepage.m_transcoder && !codepage.m_transcoder->m_binary)
    {
        std::string transcoderError, transcoderWarning;

        if (!codepage.m_transcoder->m_encode(userText, text, transcoderError, transcoderWarning))
        {
            if (error) {
                *error = transcoderError;
            }
            return false;
        }
        if (warning) {
            *warning = transcoderWarning;
        }
    }

    size_t pos = 0;

    while (pos < text.size())
    {
        size_t start = pos;
        int code = encodeOne(codepage, text, pos);

        if (code < 0)
        {
            if (error) {
                *error = "The character '" + text.substr(start, pos - start) + "' has no code in the codepage " + codepage.m_label;
            }
            return false;
        }

        codes.push_back((uint8_t) code);
    }

    return true;
}

namespace {

std::string utf8(uint32_t codePoint)
{
    std::string out;

    if (codePoint < 0x80)
    {
        out.push_back((char) codePoint);
    }
    else if (codePoint < 0x800)
    {
        out.push_back((char) (0xC0 | (codePoint >> 6)));
        out.push_back((char) (0x80 | (codePoint & 0x3F)));
    }
    else if (codePoint < 0x10000)
    {
        out.push_back((char) (0xE0 | (codePoint >> 12)));
        out.push_back((char) (0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back((char) (0x80 | (codePoint & 0x3F)));
    }
    else
    {
        out.push_back((char) (0xF0 | (codePoint >> 18)));
        out.push_back((char) (0x80 | ((codePoint >> 12) & 0x3F)));
        out.push_back((char) (0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back((char) (0x80 | (codePoint & 0x3F)));
    }

    return out;
}

} // namespace

bool RadioPagingCustomCodepage::parse(const std::string& protocol, const std::string& text, Codes& codes, bool& rtl, std::string& error)
{
    const RadioPagingCharset::Codepage *base = RadioPagingCharset::defaultFor(protocol);
    const int size = base ? base->codes() : 128;
    std::string token;
    codes.clear();
    rtl = false;
    error.clear();

    auto take = [&]() {
        if (token.empty()) {
            return true;
        }
        if ((lower(token) == "rtl") || (lower(token) == "ltr"))
        {
            rtl = (lower(token) == "rtl");
            token.clear();
            return true;
        }

        const size_t equal = token.find('=');
        char *end = nullptr;
        const long code = (equal == std::string::npos) ? -1 : std::strtol(token.substr(0, equal).c_str(), &end, 16);
        const bool codeOk = (equal != std::string::npos) && (equal > 0) && end && (*end == '\0') && (code >= 0) && (code < size);
        const unsigned long point = codeOk ? std::strtoul(token.substr(equal + 1).c_str(), &end, 16) : 0;
        const bool pointOk = codeOk && (equal + 1 < token.size()) && end && (*end == '\0') && (point > 0) && (point <= 0x10FFFF)
            && ((point < 0xD800) || (point > 0xDFFF));

        if (!pointOk)
        {
            char limit[8];
            std::snprintf(limit, sizeof(limit), "%02X", size - 1);
            error = "'" + token + "' is not code=character (hex code 00 to " + limit + ", hex Unicode code point)";
            return false;
        }

        for (auto& c : codes)
        {
            if (c.first == (int) code)
            {
                c.second = (uint32_t) point;
                token.clear();
                return true;
            }
        }

        codes.push_back(std::make_pair((int) code, (uint32_t) point));
        token.clear();
        return true;
    };

    for (char c : text)
    {
        if ((c == ' ') || (c == ',') || (c == '\t') || (c == '\n'))
        {
            if (!take()) {
                return false;
            }
        }
        else
        {
            token.push_back(c);
        }
    }

    return take();
}

std::string RadioPagingCustomCodepage::text(const Codes& codes, bool rtl)
{
    std::string out = rtl ? "rtl" : "";

    for (const auto& c : codes)
    {
        char pair[24];
        std::snprintf(pair, sizeof(pair), "%s%02X=%04X", out.empty() ? "" : " ", c.first, c.second);
        out += pair;
    }

    return out;
}

RadioPagingCustomCodepage::RadioPagingCustomCodepage(const std::string& protocol, const std::string& text) :
    m_protocol((lower(protocol) == "golay") ? std::string("gsc") : lower(protocol))
{
    const RadioPagingCharset::Codepage *base = RadioPagingCharset::defaultFor(m_protocol);
    const RadioPagingCharset::Table *table = base ? base->m_table : &RadioPagingCharsetTables::pocsagAscii;
    const int size = 1 << table->m_codeBits;
    Codes codes;
    std::string error;
    bool rtl = false;
    parse(m_protocol, text, codes, rtl, error);   // The codes before the first not valid one

    for (int code = 0; code < 128; code++)
    {
        for (int i = 0; i < RadioPagingCharset::m_maxGlyphs; i++) {
            m_entries[code][i] = (code < size) ? table->m_entries[code][i] : nullptr;
        }
    }

    // The glyphs are kept first (their storage does not move), then pointed to
    m_glyphs.reserve(codes.size());

    for (const auto& c : codes) {
        m_glyphs.push_back(utf8(c.second));
    }
    for (size_t i = 0; i < codes.size(); i++)
    {
        for (int g = 0; g < RadioPagingCharset::m_maxGlyphs; g++) {
            m_entries[codes[i].first][g] = nullptr;
        }

        m_entries[codes[i].first][0] = m_glyphs[i].c_str();
    }

    m_table.m_codeBits = table->m_codeBits;
    m_table.m_entries = m_entries;
    m_codepage.m_protocol = m_protocol.c_str();
    m_codepage.m_vendor = "generic";
    m_codepage.m_model = "generic";
    m_codepage.m_codepage = "custom";
    m_codepage.m_label = "Custom";
    m_codepage.m_table = &m_table;
    m_codepage.m_transcoder = nullptr;
    m_codepage.m_rtl = rtl;
}
