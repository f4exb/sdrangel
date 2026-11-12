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

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "dvangel61aprs.h"

const char *DVAngel61APRS::m_tocall = "APZA61";

std::string DVAngel61APRS::base91(int value, int digits)
{
    std::string text(digits, '!');

    for (int i = digits - 1; i >= 0; i--)
    {
        text[i] = (char) ('!' + value % 91);
        value /= 91;
    }

    return text;
}

bool DVAngel61APRS::isAX25Address(const std::string& callsign)
{
    size_t dash = callsign.find('-');
    std::string base = callsign.substr(0, dash);

    if (base.empty() || (base.size() > 6) || !std::all_of(base.begin(), base.end(), [](char c) { return isupper((unsigned char) c) || isdigit((unsigned char) c); })) {
        return false;
    }

    if (dash == std::string::npos) {
        return true;
    }

    std::string ssid = callsign.substr(dash + 1);
    return !ssid.empty() && (ssid.size() <= 2) && std::all_of(ssid.begin(), ssid.end(), [](char c) { return isdigit((unsigned char) c); })
        && (atoi(ssid.c_str()) <= 15);
}

std::string DVAngel61APRS::tnc2(uint64_t source, const DVAngel61::Path& path, bool hasPath, const std::string& info)
{
    // Nothing before the source's long callsign (2.6) has been heard: its identity is not known
    if ((source == DVAngel61::CallsignNone) || info.empty() || !DVAngel61::callsignKnown(source)) {
        return std::string();
    }

    // APRS addresses have no "/": the stations' identities, base callsign and SSID (2.6)
    std::string identity = DVAngel61::stationIdentity(DVAngel61::decodeCallsign(source));

    if (!isAX25Address(identity)) {
        return std::string();
    }

    std::string line = identity + ">" + m_tocall;

    // The repeaters the transmission came through, each marked as used
    for (uint64_t repeater : { hasPath ? path.m_repeater1 : 0, hasPath && path.m_repeater1 ? path.m_repeater2 : 0 })
    {
        std::string via = repeater ? DVAngel61::stationIdentity(DVAngel61::decodeCallsign(repeater)) : std::string();

        if (DVAngel61::callsignKnown(repeater) && isAX25Address(via)) {
            line += "," + via + "*";
        }
    }

    return line + ":" + info;
}

void DVAngel61APRS::defaultSymbol(const DVAngel61::Position& position, char& table, char& code)
{
    table = '/';

    if (position.m_source == 1) {
        code = '-';         // set by hand: a house
    } else if ((position.m_speed != DVAngel61::Position::SpeedUnknown) && (position.m_speed > 0)) {
        code = '>';         // a GNSS fix that is moving: a car
    } else {
        code = '[';         // a person
    }
}

std::string DVAngel61APRS::compressedPosition(const DVAngel61::Position& position, char table, char code)
{
    // APRS 1.01 chapter 9: the symbol table, four Base91 digits of latitude, four of longitude,
    // the symbol code, the course and speed, and the compression type (a software originated
    // packet with a current fix)
    double latitude = std::min(90.0, std::max(-90.0, position.m_latitude));
    double longitude = std::min(180.0, std::max(-180.0, position.m_longitude));
    int latitudeValue = (int) std::lround(380926.0 * (90.0 - latitude));
    int longitudeValue = (int) std::lround(190463.0 * (180.0 + longitude));
    std::string text;
    text += table;
    text += base91(latitudeValue, 4);
    text += base91(longitudeValue, 4);
    text += code;

    if ((position.m_course != DVAngel61::Position::CourseUnknown)
        && (position.m_speed != DVAngel61::Position::SpeedUnknown))
    {
        // A course of 0 degrees is sent as 360, since 0 means "unknown" in this field; the speed
        // is knots here and km/h in the slot
        int course = position.m_course % 360;
        double knots = position.m_speed / 1.852;
        int c = (course == 0) ? 90 : course / 4;
        int sp = (knots <= 0.0) ? 0 : (int) std::lround(std::log(knots + 1.0) / std::log(1.08));
        text += (char) ('!' + std::min(90, std::max(0, c)));
        text += (char) ('!' + std::min(90, std::max(0, sp)));
        text += (char) ('!' + 0x22);  // software originated, a current fix
    }
    else
    {
        text += " sT";                // no course or speed in this packet
    }

    return text;
}

std::string DVAngel61APRS::position(const DVAngel61::Position& position, char symbolTable, char symbolCode, const std::string& comment)
{
    if (position.m_fixAge == DVAngel61::Position::NoFix) {
        return std::string();
    }

    char table = symbolTable;
    char code = symbolCode;

    if ((table == 0) || (code == 0)) {
        defaultSymbol(position, table, code);
    }

    std::string info = "=" + compressedPosition(position, table, code);

    if (position.m_altitude != DVAngel61::Position::AltitudeUnknown)
    {
        // Feet, six digits, as the comment's altitude is defined
        int feet = (int) std::lround(position.m_altitude / 0.3048);
        feet = std::min(999999, std::max(0, feet));
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "/A=%06d", feet);
        info += buffer;
    }

    if (!comment.empty()) {
        info += comment;
    }

    return info;
}

std::string DVAngel61APRS::message(const std::string& addressee, const std::string& text)
{
    if (addressee.empty() || text.empty()) {
        return std::string();
    }

    std::string to = addressee.substr(0, 9);
    to.resize(9, ' ');
    return ":" + to + ":" + text.substr(0, 67);
}

std::string DVAngel61APRS::status(const std::string& text)
{
    return text.empty() ? std::string() : ">" + text.substr(0, 62);
}
