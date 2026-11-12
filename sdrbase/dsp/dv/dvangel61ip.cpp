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
#include <cctype>
#include <cstdio>
#include <cstring>

#include "dvangel61ip.h"

std::string DVAngel61IP::Prefix::toString() const
{
    char text[80];

    if ((m_family == 4) && (m_address.size() == 4))
    {
        snprintf(text, sizeof(text), "%d.%d.%d.%d/%d",
            m_address[0], m_address[1], m_address[2], m_address[3], m_length);
        return text;
    }

    if ((m_family == 6) && (m_address.size() == 16))
    {
        std::string out;

        for (int i = 0; i < 8; i++)
        {
            snprintf(text, sizeof(text), "%s%x", i ? ":" : "", (m_address[2 * i] << 8) | m_address[2 * i + 1]);
            out += text;
        }

        snprintf(text, sizeof(text), "/%d", m_length);
        return out + text;
    }

    return "?";
}

std::vector<uint8_t> DVAngel61IP::Control::encode() const
{
    std::vector<uint8_t> bytes;
    bytes.push_back((uint8_t) m_kind);

    switch (m_kind)
    {
    case Advertise:
    {
        bytes.push_back((uint8_t) m_flags);
        bytes.push_back((uint8_t) ((m_mtu >> 8) & 0xFF));
        bytes.push_back((uint8_t) (m_mtu & 0xFF));
        bytes.push_back((uint8_t) m_compression);
        int count = (int) m_prefixes.size();
        count = count > MaxPrefixes ? MaxPrefixes : count;
        bytes.push_back((uint8_t) count);

        for (int i = 0; i < count; i++)
        {
            const Prefix& prefix = m_prefixes[i];
            bytes.push_back((uint8_t) prefix.m_family);
            bytes.push_back((uint8_t) prefix.m_length);
            size_t size = prefix.m_family == 6 ? 16 : 4;

            for (size_t b = 0; b < size; b++) {
                bytes.push_back(b < prefix.m_address.size() ? prefix.m_address[b] : 0);
            }
        }

        break;
    }

    case Refuse:
        bytes.push_back((uint8_t) m_reason);
        break;

    case Close:
        break;

    case Repair:
    {
        bytes.push_back((uint8_t) m_messageId);
        int count = (int) m_ranges.size();
        count = count > 255 ? 255 : count;
        bytes.push_back((uint8_t) count);

        for (int i = 0; i < count; i++)
        {
            bytes.push_back((uint8_t) m_ranges[i].first);
            bytes.push_back((uint8_t) m_ranges[i].second);
        }

        break;
    }

    case ContextState:
    case ContextAck:
    {
        int count = std::min((int) m_contexts.size(), 255);
        bytes.push_back((uint8_t) count);

        for (int i = 0; i < count; i++) {
            bytes.push_back((uint8_t) m_contexts[i]);
        }

        break;
    }

    default:
        break;
    }

    return bytes;
}

bool DVAngel61IP::Control::decode(const std::vector<uint8_t>& bytes, Control& control)
{
    if (bytes.empty()) {
        return false;
    }

    control = Control();
    control.m_kind = bytes[0];

    switch (control.m_kind)
    {
    case Advertise:
    {
        if (bytes.size() < 6) {
            return false;
        }

        control.m_flags = bytes[1];
        control.m_mtu = (bytes[2] << 8) | bytes[3];
        control.m_compression = bytes[4];
        int count = bytes[5];

        if (count > MaxPrefixes) {
            return false;
        }

        size_t at = 6;

        for (int i = 0; i < count; i++)
        {
            if (at + 2 > bytes.size()) {
                return false;
            }

            Prefix prefix;
            prefix.m_family = bytes[at];
            prefix.m_length = bytes[at + 1];
            at += 2;
            size_t size = prefix.m_family == 6 ? 16 : 4;

            if ((prefix.m_family != 4) && (prefix.m_family != 6)) {
                return false;
            }

            if ((at + size > bytes.size()) || (prefix.m_length > (int) (8 * size))) {
                return false;
            }

            prefix.m_address.assign(bytes.begin() + at, bytes.begin() + at + size);
            at += size;
            control.m_prefixes.push_back(prefix);
        }

        // An MTU outside what the format can carry is a station that means something else by it
        if ((control.m_mtu < MinMTU) || (control.m_mtu > MaxMTU)) {
            return false;
        }

        return true;
    }

    case Refuse:
        if (bytes.size() < 2) {
            return false;
        }

        control.m_reason = bytes[1];
        return true;

    case Close:
        return true;

    case Repair:
    {
        if (bytes.size() < 3) {
            return false;
        }

        control.m_messageId = bytes[1];
        int count = bytes[2];

        if (bytes.size() < 3 + (size_t) 2 * count) {
            return false;
        }

        for (int i = 0; i < count; i++) {
            control.m_ranges.push_back(std::make_pair((int) bytes[3 + 2 * i], (int) bytes[4 + 2 * i]));
        }

        return true;
    }

    case ContextState:
    case ContextAck:
    {
        if ((bytes.size() < 2) || (bytes.size() < 2 + (size_t) bytes[1])) {
            return false;
        }

        control.m_contexts.assign(bytes.begin() + 2, bytes.begin() + 2 + bytes[1]);
        return true;
    }

    default:
        break;
    }

    return false;   // a kind this version does not know: the caller ignores it
}

std::string DVAngel61IP::Control::toString() const
{
    char text[128];

    switch (m_kind)
    {
    case Advertise:
    {
        snprintf(text, sizeof(text), "advertise %s%s MTU %d%s%s, %d prefix(es)",
            (m_flags & FlagIPv4) ? "IPv4 " : "", (m_flags & FlagIPv6) ? "IPv6 " : "", m_mtu,
            (m_flags & FlagForwards) ? ", forwards" : "", (m_flags & FlagRepairs) ? ", repairs" : "",
            (int) m_prefixes.size());
        std::string out = text;

        for (const Prefix& prefix : m_prefixes) {
            out += " " + prefix.toString();
        }

        return out;
    }

    case Refuse:
        snprintf(text, sizeof(text), "refuse (%d)", m_reason);
        return text;

    case Close:
        return "close";

    case Repair:
        snprintf(text, sizeof(text), "repair of message %d, %d range(s)", m_messageId, (int) m_ranges.size());
        return text;

    case ContextState:
    case ContextAck:
    {
        std::string out = m_kind == ContextState ? "context state" : "context ack";

        for (int id : m_contexts) {
            out += " " + std::to_string(id);
        }

        return out;
    }

    default:
        break;
    }

    snprintf(text, sizeof(text), "kind %d", m_kind);
    return text;
}

int DVAngel61IP::version(const std::vector<uint8_t>& datagram)
{
    if (datagram.empty()) {
        return 0;
    }

    int version = (datagram[0] >> 4) & 0xF;
    return ((version == 4) || (version == 6)) ? version : 0;
}

bool DVAngel61IP::addresses(const std::vector<uint8_t>& datagram, std::vector<uint8_t>& source,
    std::vector<uint8_t>& destination)
{
    source.clear();
    destination.clear();

    if ((version(datagram) == 4) && (datagram.size() >= 20))
    {
        source.assign(datagram.begin() + 12, datagram.begin() + 16);
        destination.assign(datagram.begin() + 16, datagram.begin() + 20);
        return true;
    }

    if ((version(datagram) == 6) && (datagram.size() >= 40))
    {
        source.assign(datagram.begin() + 8, datagram.begin() + 24);
        destination.assign(datagram.begin() + 24, datagram.begin() + 40);
        return true;
    }

    return false;
}

DVAngel61IP::Refusal DVAngel61IP::classify(const std::vector<uint8_t>& datagram, int mtu)
{
    int v = version(datagram);

    if (v == 0) {
        return NotIP;
    }

    if ((int) datagram.size() > mtu) {
        return TooLong;
    }

    if (v == 4)
    {
        if (datagram.size() < 20) {
            return BadLength;
        }

        int headerLength = 4 * (datagram[0] & 0xF);
        int total = (datagram[2] << 8) | datagram[3];

        if ((headerLength < 20) || (total != (int) datagram.size()) || (headerLength > total)) {
            return BadLength;
        }

        // 224.0.0.0/4 is multicast, 255.255.255.255 is the local broadcast, and a host route
        // pointed at a tunnel produces both without being asked
        if ((datagram[16] & 0xF0) == 0xE0) {
            return Multicast;
        }

        if ((datagram[16] == 0xFF) && (datagram[17] == 0xFF) && (datagram[18] == 0xFF) && (datagram[19] == 0xFF)) {
            return Broadcast;
        }

        return Accepted;
    }

    if (datagram.size() < 40) {
        return BadLength;
    }

    int payload = (datagram[4] << 8) | datagram[5];

    if (40 + payload != (int) datagram.size()) {
        return BadLength;
    }

    // ff00::/8 is multicast, which on IPv6 is where neighbour discovery lives
    if (datagram[24] == 0xFF) {
        return Multicast;
    }

    return Accepted;
}

DVAngel61IP::Refusal DVAngel61IP::classify(const std::vector<uint8_t>& datagram, int mtu, const Filter& filter)
{
    Refusal refusal = classify(datagram, mtu);

    // A group the operator lets through
    if (refusal == Multicast)
    {
        std::vector<uint8_t> source, destination;

        if (addresses(datagram, source, destination)
            && (std::find(filter.m_groups.begin(), filter.m_groups.end(), destination) != filter.m_groups.end())) {
            refusal = Accepted;
        }
    }

    if ((refusal != Accepted) || filter.m_allow.empty()) {
        return refusal;
    }

    int protocol, sourcePort, destinationPort;

    if (!transport(datagram, protocol, sourcePort, destinationPort)) {
        return NotIP;
    }

    return filter.allows(protocol, sourcePort, destinationPort) ? Accepted : NotAllowed;
}

bool DVAngel61IP::transport(const std::vector<uint8_t>& datagram, int& protocol, int& sourcePort, int& destinationPort)
{
    protocol = -1;
    sourcePort = destinationPort = -1;
    const int v = version(datagram);
    size_t offset;
    bool first = true;

    if (v == 4)
    {
        if (datagram.size() < 20) {
            return false;
        }

        protocol = datagram[9];
        offset = 4 * (datagram[0] & 0xF);
        first = (((datagram[6] & 0x1F) << 8) | datagram[7]) == 0;   // fragment offset 0
    }
    else if (v == 6)
    {
        if (datagram.size() < 40) {
            return false;
        }

        protocol = datagram[6];
        offset = 40;

        // The extension headers a port is behind: hop by hop (0), routing (43), destination options (60), fragment (44)
        for (int headers = 0; (headers < 8) && ((protocol == 0) || (protocol == 43) || (protocol == 60) || (protocol == 44)); headers++)
        {
            if (offset + 8 > datagram.size()) {
                return true;
            }

            const int next = datagram[offset];

            if (protocol == 44)
            {
                first = (((datagram[offset + 2] << 8) | datagram[offset + 3]) & 0xFFF8) == 0;
                offset += 8;
            }
            else
            {
                offset += 8 + 8 * (size_t) datagram[offset + 1];
            }

            protocol = next;
        }
    }
    else
    {
        return false;
    }

    // TCP, UDP and SCTP start with the two ports
    if (first && ((protocol == 6) || (protocol == 17) || (protocol == 132)) && (offset + 4 <= datagram.size()))
    {
        sourcePort = (datagram[offset] << 8) | datagram[offset + 1];
        destinationPort = (datagram[offset + 2] << 8) | datagram[offset + 3];
    }

    return true;
}

static int protocolNumber(const std::string& name)
{
    static const struct { const char *m_name; int m_number; } names[] = {
        { "icmp", 1 }, { "tcp", 6 }, { "udp", 17 }, { "gre", 47 }, { "esp", 50 }, { "ah", 51 },
        { "icmpv6", 58 }, { "sctp", 132 } };

    for (const auto& n : names) {
        if (name == n.m_name) return n.m_number;
    }

    if (!name.empty() && (name.size() <= 3) && std::all_of(name.begin(), name.end(), [](char c) { return (c >= '0') && (c <= '9'); }))
    {
        const int number = std::stoi(name);
        return number <= 255 ? number : -2;
    }

    return -2;
}

static bool portNumber(const std::string& text, int& port)
{
    if (text.empty() || (text.size() > 5) || !std::all_of(text.begin(), text.end(), [](char c) { return (c >= '0') && (c <= '9'); })) {
        return false;
    }

    port = std::stoi(text);
    return port <= 65535;
}

bool DVAngel61IP::Filter::parseAllow(const std::string& text, std::vector<Rule>& rules, std::string& error)
{
    rules.clear();
    size_t at = 0;

    while (at <= text.size())
    {
        size_t comma = text.find(',', at);
        std::string term = text.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
        at = comma == std::string::npos ? text.size() + 1 : comma + 1;
        term.erase(std::remove_if(term.begin(), term.end(), [](char c) { return (c == ' ') || (c == '\t'); }), term.end());
        std::transform(term.begin(), term.end(), term.begin(), [](char c) { return (char) std::tolower((unsigned char) c); });

        if (term.empty()) {
            continue;
        }

        Rule rule;
        const size_t colon = term.find(':');
        rule.m_protocol = protocolNumber(term.substr(0, colon));

        if (rule.m_protocol < 0)
        {
            error = term;
            return false;
        }

        if (colon != std::string::npos)
        {
            // (a port on a protocol without ports would never match: refused rather than kept silently)
            const std::string ports = term.substr(colon + 1);
            const size_t dash = ports.find('-');

            if (((rule.m_protocol != 6) && (rule.m_protocol != 17) && (rule.m_protocol != 132))
                || !portNumber(ports.substr(0, dash), rule.m_portLow)
                || !portNumber(dash == std::string::npos ? ports : ports.substr(dash + 1), rule.m_portHigh)
                || (rule.m_portHigh < rule.m_portLow))
            {
                error = term;
                return false;
            }
        }

        rules.push_back(rule);
    }

    return true;
}

std::string DVAngel61IP::Filter::allowString() const
{
    std::string text;

    for (const Rule& rule : m_allow)
    {
        if (!text.empty()) text += ", ";
        text += rule.m_protocol == 1 ? "icmp" : rule.m_protocol == 6 ? "tcp" : rule.m_protocol == 17 ? "udp"
            : rule.m_protocol == 132 ? "sctp" : std::to_string(rule.m_protocol);
        if (rule.m_portLow >= 0) text += ":" + std::to_string(rule.m_portLow);
        if (rule.m_portHigh > rule.m_portLow) text += "-" + std::to_string(rule.m_portHigh);
    }

    return text;
}

bool DVAngel61IP::Filter::allows(int protocol, int sourcePort, int destinationPort) const
{
    if (m_allow.empty()) {
        return true;
    }

    for (const Rule& rule : m_allow)
    {
        // ICMP names both ICMPs: an echo is the same thing on either family
        const bool protocolMatches = (rule.m_protocol < 0) || (rule.m_protocol == protocol)
            || ((rule.m_protocol == 1) && (protocol == 58)) || ((rule.m_protocol == 58) && (protocol == 1));

        if (!protocolMatches) {
            continue;
        }

        // A port range matches either port: a session's replies come from the port that was allowed
        if ((rule.m_portLow < 0)
            || ((sourcePort >= rule.m_portLow) && (sourcePort <= rule.m_portHigh))
            || ((destinationPort >= rule.m_portLow) && (destinationPort <= rule.m_portHigh))) {
            return true;
        }
    }

    return false;
}

const char *DVAngel61IP::refusalName(Refusal refusal)
{
    switch (refusal)
    {
    case Accepted: return "accepted";
    case NotIP: return "not IP";
    case BadLength: return "bad length";
    case Multicast: return "multicast";
    case Broadcast: return "broadcast";
    case TooLong: return "too long";
    case NotAllowed: return "not allowed";
    default: break;
    }

    return "?";
}

bool DVAngel61IP::inPrefix(const std::vector<uint8_t>& address, const Prefix& prefix)
{
    size_t size = prefix.m_family == 6 ? 16 : 4;

    if ((address.size() != size) || (prefix.m_address.size() != size)
        || (prefix.m_length < 0) || (prefix.m_length > (int) (8 * size))) {
        return false;
    }

    int whole = prefix.m_length / 8;
    int bits = prefix.m_length % 8;

    if ((whole > 0) && (std::memcmp(address.data(), prefix.m_address.data(), whole) != 0)) {
        return false;
    }

    if (bits == 0) {
        return true;
    }

    uint8_t mask = (uint8_t) (0xFF << (8 - bits));
    return (address[whole] & mask) == (prefix.m_address[whole] & mask);
}

bool DVAngel61IP::routable(const std::vector<uint8_t>& datagram, const std::vector<Prefix>& prefixes)
{
    if (prefixes.empty()) {
        return true;   // the peer said nothing about what it routes: nothing is refused here
    }

    std::vector<uint8_t> source, destination;

    if (!addresses(datagram, source, destination)) {
        return false;
    }

    for (const Prefix& prefix : prefixes)
    {
        if (inPrefix(destination, prefix)) {
            return true;
        }
    }

    return false;
}
