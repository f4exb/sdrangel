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

// ROT-13 over ASCII: each letter A-Z and a-z is replaced by the 13th letter after it, the other characters are kept.
// The same transform encodes and decodes

#include "radiopagingcharsettables.h"

namespace {

// The letters turned; a code given as <0xNN> is kept
std::string rotate(const std::string& text)
{
    std::string out;
    size_t pos = 0;

    while (pos < text.size())
    {
        if ((text.compare(pos, 3, "<0x") == 0) && (pos + 6 <= text.size()) && (text[pos + 5] == '>'))
        {
            out += text.substr(pos, 6);
            pos += 6;
            continue;
        }

        char c = text[pos++];

        if ((c >= 'a') && (c <= 'z')) {
            c = (char) ('a' + ((c - 'a' + 13) % 26));
        } else if ((c >= 'A') && (c <= 'Z')) {
            c = (char) ('A' + ((c - 'A' + 13) % 26));
        }

        out.push_back(c);
    }

    return out;
}

bool encode(const std::string& text, std::string& out, std::string&, std::string&)
{
    out = rotate(text);
    return true;
}

std::string decode(const std::string& text)
{
    return rotate(text);
}

} // namespace

const RadioPagingCharset::Transcoder RadioPagingCharsetTables::rot13 = {
    "ROT-13: the text is written and read as it is, its letters A-Z and a-z are sent turned by 13 places",
    encode,
    decode
};
