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

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <unordered_map>

#include "dvangel61.h"
#include "dvbits.h"
#include "dvblockcode.h"
#include "dvcrc.h"
#include "dvdtmf.h"
#include "dvscrambler.h"

// ---- Pulse shaping (2.1) ---------------------------------------------------------------------

const float DVAngel61::RrcAlpha = 0.2f;

void DVAngel61::rrcTaps(int samplesPerSymbol, float *taps)
{
    const double beta = RrcAlpha;
    const int nbTaps = rrcTapCount(samplesPerSymbol);
    const int half = nbTaps / 2;
    double energy = 0.0;

    for (int i = 0; i < nbTaps; i++)
    {
        double t = (i - half) / (double) samplesPerSymbol;
        double h;

        if (std::fabs(t) < 1e-9) {
            h = 1.0 + beta * (4.0 / M_PI - 1.0);
        } else if (std::fabs(std::fabs(t) - 1.0 / (4.0 * beta)) < 1e-9) {
            h = beta / std::sqrt(2.0) * ((1.0 + 2.0 / M_PI) * std::sin(M_PI / (4.0 * beta)) + (1.0 - 2.0 / M_PI) * std::cos(M_PI / (4.0 * beta)));
        } else {
            double b = 4.0 * beta * t;
            h = (std::sin(M_PI * t * (1.0 - beta)) + b * std::cos(M_PI * t * (1.0 + beta))) / (M_PI * t * (1.0 - b * b));
        }

        taps[i] = (float) h;
        energy += h * h;
    }

    float scale = (float) (1.0 / std::sqrt(energy));

    for (int i = 0; i < nbTaps; i++) {
        taps[i] *= scale;
    }
}

// ---- Symbols ---------------------------------------------------------------------------------

std::complex<float> DVAngel61::qpskSymbol(int index)
{
    const float a = 0.70710678f;
    return std::complex<float>((index & 2) ? -a : a, (index & 1) ? -a : a);
}

// ---- Callsigns (2.6) -------------------------------------------------------------------------

static const char *base40Alphabet = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.";
static const uint64_t base40Max = 262144000000000ull; // 40^9
// A long callsign's text (2.6): a pad, then A-Z, 0-9 and "/"; its base callsign: a pad, A-Z, 0-9
static const char *base38Alphabet = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/";
static const uint64_t LongTypeHash = 1ull << 43;   // the code holds a hash of the whole, not the base
static const int LongBaseCharacters = 7;

// The long callsigns heard or encoded in this process, by code
static std::mutex longCallsignsMutex;
static std::unordered_map<uint64_t, std::string> longCallsigns;

static uint64_t fnv1a64(const std::string& text)
{
    uint64_t h = 0xcbf29ce484222325ull;

    for (unsigned char c : text)
    {
        h ^= c;
        h *= 0x100000001b3ull;
    }

    return h;
}

// Upper case, up to the first space
static std::string callsignText(const std::string& callsign)
{
    std::string cs;

    for (char ch : callsign)
    {
        if (ch == ' ') {
            break;
        }
        cs += (char) toupper((unsigned char) ch);
    }

    return cs;
}

// A long callsign's body (A-Z, 0-9 and "/", no empty part) and SSID, or false
static bool splitLongCallsign(const std::string& cs, std::string& body, int& ssid)
{
    body = cs;
    ssid = 0;
    size_t dash = cs.rfind('-');

    if (dash != std::string::npos)
    {
        std::string digits = cs.substr(dash + 1);

        if (digits.empty() || (digits.size() > 2) || !std::all_of(digits.begin(), digits.end(), [](char c) { return isdigit((unsigned char) c); })) {
            return false;
        }

        ssid = atoi(digits.c_str());

        if (ssid > 15) {
            return false;
        }

        body = cs.substr(0, dash);
    }

    if (body.empty() || ((int) body.size() > DVAngel61::LongCallsignCharacters) || (body.front() == '/') || (body.back() == '/')
        || (body.find("//") != std::string::npos)) {
        return false;
    }

    for (char c : body)
    {
        if (!isupper((unsigned char) c) && !isdigit((unsigned char) c) && (c != '/')) {
            return false;
        }
    }

    return true;
}

// The canonical text of a long callsign: the body, and the SSID unless it is 0
static std::string longCallsignText(const std::string& body, int ssid)
{
    return ssid ? body + "-" + std::to_string(ssid) : body;
}

// The base callsign among the parts of "PREFIX/BASE/SUFFIX": the middle one of three when it
// looks like a callsign (a digit before its last character, which is a letter), else the longest
// part that does, else the longest part
static std::string baseOfBody(const std::string& body)
{
    std::vector<std::string> parts;
    size_t start = 0;

    while (true)
    {
        size_t slash = body.find('/', start);
        parts.push_back(body.substr(start, slash == std::string::npos ? std::string::npos : slash - start));

        if (slash == std::string::npos) {
            break;
        }

        start = slash + 1;
    }

    auto looksLikeCallsign = [](const std::string& p) {
        if ((p.size() < 3) || !isupper((unsigned char) p.back())) {
            return false;
        }
        return std::any_of(p.begin(), p.end() - 1, [](char c) { return isdigit((unsigned char) c); });
    };

    if ((parts.size() == 3) && looksLikeCallsign(parts[1])) {
        return parts[1];
    }

    int best = -1;

    for (int i = 0; i < (int) parts.size(); i++)
    {
        if (looksLikeCallsign(parts[i]) && ((best < 0) || (parts[i].size() > parts[best].size()))) {
            best = i;
        }
    }

    if (best < 0)
    {
        best = 0;

        for (int i = 1; i < (int) parts.size(); i++)
        {
            if (parts[i].size() > parts[best].size()) {
                best = i;
            }
        }
    }

    return parts[best];
}

bool DVAngel61::parseTalkgroup(const std::string& text, uint32_t& group)
{
    // "TG", at least one space, then the number: the space keeps it apart from a callsign ("TG9")
    size_t i = 0;

    while ((i < text.size()) && (text[i] == ' ')) {
        i++;
    }

    if ((i + 2 >= text.size()) || (toupper((unsigned char) text[i]) != 'T') || (toupper((unsigned char) text[i + 1]) != 'G') || (text[i + 2] != ' ')) {
        return false;
    }

    for (i += 2; (i < text.size()) && (text[i] == ' '); i++) {
    }

    uint64_t value = 0;
    size_t digits = 0;

    for (; (i < text.size()) && isdigit((unsigned char) text[i]); i++, digits++)
    {
        value = value * 10 + (uint64_t) (text[i] - '0');

        if (value > 0xFFFFFFFFull) {
            return false;
        }
    }

    while ((i < text.size()) && (text[i] == ' ')) {
        i++;
    }

    if ((digits == 0) || (i != text.size())) {
        return false;
    }

    group = (uint32_t) value;
    return true;
}

std::vector<uint32_t> DVAngel61::parseTalkgroupList(const std::string& text)
{
    std::vector<uint32_t> groups;
    size_t start = 0;

    while (start <= text.size())
    {
        size_t comma = text.find(',', start);
        std::string item = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        size_t first = item.find_first_not_of(' ');
        uint32_t group;

        if (first != std::string::npos)
        {
            item = item.substr(first);
            bool ok = parseTalkgroup(item, group);

            if (!ok && isdigit((unsigned char) item[0])) {
                ok = parseTalkgroup("TG " + item, group);
            }
            if (ok && (std::find(groups.begin(), groups.end(), group) == groups.end())) {
                groups.push_back(group);
            }
        }

        if (comma == std::string::npos) {
            break;
        }

        start = comma + 1;
    }

    return groups;
}

bool DVAngel61::talkgroupPlayed(uint64_t destination, const std::vector<uint32_t>& groups)
{
    return groups.empty() || !isTalkgroup(destination)
        || (std::find(groups.begin(), groups.end(), talkgroupOf(destination)) != groups.end());
}

uint64_t DVAngel61::encodeCallsign(const std::string& callsign)
{
    uint32_t group;

    if (parseTalkgroup(callsign, group)) {
        return talkgroupCode(group);
    }

    // "TG " and something that is not a group number is a mistyped group, not the callsign "TG"
    size_t first = callsign.find_first_not_of(' ');

    if ((first != std::string::npos) && (callsign.size() > first + 2)
        && (toupper((unsigned char) callsign[first]) == 'T') && (toupper((unsigned char) callsign[first + 1]) == 'G')
        && (callsign[first + 2] == ' ')) {
        return CallsignNone;
    }

    std::string cs = callsignText(callsign);

    if (cs.empty()) {
        return CallsignNone;
    }
    if ((cs == "CQ") || (cs == "@ALL")) {
        return CallsignBroadcast;
    }

    // Up to 9 characters of base-40: the characters themselves, as M17
    if (cs.size() <= 9)
    {
        uint64_t value = 0;
        bool ok = true;

        for (int i = (int) cs.size() - 1; (i >= 0) && ok; i--)
        {
            const char *p = strchr(base40Alphabet, cs[i]);
            ok = p && (*p != ' ') && (*p != '\0');
            value = value * 40 + (ok ? (uint64_t) (p - base40Alphabet) : 0);
        }

        if (ok) {
            return value;
        }
    }

    // Longer: a long code, the text for the long callsign slot
    std::string body;
    int ssid;

    if (!splitLongCallsign(cs, body, ssid)) {
        return CallsignNone;
    }

    std::string text = longCallsignText(body, ssid);
    uint64_t hash = fnv1a64(text);
    std::string base = baseOfBody(body);
    uint64_t code;

    if ((base.size() <= (size_t) LongBaseCharacters) && std::all_of(base.begin(), base.end(), [](char c) { return c != '/'; }))
    {
        uint64_t value = 0;

        for (int i = (int) base.size() - 1; i >= 0; i--) {
            value = value * 37 + (uint64_t) (strchr(base38Alphabet, base[i]) - base38Alphabet);
        }

        code = CallsignLongFirst + (value << 6) + (hash & 0x3F);
    }
    else
    {
        code = CallsignLongFirst + LongTypeHash + (hash & (LongTypeHash - 1));
    }

    std::lock_guard<std::mutex> lock(longCallsignsMutex);

    if (longCallsigns.size() >= 4096) {
        longCallsigns.clear(); // heard again within 2 s if still in use
    }

    longCallsigns[code] = text;
    return code;
}

bool DVAngel61::callsignKnown(uint64_t code)
{
    if (!isLongCallsign(code)) {
        return (code == CallsignNone) || (code == CallsignBroadcast) || (code < base40Max) || isTalkgroup(code);
    }

    std::lock_guard<std::mutex> lock(longCallsignsMutex);
    return longCallsigns.find(code) != longCallsigns.end();
}

std::string DVAngel61::decodeCallsign(uint64_t code)
{
    if (code == CallsignNone) {
        return "";
    }
    if (code == CallsignBroadcast) {
        return "CQ";
    }
    if (isTalkgroup(code)) {
        return talkgroupText(talkgroupOf(code));
    }

    if (isLongCallsign(code))
    {
        {
            std::lock_guard<std::mutex> lock(longCallsignsMutex);
            auto it = longCallsigns.find(code);

            if (it != longCallsigns.end()) {
                return it->second;
            }
        }

        uint64_t value = code - CallsignLongFirst;

        if (value & LongTypeHash)
        {
            char hex[16];
            snprintf(hex, sizeof(hex), "#%011llx", (unsigned long long) (value & (LongTypeHash - 1)));
            return hex;
        }

        std::string base;

        for (value >>= 6; value > 0; value /= 37) {
            base += base38Alphabet[value % 37];
        }

        return base + "...";
    }

    if (code >= base40Max) {
        return "?";
    }

    std::string cs;

    while (code > 0)
    {
        cs += base40Alphabet[code % 40];
        code /= 40;
    }

    return cs;
}

std::string DVAngel61::stationIdentity(const std::string& callsign)
{
    uint32_t group;

    if (parseTalkgroup(callsign, group)) {
        return talkgroupText(group); // a group is no station: it matches only itself
    }

    std::string cs = callsignText(callsign);

    if (cs.size() > 3 && (cs.compare(cs.size() - 3, 3, "...") == 0)) {
        return cs.substr(0, cs.size() - 3); // a long code not heard yet: its base, the SSID unknown
    }

    std::string body = cs, suffix;
    size_t dash = cs.rfind('-');

    if ((dash != std::string::npos) && (dash + 1 < cs.size()) && (cs.size() - dash - 1 <= 2)
        && std::all_of(cs.begin() + dash + 1, cs.end(), [](char c) { return isdigit((unsigned char) c); }))
    {
        body = cs.substr(0, dash);
        int ssid = atoi(cs.c_str() + dash + 1);
        suffix = ssid ? "-" + std::to_string(ssid) : "";
    }

    return body.find('/') == std::string::npos ? body + suffix : baseOfBody(body) + suffix;
}

bool DVAngel61::sameStation(const std::string& a, const std::string& b)
{
    std::string ia = stationIdentity(a), ib = stationIdentity(b);
    return !ia.empty() && (ia == ib);
}

// ---- Slots (2.5) -----------------------------------------------------------------------------

void DVAngel61::packIdleSlot(uint8_t *slot)
{
    memset(slot, 0, SlotBits);
}

void DVAngel61::packRoutingSlot(const Routing& routing, uint8_t *slot)
{
    int pos = DVBits::putBits(slot, 0, SlotRouting, 4);
    pos = DVBits::putBits(slot, pos, routing.m_source, 48);
    DVBits::putBits(slot, pos, routing.m_destination, 48);
}

void DVAngel61::packPathSlot(const Path& path, uint8_t *slot)
{
    int pos = DVBits::putBits(slot, 0, SlotPath, 4);
    pos = DVBits::putBits(slot, pos, path.m_repeater1, 48);
    DVBits::putBits(slot, pos, path.m_repeater2, 48);
}

void DVAngel61::packStationSlot(const Station& station, uint8_t *slot)
{
    int pos = DVBits::putBits(slot, 0, SlotStation, 4);
    pos = DVBits::putBits(slot, pos, station.m_protocolVersion, 4);
    pos = DVBits::putBits(slot, pos, station.m_codecFamily, 4);
    pos = DVBits::putBits(slot, pos, station.m_codecModel, 8);
    pos = DVBits::putBits(slot, pos, station.m_nextTier, 2);
    pos = DVBits::putBits(slot, pos, station.m_powerClass, 3);
    pos = DVBits::putBits(slot, pos, station.m_antennaClass, 5);
    pos = DVBits::putBits(slot, pos, station.m_locator, 48);
    // The APRS symbol (3.3): the table character and the code, 0 when the station does not say
    pos = DVBits::putBits(slot, pos, (uint8_t) station.m_symbolTable, 8);
    pos = DVBits::putBits(slot, pos, (uint8_t) station.m_symbolCode, 8);
    pos = DVBits::putBits(slot, pos, station.m_tunnelOffered ? 1 : 0, 1);
    DVBits::putBits(slot, pos, 0, 5);
}

uint32_t DVAngel61::encodeLatitude(double latitude)
{
    double u = std::round((std::min(90.0, std::max(-90.0, latitude)) + 90.0) / 180.0 * 16777215.0);
    return (uint32_t) u;
}

uint32_t DVAngel61::encodeLongitude(double longitude)
{
    double u = std::round((std::min(180.0, std::max(-180.0, longitude)) + 180.0) / 360.0 * 33554431.0);
    return (uint32_t) u;
}

double DVAngel61::decodeLatitude(uint32_t u)
{
    return u * 180.0 / 16777215.0 - 90.0;
}

double DVAngel61::decodeLongitude(uint32_t u)
{
    return u * 360.0 / 33554431.0 - 180.0;
}

void DVAngel61::packPositionSlot(const Position& position, uint8_t *slot)
{
    int pos = DVBits::putBits(slot, 0, SlotPosition, 4);
    pos = DVBits::putBits(slot, pos, encodeLatitude(position.m_latitude), 24);
    pos = DVBits::putBits(slot, pos, encodeLongitude(position.m_longitude), 25);
    int altitude = position.m_altitude == Position::AltitudeUnknown ? 0xFFFF
        : std::min(0xFFFE, std::max(0, position.m_altitude + 1000));
    pos = DVBits::putBits(slot, pos, altitude, 16);
    int speed = position.m_speed < 0 ? 0x3FF : std::min(1022, position.m_speed);
    pos = DVBits::putBits(slot, pos, speed, 10);
    int course = position.m_course < 0 ? 0x1FF : std::min(359, position.m_course);
    pos = DVBits::putBits(slot, pos, course, 9);
    int age = position.m_fixAge < 0 ? Position::NoFix : std::min((int) Position::NoFix, position.m_fixAge);
    pos = DVBits::putBits(slot, pos, age, 7);
    pos = DVBits::putBits(slot, pos, position.m_source & 3, 2);
    DVBits::putBits(slot, pos, 0, 3);
}

void DVAngel61::packTextSlot(const TextFragment& fragment, uint8_t *slot)
{
    int pos = DVBits::putBits(slot, 0, SlotText, 4);
    pos = DVBits::putBits(slot, pos, fragment.m_id & 0xF, 4);
    pos = DVBits::putBits(slot, pos, fragment.m_index & 0xF, 4);
    pos = DVBits::putBits(slot, pos, fragment.m_total & 0xF, 4);

    for (int i = 0; i < TextFragment::Characters; i++)
    {
        int ch = i < (int) fragment.m_text.size() ? (unsigned char) fragment.m_text[i] : 0;
        pos = DVBits::putBits(slot, pos, ch < 128 ? ch : '?', 7);
    }
}

void DVAngel61::packAckSlot(const Ack& ack, uint8_t *slot)
{
    int pos = DVBits::putBits(slot, 0, SlotAck, 4);
    pos = DVBits::putBits(slot, pos, ack.m_messageId & 0xFF, 8);
    pos = DVBits::putBits(slot, pos, ack.m_receiver, 48);
    pos = DVBits::putBits(slot, pos, ack.m_blockErrorRate & 0xFF, 8);
    DVBits::putBits(slot, pos, 0, 32);
}

bool DVAngel61::packDTMFSlot(const DTMF& dtmf, uint8_t *slot)
{
    const int count = (int) dtmf.m_keys.size();

    if ((count < 1) || (count > DTMF::MaxKeys)) {
        return false;
    }

    int values[DTMF::MaxKeys];

    for (int i = 0; i < count; i++)
    {
        values[i] = DVDTMF::keyValue(dtmf.m_keys[i]);
        if (values[i] < 0) {
            return false;
        }
    }

    int pos = DVBits::putBits(slot, 0, SlotDTMF, 4);
    pos = DVBits::putBits(slot, pos, dtmf.m_sequence & 0xF, 4);
    pos = DVBits::putBits(slot, pos, dtmf.m_first & 0x3F, 6);
    pos = DVBits::putBits(slot, pos, count, 5);
    pos = DVBits::putBits(slot, pos, dtmfUnits(dtmf.m_toneMs), 5);
    pos = DVBits::putBits(slot, pos, dtmfUnits(dtmf.m_gapMs), 5);

    for (int i = 0; i < DTMF::MaxKeys; i++) {
        pos = DVBits::putBits(slot, pos, i < count ? values[i] : 0, 4);
    }

    DVBits::putBits(slot, pos, 0, 3);
    return true;
}

bool DVAngel61::packLongCallsignSlot(const std::string& callsign, uint8_t *slot)
{
    std::string body;
    int ssid;

    if (!splitLongCallsign(callsignText(callsign), body, ssid)) {
        return false;
    }

    // SSID (4), the body as a base-38 number of 17 digits, first character least significant, in
    // 90 bits (three 32 bit limbs), 2 reserved
    uint32_t limbs[3] = { 0, 0, 0 };

    for (int i = LongCallsignCharacters - 1; i >= 0; i--)
    {
        uint64_t carry = i < (int) body.size() ? (uint64_t) (strchr(base38Alphabet, body[i]) - base38Alphabet) : 0;

        for (int l = 0; l < 3; l++)
        {
            uint64_t v = (uint64_t) limbs[l] * 38 + carry;
            limbs[l] = (uint32_t) v;
            carry = v >> 32;
        }
    }

    uint64_t low = ((uint64_t) limbs[1] << 32) | limbs[0];
    uint64_t high = limbs[2];   // 26 bits
    int pos = DVBits::putBits(slot, 0, SlotLongCallsign, 4);
    pos = DVBits::putBits(slot, pos, (uint64_t) ssid, 4);
    pos = DVBits::putBits(slot, pos, high, 26);
    pos = DVBits::putBits(slot, pos, low, 64);
    DVBits::putBits(slot, pos, 0, 2);
    return true;
}

bool DVAngel61::unpackLongCallsignSlot(const uint8_t *slot, std::string& callsign)
{
    if (slotID(slot) != SlotLongCallsign) {
        return false;
    }

    int ssid = (int) DVBits::getBits(slot, 4, 4);
    uint64_t high = DVBits::getBits(slot, 8, 26);
    uint64_t low = DVBits::getBits(slot, 34, 64);
    uint32_t limbs[3] = { (uint32_t) low, (uint32_t) (low >> 32), (uint32_t) high };
    std::string body;

    for (int i = 0; i < LongCallsignCharacters; i++)
    {
        // Divide by 38, most significant limb first
        uint64_t remainder = 0;

        for (int l = 2; l >= 0; l--)
        {
            uint64_t v = (remainder << 32) | limbs[l];
            limbs[l] = (uint32_t) (v / 38);
            remainder = v % 38;
        }

        if (remainder) {
            body += base38Alphabet[remainder];
        }
    }

    std::string cs = ssid ? body + "-" + std::to_string(ssid) : body;
    std::string check;
    int checkSsid;

    if (!splitLongCallsign(cs, check, checkSsid)) {
        return false;
    }

    callsign = cs;
    return true;
}

bool DVAngel61::unpackRoutingSlot(const uint8_t *slot, Routing& routing)
{
    if (slotID(slot) != SlotRouting) {
        return false;
    }
    routing.m_source = DVBits::getBits(slot, 4, 48);
    routing.m_destination = DVBits::getBits(slot, 52, 48);
    return true;
}

bool DVAngel61::unpackPathSlot(const uint8_t *slot, Path& path)
{
    if (slotID(slot) != SlotPath) {
        return false;
    }
    path.m_repeater1 = DVBits::getBits(slot, 4, 48);
    path.m_repeater2 = DVBits::getBits(slot, 52, 48);
    return true;
}

bool DVAngel61::unpackStationSlot(const uint8_t *slot, Station& station)
{
    if (slotID(slot) != SlotStation) {
        return false;
    }
    // 2.5: ID 4, version 4, codec family 4, codec model 8, tier 2, power 3, antenna 5,
    // locator 48, APRS symbol 16, tunnel offered 1, reserved 5
    station.m_protocolVersion = (int) DVBits::getBits(slot, 4, 4);
    station.m_codecFamily = (int) DVBits::getBits(slot, 8, 4);
    station.m_codecModel = (int) DVBits::getBits(slot, 12, 8);
    station.m_nextTier = (int) DVBits::getBits(slot, 20, 2);
    station.m_powerClass = (int) DVBits::getBits(slot, 22, 3);
    station.m_antennaClass = (int) DVBits::getBits(slot, 25, 5);
    station.m_locator = DVBits::getBits(slot, 30, 48);
    station.m_symbolTable = (char) DVBits::getBits(slot, 78, 8);
    station.m_symbolCode = (char) DVBits::getBits(slot, 86, 8);
    station.m_tunnelOffered = DVBits::getBits(slot, 94, 1) != 0;
    return true;
}

bool DVAngel61::unpackPositionSlot(const uint8_t *slot, Position& position)
{
    if (slotID(slot) != SlotPosition) {
        return false;
    }
    position.m_latitude = decodeLatitude((uint32_t) DVBits::getBits(slot, 4, 24));
    position.m_longitude = decodeLongitude((uint32_t) DVBits::getBits(slot, 28, 25));
    int altitude = (int) DVBits::getBits(slot, 53, 16);
    position.m_altitude = altitude == 0xFFFF ? Position::AltitudeUnknown : altitude - 1000;
    int speed = (int) DVBits::getBits(slot, 69, 10);
    position.m_speed = speed == 0x3FF ? Position::SpeedUnknown : speed;
    int course = (int) DVBits::getBits(slot, 79, 9);
    position.m_course = course > 359 ? Position::CourseUnknown : course;
    position.m_fixAge = (int) DVBits::getBits(slot, 88, 7);
    position.m_source = (int) DVBits::getBits(slot, 95, 2);
    return true;
}

bool DVAngel61::unpackTextSlot(const uint8_t *slot, TextFragment& fragment)
{
    if (slotID(slot) != SlotText) {
        return false;
    }
    fragment.m_id = (int) DVBits::getBits(slot, 4, 4);
    fragment.m_index = (int) DVBits::getBits(slot, 8, 4);
    fragment.m_total = (int) DVBits::getBits(slot, 12, 4);
    fragment.m_text.clear();

    for (int i = 0; i < TextFragment::Characters; i++)
    {
        int ch = (int) DVBits::getBits(slot, 16 + 7 * i, 7);
        if (ch == 0) {
            break;
        }
        fragment.m_text += (char) ch;
    }

    return (fragment.m_total >= 1) && (fragment.m_index < fragment.m_total);
}

bool DVAngel61::unpackDTMFSlot(const uint8_t *slot, DTMF& dtmf)
{
    if (slotID(slot) != SlotDTMF) {
        return false;
    }

    const int count = (int) DVBits::getBits(slot, 14, 5);
    const int tone = (int) DVBits::getBits(slot, 19, 5);
    const int gap = (int) DVBits::getBits(slot, 24, 5);

    if ((count < 1) || (count > DTMF::MaxKeys) || (tone == 0) || (gap == 0)) {
        return false;
    }

    dtmf.m_sequence = (int) DVBits::getBits(slot, 4, 4);
    dtmf.m_first = (int) DVBits::getBits(slot, 8, 6);
    dtmf.m_toneMs = tone * DTMF::UnitMs;
    dtmf.m_gapMs = gap * DTMF::UnitMs;
    dtmf.m_keys.clear();

    for (int i = 0; i < count; i++) {
        dtmf.m_keys += DVDTMF::keyChar((int) DVBits::getBits(slot, 29 + 4 * i, 4));
    }

    return true;
}

bool DVAngel61::unpackAckSlot(const uint8_t *slot, Ack& ack)
{
    if (slotID(slot) != SlotAck) {
        return false;
    }
    ack.m_messageId = (int) DVBits::getBits(slot, 4, 8);
    ack.m_receiver = DVBits::getBits(slot, 12, 48);
    ack.m_blockErrorRate = (int) DVBits::getBits(slot, 60, 8);
    return true;
}

// ---- Text fragments (2.5) -------------------------------------------------------------------

int DVAngel61::fragmentText(const std::string& text, int id, std::vector<TextFragment>& fragments)
{
    fragments.clear();
    int total = ((int) text.size() + TextFragment::Characters - 1) / TextFragment::Characters;

    if ((total == 0) || (total > 15)) {
        return 0;
    }

    for (int i = 0; i < total; i++)
    {
        TextFragment f;
        f.m_id = id & 0xF;
        f.m_index = i;
        f.m_total = total;
        f.m_text = text.substr(i * TextFragment::Characters, TextFragment::Characters);
        for (char& ch : f.m_text)
        {
            if ((unsigned char) ch >= 128) {
                ch = '?';
            }
        }
        fragments.push_back(f);
    }

    return total;
}

// ---- Reassembly (3.2) ------------------------------------------------------------------------

bool DVAngel61Reassembler::addTextFragment(uint64_t source, const DVAngel61::TextFragment& fragment, int64_t timeMs, Message& completed)
{
    expire(timeMs);
    std::pair<uint64_t, int> key(source, fragment.m_id);
    std::vector<uint8_t> payload(fragment.m_text.begin(), fragment.m_text.end());
    auto it = m_text.find(key);

    if ((it != m_text.end()) && (it->second.m_total != fragment.m_total))
    {
        // A new message under this id: start again
        m_text.erase(it);
        it = m_text.end();
    }

    if (it == m_text.end())
    {
        Partial p;
        p.m_total = fragment.m_total;
        it = m_text.insert(std::make_pair(key, p)).first;
    }

    Partial& partial = it->second;
    partial.m_lastTimeMs = timeMs;
    auto existing = partial.m_fragments.find(fragment.m_index);

    if (existing != partial.m_fragments.end())
    {
        if (existing->second != payload)
        {
            // Different content under a used id: a new message (the id was reused after
            // completion) or a corruption (discarded either way). Whether it was complete has
            // to be read before the erase: partial is a reference into what is being destroyed
            const bool wasComplete = partial.m_complete;
            m_text.erase(it);

            if (wasComplete)
            {
                Partial p;
                p.m_total = fragment.m_total;
                p.m_fragments[fragment.m_index] = payload;
                p.m_lastTimeMs = timeMs;
                m_text.insert(std::make_pair(key, p));
            }
        }
        return false;
    }

    if (partial.m_complete) {
        return false;
    }

    partial.m_fragments[fragment.m_index] = payload;

    if ((int) partial.m_fragments.size() < partial.m_total) {
        return false;
    }

    completed.m_source = source;
    completed.m_contentType = 0;
    completed.m_data.clear();

    for (int i = 0; i < partial.m_total; i++) {
        completed.m_data.insert(completed.m_data.end(), partial.m_fragments[i].begin(), partial.m_fragments[i].end());
    }

    // Kept, complete, until it expires: the transmitter repeats the fragments, which are then
    // duplicates with the same content and ignored
    partial.m_complete = true;
    return true;
}

bool DVAngel61Reassembler::addMessageFragment(uint64_t source, const DVAngel61::MessageFragment& fragment, int64_t timeMs, Message& completed)
{
    expire(timeMs);

    if ((fragment.m_total < 1) || (fragment.m_index >= fragment.m_total)) {
        return false;
    }

    std::pair<uint64_t, int> key(source, fragment.m_id);
    auto it = m_messages.find(key);

    if ((it != m_messages.end()) && ((it->second.m_total != fragment.m_total) || (it->second.m_contentType != fragment.m_contentType)
        || (it->second.m_flags != fragment.m_flags)))
    {
        // A new message under this id: start again
        m_messages.erase(it);
        it = m_messages.end();
    }

    if (it == m_messages.end())
    {
        Partial p;
        p.m_total = fragment.m_total;
        p.m_contentType = fragment.m_contentType;
        p.m_flags = fragment.m_flags;
        it = m_messages.insert(std::make_pair(key, p)).first;
    }

    Partial& partial = it->second;
    partial.m_lastTimeMs = timeMs;
    auto existing = partial.m_fragments.find(fragment.m_index);

    if (existing != partial.m_fragments.end())
    {
        if (existing->second != fragment.m_payload)
        {
            // partial is a reference into the element being erased: read it first
            const bool wasComplete = partial.m_complete;
            m_messages.erase(it);

            if (wasComplete)
            {
                Partial p;
                p.m_total = fragment.m_total;
                p.m_contentType = fragment.m_contentType;
                p.m_flags = fragment.m_flags;
                p.m_fragments[fragment.m_index] = fragment.m_payload;
                p.m_lastTimeMs = timeMs;
                m_messages.insert(std::make_pair(key, p));
            }
        }
        return false;
    }

    if (partial.m_complete) {
        return false;
    }

    partial.m_fragments[fragment.m_index] = fragment.m_payload;

    if (fragment.m_index == 0)
    {
        if (fragment.m_payload.size() < 2)
        {
            m_messages.erase(it);
            return false;
        }
        partial.m_length = (fragment.m_payload[0] << 8) | fragment.m_payload[1];
    }

    if ((int) partial.m_fragments.size() < partial.m_total) {
        return false;
    }

    std::vector<uint8_t> stream;

    for (int i = 0; i < partial.m_total; i++) {
        stream.insert(stream.end(), partial.m_fragments[i].begin(), partial.m_fragments[i].end());
    }

    bool ok = (partial.m_length >= 0) && ((int) stream.size() >= 2 + partial.m_length);

    if (ok)
    {
        completed.m_source = source;
        completed.m_contentType = partial.m_contentType;
        completed.m_flags = partial.m_flags;
        completed.m_data.assign(stream.begin() + 2, stream.begin() + 2 + partial.m_length);
        partial.m_complete = true; // kept until it expires: the repeated fragments are duplicates
    }
    else
    {
        m_messages.erase(it);
    }

    return ok;
}

std::vector<DVAngel61Reassembler::Incomplete> DVAngel61Reassembler::incomplete(uint64_t source, int contentType, int64_t sinceMs) const
{
    std::vector<Incomplete> messages;

    for (const auto& entry : m_messages)
    {
        const Partial& p = entry.second;

        if ((entry.first.first != source) || (p.m_contentType != contentType) || p.m_complete || (p.m_lastTimeMs < sinceMs)) {
            continue;
        }

        Incomplete m;
        m.m_id = entry.first.second;
        m.m_total = p.m_total;

        for (int index = 0; index < p.m_total; )
        {
            if (p.m_fragments.count(index)) {
                index++;
                continue;
            }

            const int first = index;

            while ((index < p.m_total) && !p.m_fragments.count(index)) {
                index++;
            }

            m.m_missing.push_back(std::make_pair(first, index - first));
        }

        if (!m.m_missing.empty()) {
            messages.push_back(m);
        }
    }

    return messages;
}

int DVAngel61Reassembler::pending() const
{
    int n = 0;

    for (const auto& kv : m_text) {
        n += kv.second.m_complete ? 0 : 1;
    }

    for (const auto& kv : m_messages) {
        n += kv.second.m_complete ? 0 : 1;
    }

    return n;
}

void DVAngel61Reassembler::expire(int64_t timeMs)
{
    for (auto it = m_text.begin(); it != m_text.end(); )
    {
        if (timeMs - it->second.m_lastTimeMs > ExpiryMs) {
            it = m_text.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_messages.begin(); it != m_messages.end(); )
    {
        if (timeMs - it->second.m_lastTimeMs > ExpiryMs) {
            it = m_messages.erase(it);
        } else {
            ++it;
        }
    }
}
