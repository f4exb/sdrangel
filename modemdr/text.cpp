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

#include "text.h"

namespace modemdr
{

namespace
{

// Complete EBU Latin based repertoire (TS 101 756 annex C) to Unicode. Codes 0x00, 0x0A,
// 0x0B and 0x1F are reserved (control codes in dynamic labels).
const std::uint16_t ebuLatin[256] = {
    0x0000, 0x0118, 0x012E, 0x0172, 0x0102, 0x0116, 0x010E, 0x0218,
    0x021A, 0x010A, 0x0000, 0x0000, 0x0120, 0x0139, 0x017B, 0x0143,
    0x0105, 0x0119, 0x012F, 0x0173, 0x0103, 0x0117, 0x010F, 0x0219,
    0x021B, 0x010B, 0x0147, 0x011A, 0x0121, 0x013A, 0x017C, 0x0000,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0142, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005A, 0x005B, 0x016E, 0x005D, 0x0141, 0x005F,
    0x0104, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x0079, 0x007A, 0x00AB, 0x016F, 0x00BB, 0x013D, 0x0126,
    0x00E1, 0x00E0, 0x00E9, 0x00E8, 0x00ED, 0x00EC, 0x00F3, 0x00F2,
    0x00FA, 0x00F9, 0x00D1, 0x00C7, 0x015E, 0x00DF, 0x00A1, 0x0178,
    0x00E2, 0x00E4, 0x00EA, 0x00EB, 0x00EE, 0x00EF, 0x00F4, 0x00F6,
    0x00FB, 0x00FC, 0x00F1, 0x00E7, 0x015F, 0x011F, 0x0131, 0x00FF,
    0x0136, 0x0145, 0x00A9, 0x0122, 0x011E, 0x011B, 0x0148, 0x0151,
    0x0150, 0x20AC, 0x00A3, 0x0024, 0x0100, 0x0112, 0x012A, 0x016A,
    0x0137, 0x0146, 0x013B, 0x0123, 0x013C, 0x0130, 0x0144, 0x0171,
    0x0170, 0x00BF, 0x013E, 0x00B0, 0x0101, 0x0113, 0x012B, 0x016B,
    0x00C1, 0x00C0, 0x00C9, 0x00C8, 0x00CD, 0x00CC, 0x00D3, 0x00D2,
    0x00DA, 0x00D9, 0x0158, 0x010C, 0x0160, 0x017D, 0x00D0, 0x013F,
    0x00C2, 0x00C4, 0x00CA, 0x00CB, 0x00CE, 0x00CF, 0x00D4, 0x00D6,
    0x00DB, 0x00DC, 0x0159, 0x010D, 0x0161, 0x017E, 0x0111, 0x0140,
    0x00C3, 0x00C5, 0x00C6, 0x0152, 0x0177, 0x00DD, 0x00D5, 0x00D8,
    0x00DE, 0x014A, 0x0154, 0x0106, 0x015A, 0x0179, 0x0164, 0x00F0,
    0x00E3, 0x00E5, 0x00E6, 0x0153, 0x0175, 0x00FD, 0x00F5, 0x00F8,
    0x00FE, 0x014B, 0x0155, 0x0107, 0x015B, 0x017A, 0x0165, 0x0127
};

void appendUTF8(std::string& out, uint32_t codePoint)
{
    if (codePoint < 0x80)
    {
        out.push_back(static_cast<char>(codePoint));
    }
    else if (codePoint < 0x800)
    {
        out.push_back(static_cast<char>(0xc0 | (codePoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    }
    else if (codePoint < 0x10000)
    {
        out.push_back(static_cast<char>(0xe0 | (codePoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    }
    else if (codePoint < 0x110000)
    {
        out.push_back(static_cast<char>(0xf0 | (codePoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    }
}

// Control codes shared by every character set in labels: preferred line break (0x0a),
// end of headline (0x0b) and preferred word break (0x1f).
bool appendControl(std::string& out, uint32_t code)
{
    if ((code == 0x0a) || (code == 0x0b))
    {
        out.push_back('\n');
        return true;
    }
    if (code == 0x1f) {
        return true;
    }
    if (code < 0x20) {
        return true;    // Other C0 controls are never displayed
    }
    return false;
}

// Keep only well formed UTF-8 (broadcast text is untrusted).
std::string sanitizeUTF8(const uint8_t *data, std::size_t length)
{
    std::string out;
    std::size_t i = 0;

    while (i < length)
    {
        uint8_t c = data[i];
        int extra;
        uint32_t cp;

        if (c < 0x80) {
            extra = 0;
            cp = c;
        } else if ((c & 0xe0) == 0xc0) {
            extra = 1;
            cp = c & 0x1f;
        } else if ((c & 0xf0) == 0xe0) {
            extra = 2;
            cp = c & 0x0f;
        } else if ((c & 0xf8) == 0xf0) {
            extra = 3;
            cp = c & 0x07;
        } else {
            i++;
            continue;
        }
        if (i + extra >= length) {
            break;  // Truncated sequence
        }
        bool ok = true;
        for (int j = 1; ok && j <= extra; j++)
        {
            uint8_t cc = data[i + j];
            if ((cc & 0xc0) != 0x80) {
                ok = false;
            } else {
                cp = (cp << 6) | (cc & 0x3f);
            }
        }
        if (!ok)
        {
            i++;
            continue;
        }
        i += extra + 1;
        if (!appendControl(out, cp)) {
            appendUTF8(out, cp);
        }
    }
    return out;
}

} // anonymous namespace

std::string Text::toUTF8(const std::uint8_t *data, std::size_t length, int charset)
{
    // Labels are padded with zeros
    while ((length > 0) && (data[length - 1] == 0)) {
        length--;
    }

    std::string out;

    switch (charset)
    {
    case 0: // Complete EBU Latin based repertoire
        for (std::size_t i = 0; i < length; i++)
        {
            if (!appendControl(out, data[i]))
            {
                uint16_t cp = ebuLatin[data[i]];
                if (cp) {
                    appendUTF8(out, cp);
                }
            }
        }
        break;
    case 4: // ISO 8859-1 (MOT ContentName)
        for (std::size_t i = 0; i < length; i++)
        {
            if (!appendControl(out, data[i])) {
                appendUTF8(out, data[i]);
            }
        }
        break;
    case 6: // UCS-2 / UTF-16 BE, basic multilingual plane
        for (std::size_t i = 0; i + 1 < length; i += 2)
        {
            uint32_t cp = (static_cast<uint32_t>(data[i]) << 8) | data[i + 1];
            if ((cp >= 0xd800) && (cp <= 0xdfff)) {
                continue;
            }
            if (!appendControl(out, cp)) {
                appendUTF8(out, cp);
            }
        }
        break;
    case 15: // UTF-8
        out = sanitizeUTF8(data, length);
        break;
    default:
        break;
    }

    return out;
}

std::string Text::contentNameToUTF8(const std::uint8_t *data, std::size_t length, int charset)
{
    if (charset != 0) {
        return toUTF8(data, length, charset);
    }

    std::string out;
    for (std::size_t i = 0; i < length; i++)
    {
        if (data[i] < 0x80)
        {
            if (data[i] >= 0x20) {
                out.push_back(static_cast<char>(data[i]));
            }
        }
        else if (ebuLatin[data[i]])
        {
            appendUTF8(out, ebuLatin[data[i]]);
        }
    }
    return out;
}

} // namespace modemdr
