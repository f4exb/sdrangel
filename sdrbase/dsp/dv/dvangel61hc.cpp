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

#include "dvangel61hc.h"

namespace
{

const int ProtocolTCP = 6;
const int ProtocolUDP = 17;
const uint8_t FlagPush = 0x08;
const uint8_t FlagUrgent = 0x20;

// Where the headers of a compressible datagram are
struct Layout
{
    int m_version = 0;
    int m_ip = 0;           // IP header bytes: 20 or 40
    int m_protocol = 0;     // TCP or UDP
    int m_transport = 0;    // transport header bytes, TCP options included
    int m_timestamp = -1;   // offset of the TCP timestamp option's TSval, -1 without one
    int headers() const { return m_ip + m_transport; }
};

uint16_t get16(const std::vector<uint8_t>& d, int at) { return (uint16_t) ((d[at] << 8) | d[at + 1]); }
uint32_t get32(const std::vector<uint8_t>& d, int at) { return ((uint32_t) get16(d, at) << 16) | get16(d, at + 2); }
void put16(std::vector<uint8_t>& d, int at, uint32_t v) { d[at] = (uint8_t) (v >> 8); d[at + 1] = (uint8_t) v; }
void put32(std::vector<uint8_t>& d, int at, uint32_t v) { put16(d, at, v >> 16); put16(d, at + 2, v & 0xFFFF); }

bool parse(const std::vector<uint8_t>& d, Layout& l)
{
    if (d.size() < 20) {
        return false;
    }

    l = Layout();
    l.m_version = d[0] >> 4;

    if (l.m_version == 4)
    {
        // No options, not a fragment, the length its own
        if (((d[0] & 0x0F) != 5) || (get16(d, 2) != d.size()) || (d[6] & 0x20) || (get16(d, 6) & 0x1FFF)) {
            return false;
        }

        l.m_ip = 20;
        l.m_protocol = d[9];
    }
    else if (l.m_version == 6)
    {
        // An extension header is another next header value: not compressible
        if ((d.size() < 40) || (get16(d, 4) + 40u != d.size())) {
            return false;
        }

        l.m_ip = 40;
        l.m_protocol = d[6];
    }
    else
    {
        return false;
    }

    if (l.m_protocol == ProtocolTCP)
    {
        if ((int) d.size() < l.m_ip + 20) {
            return false;
        }

        int offset = d[l.m_ip + 12] >> 4;

        if ((offset < 5) || ((int) d.size() < l.m_ip + 4 * offset)) {
            return false;
        }

        l.m_transport = 4 * offset;

        for (int at = l.m_ip + 20, end = l.m_ip + l.m_transport; at < end;)
        {
            int kind = d[at];

            if (kind == 0) {
                break;          // end of options: the rest is padding
            }
            if (kind == 1)
            {
                at++;           // no operation
                continue;
            }
            if ((at + 1 >= end) || (d[at + 1] < 2) || (at + d[at + 1] > end)) {
                return false;   // malformed options go as they are
            }
            if ((kind == 8) && (d[at + 1] == 10)) {
                l.m_timestamp = at + 2;
            }

            at += d[at + 1];
        }
    }
    else if (l.m_protocol == ProtocolUDP)
    {
        if (((int) d.size() < l.m_ip + 8) || (get16(d, l.m_ip + 4) + (size_t) l.m_ip != d.size())) {
            return false;
        }

        l.m_transport = 8;
    }
    else
    {
        return false;
    }

    return true;
}

// The flow: addresses, protocol and ports
std::vector<uint8_t> keyOf(const std::vector<uint8_t>& d, const Layout& l)
{
    std::vector<uint8_t> key = { (uint8_t) l.m_version, (uint8_t) l.m_protocol };

    if (l.m_version == 4) {
        key.insert(key.end(), d.begin() + 12, d.begin() + 20);
    } else {
        key.insert(key.end(), d.begin() + 8, d.begin() + 40);
    }

    key.insert(key.end(), d.begin() + l.m_ip, d.begin() + l.m_ip + 4);
    return key;
}

uint32_t sum16(const uint8_t *p, size_t n, uint32_t sum)
{
    for (size_t i = 0; i + 1 < n; i += 2) {
        sum += (uint32_t) ((p[i] << 8) | p[i + 1]);
    }
    if (n & 1) {
        sum += (uint32_t) (p[n - 1] << 8);
    }
    return sum;
}

uint16_t fold(uint32_t sum)
{
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t) sum;
}

// The transport checksum's sum: the pseudo header and the segment
uint16_t transportSum(const std::vector<uint8_t>& d, const Layout& l)
{
    uint32_t length = (uint32_t) (d.size() - l.m_ip);
    uint32_t sum = 0;

    if (l.m_version == 4) {
        sum = sum16(&d[12], 8, sum) + l.m_protocol + length;
    } else {
        sum = sum16(&d[8], 32, sum) + (length >> 16) + (length & 0xFFFF) + l.m_protocol;
    }

    return fold(sum16(&d[l.m_ip], d.size() - l.m_ip, sum));
}

int checksumOffset(const Layout& l) { return l.m_ip + (l.m_protocol == ProtocolTCP ? 16 : 6); }

void setIPv4Checksum(std::vector<uint8_t>& d)
{
    put16(d, 10, 0);
    put16(d, 10, (uint16_t) ~fold(sum16(&d[0], 20, 0)));
}

// RFC 1144's encoding: 1 to 255 in a byte, anything else to 65535 as a zero byte and 16 bits
bool putDelta(std::vector<uint8_t>& out, uint32_t delta)
{
    if ((delta >= 1) && (delta <= 255))
    {
        out.push_back((uint8_t) delta);
        return true;
    }

    if (delta > 0xFFFF) {
        return false;
    }

    out.push_back(0);
    out.push_back((uint8_t) (delta >> 8));
    out.push_back((uint8_t) delta);
    return true;
}

bool getDelta(const std::vector<uint8_t>& in, size_t& at, uint32_t& delta)
{
    if (at >= in.size()) {
        return false;
    }

    if (in[at])
    {
        delta = in[at++];
        return true;
    }

    if (at + 3 > in.size()) {
        return false;
    }

    delta = (uint32_t) ((in[at + 1] << 8) | in[at + 2]);
    at += 3;
    return true;
}

// A datagram against its context's reference, as form 2 after the id and sequence already in
// out; false when a field the masks cannot carry differs (the datagram goes as form 1)
bool encodeCompressed(const std::vector<uint8_t>& d, const Layout& l, const std::vector<uint8_t>& ref, std::vector<uint8_t>& out)
{
    if (ref.size() != (size_t) l.headers()) {
        return false;
    }

    // The reference is headers only: its lengths say nothing, so its shape is compared field by field
    if ((ref[0] != d[0]) || (l.m_version == 4 ? ref[9] != d[9] : ref[6] != d[6])) {
        return false;
    }

    uint8_t mask = 0, extension = 0;
    std::vector<uint8_t> tcpFields, ipFields, extensionFields;
    const int t = l.m_ip;

    if (l.m_version == 4)
    {
        // DSCP and ECN, DF, the protocol: copied from the reference, so they must equal it
        if ((d[1] != ref[1]) || (d[6] != ref[6]) || (d[7] != ref[7])) {
            return false;
        }

        uint16_t id = get16(d, 4), refId = get16(ref, 4);

        if (id != (uint16_t) (refId + 1))
        {
            mask |= DVAngel61HC::MaskIdentification;
            putDelta(ipFields, (uint16_t) (id - refId));
        }

        if (d[8] != ref[8])
        {
            mask |= DVAngel61HC::MaskTTL;
            ipFields.push_back(d[8]);
        }
    }
    else
    {
        // Version, traffic class and flow label
        if (!std::equal(d.begin(), d.begin() + 4, ref.begin())) {
            return false;
        }

        if (d[7] != ref[7])
        {
            mask |= DVAngel61HC::MaskTTL;
            ipFields.push_back(d[7]);
        }
    }

    if (l.m_protocol == ProtocolTCP)
    {
        // The data offset and every flag but PSH and URG as the reference's: SYN, FIN, RST, ACK,
        // ECE and CWR changing go as a full header
        if ((d[t + 12] != ref[t + 12]) || ((d[t + 13] & ~(FlagPush | FlagUrgent)) != (ref[t + 13] & ~(FlagPush | FlagUrgent)))) {
            return false;
        }

        uint32_t sequence = get32(d, t + 4) - get32(ref, t + 4);
        uint32_t ack = get32(d, t + 8) - get32(ref, t + 8);
        uint16_t window = (uint16_t) (get16(d, t + 14) - get16(ref, t + 14));

        if (sequence)
        {
            mask |= DVAngel61HC::MaskSequence;
            if (!putDelta(tcpFields, sequence)) { return false; }
        }
        if (ack)
        {
            mask |= DVAngel61HC::MaskAck;
            if (!putDelta(tcpFields, ack)) { return false; }
        }
        if (window)
        {
            mask |= DVAngel61HC::MaskWindow;
            putDelta(tcpFields, window);
        }
        if (d[t + 13] & FlagUrgent)
        {
            mask |= DVAngel61HC::MaskUrgent;
            tcpFields.push_back(d[t + 18]);
            tcpFields.push_back(d[t + 19]);
        }
        else if (get16(d, t + 18) != get16(ref, t + 18))
        {
            return false;
        }
        if (d[t + 13] & FlagPush) {
            mask |= DVAngel61HC::MaskPush;
        }

        // The options as the reference's but for the timestamp's two values
        for (int i = t + 20; i < t + l.m_transport; i++)
        {
            bool timestamp = (l.m_timestamp >= 0) && (i >= l.m_timestamp) && (i < l.m_timestamp + 8);

            if (!timestamp && (d[i] != ref[i])) {
                return false;
            }
        }

        if (l.m_timestamp >= 0)
        {
            uint32_t tsval = get32(d, l.m_timestamp) - get32(ref, l.m_timestamp);
            uint32_t tsecr = get32(d, l.m_timestamp + 4) - get32(ref, l.m_timestamp + 4);

            if (tsval)
            {
                extension |= DVAngel61HC::ExtensionTSval;
                if (!putDelta(extensionFields, tsval)) { return false; }
            }
            if (tsecr)
            {
                extension |= DVAngel61HC::ExtensionTSecr;
                if (!putDelta(extensionFields, tsecr)) { return false; }
            }
        }
    }

    if (extension) {
        mask |= DVAngel61HC::MaskExtended;
    }

    out.push_back(mask);

    if (extension) {
        out.push_back(extension);
    }

    int checksum = checksumOffset(l);
    out.push_back(d[checksum]);
    out.push_back(d[checksum + 1]);
    out.insert(out.end(), tcpFields.begin(), tcpFields.end());
    out.insert(out.end(), ipFields.begin(), ipFields.end());
    out.insert(out.end(), extensionFields.begin(), extensionFields.end());
    out.insert(out.end(), d.begin() + l.headers(), d.end());
    return true;
}

// Serial number arithmetic (RFC 1982) on the 8 bit context sequence: 1 to 127 ahead
bool ahead(int sequence, int last)
{
    int difference = (sequence - last) & 0xFF;
    return (difference >= 1) && (difference <= 127);
}

} // namespace

int DVAngel61HC::capability(bool tcp, bool udp, bool sendsState, int contexts)
{
    int log2 = 0;

    while ((log2 < 8) && ((1 << log2) < contexts)) {
        log2++;
    }

    return (tcp ? CapabilityTCP : 0) | (udp ? CapabilityUDP : 0) | (sendsState ? CapabilitySendsState : 0)
        | ((contexts > 0 ? log2 : 0) << 4);
}

int DVAngel61HC::contextsOf(int capability)
{
    int log2 = (capability >> 4) & 0x0F;
    return log2 == 0 ? 0 : 1 << std::min(log2, 8);
}

bool DVAngel61HC::compressible(const std::vector<uint8_t>& datagram)
{
    Layout l;
    return parse(datagram, l);
}

bool DVAngel61HC::transportChecksumOk(const std::vector<uint8_t>& datagram)
{
    Layout l;

    if (!parse(datagram, l)) {
        return false;
    }

    if ((l.m_protocol == ProtocolUDP) && (l.m_version == 4) && (get16(datagram, checksumOffset(l)) == 0)) {
        return true;
    }

    return transportSum(datagram, l) == 0xFFFF;
}

void DVAngel61HC::setChecksums(std::vector<uint8_t>& datagram)
{
    Layout l;

    if (!parse(datagram, l)) {
        return;
    }

    if (l.m_version == 4) {
        setIPv4Checksum(datagram);
    }

    int at = checksumOffset(l);
    put16(datagram, at, 0);
    uint16_t checksum = (uint16_t) ~transportSum(datagram, l);
    put16(datagram, at, ((l.m_protocol == ProtocolUDP) && (checksum == 0)) ? 0xFFFF : checksum);
}

// ---- Compressor ------------------------------------------------------------------------------

DVAngel61HC::Compressor::Compressor() :
    m_peer(0),
    m_contexts(DefaultContexts),
    m_uses(0)
{
}

void DVAngel61HC::Compressor::setPeer(int capability)
{
    // A peer that changes what it takes (or closes and opens again) holds no context of ours.
    // The statistics are kept: they are the tunnel's, not the peer's
    if (capability != m_peer)
    {
        Statistics statistics = m_statistics;
        reset();
        m_statistics = statistics;
    }

    m_peer = capability;
}

void DVAngel61HC::Compressor::reset()
{
    // The sequences are kept: they belong to the context ids, and a peer still holds them
    for (Context& c : m_contexts)
    {
        c.m_used = false;
        c.m_needsFull = true;
        c.m_reference.clear();
    }

    m_statistics = Statistics();
}

void DVAngel61HC::Compressor::refresh(const std::vector<int>& contexts)
{
    for (int id : contexts)
    {
        if ((id >= 0) && (id < (int) m_contexts.size())) {
            m_contexts[id].m_needsFull = true;
        }
    }
}

void DVAngel61HC::Compressor::compress(const std::vector<uint8_t>& datagram, int64_t timeMs, std::vector<uint8_t>& message, int& form)
{
    m_statistics.m_bytesIn += (int64_t) datagram.size();
    Layout l;
    const int contexts = std::min((int) m_contexts.size(), contextsOf(m_peer));
    bool taken = parse(datagram, l)
        && (((l.m_protocol == ProtocolTCP) && (m_peer & CapabilityTCP)) || ((l.m_protocol == ProtocolUDP) && (m_peer & CapabilityUDP)))
        && (contexts > 0);

    if (!taken)
    {
        // Verbatim: nothing is touched
        form = FormVerbatim;
        message = datagram;
        m_statistics.m_verbatim++;
        m_statistics.m_bytesOut += (int64_t) message.size();
        return;
    }

    // The flow's context, or a new one in the least recently used
    std::vector<uint8_t> key = keyOf(datagram, l);
    int id = -1;

    for (int i = 0; (i < contexts) && (id < 0); i++)
    {
        if (m_contexts[i].m_used && (m_contexts[i].m_key == key)) {
            id = i;
        }
    }

    if (id < 0)
    {
        id = 0;

        for (int i = 0; i < contexts; i++)
        {
            if (!m_contexts[i].m_used)
            {
                id = i;
                break;
            }
            if (m_contexts[i].m_lastUsed < m_contexts[id].m_lastUsed) {
                id = i;
            }
        }

        Context& fresh = m_contexts[id];
        fresh.m_used = true;
        fresh.m_key = key;
        fresh.m_reference.clear();
        fresh.m_needsFull = true;
    }

    Context& c = m_contexts[id];
    c.m_lastUsed = ++m_uses;
    std::vector<uint8_t> out = { (uint8_t) id, (uint8_t) c.m_sequence };
    bool full = c.m_needsFull || c.m_reference.empty() || (c.m_sinceFull >= RefreshPackets) || (timeMs - c.m_fullMs >= RefreshMs)
        || !encodeCompressed(datagram, l, c.m_reference, out);

    if (full)
    {
        out.resize(2);
        out.insert(out.end(), datagram.begin(), datagram.end());
        form = FormFull;
        c.m_sinceFull = 0;
        c.m_fullMs = timeMs;
        c.m_needsFull = false;
        m_statistics.m_full++;
    }
    else
    {
        form = FormCompressed;
        c.m_sinceFull++;
        m_statistics.m_compressed++;
    }

    c.m_reference.assign(datagram.begin(), datagram.begin() + l.headers());
    c.m_sequence = (c.m_sequence + 1) & 0xFF;
    message = out;
    m_statistics.m_bytesOut += (int64_t) message.size();
}

// ---- Decompressor ----------------------------------------------------------------------------

DVAngel61HC::Decompressor::Decompressor() :
    m_contexts(DefaultContexts),
    m_discarded(0),
    m_rebuilt(0)
{
}

void DVAngel61HC::Decompressor::reset()
{
    for (Context& c : m_contexts) {
        c = Context();
    }

    m_stale.clear();
    m_acks.clear();
}

void DVAngel61HC::Decompressor::markStale(int id)
{
    m_contexts[id].m_stale = true;

    if (std::find(m_stale.begin(), m_stale.end(), id) == m_stale.end()) {
        m_stale.push_back(id);
    }
}

std::vector<int> DVAngel61HC::Decompressor::takeStale()
{
    std::vector<int> stale;
    stale.swap(m_stale);
    return stale;
}

std::vector<int> DVAngel61HC::Decompressor::takeAckRequests()
{
    std::vector<int> acks;
    acks.swap(m_acks);
    return acks;
}

bool DVAngel61HC::Decompressor::decompress(const std::vector<uint8_t>& message, int form, std::vector<uint8_t>& datagram)
{
    if (form == FormVerbatim)
    {
        datagram = message;
        return true;
    }

    if ((message.size() < 2) || ((form != FormFull) && (form != FormCompressed)))
    {
        m_discarded++;
        return false;
    }

    const int id = message[0], sequence = message[1];
    const bool known = id < (int) m_contexts.size();

    if (form == FormFull)
    {
        // Delivered whatever the context: a full header that is not ahead of the last accepted
        // is an old one arriving late, which must not roll the reference back
        datagram.assign(message.begin() + 2, message.end());
        Layout l;

        if (known && parse(datagram, l))
        {
            Context& c = m_contexts[id];

            if (!c.m_valid || c.m_stale || ahead(sequence, c.m_sequence))
            {
                c.m_valid = true;
                c.m_stale = false;
                c.m_sequence = sequence;
                c.m_reference.assign(datagram.begin(), datagram.begin() + l.headers());
            }
        }

        return true;
    }

    // Compressed: only onto the reference of the sequence just before it
    if (!known)
    {
        m_discarded++;
        return false;
    }

    Context& c = m_contexts[id];

    if (!c.m_valid || c.m_stale || (sequence != ((c.m_sequence + 1) & 0xFF)))
    {
        markStale(id);
        m_discarded++;
        return false;
    }

    // The reference's shape: its IP version and header, its transport header with the options
    std::vector<uint8_t> h = c.m_reference;
    Layout rl;
    rl.m_version = h[0] >> 4;
    rl.m_ip = rl.m_version == 4 ? 20 : 40;
    rl.m_protocol = rl.m_version == 4 ? h[9] : h[6];
    rl.m_transport = (int) h.size() - rl.m_ip;
    rl.m_timestamp = -1;

    if (rl.m_protocol == ProtocolTCP)
    {
        for (int at = rl.m_ip + 20, end = (int) h.size(); at < end;)
        {
            if (h[at] == 0) { break; }
            if (h[at] == 1) { at++; continue; }
            if ((h[at] == 8) && (h[at + 1] == 10)) { rl.m_timestamp = at + 2; }
            at += h[at + 1];
        }
    }

    size_t at = 2;
    uint8_t mask = at < message.size() ? message[at++] : 0;
    uint8_t extension = 0;
    bool ok = at <= message.size();

    if (ok && (mask & MaskExtended))
    {
        ok = at < message.size();

        if (ok) {
            extension = message[at++];
        }
    }

    ok = ok && (at + 2 <= message.size());
    uint16_t checksum = ok ? (uint16_t) ((message[at] << 8) | message[at + 1]) : 0;
    at += 2;
    const int t = rl.m_ip;
    uint32_t delta = 0;

    if (ok && (rl.m_protocol != ProtocolTCP)
        && ((mask & (MaskSequence | MaskAck | MaskWindow | MaskUrgent | MaskPush)) || (extension & (ExtensionTSval | ExtensionTSecr)))) {
        ok = false;    // TCP's fields on a UDP context
    }
    if (ok && (rl.m_timestamp < 0) && (extension & (ExtensionTSval | ExtensionTSecr))) {
        ok = false;    // timestamps without the option
    }

    if (ok && (rl.m_protocol == ProtocolTCP))
    {
        if (ok && (mask & MaskSequence) && (ok = getDelta(message, at, delta))) {
            put32(h, t + 4, get32(h, t + 4) + delta);
        }
        if (ok && (mask & MaskAck) && (ok = getDelta(message, at, delta))) {
            put32(h, t + 8, get32(h, t + 8) + delta);
        }
        if (ok && (mask & MaskWindow) && (ok = getDelta(message, at, delta))) {
            put16(h, t + 14, (uint16_t) (get16(h, t + 14) + delta));
        }

        h[t + 13] = (uint8_t) ((h[t + 13] & ~(FlagPush | FlagUrgent)) | ((mask & MaskPush) ? FlagPush : 0) | ((mask & MaskUrgent) ? FlagUrgent : 0));

        if (ok && (mask & MaskUrgent))
        {
            ok = at + 2 <= message.size();

            if (ok)
            {
                h[t + 18] = message[at];
                h[t + 19] = message[at + 1];
                at += 2;
            }
        }
    }

    if (ok && (rl.m_version == 4))
    {
        uint32_t idDelta = 1;

        if (mask & MaskIdentification) {
            ok = getDelta(message, at, idDelta);
        }

        put16(h, 4, (uint16_t) (get16(h, 4) + idDelta));
    }
    else if (ok && (mask & MaskIdentification))
    {
        ok = false;    // IPv6 has no identification
    }

    if (ok && (mask & MaskTTL))
    {
        ok = at < message.size();

        if (ok) {
            h[rl.m_version == 4 ? 8 : 7] = message[at++];
        }
    }

    if (ok && (extension & ExtensionTSval) && (ok = getDelta(message, at, delta))) {
        put32(h, rl.m_timestamp, get32(h, rl.m_timestamp) + delta);
    }
    if (ok && (extension & ExtensionTSecr) && (ok = getDelta(message, at, delta))) {
        put32(h, rl.m_timestamp + 4, get32(h, rl.m_timestamp + 4) + delta);
    }

    if (!ok || (at > message.size()))
    {
        markStale(id);
        m_discarded++;
        return false;
    }

    // The lengths from the message's own, the checksums
    datagram = h;
    datagram.insert(datagram.end(), message.begin() + at, message.end());

    if (datagram.size() > 65535)
    {
        markStale(id);
        m_discarded++;
        return false;
    }

    if (rl.m_version == 4)
    {
        put16(datagram, 2, (uint32_t) datagram.size());
        setIPv4Checksum(datagram);
    }
    else
    {
        put16(datagram, 4, (uint32_t) datagram.size() - 40);
    }

    if (rl.m_protocol == ProtocolUDP) {
        put16(datagram, t + 4, (uint32_t) datagram.size() - t);
    }

    put16(datagram, checksumOffset(rl), checksum);

    // The end to end check: a wrong reference gives a wrong checksum, never a wrong datagram
    if (!transportChecksumOk(datagram))
    {
        markStale(id);
        m_discarded++;
        return false;
    }

    c.m_sequence = sequence;
    c.m_reference.assign(datagram.begin(), datagram.begin() + h.size());
    m_rebuilt++;

    if ((extension & ExtensionAckRequest) && (std::find(m_acks.begin(), m_acks.end(), id) == m_acks.end())) {
        m_acks.push_back(id);
    }

    return true;
}
