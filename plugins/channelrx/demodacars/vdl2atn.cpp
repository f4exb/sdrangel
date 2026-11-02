///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// The X.25, CLNP and COTP layouts follow dumpvdl2 by Tomasz Lemiech,            //
// https://github.com/szpajder/dumpvdl2, GPL-3.0.                                //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                              //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "vdl2atn.h"

#include <QRegularExpression>

// For xidDestinationAirport(), which the Aircraft feature reads too
#include "util/aircraftreport.h"

#include <libacars/libacars.h>
#include <libacars/vstring.h>

// From the vendored dumpvdl2 decoder in atn/. The vendored headers are not C++-clean,
// so the entry points are declared here rather than by including atn/icao.h / atn/xid.h.
extern "C" la_proto_node *icao_apdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type);
extern "C" la_proto_node *xid_parse(uint8_t cr, uint8_t pf, uint8_t *buf, uint32_t len, uint32_t *msg_type);

// Message type flags, matching atn/dumpvdl2.h
#define MSGFLT_SRC_GND      (1 <<  0)
#define MSGFLT_SRC_AIR      (1 <<  1)
#define MSGFLT_XID_NO_GSIF  (1 <<  7)
#define MSGFLT_XID_GSIF     (1 <<  8)
#define MSGFLT_CM           (1 << 14)
#define MSGFLT_CPDLC        (1 << 15)
#define MSGFLT_ADSC         (1 << 16)

// X.25 packet type identifiers (ISO 8208 / dumpvdl2 x25.h)
#define X25_CALL_REQUEST    0x0b
#define X25_CALL_ACCEPTED   0x0f
#define X25_CLEAR_REQUEST   0x13
#define X25_CLEAR_CONFIRM   0x17
#define X25_DATA            0x00
#define X25_RR              0x01
#define X25_REJ             0x09
#define X25_RESET_REQUEST   0x1b
#define X25_RESET_CONFIRM   0x1f
#define X25_RESTART_REQUEST 0xfb
#define X25_RESTART_CONFIRM 0xff
#define X25_DIAG            0xf1

// Subnetwork protocol discriminators
#define SN_PROTO_CLNP       0x81
#define SN_PROTO_ESIS       0x82
#define SN_PROTO_IDRP       0x85

// COTP TPDU codes (ISO 8073 / dumpvdl2 cotp.h)
#define COTP_TPDU_CR        0xe0
#define COTP_TPDU_CC        0xd0
#define COTP_TPDU_DR        0x80
#define COTP_TPDU_DC        0xc0
#define COTP_TPDU_DT        0xf0
#define COTP_TPDU_ED        0x10
#define COTP_TPDU_AK        0x60
#define COTP_TPDU_EA        0x20
#define COTP_TPDU_RJ        0x50
#define COTP_TPDU_ER        0x70

// A reassembly buffer beyond this is a stream that lost its final fragment; drop it
// rather than let a corrupted M-bit grow it without bound
#define REASSEMBLY_MAX_BYTES 65536

// Flows whose final fragment never arrives are dropped after this many decoded
// frames without activity, and the flow count itself is bounded (oldest first),
// so a long running session cannot accumulate stale entries without limit
#define REASSEMBLY_MAX_AGE_FRAMES 4096
#define REASSEMBLY_MAX_FLOWS 64

template <typename K, typename T>
static void pruneReassemblyMap(QHash<K, T>& map, quint64 now)
{
    for (auto it = map.begin(); it != map.end(); )
    {
        if (it.value().m_lastUsed + REASSEMBLY_MAX_AGE_FRAMES < now) {
            it = map.erase(it);
        } else {
            ++it;
        }
    }
    while (map.size() > REASSEMBLY_MAX_FLOWS)
    {
        auto oldest = map.begin();
        for (auto it = map.begin(); it != map.end(); ++it)
        {
            if (it.value().m_lastUsed < oldest.value().m_lastUsed) {
                oldest = it;
            }
        }
        map.erase(oldest);
    }
}

void Vdl2AtnDecoder::pruneStale()
{
    pruneReassemblyMap(m_x25Reassembly, m_frameCount);
    pruneReassemblyMap(m_clnpReassembly, m_frameCount);
    pruneReassemblyMap(m_cotpReassembly, m_frameCount);
}

void Vdl2AtnDecoder::clear()
{
    m_x25Reassembly.clear();
    m_clnpReassembly.clear();
    m_cotpReassembly.clear();
}

// Drop the reassembly state of the current logical channel in both directions,
// when the virtual circuit is cleared or reset
void Vdl2AtnDecoder::dropFlow(quint32 srcAddr, quint32 dstAddr)
{
    FlowKey forward{srcAddr, dstAddr, m_channel};
    FlowKey reverse{dstAddr, srcAddr, m_channel};
    m_x25Reassembly.remove(forward);
    m_x25Reassembly.remove(reverse);
    m_clnpReassembly.remove(forward);
    m_clnpReassembly.remove(reverse);
    m_cotpReassembly.remove(forward);
    m_cotpReassembly.remove(reverse);
}

// Nibble-packed BCD address from an X.25 call setup address block
static QString bcdAddress(const uint8_t *buf, int nibbles)
{
    QString s;
    for (int i = 0; i < nibbles; i++)
    {
        uint8_t nibble = (i & 1) ? (buf[i/2] & 0xf) : (buf[i/2] >> 4);
        s.append(QChar("0123456789abcdef"[nibble]));
    }
    return s;
}

Vdl2AtnDecoder::Result Vdl2AtnDecoder::decode(const QByteArray& info, quint32 srcAddr, quint32 dstAddr, bool fromAircraft)
{
    Result result;
    const uint8_t *buf = (const uint8_t *) info.constData();
    int len = info.size();

    m_fromAircraft = fromAircraft;
    m_frameCount++;
    pruneStale();

    // General format identifier: modulo-8 sequence numbering is the only format VDL-2 uses
    if ((len < 3) || ((buf[0] >> 4) != 0x1))
    {
        result.m_summary = "Not X.25";
        return result;
    }

    // Logical channel identifier: group in the low nibble of the GFI octet, number in
    // the second octet. Keys every reassembly, so concurrent virtual circuits between
    // the same addresses stay separate.
    m_channel = ((quint32) (buf[0] & 0xf) << 8) | buf[1];

    uint8_t type = buf[2];
    const uint8_t *ptr = buf + 3;
    int remaining = len - 3;

    if ((type & 1) == 0)
    {
        // Data packet: P(R) M P(S) 0. Accumulate while the M bit asks for more; a
        // P(S) that does not follow the previous fragment means a data packet was
        // lost, so the partial buffer is stale and gets dropped.
        bool more = (type & 0x10) != 0;
        int ps = (type >> 1) & 7;
        FlowKey key{srcAddr, dstAddr, m_channel};
        X25Reasm& reasm = m_x25Reassembly[key];
        if ((reasm.m_data.size() > REASSEMBLY_MAX_BYTES)
            || ((reasm.m_nextPs >= 0) && (ps != reasm.m_nextPs))) {
            reasm.m_data.clear();
        }
        reasm.m_data.append((const char *) ptr, remaining);
        reasm.m_nextPs = (ps + 1) & 7;
        reasm.m_lastUsed = m_frameCount;

        if (more)
        {
            result.m_summary = QString("X.25 DATA (fragment, %1 bytes held)").arg(reasm.m_data.size());
            return result;
        }

        QByteArray data = reasm.m_data;
        m_x25Reassembly.remove(key);
        result.m_summary = "X.25 DATA";
        parseX25UserData((const uint8_t *) data.constData(), data.size(), srcAddr, dstAddr, result);
        return result;
    }

    switch (type)
    {
    case X25_CALL_REQUEST:
    case X25_CALL_ACCEPTED:
    {
        result.m_summary = type == X25_CALL_REQUEST ? "X.25 CALL REQUEST" : "X.25 CALL ACCEPTED";

        // Address block: calling/called BCD digit counts, then packed digits
        if (remaining < 1) {
            return result;
        }
        int callingNibbles = ptr[0] >> 4;
        int calledNibbles = ptr[0] & 0xf;
        int addrBytes = (callingNibbles + calledNibbles + 1) / 2;
        ptr++; remaining--;
        if (remaining < addrBytes) {
            return result;
        }
        if (calledNibbles > 0) {
            result.m_summary.append(QString(" to %1").arg(bcdAddress(ptr, calledNibbles)));
        }
        ptr += addrBytes; remaining -= addrBytes;

        // Facilities
        if (remaining < 1) {
            return result;
        }
        int facLen = ptr[0];
        ptr += 1 + facLen; remaining -= 1 + facLen;
        if (remaining < 0) {
            return result;
        }

        // Call request carries an SNDCF field, call accepted a single compression octet
        if (type == X25_CALL_REQUEST)
        {
            if ((remaining < 2) || (ptr[0] != 0xc1)) {
                return result;
            }
            int sndcfLen = ptr[1];
            ptr += 2 + sndcfLen; remaining -= 2 + sndcfLen;
        }
        else if (remaining > 0)
        {
            ptr++; remaining--;
        }
        if (remaining <= 0) {
            return result;
        }

        // Fast select: call setup can carry a data PDU
        parseX25UserData(ptr, remaining, srcAddr, dstAddr, result);
        return result;
    }
    case X25_CLEAR_REQUEST:
        result.m_summary = "X.25 CLEAR REQUEST";
        if (remaining >= 1) {
            result.m_summary.append(QString(" cause 0x%1").arg(ptr[0], 2, 16, QChar('0')));
        }
        dropFlow(srcAddr, dstAddr);
        return result;
    case X25_CLEAR_CONFIRM: result.m_summary = "X.25 CLEAR CONFIRM"; dropFlow(srcAddr, dstAddr); return result;
    case X25_RESET_REQUEST: result.m_summary = "X.25 RESET REQUEST"; dropFlow(srcAddr, dstAddr); return result;
    case X25_RESET_CONFIRM: result.m_summary = "X.25 RESET CONFIRM"; dropFlow(srcAddr, dstAddr); return result;
    // A restart tears down every virtual circuit, so all held fragments are stale
    case X25_RESTART_REQUEST: result.m_summary = "X.25 RESTART REQUEST"; clear(); return result;
    case X25_RESTART_CONFIRM: result.m_summary = "X.25 RESTART CONFIRM"; clear(); return result;
    case X25_DIAG: result.m_summary = "X.25 DIAGNOSTIC"; return result;
    default:
        if ((type & 0x1f) == X25_RR) {
            result.m_summary = "X.25 RR";
        } else if ((type & 0x1f) == X25_REJ) {
            result.m_summary = "X.25 REJ";
        } else {
            result.m_summary = QString("X.25 type 0x%1").arg(type, 2, 16, QChar('0'));
        }
        return result;
    }
}

// The XID VDL parameter set carries the destination as a four letter ICAO ident:
//
//    VDL params:
//     Parameter set ID: V
//     Connection management: 05
//     ...
//     Destination airport: EGCC
//
// Anchored to the end of the line so a longer value - the parameter is an octet string,
// so nothing guarantees four characters - is rejected rather than truncated to its first
// four. Only exactly four letters is an ICAO ident.
QString Vdl2AtnDecoder::xidDestinationAirport(const QString& decoded)
{
    // One definition, in sdrbase, because the Aircraft feature reads the same field out
    // of stored logons and cannot link this plugin
    return ::xidDestinationAirport(decoded);
}

Vdl2AtnDecoder::Result Vdl2AtnDecoder::decodeXid(const QByteArray& info, bool cr, bool pf)
{
    Result result;
    uint32_t msgType = 0;

    la_proto_node *node = xid_parse(cr ? 1 : 0, pf ? 1 : 0,
                                    (uint8_t *) const_cast<char *>(info.constData()),
                                    (uint32_t) info.size(), &msgType);
    if (node)
    {
        la_vstring *vstr = la_proto_tree_format_text(NULL, node);
        if (vstr)
        {
            result.m_decoded = QString::fromUtf8(vstr->str).trimmed();
            la_vstring_destroy(vstr, true);
        }
        la_proto_tree_destroy(node);
    }

    // The first line of the decode carries the message type, e.g.
    // "XID: Ground Station Information Frame"
    result.m_summary = result.m_decoded.section('\n', 0, 0).trimmed();
    if (result.m_summary.isEmpty()) {
        result.m_summary = "XID";
    }

    result.m_destinationAirport = xidDestinationAirport(result.m_decoded);

    // Pull the position out of a location parameter: "Aircraft location: 51.6N 2.7E
    // 37000 ft" (aircraft, with altitude) or "Ground station location: 51.6N 2.7E"
    static const QRegularExpression locRe(
        "(?:Aircraft location|Ground station location): "
        "(\\d+(?:\\.\\d+)?)([NS]) (\\d+(?:\\.\\d+)?)([EW])(?: (-?\\d+) ft)?");
    QRegularExpressionMatch match = locRe.match(result.m_decoded);
    if (match.hasMatch())
    {
        result.m_hasPosition = true;
        result.m_latitude = match.captured(1).toFloat() * (match.captured(2) == "S" ? -1.0f : 1.0f);
        result.m_longitude = match.captured(3).toFloat() * (match.captured(4) == "W" ? -1.0f : 1.0f);
        // The XID altitude is a single octet in units of 1000 ft, so the field cannot
        // express more than 255000 ft and most avionics fill it correctly - measured on
        // a live capture, 28 of 32 reports came back between 0 and 40000 ft. A few send
        // something else entirely: G-RUKF reported 0x7b, which reads as 123000 ft, while
        // ADS-B had it at 37000 at the same moment. Those frames pass the AVLC FCS, so
        // the octet really is what was transmitted - it is the sender that is wrong, and
        // there is no scaling that reconciles them with the ones that are right.
        //
        // A position from one of these is still useful, and is ranked as coarse anyway.
        // The altitude is not: passed on it would feed the Aircraft feature's altitude
        // record and put a 123000 ft entry in the statistics.
        const int altFt = match.captured(5).isEmpty() ? 0 : match.captured(5).toInt();
        // The XID altitude octet is in units of 1000 ft, which is upstream dumpvdl2's
        // reading and is confirmed by measurement - but a minority of aircraft do not
        // fill it correctly, and the value is not necessarily current even when they do.
        //
        // Checked against ADS-B on two simultaneous captures: 227 altitude reports from
        // 101 aircraft. Where an aircraft could be matched to its own ADS-B track the
        // scaling agreed, several times almost exactly - 40000/40000, 38000/38000,
        // 19000/19050, 29000/29030 ft.
        //
        // Two failure modes remain, and neither can be explained by the staleness noted
        // below, because no aircraft is ever at 90000 ft or at zero while cruising:
        //
        //   14 reports above 50000 ft, from 5 aircraft. Four are B738 (123, 90, 76, 75,
        //   65) and one is a B772 sending 255 four times, which reads as a deliberate
        //   "unavailable" sentinel rather than a scaling error.
        //
        //   27 reports of exactly zero, from 16 aircraft, 15 of them B738. Where ADS-B
        //   could confirm it the aircraft was at cruise - 4ca9c1 at 31000 ft, 4ca849 at
        //   32000, 407e69 at 35000 across six handoffs - and none was on the ground.
        //
        // 19 of the 21 aircraft involved are Boeing 737NGs, so this looks like one
        // avionics fit rather than a scattering of individual faults. The 737 MAX is not
        // affected.
        //
        // STALENESS. Separately, the reported altitude can be minutes old: EI-HGP, a
        // B38M descending at 2000 ft/min, reported 3200 ft high on three consecutive
        // handoffs and was accurate once level. So a correctly scaled value is still not
        // a current one, and neither this altitude nor the position beside it should be
        // treated as a fresh fix. The position is ranked coarse and plausibility checked
        // for exactly that reason.
        //
        // Hence: zero means "not available" rather than sea level - taking it literally
        // put cruising aircraft at 0 ft - and anything above the ceiling is discarded.
        //
        // The ceiling alone cannot be enough, though, and should not be relied on as if
        // it were: a corrupt octet can just as easily land at a value that looks like a
        // real cruise. EI-GXJ, another Ryanair 737-800, was later seen sending 59000,
        // which the original 60000 ft ceiling let through and which then stood as an
        // aircraft's highest-altitude record. The Aircraft feature therefore ranks this
        // altitude AltitudeCoarse and keeps it out of its records altogether; the ceiling
        // here only stops the most obvious nonsense reaching the table.
        if (!match.captured(5).isEmpty() && (altFt > 0)
            && (altFt <= ACARSVDL2_MAX_XID_ALTITUDE_FT))
        {
            result.m_hasAltitude = true;
            result.m_altitudeFt = altFt;
        }
    }

    return result;
}

void Vdl2AtnDecoder::parseX25UserData(const uint8_t *buf, int len, quint32 srcAddr, quint32 dstAddr, Result& result)
{
    if (len <= 0) {
        return;
    }
    uint8_t proto = buf[0];
    if (proto == SN_PROTO_CLNP)
    {
        parseClnp(buf, len, srcAddr, dstAddr, result);
    }
    else if (proto == SN_PROTO_ESIS)
    {
        result.m_summary.append(" > ES-IS");
    }
    else
    {
        uint8_t pduType = proto >> 4;
        if ((pduType < 0x4) || (pduType == 0x6) || (pduType == 0x7) || (pduType == 0x9) || (pduType == 0xa)) {
            parseClnpCompressed(buf, len, srcAddr, dstAddr, result);
        } else if (proto == 0xe0) {
            result.m_summary.append(" > SNDCF error report");
        } else {
            result.m_summary.append(" > unknown subnetwork protocol");
        }
    }
}

// Uncompressed ISO 8473 CLNP
void Vdl2AtnDecoder::parseClnp(const uint8_t *buf, int len, quint32 srcAddr, quint32 dstAddr, Result& result)
{
    result.m_summary.append(" > CLNP");
    if (len < 9) {
        return;
    }
    int hdrLen = buf[1];
    if ((hdrLen < 9) || (hdrLen > len)) {
        return;
    }
    uint8_t type = buf[4] & 0x1f;
    if (type != 0x1c) { // Data PDU
        result.m_summary.append(QString(" type 0x%1").arg(type, 2, 16, QChar('0')));
        return;
    }
    // Payload follows the full header (options and any segmentation part included in hdrLen)
    parseClnpPayload(buf + hdrLen, len - hdrLen, srcAddr, dstAddr, result);
}

// A CLNP data payload is usually COTP transport, but can also be a routing or
// reachability protocol
void Vdl2AtnDecoder::parseClnpPayload(const uint8_t *buf, int len, quint32 srcAddr, quint32 dstAddr, Result& result)
{
    if (len <= 0) {
        return;
    }
    switch (buf[0])
    {
    case SN_PROTO_ESIS: result.m_summary.append(" > ES-IS"); return;
    case SN_PROTO_IDRP: result.m_summary.append(" > IDRP"); return;
    case SN_PROTO_CLNP: result.m_summary.append(" > CLNP (nested)"); return;
    default: parseCotp(buf, len, srcAddr, dstAddr, result); return;
    }
}

// ATN compressed (local reference) CLNP, per the SNDCF in ICAO doc 9705 and dumpvdl2 clnp.c
void Vdl2AtnDecoder::parseClnpCompressed(const uint8_t *buf, int len, quint32 srcAddr, quint32 dstAddr, Result& result)
{
    result.m_summary.append(" > CLNP");
    if (len < 4) {
        return;
    }
    uint8_t type = buf[0] >> 4;
    bool exp = (buf[3] & 0x80) != 0;
    bool derived = (type == 0x6) || (type == 0x7) || (type == 0x9) || (type == 0xa);
    bool segmentationPermitted = (type == 0x1) || (type == 0x3) || derived;
    bool moreSegments = (type == 0x7) || (type == 0xa);

    int hdrLen = 4;
    if (exp) {
        hdrLen += 1;
    }
    if (segmentationPermitted) {
        hdrLen += 2;
    }
    if (derived) {
        hdrLen += 4;
    }
    if (len < hdrLen) {
        return;
    }

    const uint8_t *payload = buf + hdrLen;
    int payloadLen = len - hdrLen;

    if (derived)
    {
        // Segmented: accumulate in order until the final segment
        FlowKey key{srcAddr, dstAddr, m_channel};
        ReasmBuf& reasm = m_clnpReassembly[key];
        if (reasm.m_data.size() > REASSEMBLY_MAX_BYTES) {
            reasm.m_data.clear();
        }
        reasm.m_data.append((const char *) payload, payloadLen);
        reasm.m_lastUsed = m_frameCount;
        if (moreSegments)
        {
            result.m_summary.append(QString(" (fragment, %1 bytes held)").arg(reasm.m_data.size()));
            return;
        }
        QByteArray data = reasm.m_data;
        m_clnpReassembly.remove(key);
        parseClnpPayload((const uint8_t *) data.constData(), data.size(), srcAddr, dstAddr, result);
        return;
    }

    parseClnpPayload(payload, payloadLen, srcAddr, dstAddr, result);
}

// ISO 8073 COTP, possibly several TPDUs concatenated; only the final one carries user data
void Vdl2AtnDecoder::parseCotp(const uint8_t *buf, int len, quint32 srcAddr, quint32 dstAddr, Result& result)
{
    while (len > 0)
    {
        if (len < 2) {
            return;
        }
        int li = buf[0];
        if ((li == 0) || (li == 255) || (len - 1 < li)) {
            result.m_summary.append(" > COTP (malformed)");
            return;
        }
        uint8_t code = buf[1];
        uint8_t codeClass = code & 0xf0;
        if ((codeClass == COTP_TPDU_CR) || (codeClass == COTP_TPDU_CC) || (codeClass == COTP_TPDU_AK) || (codeClass == COTP_TPDU_RJ)) {
            code = codeClass;
        } else if (codeClass == COTP_TPDU_DT) {
            code = code & 0xfe;
        }

        const char *name;
        bool finalPdu = false;
        switch (code)
        {
        case COTP_TPDU_CR: name = "CR"; finalPdu = true; break;
        case COTP_TPDU_CC: name = "CC"; finalPdu = true; break;
        case COTP_TPDU_DR: name = "DR"; finalPdu = true; break;
        case COTP_TPDU_DC: name = "DC"; break;
        case COTP_TPDU_DT: name = "DT"; finalPdu = true; break;
        case COTP_TPDU_ED: name = "ED"; finalPdu = true; break;
        case COTP_TPDU_AK: name = "AK"; break;
        case COTP_TPDU_EA: name = "EA"; break;
        case COTP_TPDU_RJ: name = "RJ"; break;
        case COTP_TPDU_ER: name = "ER"; break;
        default:
            result.m_summary.append(" > COTP (unknown TPDU)");
            return;
        }
        result.m_summary.append(QString(" > COTP %1").arg(name));

        if (!finalPdu)
        {
            // Header only; further TPDUs may be concatenated after it
            buf += 1 + li;
            len -= 1 + li;
            continue;
        }

        const uint8_t *userData = buf + 1 + li;
        int userDataLen = len - 1 - li;
        if (userDataLen <= 0) {
            return;
        }

        if ((code == COTP_TPDU_DT) || (code == COTP_TPDU_ED))
        {
            // End-of-TSDU flag: normal format has it in the byte after dst-ref; extended
            // (odd header length) in the high bit of a 4 byte sequence number
            if (li < 4) {
                return;
            }
            bool eot = (buf[4] & 0x80) != 0;
            FlowKey key{srcAddr, dstAddr, m_channel};
            ReasmBuf& reasm = m_cotpReassembly[key];
            if (reasm.m_data.size() > REASSEMBLY_MAX_BYTES) {
                reasm.m_data.clear();
            }
            reasm.m_data.append((const char *) userData, userDataLen);
            reasm.m_lastUsed = m_frameCount;
            if (!eot)
            {
                result.m_summary.append(QString(" (fragment, %1 bytes held)").arg(reasm.m_data.size()));
                return;
            }
            QByteArray data = reasm.m_data;
            m_cotpReassembly.remove(key);
            parseIcao((const uint8_t *) data.constData(), data.size(), m_fromAircraft, result);
        }
        else if (code == COTP_TPDU_DR)
        {
            // A single user data byte in DR is the X.225 disconnect reason
            if (userDataLen > 1) {
                parseIcao(userData, userDataLen, m_fromAircraft, result);
            }
        }
        else
        {
            parseIcao(userData, userDataLen, m_fromAircraft, result);
        }
        return;
    }
}

// Pull the expanded CPDLC message elements out of the formatted decode: the lines
// indented under "Message data:", which are the element text ("WILCO",
// "REQUEST [level]") followed by any argument lines ("Flight level: 370"). This is the
// ATN equivalent of the DO-219 expansion the ACARS path puts in the ATC column.
//
// The arguments are put into the element's own placeholders rather than listed after
// it, so "NEXT DATA AUTHORITY ([facility designation])" with "Facility designation:
// EDYY" under it reads "NEXT DATA AUTHORITY (EDYY)". Only the value is used: the
// placeholder is usually an abbreviation of the argument's name rather than something
// it adds to, so keeping both gives "PREFERRED LEVEL Flight level: 370"
static QString extractCpdlcMessageData(const QString& decoded)
{
    auto indentOf = [](const QString& line)
    {
        int i = 0;
        while ((i < line.size()) && (line[i] == ' ')) {
            i++;
        }
        return i;
    };

    QStringList elements;
    const QStringList lines = decoded.split('\n');

    for (int i = 0; i < lines.size(); i++)
    {
        if (lines[i].trimmed() != "Message data:") {
            continue;
        }
        const int dataIndent = indentOf(lines[i]);

        while ((i + 1 < lines.size()) && (indentOf(lines[i+1]) > dataIndent) && !lines[i+1].trimmed().isEmpty())
        {
            i++;
            const int elementIndent = indentOf(lines[i]);
            QString element = lines[i].trimmed();

            // Argument lines are indented under the element they belong to. If a decode
            // ever puts them at the same level they are treated as elements of their
            // own, which is what this did for every message before
            QStringList names;
            QStringList values;

            while ((i + 1 < lines.size()) && (indentOf(lines[i+1]) > elementIndent) && !lines[i+1].trimmed().isEmpty())
            {
                i++;
                const QString argument = lines[i].trimmed();
                const int colon = argument.indexOf(':');

                if (colon >= 0)
                {
                    names.append(argument.left(colon).trimmed());
                    values.append(argument.mid(colon + 1).trimmed());
                }
                else
                {
                    names.append(QString());
                    values.append(argument);
                }
            }

            auto argumentText = [&names, &values](int index)
            {
                return names[index].isEmpty() ? values[index]
                                              : names[index] + ": " + values[index];
            };

            // Placeholders are filled left to right, which is the order libacars lists
            // the arguments in
            int argument = 0;
            int from = 0;

            while (argument < names.size())
            {
                const int open = element.indexOf('[', from);
                if (open < 0) {
                    break;
                }
                const int close = element.indexOf(']', open);
                if (close < 0) {
                    break;
                }
                const QString& replacement = values[argument];

                element.replace(open, close - open + 1, replacement);
                from = open + replacement.size();
                argument++;
            }

            // More arguments than placeholders, so the rest are listed as before rather
            // than dropped
            for (; argument < names.size(); argument++) {
                element.append(", " + argumentText(argument));
            }

            elements.append(element.simplified());
        }
    }

    return elements.join(", ");
}

// The ICAO ULCS / application layer, decoded by the vendored dumpvdl2 ASN.1 decoder
void Vdl2AtnDecoder::parseIcao(const uint8_t *buf, int len, bool fromAircraft, Result& result)
{
    if (len <= 0) {
        return;
    }

    uint32_t msgType = fromAircraft ? MSGFLT_SRC_AIR : MSGFLT_SRC_GND;
    la_proto_node *node = icao_apdu_parse(const_cast<uint8_t *>(buf), (uint32_t) len, &msgType);

    result.m_isCpdlc = (msgType & MSGFLT_CPDLC) != 0;
    result.m_isCm = (msgType & MSGFLT_CM) != 0;
    result.m_isAdsc = (msgType & MSGFLT_ADSC) != 0;

    if (result.m_isCpdlc) {
        result.m_summary.append(" > CPDLC");
    } else if (result.m_isCm) {
        result.m_summary.append(" > CM");
    } else if (result.m_isAdsc) {
        result.m_summary.append(" > ADS-C v2");
    } else {
        result.m_summary.append(" > ICAO APDU");
    }

    if (node)
    {
        la_vstring *vstr = la_proto_tree_format_text(NULL, node);
        if (vstr)
        {
            result.m_decoded = QString::fromUtf8(vstr->str).trimmed();
            la_vstring_destroy(vstr, true);
        }
        la_proto_tree_destroy(node);
    }

    if (result.m_isCpdlc) {
        result.m_atc = extractCpdlcMessageData(result.m_decoded);
    }
    if (result.m_isAdsc) {
        extractAdscPosition(result);
    }
}

// Pull the aircraft position out of an ADS-C v2 report's decode. The report's own
// position block comes first, before any EPP waypoints (which are labelled "Waypoint
// data" rather than "Position"), e.g:
//   Position:
//    Lat:  51 19' 03.6" north
//    Lon: 000 38' 28.1" east
//    Alt: 19940 ft
void Vdl2AtnDecoder::extractAdscPosition(Result& result)
{
    static const QRegularExpression posRe(
        "Position:\\s*\\n"
        "\\s*Lat:\\s*(\\d+) (\\d+)' ([\\d.]+)\" (north|south)\\s*\\n"
        "\\s*Lon:\\s*(\\d+) (\\d+)' ([\\d.]+)\" (east|west)"
        "(?:\\s*\\n\\s*Alt: (-?\\d+) ft)?");

    QRegularExpressionMatch match = posRe.match(result.m_decoded);
    if (!match.hasMatch()) {
        return;
    }

    result.m_hasPosition = true;
    result.m_latitude = match.captured(1).toFloat()
                      + match.captured(2).toFloat() / 60.0f
                      + match.captured(3).toFloat() / 3600.0f;
    if (match.captured(4) == "south") {
        result.m_latitude = -result.m_latitude;
    }
    result.m_longitude = match.captured(5).toFloat()
                       + match.captured(6).toFloat() / 60.0f
                       + match.captured(7).toFloat() / 3600.0f;
    if (match.captured(8) == "west") {
        result.m_longitude = -result.m_longitude;
    }
    if (!match.captured(9).isEmpty())
    {
        result.m_hasAltitude = true;
        result.m_altitudeFt = match.captured(9).toInt();
    }
}
