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

#include "jsonscan.h"

namespace
{

void skipSpace(const std::string& json, size_t& at)
{
    while ((at < json.size()) && ((json[at] == ' ') || (json[at] == '\t') || (json[at] == '\r') || (json[at] == '\n'))) {
        at++;
    }
}

//!< Steps over a string, starting on its opening quote and finishing past its closing one
bool skipString(const std::string& json, size_t& at)
{
    if ((at >= json.size()) || (json[at] != '"')) {
        return false;
    }

    at++;

    while (at < json.size())
    {
        if (json[at] == '\\')
        {
            at += 2; // an escape and whatever it escapes, \uXXXX included: the X are not quotes
            continue;
        }

        if (json[at] == '"')
        {
            at++;
            return true;
        }

        at++;
    }

    return false;
}

//!< Steps over any value, however deeply nested
bool skipValue(const std::string& json, size_t& at)
{
    skipSpace(json, at);

    if (at >= json.size()) {
        return false;
    }

    if (json[at] == '"') {
        return skipString(json, at);
    }

    if ((json[at] == '{') || (json[at] == '['))
    {
        int depth = 0;

        while (at < json.size())
        {
            char c = json[at];

            if (c == '"')
            {
                if (!skipString(json, at)) {
                    return false;
                }

                continue;
            }

            if ((c == '{') || (c == '[')) {
                depth++;
            } else if ((c == '}') || (c == ']')) {
                depth--;
            }

            at++;

            if (depth == 0) {
                return true;
            }
        }

        return false;
    }

    // A number, true, false or null: runs until the value separator
    size_t start = at;

    while ((at < json.size()) && (json[at] != ',') && (json[at] != '}') && (json[at] != ']')
        && (json[at] != ' ') && (json[at] != '\t') && (json[at] != '\r') && (json[at] != '\n')) {
        at++;
    }

    return at > start;
}

} // namespace

std::string jsonMember(const std::string& json, const std::string& key)
{
    size_t at = 0;
    skipSpace(json, at);

    if ((at >= json.size()) || (json[at] != '{')) {
        return std::string();
    }

    at++;

    for (;;)
    {
        skipSpace(json, at);

        if ((at >= json.size()) || (json[at] == '}')) {
            return std::string();
        }

        size_t nameStart = at;

        if (!skipString(json, at)) {
            return std::string();
        }

        // Compared with the quotes on, so a key needing escapes simply does not match
        std::string name = json.substr(nameStart + 1, at - nameStart - 2);

        skipSpace(json, at);

        if ((at >= json.size()) || (json[at] != ':')) {
            return std::string();
        }

        at++;
        skipSpace(json, at);
        size_t valueStart = at;

        if (!skipValue(json, at)) {
            return std::string();
        }

        if (name == key) {
            return json.substr(valueStart, at - valueStart);
        }

        skipSpace(json, at);

        if ((at >= json.size()) || (json[at] != ',')) {
            return std::string();
        }

        at++;
    }
}

std::string jsonStringMember(const std::string& json, const std::string& key)
{
    std::string raw = jsonMember(json, key);

    if ((raw.size() < 2) || (raw.front() != '"') || (raw.back() != '"')) {
        return std::string();
    }

    std::string text;

    for (size_t at = 1; at + 1 < raw.size(); at++)
    {
        if ((raw[at] == '\\') && (at + 2 < raw.size()))
        {
            char escaped = raw[++at];

            switch (escaped)
            {
            case 'n': text += '\n'; break;
            case 't': text += '\t'; break;
            case 'r': text += '\r'; break;
            case 'b': text += '\b'; break;
            case 'f': text += '\f'; break;
            default:  text += escaped; break; // \\ \" \/ and, crudely, \u
            }

            continue;
        }

        text += raw[at];
    }

    return text;
}
