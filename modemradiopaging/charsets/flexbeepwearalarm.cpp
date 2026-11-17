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

// Timex BeepwearPRO alarm command: a FLEX alphanumeric page of ASCII characters that the watch takes as a command,
// not as a message to show.
//
//   /// A 1 <arm> <hour> <minute> <month> <day> <text> <checksum>
//
// "///" marks a command, "A" is the alarm command, "1" is fixed. arm is '1' (arm) or '0' (disarm). Hour (0-23),
// minute (0-59), month (0-12) and day (0-31) are a byte each, as two characters of 4 bits, the high one first, each
// the 4 bits + 0x30 ('0' to '?'). The text is 1 to 16 uppercase characters. The checksum is the sum of the
// characters from "A" to the end of the text, modulo 256, as two characters of 4 bits. Month 0 and day 0 is a daily
// alarm, month 0 and a day a monthly one, a month and a day a yearly one.
//
// The user writes and reads the command as key=value: arm=1 hour=7 min=0 month=0 day=0 msg=WAKE UP

#include <cctype>
#include <cstdio>
#include <set>

#include "radiopagingcharsettables.h"

namespace {

const int maxText = 16;
const size_t headerChars = 3 + 11;      // "///", then A 1 arm and the 4 bytes
const size_t checksumChars = 2;

std::string nibbles(unsigned value)
{
    std::string s;
    s.push_back((char) (0x30 + ((value >> 4) & 0x0f)));
    s.push_back((char) (0x30 + (value & 0x0f)));
    return s;
}

// The byte of the two characters of 4 bits at pos, -1 if they are not
int fromNibbles(const std::string& text, size_t pos)
{
    if (pos + 2 > text.size()) {
        return -1;
    }

    unsigned char hi = (unsigned char) text[pos];
    unsigned char lo = (unsigned char) text[pos + 1];

    if ((hi < 0x30) || (hi > 0x3f) || (lo < 0x30) || (lo > 0x3f)) {
        return -1;
    }

    return ((hi - 0x30) << 4) | (lo - 0x30);
}

unsigned checksum(const std::string& body)
{
    unsigned sum = 0;

    for (char c : body) {
        sum += (unsigned char) c;
    }

    return sum & 0xff;
}

bool number(const std::string& value, int& n)
{
    if (value.empty() || (value.size() > 3)) {
        return false;
    }

    n = 0;

    for (char c : value)
    {
        if ((c < '0') || (c > '9')) {
            return false;
        }

        n = n * 10 + (c - '0');
    }

    return true;
}

bool encode(const std::string& text, std::string& out, std::string& error, std::string& warning)
{
    int arm = 1;
    int hour = -1;
    int minute = -1;
    int month = 0;
    int day = 0;
    std::string msg;
    bool hasMsg = false;
    std::set<std::string> seen;
    size_t pos = 0;

    while (pos < text.size())
    {
        if (text[pos] == ' ')
        {
            pos++;
            continue;
        }

        size_t end = text.find(' ', pos);
        size_t eq = text.find('=', pos);

        if ((eq == std::string::npos) || ((end != std::string::npos) && (eq > end)))
        {
            error = "BeepwearPRO alarm: expected key=value, got '" + text.substr(pos, (end == std::string::npos) ? std::string::npos : end - pos) + "'";
            return false;
        }

        std::string key = text.substr(pos, eq - pos);

        for (char& c : key) {
            c = (char) std::tolower((unsigned char) c);
        }

        if (seen.count(key) > 0)
        {
            error = "BeepwearPRO alarm: " + key + " given twice";
            return false;
        }

        seen.insert(key);

        // The text is the rest
        if (key == "msg")
        {
            msg = text.substr(eq + 1);
            hasMsg = true;
            break;
        }

        end = text.find(' ', eq);
        const std::string value = text.substr(eq + 1, (end == std::string::npos) ? std::string::npos : end - eq - 1);
        pos = (end == std::string::npos) ? text.size() : end;
        int n;
        int max;
        int *field;

        if (key == "arm") {
            field = &arm, max = 1;
        } else if (key == "hour") {
            field = &hour, max = 23;
        } else if (key == "min") {
            field = &minute, max = 59;
        } else if (key == "month") {
            field = &month, max = 12;
        } else if (key == "day") {
            field = &day, max = 31;
        }
        else
        {
            error = "BeepwearPRO alarm: unknown key " + key + " (arm, hour, min, month, day, msg)";
            return false;
        }

        if (!number(value, n) || (n > max))
        {
            error = "BeepwearPRO alarm: " + key + " must be 0 to " + std::to_string(max);
            return false;
        }

        *field = n;
    }

    if ((hour < 0) || (minute < 0) || !hasMsg)
    {
        error = "BeepwearPRO alarm: hour, min and msg are needed";
        return false;
    }
    if ((month > 0) && (day == 0))
    {
        error = "BeepwearPRO alarm: a yearly alarm (month 1-12) needs its day 1-31";
        return false;
    }

    for (char& c : msg)
    {
        unsigned char u = (unsigned char) c;

        if ((u < 0x20) || (u > 0x7e))
        {
            error = "BeepwearPRO alarm: msg is ASCII only";
            return false;
        }

        c = (char) std::toupper(u);
    }

    if (msg.empty())
    {
        error = "BeepwearPRO alarm: msg is empty";
        return false;
    }
    if ((int) msg.size() > maxText)
    {
        warning = "BeepwearPRO alarm: msg longer than 16 characters, cut: '" + msg.substr(maxText) + "' dropped";
        msg.resize(maxText);
    }

    std::string body = std::string("A1") + (arm ? '1' : '0') + nibbles((unsigned) hour) + nibbles((unsigned) minute)
        + nibbles((unsigned) month) + nibbles((unsigned) day) + msg;
    out = "///" + body + nibbles(checksum(body));
    return true;
}

std::string decode(const std::string& text)
{
    // Not a command: a message the watch shows
    if (text.compare(0, 3, "///") != 0) {
        return text;
    }
    if (text.compare(3, 2, "A1") != 0) {
        return "error=unknown_command raw=" + text;
    }
    if (text.size() < headerChars + checksumChars) {
        return "error=format raw=" + text;
    }

    const std::string body = text.substr(3, text.size() - 3 - checksumChars);
    const char arm = text[5];
    const int hour = fromNibbles(text, 6);
    const int minute = fromNibbles(text, 8);
    const int month = fromNibbles(text, 10);
    const int day = fromNibbles(text, 12);
    const int check = fromNibbles(text, text.size() - checksumChars);

    if (((arm != '0') && (arm != '1')) || (hour < 0) || (minute < 0) || (month < 0) || (day < 0) || (check < 0)) {
        return "error=format raw=" + text;
    }
    if ((unsigned) check != checksum(body)) {
        return "error=checksum raw=" + text;
    }

    char fields[64];
    std::snprintf(fields, sizeof(fields), "arm=%c hour=%d min=%d month=%d day=%d msg=", arm, hour, minute, month, day);
    return std::string(fields) + text.substr(headerChars, text.size() - headerChars - checksumChars);
}

} // namespace

const RadioPagingCharset::Transcoder RadioPagingCharsetTables::beepwearProAlarm = {
    "BeepwearPRO alarm command as key=value, e.g. arm=1 hour=7 min=0 month=0 day=0 msg=WAKE UP. hour, min and msg are "
    "needed; msg comes last and takes the rest of the text, up to 16 characters (cut with a warning), sent uppercase. "
    "arm 1 or 0 (default 1); month 0 and day 0: daily, day 1-31: monthly, month 1-12 and its day: yearly. A message "
    "received is shown the same way; a bad checksum gives error=checksum, another command error=unknown_command",
    encode,
    decode
};
