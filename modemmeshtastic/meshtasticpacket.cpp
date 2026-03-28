///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Alejandro Aleman                                           //
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include "meshtasticpacket.h"

#include <QByteArrayList>
#include <QDateTime>
#include <QDebug>
#include <QMap>
#include <QProcessEnvironment>
#include <QRandomGenerator>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <set>
#include <vector>

namespace modemmeshtastic
{
namespace
{

constexpr uint8_t kFlagHopLimitMask = 0x07;
constexpr uint8_t kFlagWantAckMask = 0x08;
constexpr uint8_t kFlagViaMqttMask = 0x10;
constexpr uint8_t kFlagHopStartMask = 0xE0;
constexpr int kHeaderLength = 16;
constexpr uint32_t kBroadcastNode = 0xFFFFFFFFu;

struct Header
{
    uint32_t to = kBroadcastNode;
    uint32_t from = 0;
    uint32_t id = 0;
    uint8_t flags = 0;
    uint8_t channel = 0;
    uint8_t nextHop = 0;
    uint8_t relayNode = 0;
};

struct DataFields
{
    bool hasPortnum = false;
    uint32_t portnum = 0;
    QByteArray payload;

    bool wantResponse = false;

    bool hasDest = false;
    uint32_t dest = 0;

    bool hasSource = false;
    uint32_t source = 0;

    bool hasRequestId = false;
    uint32_t requestId = 0;

    bool hasReplyId = false;
    uint32_t replyId = 0;

    bool hasEmoji = false;
    uint32_t emoji = 0;

    bool hasBitfield = false;
    uint32_t bitfield = 0;
};

struct KeyEntry
{
    QString label;
    QString channelName;
    QByteArray key;
    bool hasExpectedHash = false;
    uint8_t expectedHash = 0;
};

struct CommandConfig
{
    Header header;
    DataFields data;
    bool encrypt = true;
    QByteArray key;
    QString keyLabel;
    QString channelName;
    QString presetName;
    bool hasRegion = false;
    QString regionName;
    bool hasChannelNum = false;
    uint32_t channelNum = 0; // 1-based
    bool hasOverrideFrequencyMHz = false;
    double overrideFrequencyMHz = 0.0;
    double frequencyOffsetMHz = 0.0;
    bool hasFrequencyOffsetMHz = false;
};

struct RegionBand
{
    const char* name;
    double freqStartMHz;
    double freqEndMHz;
    double spacingMHz;
    bool wideLora;
};

const std::array<uint8_t, 16> kDefaultChannelKey = {
    0xD4, 0xF1, 0xBB, 0x3A, 0x20, 0x29, 0x07, 0x59,
    0xF0, 0xBC, 0xFF, 0xAB, 0xCF, 0x4E, 0x69, 0x01
};

// Port numbers from meshtastic/portnums.proto (high value subset is accepted as numeric fallback).
QMap<QString, uint32_t> makePortMap()
{
    QMap<QString, uint32_t> m;
    m.insert("UNKNOWN_APP", 0);
    m.insert("TEXT", 1);
    m.insert("TEXT_MESSAGE_APP", 1);
    m.insert("REMOTE_HARDWARE_APP", 2);
    m.insert("POSITION_APP", 3);
    m.insert("NODEINFO_APP", 4);
    m.insert("ROUTING_APP", 5);
    m.insert("ADMIN_APP", 6);
    m.insert("TEXT_MESSAGE_COMPRESSED_APP", 7);
    m.insert("WAYPOINT_APP", 8);
    m.insert("AUDIO_APP", 9);
    m.insert("DETECTION_SENSOR_APP", 10);
    m.insert("ALERT_APP", 11);
    m.insert("KEY_VERIFICATION_APP", 12);
    m.insert("REPLY_APP", 32);
    m.insert("IP_TUNNEL_APP", 33);
    m.insert("PAXCOUNTER_APP", 34);
    m.insert("STORE_FORWARD_PLUSPLUS_APP", 35);
    m.insert("NODE_STATUS_APP", 36);
    m.insert("SERIAL_APP", 64);
    m.insert("STORE_FORWARD_APP", 65);
    m.insert("RANGE_TEST_APP", 66);
    m.insert("TELEMETRY_APP", 67);
    m.insert("ZPS_APP", 68);
    m.insert("SIMULATOR_APP", 69);
    m.insert("TRACEROUTE_APP", 70);
    m.insert("NEIGHBORINFO_APP", 71);
    m.insert("ATAK_PLUGIN", 72);
    return m;
}

const QMap<QString, uint32_t> kPortMap = makePortMap();

const std::array<RegionBand, 26> kRegionBands = {{
    {"US", 902.0, 928.0, 0.0, false},
    {"EU_433", 433.0, 434.0, 0.0, false},
    {"EU_868", 869.4, 869.65, 0.0, false},
    {"CN", 470.0, 510.0, 0.0, false},
    {"JP", 920.5, 923.5, 0.0, false},
    {"ANZ", 915.0, 928.0, 0.0, false},
    {"ANZ_433", 433.05, 434.79, 0.0, false},
    {"RU", 868.7, 869.2, 0.0, false},
    {"KR", 920.0, 923.0, 0.0, false},
    {"TW", 920.0, 925.0, 0.0, false},
    {"IN", 865.0, 867.0, 0.0, false},
    {"NZ_865", 864.0, 868.0, 0.0, false},
    {"TH", 920.0, 925.0, 0.0, false},
    {"UA_433", 433.0, 434.7, 0.0, false},
    {"UA_868", 868.0, 868.6, 0.0, false},
    {"MY_433", 433.0, 435.0, 0.0, false},
    {"MY_919", 919.0, 924.0, 0.0, false},
    {"SG_923", 917.0, 925.0, 0.0, false},
    {"PH_433", 433.0, 434.7, 0.0, false},
    {"PH_868", 868.0, 869.4, 0.0, false},
    {"PH_915", 915.0, 918.0, 0.0, false},
    {"KZ_433", 433.075, 434.775, 0.0, false},
    {"KZ_863", 863.0, 868.0, 0.0, false},
    {"NP_865", 865.0, 868.0, 0.0, false},
    {"BR_902", 902.0, 907.5, 0.0, false},
    {"LORA_24", 2400.0, 2483.5, 0.0, true}
}};

const int kRegionBandsCount = static_cast<int>(kRegionBands.size());

QString trimQuotes(const QString& s)
{
    QString out = s.trimmed();
    if ((out.startsWith('"') && out.endsWith('"')) || (out.startsWith('\'') && out.endsWith('\''))) {
        out = out.mid(1, out.size() - 2);
    }
    return out;
}

bool parseBool(const QString& s, bool& out)
{
    const QString v = s.trimmed().toLower();

    if (v == "1" || v == "true" || v == "yes" || v == "on") {
        out = true;
        return true;
    }

    if (v == "0" || v == "false" || v == "no" || v == "off") {
        out = false;
        return true;
    }

    return false;
}

bool parseUInt(const QString& s, uint64_t& out)
{
    QString v = s.trimmed();
    v.remove('_');

    bool ok = false;
    int base = 10;

    if (v.startsWith("0x") || v.startsWith("0X")) {
        base = 16;
        v = v.mid(2);
    }

    out = v.toULongLong(&ok, base);
    return ok;
}

bool parseDouble(const QString& s, double& out)
{
    QString v = s.trimmed();
    v.remove('_');
    bool ok = false;
    out = v.toDouble(&ok);
    return ok;
}

QString normalizeToken(const QString& value)
{
    QString v = value.trimmed().toUpper();
    v.replace('-', '_');
    v.replace(' ', '_');
    return v;
}

QByteArray normalizeHex(const QString& s)
{
    QByteArray out;
    const QByteArray in = s.toLatin1();

    for (char c : in)
    {
        const bool isHex = ((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F'));
        if (isHex) {
            out.append(c);
        }
    }

    return out;
}

bool parseHexBytes(const QString& s, QByteArray& out)
{
    QByteArray hex = normalizeHex(s);

    if (hex.isEmpty() || (hex.size() % 2) != 0) {
        return false;
    }

    out = QByteArray::fromHex(hex);
    return !out.isEmpty();
}

QByteArray expandSimpleKey(unsigned int simple)
{
    if (simple == 0) {
        return QByteArray();
    }

    if (simple > 10) {
        return QByteArray();
    }

    QByteArray key(reinterpret_cast<const char*>(kDefaultChannelKey.data()), static_cast<int>(kDefaultChannelKey.size()));

    if (simple > 1) {
        const auto offset = static_cast<int>(simple - 1);
        key[15] = static_cast<char>(static_cast<uint8_t>(kDefaultChannelKey[15] + offset));
    }

    return key;
}

bool parseKeySpec(const QString& rawSpec, QByteArray& key, QString& label)
{
    QString spec = rawSpec.trimmed();
    const QString lower = spec.toLower();

    if (lower.isEmpty() || lower == "default" || lower == "simple1") {
        key = expandSimpleKey(1);
        label = "default";
        return true;
    }

    if (lower == "none" || lower == "unencrypted" || lower == "simple0" || lower == "0") {
        key = QByteArray();
        label = "none";
        return true;
    }

    if (lower.startsWith("simple"))
    {
        uint64_t n = 0;
        if (!parseUInt(lower.mid(6), n)) {
            return false;
        }

        if (n > 10) {
            return false;
        }

        key = expandSimpleKey(static_cast<unsigned int>(n));
        label = QString("simple%1").arg(n);
        return true;
    }

    if (lower.startsWith("hex:"))
    {
        if (!parseHexBytes(spec.mid(4), key)) {
            return false;
        }

        label = "hex";
        return key.size() == 16 || key.size() == 32;
    }

    if (lower.startsWith("base64:") || lower.startsWith("b64:"))
    {
        const int p = spec.indexOf(':');
        key = QByteArray::fromBase64(spec.mid(p + 1).trimmed().toLatin1());

        if (key.isEmpty()) {
            return false;
        }

        label = "base64";
        return key.size() == 16 || key.size() == 32;
    }

    if (lower == "1" || lower == "2" || lower == "3" || lower == "4" || lower == "5" || lower == "6" || lower == "7" || lower == "8" || lower == "9" || lower == "10")
    {
        uint64_t n = 0;
        parseUInt(lower, n);
        key = expandSimpleKey(static_cast<unsigned int>(n));
        label = QString("simple%1").arg(n);
        return true;
    }

    QByteArray parsed;

    if (parseHexBytes(spec, parsed) && (parsed.size() == 16 || parsed.size() == 32)) {
        key = parsed;
        label = "hex";
        return true;
    }

    parsed = QByteArray::fromBase64(spec.toLatin1());

    if (parsed.size() == 16 || parsed.size() == 32) {
        key = parsed;
        label = "base64";
        return true;
    }

    return false;
}

uint32_t readU32LE(const char* p)
{
    return static_cast<uint32_t>(static_cast<uint8_t>(p[0]))
        | (static_cast<uint32_t>(static_cast<uint8_t>(p[1])) << 8)
        | (static_cast<uint32_t>(static_cast<uint8_t>(p[2])) << 16)
        | (static_cast<uint32_t>(static_cast<uint8_t>(p[3])) << 24);
}

void writeU32LE(char* p, uint32_t v)
{
    p[0] = static_cast<char>(v & 0xFF);
    p[1] = static_cast<char>((v >> 8) & 0xFF);
    p[2] = static_cast<char>((v >> 16) & 0xFF);
    p[3] = static_cast<char>((v >> 24) & 0xFF);
}

bool parseHeader(const QByteArray& frame, Header& h)
{
    if (frame.size() < kHeaderLength) {
        return false;
    }

    const char* p = frame.constData();
    h.to = readU32LE(p + 0);
    h.from = readU32LE(p + 4);
    h.id = readU32LE(p + 8);
    h.flags = static_cast<uint8_t>(p[12]);
    h.channel = static_cast<uint8_t>(p[13]);
    h.nextHop = static_cast<uint8_t>(p[14]);
    h.relayNode = static_cast<uint8_t>(p[15]);
    return true;
}

QByteArray encodeHeader(const Header& h)
{
    QByteArray out(kHeaderLength, 0);
    char* p = out.data();
    writeU32LE(p + 0, h.to);
    writeU32LE(p + 4, h.from);
    writeU32LE(p + 8, h.id);
    p[12] = static_cast<char>(h.flags);
    p[13] = static_cast<char>(h.channel);
    p[14] = static_cast<char>(h.nextHop);
    p[15] = static_cast<char>(h.relayNode);
    return out;
}

bool readVarint(const QByteArray& bytes, int& pos, uint64_t& value)
{
    value = 0;
    int shift = 0;

    while (pos < bytes.size() && shift <= 63)
    {
        const auto b = static_cast<uint8_t>(bytes[pos++]);
        value |= static_cast<uint64_t>(b & 0x7F) << shift;

        if ((b & 0x80) == 0) {
            return true;
        }

        shift += 7;
    }

    return false;
}

bool readFixed32(const QByteArray& bytes, int& pos, uint32_t& value)
{
    if ((pos + 4) > bytes.size()) {
        return false;
    }

    value = readU32LE(bytes.constData() + pos);
    pos += 4;
    return true;
}

bool readFixed64(const QByteArray& bytes, int& pos, uint64_t& value)
{
    if ((pos + 8) > bytes.size()) {
        return false;
    }

    const char* p = bytes.constData() + pos;
    value = static_cast<uint64_t>(static_cast<uint8_t>(p[0]))
        | (static_cast<uint64_t>(static_cast<uint8_t>(p[1])) << 8)
        | (static_cast<uint64_t>(static_cast<uint8_t>(p[2])) << 16)
        | (static_cast<uint64_t>(static_cast<uint8_t>(p[3])) << 24)
        | (static_cast<uint64_t>(static_cast<uint8_t>(p[4])) << 32)
        | (static_cast<uint64_t>(static_cast<uint8_t>(p[5])) << 40)
        | (static_cast<uint64_t>(static_cast<uint8_t>(p[6])) << 48)
        | (static_cast<uint64_t>(static_cast<uint8_t>(p[7])) << 56);
    pos += 8;
    return true;
}

bool skipField(const QByteArray& bytes, int& pos, uint32_t wireType)
{
    switch (wireType)
    {
    case 0: {
        uint64_t v = 0;
        return readVarint(bytes, pos, v);
    }
    case 1:
        if ((pos + 8) > bytes.size()) {
            return false;
        }
        pos += 8;
        return true;
    case 2: {
        uint64_t len = 0;
        if (!readVarint(bytes, pos, len)) {
            return false;
        }
        if (len > static_cast<uint64_t>(bytes.size() - pos)) {
            return false;
        }
        pos += static_cast<int>(len);
        return true;
    }
    case 5:
        if ((pos + 4) > bytes.size()) {
            return false;
        }
        pos += 4;
        return true;
    default:
        return false;
    }
}

bool parseData(const QByteArray& bytes, DataFields& d)
{
    int pos = 0;

    while (pos < bytes.size())
    {
        uint64_t rawTag = 0;

        if (!readVarint(bytes, pos, rawTag)) {
            return false;
        }

        const auto field = static_cast<uint32_t>(rawTag >> 3);
        const auto wire = static_cast<uint32_t>(rawTag & 0x7);

        switch (field)
        {
        case 1: {
            if (wire != 0) {
                return false;
            }

            uint64_t v = 0;
            if (!readVarint(bytes, pos, v)) {
                return false;
            }

            d.hasPortnum = true;
            d.portnum = static_cast<uint32_t>(v);
            break;
        }

        case 2: {
            if (wire != 2) {
                return false;
            }

            uint64_t len = 0;
            if (!readVarint(bytes, pos, len)) {
                return false;
            }
            if (len > static_cast<uint64_t>(bytes.size() - pos)) {
                return false;
            }

            d.payload = bytes.mid(pos, static_cast<int>(len));
            pos += static_cast<int>(len);
            break;
        }

        case 3: {
            if (wire != 0) {
                return false;
            }

            uint64_t v = 0;
            if (!readVarint(bytes, pos, v)) {
                return false;
            }

            d.wantResponse = (v != 0);
            break;
        }

        case 4: {
            if (wire != 5) {
                return false;
            }

            uint32_t v = 0;
            if (!readFixed32(bytes, pos, v)) {
                return false;
            }

            d.hasDest = true;
            d.dest = v;
            break;
        }

        case 5: {
            if (wire != 5) {
                return false;
            }

            uint32_t v = 0;
            if (!readFixed32(bytes, pos, v)) {
                return false;
            }

            d.hasSource = true;
            d.source = v;
            break;
        }

        case 6: {
            if (wire != 5) {
                return false;
            }

            uint32_t v = 0;
            if (!readFixed32(bytes, pos, v)) {
                return false;
            }

            d.hasRequestId = true;
            d.requestId = v;
            break;
        }

        case 7: {
            if (wire != 5) {
                return false;
            }

            uint32_t v = 0;
            if (!readFixed32(bytes, pos, v)) {
                return false;
            }

            d.hasReplyId = true;
            d.replyId = v;
            break;
        }

        case 8: {
            if (wire != 5) {
                return false;
            }

            uint32_t v = 0;
            if (!readFixed32(bytes, pos, v)) {
                return false;
            }

            d.hasEmoji = true;
            d.emoji = v;
            break;
        }

        case 9: {
            if (wire != 0) {
                return false;
            }

            uint64_t v = 0;
            if (!readVarint(bytes, pos, v)) {
                return false;
            }

            d.hasBitfield = true;
            d.bitfield = static_cast<uint32_t>(v);
            break;
        }

        default:
            if (!skipField(bytes, pos, wire)) {
                return false;
            }
            break;
        }
    }

    return d.hasPortnum;
}

void writeVarint(QByteArray& out, uint64_t value)
{
    while (true)
    {
        auto b = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;

        if (value != 0) {
            b |= 0x80;
            out.append(static_cast<char>(b));
        } else {
            out.append(static_cast<char>(b));
            break;
        }
    }
}

void writeTag(QByteArray& out, uint32_t field, uint32_t wire)
{
    const uint64_t tag = (static_cast<uint64_t>(field) << 3) | static_cast<uint64_t>(wire);
    writeVarint(out, tag);
}

void writeFixed32(QByteArray& out, uint32_t v)
{
    char b[4];
    writeU32LE(b, v);
    out.append(b, 4);
}

QByteArray encodeData(const DataFields& d)
{
    QByteArray out;

    writeTag(out, 1, 0);
    writeVarint(out, d.portnum);

    if (!d.payload.isEmpty()) {
        writeTag(out, 2, 2);
        writeVarint(out, static_cast<uint64_t>(d.payload.size()));
        out.append(d.payload);
    }

    if (d.wantResponse) {
        writeTag(out, 3, 0);
        writeVarint(out, 1);
    }

    if (d.hasDest) {
        writeTag(out, 4, 5);
        writeFixed32(out, d.dest);
    }

    if (d.hasSource) {
        writeTag(out, 5, 5);
        writeFixed32(out, d.source);
    }

    if (d.hasRequestId) {
        writeTag(out, 6, 5);
        writeFixed32(out, d.requestId);
    }

    if (d.hasReplyId) {
        writeTag(out, 7, 5);
        writeFixed32(out, d.replyId);
    }

    if (d.hasEmoji) {
        writeTag(out, 8, 5);
        writeFixed32(out, d.emoji);
    }

    if (d.hasBitfield) {
        writeTag(out, 9, 0);
        writeVarint(out, d.bitfield);
    }

    return out;
}

uint8_t xorHash(const QByteArray& bytes)
{
    uint8_t h = 0;

    for (char c : bytes) {
        h ^= static_cast<uint8_t>(c);
    }

    return h;
}

uint8_t generateChannelHash(const QString& channelName, const QByteArray& key)
{
    QByteArray name = channelName.toUtf8();

    if (name.isEmpty()) {
        name = "X";
    }

    return xorHash(name) ^ xorHash(key);
}

// Tiny AES implementation adapted from tiny-AES-c (public domain / unlicense).
class AesCtx
{
public:
    bool init(const QByteArray& key)
    {
        if (key.size() != 16 && key.size() != 32) {
            return false;
        }

        m_nk = key.size() / 4;
        m_nr = m_nk + 6;
        const int words = 4 * (m_nr + 1);
        m_roundKey.resize(words * 4);
        keyExpansion(reinterpret_cast<const uint8_t*>(key.constData()));
        return true;
    }

    void encryptBlock(const uint8_t in[16], uint8_t out[16]) const
    {
        std::array<uint8_t, 16> state;
        memcpy(state.data(), in, 16);

        addRoundKey(state.data(), 0);

        for (int round = 1; round < m_nr; ++round)
        {
            subBytes(state.data());
            shiftRows(state.data());
            mixColumns(state.data());
            addRoundKey(state.data(), round);
        }

        subBytes(state.data());
        shiftRows(state.data());
        addRoundKey(state.data(), m_nr);

        memcpy(out, state.data(), 16);
    }

private:
    int m_nk = 0;
    int m_nr = 0;
    std::vector<uint8_t> m_roundKey;

    static uint8_t xtime(uint8_t x)
    {
        return static_cast<uint8_t>((x << 1) ^ (((x >> 7) & 1) * 0x1B));
    }

    static uint8_t mul(uint8_t a, uint8_t b)
    {
        uint8_t res = 0;
        uint8_t x = a;
        uint8_t y = b;

        while (y)
        {
            if (y & 1) {
                res ^= x;
            }

            x = xtime(x);
            y >>= 1;
        }

        return res;
    }

    static uint8_t sub(uint8_t x)
    {
        static const std::array<uint8_t, 256> sbox = {
            0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
            0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
            0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
            0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
            0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
            0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
            0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
            0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
            0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
            0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
            0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
            0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
            0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
            0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
            0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
            0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
        };

        return sbox[x];
    }

    static void subBytes(uint8_t state[16])
    {
        for (int i = 0; i < 16; ++i) {
            state[i] = sub(state[i]);
        }
    }

    static void shiftRows(uint8_t state[16])
    {
        uint8_t t;

        t = state[1];
        state[1] = state[5];
        state[5] = state[9];
        state[9] = state[13];
        state[13] = t;

        t = state[2];
        state[2] = state[10];
        state[10] = t;
        t = state[6];
        state[6] = state[14];
        state[14] = t;

        t = state[3];
        state[3] = state[15];
        state[15] = state[11];
        state[11] = state[7];
        state[7] = t;
    }

    static void mixColumns(uint8_t state[16])
    {
        for (int c = 0; c < 4; ++c)
        {
            uint8_t* col = &state[c * 4];
            const uint8_t a0 = col[0];
            const uint8_t a1 = col[1];
            const uint8_t a2 = col[2];
            const uint8_t a3 = col[3];

            col[0] = static_cast<uint8_t>(mul(a0, 2) ^ mul(a1, 3) ^ a2 ^ a3);
            col[1] = static_cast<uint8_t>(a0 ^ mul(a1, 2) ^ mul(a2, 3) ^ a3);
            col[2] = static_cast<uint8_t>(a0 ^ a1 ^ mul(a2, 2) ^ mul(a3, 3));
            col[3] = static_cast<uint8_t>(mul(a0, 3) ^ a1 ^ a2 ^ mul(a3, 2));
        }
    }

    void addRoundKey(uint8_t state[16], int round) const
    {
        const uint8_t* rk = &m_roundKey[round * 16];
        for (int i = 0; i < 16; ++i) {
            state[i] ^= rk[i];
        }
    }

    static uint32_t subWord(uint32_t w)
    {
        return (static_cast<uint32_t>(sub(static_cast<uint8_t>((w >> 24) & 0xFF))) << 24)
             | (static_cast<uint32_t>(sub(static_cast<uint8_t>((w >> 16) & 0xFF))) << 16)
             | (static_cast<uint32_t>(sub(static_cast<uint8_t>((w >> 8) & 0xFF))) << 8)
             | static_cast<uint32_t>(sub(static_cast<uint8_t>(w & 0xFF)));
    }

    static uint32_t rotWord(uint32_t w)
    {
        return (w << 8) | (w >> 24);
    }

    void keyExpansion(const uint8_t* key)
    {
        static const std::array<uint8_t, 11> rcon = {0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1B,0x36};

        const int words = 4 * (m_nr + 1);
        std::vector<uint32_t> w(words, 0);

        for (int i = 0; i < m_nk; ++i)
        {
            w[i] = (static_cast<uint32_t>(key[4 * i]) << 24)
                 | (static_cast<uint32_t>(key[4 * i + 1]) << 16)
                 | (static_cast<uint32_t>(key[4 * i + 2]) << 8)
                 | static_cast<uint32_t>(key[4 * i + 3]);
        }

        for (int i = m_nk; i < words; ++i)
        {
            uint32_t temp = w[i - 1];

            if ((i % m_nk) == 0) {
                temp = subWord(rotWord(temp)) ^ (static_cast<uint32_t>(rcon[i / m_nk]) << 24);
            } else if (m_nk > 6 && (i % m_nk) == 4) {
                temp = subWord(temp);
            }

            w[i] = w[i - m_nk] ^ temp;
        }

        for (int i = 0; i < words; ++i)
        {
            m_roundKey[4 * i] = static_cast<uint8_t>((w[i] >> 24) & 0xFF);
            m_roundKey[4 * i + 1] = static_cast<uint8_t>((w[i] >> 16) & 0xFF);
            m_roundKey[4 * i + 2] = static_cast<uint8_t>((w[i] >> 8) & 0xFF);
            m_roundKey[4 * i + 3] = static_cast<uint8_t>(w[i] & 0xFF);
        }
    }
};

enum class CounterMode
{
    BigEndian,
    LittleEndian
};

void incrementCounter4(uint8_t counter[16], CounterMode mode)
{
    if (mode == CounterMode::BigEndian)
    {
        for (int i = 15; i >= 12; --i)
        {
            counter[i] = static_cast<uint8_t>(counter[i] + 1);
            if (counter[i] != 0) {
                break;
            }
        }
    }
    else
    {
        for (int i = 12; i <= 15; ++i)
        {
            counter[i] = static_cast<uint8_t>(counter[i] + 1);
            if (counter[i] != 0) {
                break;
            }
        }
    }
}

void initNonce(uint8_t nonce[16], uint32_t fromNode, uint32_t packetId)
{
    memset(nonce, 0, 16);

    const uint64_t packetId64 = packetId;
    memcpy(nonce, &packetId64, sizeof(packetId64));
    memcpy(nonce + sizeof(packetId64), &fromNode, sizeof(fromNode));
}

QByteArray aesCtrCrypt(const QByteArray& in, const QByteArray& key, uint32_t fromNode, uint32_t packetId, CounterMode mode)
{
    QByteArray out(in);

    if (key.isEmpty()) {
        return out;
    }

    AesCtx aes;

    if (!aes.init(key)) {
        return QByteArray();
    }

    std::array<uint8_t, 16> counter;
    initNonce(counter.data(), fromNode, packetId);

    int pos = 0;
    while (pos < out.size())
    {
        std::array<uint8_t, 16> keystream;
        aes.encryptBlock(counter.data(), keystream.data());

        const int remain = std::min<int>(16, static_cast<int>(out.size() - pos));
        for (int i = 0; i < remain; ++i) {
            out[pos + i] = static_cast<char>(static_cast<uint8_t>(out[pos + i]) ^ keystream[i]);
        }

        pos += remain;
        incrementCounter4(counter.data(), mode);
    }

    return out;
}

QString formatNode(uint32_t node)
{
    return QString("0x%1").arg(node, 8, 16, QChar('0'));
}

QString payloadToText(const QByteArray& payload)
{
    QString s = QString::fromUtf8(payload);

    if (s.isEmpty() && !payload.isEmpty()) {
        return QString();
    }

    return s;
}

QString portToName(uint32_t p)
{
    for (auto it = kPortMap.constBegin(); it != kPortMap.constEnd(); ++it)
    {
        if (it.value() == p && it.key().endsWith("_APP")) {
            return it.key();
        }
    }

    return QString("PORT_%1").arg(p);
}

bool addKeyEntry(
    std::vector<KeyEntry>& keys,
    const QString& channelName,
    const QString& keySpec,
    QString* error = nullptr)
{
    QByteArray key;
    QString label;

    if (!parseKeySpec(keySpec, key, label))
    {
        if (error) {
            *error = QString("invalid key spec '%1'").arg(keySpec);
        }
        return false;
    }

    KeyEntry e;
    e.key = key;
    e.channelName = channelName;
    e.label = channelName.isEmpty() ? label : QString("%1:%2").arg(channelName, label);

    if (!channelName.isEmpty())
    {
        e.hasExpectedHash = true;
        e.expectedHash = generateChannelHash(channelName, key);
    }

    keys.push_back(e);
    return true;
}

bool parseKeyListEntry(
    const QString& rawEntry,
    QString& channelName,
    QString& keySpec,
    QString* error = nullptr)
{
    const QString entry = rawEntry.trimmed();

    if (entry.isEmpty() || entry.startsWith('#')) {
        return false;
    }

    QByteArray parsedKey;
    QString parsedLabel;

    if (parseKeySpec(entry, parsedKey, parsedLabel))
    {
        channelName.clear();
        keySpec = entry;
        return true;
    }

    const int eq = entry.indexOf('=');

    if (eq <= 0)
    {
        if (error) {
            *error = QString("invalid key entry '%1' (use 'channel=key' or key only)").arg(entry);
        }
        return false;
    }

    channelName = trimQuotes(entry.left(eq));
    keySpec = trimQuotes(entry.mid(eq + 1));

    if (channelName.isEmpty())
    {
        if (error) {
            *error = QString("missing channel name in '%1'").arg(entry);
        }
        return false;
    }

    if (keySpec.isEmpty())
    {
        if (error) {
            *error = QString("missing key spec in '%1'").arg(entry);
        }
        return false;
    }

    return true;
}

bool parseKeySpecList(
    const QString& rawList,
    std::vector<KeyEntry>& keys,
    QString* error = nullptr,
    int* keyCount = nullptr,
    bool strict = true)
{
    QString input = rawList;
    input.replace('\r', '\n');
    input.replace(';', '\n');
    input.replace(',', '\n');

    const QStringList entries = input.split('\n', Qt::SkipEmptyParts);
    int parsedCount = 0;

    for (const QString& rawEntry : entries)
    {
        QString channelName;
        QString keySpec;
        QString entryError;

        const bool hasEntry = parseKeyListEntry(rawEntry, channelName, keySpec, &entryError);

        if (!hasEntry)
        {
            if (!entryError.isEmpty() && strict)
            {
                if (error) {
                    *error = entryError;
                }
                return false;
            }

            continue;
        }

        if (!addKeyEntry(keys, channelName, keySpec, &entryError))
        {
            if (strict)
            {
                if (error) {
                    *error = QString("%1 in '%2'").arg(entryError, rawEntry.trimmed());
                }
                return false;
            }
            continue;
        }

        parsedCount++;
    }

    if (keyCount) {
        *keyCount = parsedCount;
    }

    return true;
}

std::vector<KeyEntry> defaultKeysFromEnv()
{
    std::vector<KeyEntry> keys;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    const QString envKeys = env.value("SDRANGEL_MESHTASTIC_KEYS").trimmed();

    if (!envKeys.isEmpty())
    {
        parseKeySpecList(envKeys, keys, nullptr, nullptr, false);
    }
    else
    {
        const QString channel = env.value("SDRANGEL_MESHTASTIC_CHANNEL_NAME", "LongFast").trimmed();
        const QString keySpec = env.value("SDRANGEL_MESHTASTIC_KEY", "default").trimmed();

        addKeyEntry(keys, channel, keySpec);
        addKeyEntry(keys, QString(), "none");
    }

    if (keys.empty()) {
        addKeyEntry(keys, "LongFast", "default");
        addKeyEntry(keys, QString(), "none");
    }

    return keys;
}

bool parsePortValue(const QString& raw, uint32_t& port)
{
    uint64_t numeric = 0;
    if (parseUInt(raw, numeric)) {
        port = static_cast<uint32_t>(numeric);
        return true;
    }

    const QString upper = raw.trimmed().toUpper();

    if (kPortMap.contains(upper)) {
        port = kPortMap.value(upper);
        return true;
    }

    if (kPortMap.contains(upper + "_APP")) {
        port = kPortMap.value(upper + "_APP");
        return true;
    }

    return false;
}

bool parsePresetName(const QString& presetValue, QString& presetName)
{
    QString p = normalizeToken(presetValue);
    p.remove('_');

    if (p == "LONGFAST") { presetName = "LONG_FAST"; return true; }
    if (p == "LONGSLOW") { presetName = "LONG_SLOW"; return true; }
    if (p == "LONGTURBO") { presetName = "LONG_TURBO"; return true; }
    if (p == "LONGMODERATE") { presetName = "LONG_MODERATE"; return true; }
    if (p == "MEDIUMFAST") { presetName = "MEDIUM_FAST"; return true; }
    if (p == "MEDIUMSLOW") { presetName = "MEDIUM_SLOW"; return true; }
    if (p == "SHORTFAST") { presetName = "SHORT_FAST"; return true; }
    if (p == "SHORTSLOW") { presetName = "SHORT_SLOW"; return true; }
    if (p == "SHORTTURBO") { presetName = "SHORT_TURBO"; return true; }
    return false;
}

bool presetToChannelName(const QString& presetName, QString& channelName)
{
    if (presetName == "LONG_FAST") { channelName = "LongFast"; return true; }
    if (presetName == "LONG_SLOW") { channelName = "LongSlow"; return true; }
    if (presetName == "LONG_TURBO") { channelName = "LongTurbo"; return true; }
    if (presetName == "LONG_MODERATE") { channelName = "LongModerate"; return true; }
    if (presetName == "MEDIUM_FAST") { channelName = "MediumFast"; return true; }
    if (presetName == "MEDIUM_SLOW") { channelName = "MediumSlow"; return true; }
    if (presetName == "SHORT_FAST") { channelName = "ShortFast"; return true; }
    if (presetName == "SHORT_SLOW") { channelName = "ShortSlow"; return true; }
    if (presetName == "SHORT_TURBO") { channelName = "ShortTurbo"; return true; }
    return false;
}

bool presetToParams(const QString& presetName, bool wideLora, int& bandwidthHz, int& spreadFactor, int& parityBits)
{
    int bwKHz = 0;
    int sf = 0;
    int cr = 0;

    if (presetName == "SHORT_TURBO") { bwKHz = wideLora ? 1625 : 500; cr = 5; sf = 7; }
    else if (presetName == "SHORT_FAST") { bwKHz = wideLora ? 812 : 250; cr = 5; sf = 7; }
    else if (presetName == "SHORT_SLOW") { bwKHz = wideLora ? 812 : 250; cr = 5; sf = 8; }
    else if (presetName == "MEDIUM_FAST") { bwKHz = wideLora ? 812 : 250; cr = 5; sf = 9; }
    else if (presetName == "MEDIUM_SLOW") { bwKHz = wideLora ? 812 : 250; cr = 5; sf = 10; }
    else if (presetName == "LONG_TURBO") { bwKHz = wideLora ? 1625 : 500; cr = 8; sf = 11; }
    else if (presetName == "LONG_MODERATE") { bwKHz = wideLora ? 406 : 125; cr = 8; sf = 11; }
    else if (presetName == "LONG_SLOW") { bwKHz = wideLora ? 406 : 125; cr = 8; sf = 12; }
    else if (presetName == "LONG_FAST") { bwKHz = wideLora ? 812 : 250; cr = 5; sf = 11; }
    else {
        return false;
    }

    bandwidthHz = bwKHz * 1000;
    spreadFactor = sf;
    parityBits = std::max(1, std::min(4, cr - 4));
    return true;
}

QString presetToDisplayName(const QString& presetName)
{
    QString channelName;
    if (presetToChannelName(presetName, channelName)) {
        return channelName;
    }
    return QString("LongFast");
}

uint32_t meshHashDjb2(const QString& s)
{
    const QByteArray bytes = s.toUtf8();
    uint32_t h = 5381u;
    for (char c : bytes) {
        h = ((h << 5) + h) + static_cast<uint8_t>(c);
    }
    return h;
}

const RegionBand* findRegionBand(const QString& regionValue)
{
    QString r = normalizeToken(regionValue);
    r.remove('_');

    if (r == "EU868") { r = "EU_868"; }
    else if (r == "EU433") { r = "EU_433"; }
    else if (r == "NZ865") { r = "NZ_865"; }
    else if (r == "UA868") { r = "UA_868"; }
    else if (r == "UA433") { r = "UA_433"; }
    else if (r == "MY433") { r = "MY_433"; }
    else if (r == "MY919") { r = "MY_919"; }
    else if (r == "SG923") { r = "SG_923"; }
    else if (r == "PH433") { r = "PH_433"; }
    else if (r == "PH868") { r = "PH_868"; }
    else if (r == "PH915") { r = "PH_915"; }
    else if (r == "KZ433") { r = "KZ_433"; }
    else if (r == "KZ863") { r = "KZ_863"; }
    else if (r == "NP865") { r = "NP_865"; }
    else if (r == "BR902") { r = "BR_902"; }
    else if (r == "ANZ433") { r = "ANZ_433"; }
    else if (r == "LORA24") { r = "LORA_24"; }
    else { r = normalizeToken(regionValue); }

    for (int i = 0; i < kRegionBandsCount; ++i) {
        if (r == kRegionBands[i].name) {
            return &kRegionBands[i];
        }
    }

    return nullptr;
}

bool parseCommand(const QString& command, CommandConfig& cfg, QString& error)
{
    if (!Packet::isCommand(command)) {
        error = "command must start with MESH:";
        return false;
    }

    cfg.header.to = kBroadcastNode;
    cfg.header.from = 0;
    cfg.header.id = QRandomGenerator::global()->generate();
    cfg.header.channel = 0;
    cfg.header.nextHop = 0;
    cfg.header.relayNode = 0;
    cfg.data.hasPortnum = true;
    cfg.data.portnum = 1; // TEXT_MESSAGE_APP
    cfg.data.payload.clear();
    cfg.encrypt = true;
    cfg.key = expandSimpleKey(1);
    cfg.keyLabel = "default";
    cfg.channelName = "LongFast";
    cfg.presetName = "LONG_FAST";
    bool encryptAuto = true;

    uint8_t hopLimit = 3;
    uint8_t hopStart = 3;
    bool wantAck = false;
    bool viaMqtt = false;

    QString body = command.mid(5).trimmed();
    QStringList parts = body.split(';', Qt::SkipEmptyParts);
    QStringList freeText;

    for (QString part : parts)
    {
        part = part.trimmed();
        if (part.isEmpty()) {
            continue;
        }

        const int sep = part.indexOf('=');
        if (sep <= 0)
        {
            freeText.push_back(trimQuotes(part));
            continue;
        }

        const QString key = part.left(sep).trimmed().toLower();
        const QString value = trimQuotes(part.mid(sep + 1));

        uint64_t u = 0;

        if (key == "to")
        {
            if (!parseUInt(value, u)) {
                error = "invalid to";
                return false;
            }
            cfg.header.to = static_cast<uint32_t>(u);
        }
        else if (key == "from")
        {
            if (!parseUInt(value, u)) {
                error = "invalid from";
                return false;
            }
            cfg.header.from = static_cast<uint32_t>(u);
        }
        else if (key == "id")
        {
            if (!parseUInt(value, u)) {
                error = "invalid id";
                return false;
            }
            cfg.header.id = static_cast<uint32_t>(u);
        }
        else if (key == "port" || key == "portnum")
        {
            uint32_t p = 0;
            if (!parsePortValue(value, p)) {
                error = "invalid port/portnum";
                return false;
            }
            cfg.data.portnum = p;
            cfg.data.hasPortnum = true;
        }
        else if (key == "text")
        {
            cfg.data.payload = value.toUtf8();
        }
        else if (key == "payload_hex")
        {
            QByteArray p;
            if (!parseHexBytes(value, p)) {
                error = "invalid payload_hex";
                return false;
            }
            cfg.data.payload = p;
        }
        else if (key == "payload_b64" || key == "payload_base64")
        {
            QByteArray p = QByteArray::fromBase64(value.toLatin1());
            if (p.isEmpty() && !value.isEmpty()) {
                error = "invalid payload_base64";
                return false;
            }
            cfg.data.payload = p;
        }
        else if (key == "want_response")
        {
            bool b = false;
            if (!parseBool(value, b)) {
                error = "invalid want_response";
                return false;
            }
            cfg.data.wantResponse = b;
        }
        else if (key == "dest")
        {
            if (!parseUInt(value, u)) {
                error = "invalid dest";
                return false;
            }
            cfg.data.hasDest = true;
            cfg.data.dest = static_cast<uint32_t>(u);
        }
        else if (key == "source")
        {
            if (!parseUInt(value, u)) {
                error = "invalid source";
                return false;
            }
            cfg.data.hasSource = true;
            cfg.data.source = static_cast<uint32_t>(u);
        }
        else if (key == "request_id")
        {
            if (!parseUInt(value, u)) {
                error = "invalid request_id";
                return false;
            }
            cfg.data.hasRequestId = true;
            cfg.data.requestId = static_cast<uint32_t>(u);
        }
        else if (key == "reply_id")
        {
            if (!parseUInt(value, u)) {
                error = "invalid reply_id";
                return false;
            }
            cfg.data.hasReplyId = true;
            cfg.data.replyId = static_cast<uint32_t>(u);
        }
        else if (key == "emoji")
        {
            if (!parseUInt(value, u)) {
                error = "invalid emoji";
                return false;
            }
            cfg.data.hasEmoji = true;
            cfg.data.emoji = static_cast<uint32_t>(u);
        }
        else if (key == "bitfield")
        {
            if (!parseUInt(value, u)) {
                error = "invalid bitfield";
                return false;
            }
            cfg.data.hasBitfield = true;
            cfg.data.bitfield = static_cast<uint32_t>(u);
        }
        else if (key == "hop_limit")
        {
            if (!parseUInt(value, u) || u > 7) {
                error = "invalid hop_limit";
                return false;
            }
            hopLimit = static_cast<uint8_t>(u);
        }
        else if (key == "hop_start")
        {
            if (!parseUInt(value, u) || u > 7) {
                error = "invalid hop_start";
                return false;
            }
            hopStart = static_cast<uint8_t>(u);
        }
        else if (key == "want_ack")
        {
            if (!parseBool(value, wantAck)) {
                error = "invalid want_ack";
                return false;
            }
        }
        else if (key == "via_mqtt")
        {
            if (!parseBool(value, viaMqtt)) {
                error = "invalid via_mqtt";
                return false;
            }
        }
        else if (key == "next_hop")
        {
            if (!parseUInt(value, u) || u > 255) {
                error = "invalid next_hop";
                return false;
            }
            cfg.header.nextHop = static_cast<uint8_t>(u);
        }
        else if (key == "relay_node")
        {
            if (!parseUInt(value, u) || u > 255) {
                error = "invalid relay_node";
                return false;
            }
            cfg.header.relayNode = static_cast<uint8_t>(u);
        }
        else if (key == "channel_hash" || key == "channel")
        {
            if (!parseUInt(value, u) || u > 255) {
                error = "invalid channel_hash";
                return false;
            }
            cfg.header.channel = static_cast<uint8_t>(u);
            cfg.channelName.clear();
        }
        else if (key == "channel_name")
        {
            cfg.channelName = value;
        }
        else if (key == "preset" || key == "modem_preset")
        {
            QString presetName;
            if (!parsePresetName(value, presetName)) {
                error = "invalid preset/modem_preset";
                return false;
            }

            cfg.presetName = presetName;
            presetToChannelName(cfg.presetName, cfg.channelName);
        }
        else if (key == "region" || key == "region_code")
        {
            const RegionBand* band = findRegionBand(value);
            if (!band) {
                error = "invalid region";
                return false;
            }

            cfg.hasRegion = true;
            cfg.regionName = band->name;
        }
        else if (key == "channel_num" || key == "slot")
        {
            if (!parseUInt(value, u) || u < 1 || u > 10000) {
                error = "invalid channel_num";
                return false;
            }
            cfg.hasChannelNum = true;
            cfg.channelNum = static_cast<uint32_t>(u);
        }
        else if (key == "frequency" || key == "freq" || key == "override_frequency" || key == "frequency_mhz" || key == "freq_mhz")
        {
            double f = 0.0;
            if (!parseDouble(value, f) || f <= 0.0) {
                error = "invalid frequency";
                return false;
            }

            if (f > 1000000.0) {
                cfg.overrideFrequencyMHz = f / 1000000.0;
            } else {
                cfg.overrideFrequencyMHz = f;
            }

            cfg.hasOverrideFrequencyMHz = true;
        }
        else if (key == "freq_hz" || key == "frequency_hz")
        {
            if (!parseUInt(value, u) || u < 1000000ULL) {
                error = "invalid frequency_hz";
                return false;
            }
            cfg.overrideFrequencyMHz = static_cast<double>(u) / 1000000.0;
            cfg.hasOverrideFrequencyMHz = true;
        }
        else if (key == "frequency_offset" || key == "freq_offset")
        {
            double off = 0.0;
            if (!parseDouble(value, off)) {
                error = "invalid frequency_offset";
                return false;
            }

            // Meshtastic uses MHz for frequency offset. We also accept Hz-like values.
            if (std::fabs(off) > 100000.0) {
                off /= 1000000.0;
            }

            cfg.frequencyOffsetMHz = off;
            cfg.hasFrequencyOffsetMHz = true;
        }
        else if (key == "frequency_offset_hz" || key == "freq_offset_hz")
        {
            double offHz = 0.0;
            if (!parseDouble(value, offHz)) {
                error = "invalid frequency_offset_hz";
                return false;
            }
            cfg.frequencyOffsetMHz = offHz / 1000000.0;
            cfg.hasFrequencyOffsetMHz = true;
        }
        else if (key == "key" || key == "psk")
        {
            QByteArray parsedKey;
            QString keyLabel;
            if (!parseKeySpec(value, parsedKey, keyLabel)) {
                error = "invalid key/psk";
                return false;
            }
            cfg.key = parsedKey;
            cfg.keyLabel = keyLabel;

            if (encryptAuto) {
                cfg.encrypt = !cfg.key.isEmpty();
            }
        }
        else if (key == "encrypt")
        {
            const QString lower = value.toLower();
            if (lower == "auto") {
                encryptAuto = true;
                cfg.encrypt = !cfg.key.isEmpty();
            }
            else
            {
                bool b = false;
                if (!parseBool(value, b)) {
                    error = "invalid encrypt";
                    return false;
                }
                encryptAuto = false;
                cfg.encrypt = b;
            }
        }
        else
        {
            error = QString("unknown key '%1'").arg(key);
            return false;
        }
    }

    if (cfg.data.payload.isEmpty() && !freeText.isEmpty()) {
        cfg.data.payload = freeText.join(';').toUtf8();
    }

    if (cfg.header.channel == 0 && !cfg.channelName.isEmpty()) {
        cfg.header.channel = generateChannelHash(cfg.channelName, cfg.key);
    }

    if (encryptAuto) {
        cfg.encrypt = !cfg.key.isEmpty();
    }

    if (cfg.encrypt && cfg.key.isEmpty()) {
        error = "encrypt=true but key resolves to none";
        return false;
    }

    cfg.header.flags = static_cast<uint8_t>(hopLimit & 0x07);
    if (wantAck) {
        cfg.header.flags |= kFlagWantAckMask;
    }
    if (viaMqtt) {
        cfg.header.flags |= kFlagViaMqttMask;
    }
    cfg.header.flags |= static_cast<uint8_t>((hopStart & 0x07) << 5);

    return true;
}

// Forward declaration: summarizePortPayload is defined after the per-port parsers below.
QString summarizePortPayload(const DataFields& d);

QString summarizeHeader(const Header& h)
{
    const int hopLimit = h.flags & kFlagHopLimitMask;
    const int hopStart = (h.flags & kFlagHopStartMask) >> 5;
    const bool wantAck = (h.flags & kFlagWantAckMask) != 0;
    const bool viaMqtt = (h.flags & kFlagViaMqttMask) != 0;

    return QString("to=%1 from=%2 id=0x%3 ch=0x%4 hop=%5/%6 ack=%7 mqtt=%8 next=%9 relay=%10")
        .arg(formatNode(h.to))
        .arg(formatNode(h.from))
        .arg(h.id, 8, 16, QChar('0'))
        .arg(h.channel, 2, 16, QChar('0'))
        .arg(hopLimit)
        .arg(hopStart)
        .arg(wantAck ? 1 : 0)
        .arg(viaMqtt ? 1 : 0)
        .arg(h.nextHop)
        .arg(h.relayNode);
}

QString summarizeData(const DataFields& d)
{
    const QString portName = portToName(d.portnum);
    QString s = QString("port=%1(%2)").arg(portName).arg(d.portnum);

    if (d.wantResponse) {
        s += " want_response=1";
    }

    if (d.hasDest) {
        s += QString(" dest=%1").arg(formatNode(d.dest));
    }

    if (d.hasSource) {
        s += QString(" source=%1").arg(formatNode(d.source));
    }

    if (d.hasRequestId) {
        s += QString(" request_id=0x%1").arg(d.requestId, 8, 16, QChar('0'));
    }

    if (d.hasReplyId) {
        s += QString(" reply_id=0x%1").arg(d.replyId, 8, 16, QChar('0'));
    }

    if (d.hasEmoji) {
        s += QString(" emoji=%1").arg(d.emoji);
    }

    if (d.hasBitfield) {
        s += QString(" bitfield=0x%1").arg(d.bitfield, 0, 16);
    }

    s += summarizePortPayload(d);

    return s;
}

void addDecodeField(DecodeResult& result, const QString& path, const QString& value)
{
    DecodeResult::Field f;
    f.path = path;
    f.value = value;
    result.fields.append(f);
}

void addDecodeField(DecodeResult& result, const QString& path, uint32_t value)
{
    addDecodeField(result, path, QString::number(value));
}

void addDecodeField(DecodeResult& result, const QString& path, bool value)
{
    addDecodeField(result, path, QString(value ? "true" : "false"));
}

void appendHeaderDecodeFields(const Header& h, DecodeResult& result)
{
    const int hopLimit = h.flags & kFlagHopLimitMask;
    const int hopStart = (h.flags & kFlagHopStartMask) >> 5;
    const bool wantAck = (h.flags & kFlagWantAckMask) != 0;
    const bool viaMqtt = (h.flags & kFlagViaMqttMask) != 0;

    addDecodeField(result, "header.to", formatNode(h.to));
    addDecodeField(result, "header.from", formatNode(h.from));
    addDecodeField(result, "header.id", QString("0x%1").arg(h.id, 8, 16, QChar('0')));
    addDecodeField(result, "header.channel_hash", QString("0x%1").arg(h.channel, 2, 16, QChar('0')));
    addDecodeField(result, "header.hop_limit", QString::number(hopLimit));
    addDecodeField(result, "header.hop_start", QString::number(hopStart));
    addDecodeField(result, "header.want_ack", wantAck);
    addDecodeField(result, "header.via_mqtt", viaMqtt);
    addDecodeField(result, "header.next_hop", QString::number(h.nextHop));
    addDecodeField(result, "header.relay_node", QString::number(h.relayNode));
}

// =============================================================================
// Per-port protobuf payload decoders
// Strategy: extend the existing hand-parser with one sub-parser per port type.
// Each port section has three parts:
//   1. A plain data struct that holds the decoded fields.
//   2. A parse*Payload() function that populates the struct from raw bytes.
//   3. An append*DecodeFields() function that pushes the fields into DecodeResult.
// =============================================================================

// --- Shared decode helpers ---

/** Zigzag-decode a protobuf sint32 value. */
int32_t zigzagDecode32(uint64_t n)
{
    return static_cast<int32_t>((n >> 1) ^ static_cast<uint64_t>(-(static_cast<int64_t>(n & 1))));
}

/** Reinterpret the 4 bytes of a protobuf fixed32 field as an IEEE 754 float. */
float fixed32ToFloat(uint32_t v)
{
    float f;
    memcpy(&f, &v, 4);
    return f;
}

double fixed64ToDouble(uint64_t v)
{
    double d;
    memcpy(&d, &v, 8);
    return d;
}

bool decodeCoordinateFromFixed32(uint32_t raw, double& coordinate)
{
    const auto fixedValue = static_cast<int32_t>(raw);
    const double scaled = static_cast<double>(fixedValue) / 1e7;

    if (std::isfinite(scaled) && std::fabs(scaled) <= 180.0)
    {
        coordinate = scaled;
        return true;
    }

    const auto floatValue = static_cast<double>(fixed32ToFloat(raw));
    if (std::isfinite(floatValue) && std::fabs(floatValue) <= 180.0)
    {
        coordinate = floatValue;
        return true;
    }

    return false;
}

// =============================================================================
// POSITION_APP (portnum = 3)
// Proto: meshtastic/mesh.proto  message Position
// =============================================================================

struct PositionFields
{
    bool hasLatitude = false;
    double latitude = 0.0;

    bool hasLongitude = false;
    double longitude = 0.0;

    bool hasLatitudeI = false;
    int32_t latitudeI = 0;        // degrees * 1e7  (sint32 field 1)

    bool hasLongitudeI = false;
    int32_t longitudeI = 0;       // degrees * 1e7  (sint32 field 2)

    bool hasAltitude = false;
    int32_t altitude = 0;         // metres above MSL  (int32 field 3)

    bool hasTime = false;
    uint32_t time = 0;            // Unix timestamp, deprecated (uint32 field 4)

    bool hasPdop = false;
    uint32_t pdop = 0;            // position DOP * 100  (uint32 field 8)

    bool hasGroundSpeed = false;
    uint32_t groundSpeed = 0;     // km/h  (uint32 field 12)

    bool hasGroundTrack = false;
    uint32_t groundTrack = 0;     // centi-degrees  (uint32 field 13)

    bool hasSatsInView = false;
    uint32_t satsInView = 0;      // (int32 field 16)

    bool hasTimestamp = false;
    uint32_t timestamp = 0;       // Unix timestamp, preferred (uint32 field 32)
};

bool parsePositionPayload(const QByteArray& bytes, PositionFields& p)
{
    int pos = 0;

    while (pos < bytes.size())
    {
        uint64_t rawTag = 0;
        if (!readVarint(bytes, pos, rawTag)) {
            return false;
        }

        const auto field = static_cast<uint32_t>(rawTag >> 3);
        const auto wire  = static_cast<uint32_t>(rawTag & 0x7);

        switch (field)
        {
        case 1: {  // latitude_i: sint32
            if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                p.latitudeI    = zigzagDecode32(v);
                p.hasLatitudeI = true;
                p.latitude = p.latitudeI / 1e7;
                p.hasLatitude = true;
            }
            else if (wire == 5)
            {
                uint32_t v = 0;
                if (!readFixed32(bytes, pos, v)) { return false; }
                double latitude = 0.0;
                if (decodeCoordinateFromFixed32(v, latitude)) {
                    p.latitude = latitude;
                    p.hasLatitude = true;
                    p.latitudeI = static_cast<int32_t>(std::lround(latitude * 1e7));
                    p.hasLatitudeI = true;
                }
            }
            else if (wire == 1)
            {
                uint64_t v = 0;
                if (!readFixed64(bytes, pos, v)) { return false; }
                p.latitude = fixed64ToDouble(v);
                p.hasLatitude = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 2: {  // longitude_i: sint32
            if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                p.longitudeI    = zigzagDecode32(v);
                p.hasLongitudeI = true;
                p.longitude = p.longitudeI / 1e7;
                p.hasLongitude = true;
            }
            else if (wire == 5)
            {
                uint32_t v = 0;
                if (!readFixed32(bytes, pos, v)) { return false; }
                double longitude = 0.0;
                if (decodeCoordinateFromFixed32(v, longitude)) {
                    p.longitude = longitude;
                    p.hasLongitude = true;
                    p.longitudeI = static_cast<int32_t>(std::lround(longitude * 1e7));
                    p.hasLongitudeI = true;
                }
            }
            else if (wire == 1)
            {
                uint64_t v = 0;
                if (!readFixed64(bytes, pos, v)) { return false; }
                p.longitude = fixed64ToDouble(v);
                p.hasLongitude = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 3: {  // altitude: int32
            if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                p.altitude    = static_cast<int32_t>(v);
                p.hasAltitude = true;
            }
            else if (wire == 5)
            {
                uint32_t v = 0;
                if (!readFixed32(bytes, pos, v)) { return false; }
                p.altitude = static_cast<int32_t>(std::lround(static_cast<double>(fixed32ToFloat(v))));
                p.hasAltitude = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 4: {  // time: uint32 (deprecated, still widely used)
            if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                p.time    = static_cast<uint32_t>(v);
                p.hasTime = true;
            }
            else if (wire == 5)
            {
                uint32_t v = 0;
                if (!readFixed32(bytes, pos, v)) { return false; }
                p.time = v;
                p.hasTime = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 8: {  // PDOP: uint32
            if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                p.pdop    = static_cast<uint32_t>(v);
                p.hasPdop = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 12: {  // ground_speed: uint32 (km/h)
            if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                p.groundSpeed    = static_cast<uint32_t>(v);
                p.hasGroundSpeed = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 13: {  // ground_track: uint32 (centi-degrees)
            if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                p.groundTrack    = static_cast<uint32_t>(v);
                p.hasGroundTrack = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 16: {  // sats_in_view: int32
            if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                p.satsInView    = static_cast<uint32_t>(v);
                p.hasSatsInView = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 32: {  // timestamp: uint32 (preferred over field 4)
            if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                p.timestamp    = static_cast<uint32_t>(v);
                p.hasTimestamp = true;
            }
            else if (wire == 5)
            {
                uint32_t v = 0;
                if (!readFixed32(bytes, pos, v)) { return false; }
                p.timestamp = v;
                p.hasTimestamp = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        default:
            if (!skipField(bytes, pos, wire)) { return false; }
            break;
        }
    }

    if (!p.hasLatitude && p.hasLatitudeI) {
        p.latitude = p.latitudeI / 1e7;
        p.hasLatitude = true;
    }
    if (!p.hasLongitude && p.hasLongitudeI) {
        p.longitude = p.longitudeI / 1e7;
        p.hasLongitude = true;
    }

    return p.hasLatitude || p.hasLongitude || p.hasAltitude || p.hasTime || p.hasTimestamp;
}

void appendPositionDecodeFields(const PositionFields& p, DecodeResult& result)
{
    if (p.hasLatitude) {
        addDecodeField(result, "position.latitude",
                       QString::number(p.latitude, 'f', 7));
    }
    if (p.hasLongitude) {
        addDecodeField(result, "position.longitude",
                       QString::number(p.longitude, 'f', 7));
    }
    if (p.hasAltitude) {
        addDecodeField(result, "position.altitude_m", QString::number(p.altitude));
    }

    // Prefer field 32 (timestamp) over deprecated field 4 (time).
    const uint32_t ts = p.hasTimestamp ? p.timestamp : (p.hasTime ? p.time : 0u);
    if (ts != 0) {
        addDecodeField(result, "position.timestamp", QString::number(ts));
        const QDateTime dt = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(ts), Qt::UTC);
        addDecodeField(result, "position.datetime_utc", dt.toString(Qt::ISODate));
    }

    if (p.hasGroundSpeed) {
        addDecodeField(result, "position.ground_speed_kmh", QString::number(p.groundSpeed));
    }
    if (p.hasGroundTrack) {
        addDecodeField(result, "position.ground_track_deg",
                       QString::number(p.groundTrack / 100.0, 'f', 2));
    }
    if (p.hasSatsInView) {
        addDecodeField(result, "position.sats_in_view", QString::number(p.satsInView));
    }
    if (p.hasPdop) {
        addDecodeField(result, "position.pdop", QString::number(p.pdop));
    }
}

// =============================================================================
// NODEINFO_APP (portnum = 4)
// Proto: meshtastic/mesh.proto  message User
// =============================================================================

struct UserFields
{
    bool    hasId        = false;
    QString id;                    // node ID string e.g. "!aabbccdd"  (string field 1)

    bool    hasLongName  = false;
    QString longName;              // human-readable name  (string field 2)

    bool    hasShortName = false;
    QString shortName;             // 2-4 char callsign  (string field 3)

    bool       hasMacaddr = false;
    QByteArray macaddr;            // 6-byte MAC address  (bytes field 4)

    bool     hasHwModel  = false;
    uint32_t hwModel     = 0;     // HardwareModel enum  (uint32 field 5)

    bool hasIsLicensed   = false;
    bool isLicensed      = false; // licensed amateur radio operator  (bool field 7)
};

bool parseUserPayload(const QByteArray& bytes, UserFields& u)
{
    int pos = 0;

    while (pos < bytes.size())
    {
        uint64_t rawTag = 0;
        if (!readVarint(bytes, pos, rawTag)) { return false; }

        const auto field = static_cast<uint32_t>(rawTag >> 3);
        const auto wire  = static_cast<uint32_t>(rawTag & 0x7);

        switch (field)
        {
        case 1: {  // id: string
            if (wire == 2)
            {
                uint64_t len = 0;
                if (!readVarint(bytes, pos, len)) { return false; }
                if (len > static_cast<uint64_t>(bytes.size() - pos)) { return false; }
                u.id    = QString::fromUtf8(bytes.constData() + pos, static_cast<int>(len));
                u.hasId = true;
                pos += static_cast<int>(len);
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 2: {  // long_name: string
            if (wire == 2)
            {
                uint64_t len = 0;
                if (!readVarint(bytes, pos, len)) { return false; }
                if (len > static_cast<uint64_t>(bytes.size() - pos)) { return false; }
                u.longName    = QString::fromUtf8(bytes.constData() + pos, static_cast<int>(len));
                u.hasLongName = true;
                pos += static_cast<int>(len);
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 3: {  // short_name: string
            if (wire == 2)
            {
                uint64_t len = 0;
                if (!readVarint(bytes, pos, len)) { return false; }
                if (len > static_cast<uint64_t>(bytes.size() - pos)) { return false; }
                u.shortName    = QString::fromUtf8(bytes.constData() + pos, static_cast<int>(len));
                u.hasShortName = true;
                pos += static_cast<int>(len);
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 4: {  // macaddr: bytes
            if (wire == 2)
            {
                uint64_t len = 0;
                if (!readVarint(bytes, pos, len)) { return false; }
                if (len > static_cast<uint64_t>(bytes.size() - pos)) { return false; }
                u.macaddr    = bytes.mid(pos, static_cast<int>(len));
                u.hasMacaddr = true;
                pos += static_cast<int>(len);
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 5: {  // hw_model: HardwareModel (enum → uint32)
            if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                u.hwModel    = static_cast<uint32_t>(v);
                u.hasHwModel = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 6:  // is_licensed: bool (current Meshtastic)
        case 7: { // is_licensed: bool (legacy compatibility)
            if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                u.isLicensed    = (v != 0);
                u.hasIsLicensed = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        default:
            if (!skipField(bytes, pos, wire)) { return false; }
            break;
        }
    }

    return u.hasId || u.hasLongName || u.hasShortName || u.hasMacaddr || u.hasHwModel || u.hasIsLicensed;
}

void appendUserDecodeFields(const UserFields& u, DecodeResult& result)
{
    if (u.hasId)        { addDecodeField(result, "nodeinfo.id",       u.id); }
    if (u.hasLongName)  { addDecodeField(result, "nodeinfo.long_name",  u.longName); }
    if (u.hasShortName) { addDecodeField(result, "nodeinfo.short_name", u.shortName); }
    if (u.hasMacaddr)   { addDecodeField(result, "nodeinfo.macaddr",    QString(u.macaddr.toHex(':'))); }
    if (u.hasHwModel)   { addDecodeField(result, "nodeinfo.hw_model",   u.hwModel); }
    if (u.hasIsLicensed){ addDecodeField(result, "nodeinfo.is_licensed", u.isLicensed); }
}

// =============================================================================
// TELEMETRY_APP (portnum = 67)
// Proto: meshtastic/telemetry.proto  message Telemetry
//        which embeds DeviceMetrics (field 2) and EnvironmentMetrics (field 3)
// =============================================================================

struct DeviceMetrics
{
    bool  hasBatteryLevel       = false;
    float batteryLevel          = 0.0f;  // 0–100 %

    bool  hasVoltage            = false;
    float voltage               = 0.0f;  // V

    bool  hasChannelUtilization = false;
    float channelUtilization    = 0.0f;  // %

    bool  hasAirUtilTx          = false;
    float airUtilTx             = 0.0f;  // %

    bool     hasUptimeSeconds   = false;
    uint32_t uptimeSeconds      = 0;
};

struct EnvironmentMetrics
{
    bool  hasTemperature         = false;
    float temperature            = 0.0f;  // °C

    bool  hasRelativeHumidity    = false;
    float relativeHumidity       = 0.0f;  // %

    bool  hasBarometricPressure  = false;
    float barometricPressure     = 0.0f;  // hPa

    bool  hasGasResistance       = false;
    float gasResistance          = 0.0f;  // MOhm

    bool  hasIaq                 = false;
    float iaq                    = 0.0f;  // indoor air quality index
};

struct TelemetryFields
{
    bool     hasTime               = false;
    uint32_t time                  = 0;

    bool           hasDeviceMetrics      = false;
    DeviceMetrics  deviceMetrics;

    bool                hasEnvironmentMetrics = false;
    EnvironmentMetrics  environmentMetrics;
};

bool parseDeviceMetrics(const QByteArray& bytes, DeviceMetrics& dm)
{
    int pos = 0;

    while (pos < bytes.size())
    {
        uint64_t rawTag = 0;
        if (!readVarint(bytes, pos, rawTag)) { return false; }

        const auto field = static_cast<uint32_t>(rawTag >> 3);
        const auto wire  = static_cast<uint32_t>(rawTag & 0x7);

        switch (field)
        {
        case 1: {  // battery_level: float
            if (wire == 5)
            {
                uint32_t v = 0;
                if (!readFixed32(bytes, pos, v)) { return false; }
                dm.batteryLevel = fixed32ToFloat(v);
                dm.hasBatteryLevel = true;
            }
            else if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                dm.batteryLevel = static_cast<float>(v);
                dm.hasBatteryLevel = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 2: {  // voltage: float
            if (wire == 5)
            {
                uint32_t v = 0;
                if (!readFixed32(bytes, pos, v)) { return false; }
                dm.voltage = fixed32ToFloat(v);
                dm.hasVoltage = true;
            }
            else if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                const auto raw = static_cast<double>(v);
                dm.voltage = static_cast<float>((raw > 1000.0) ? (raw / 1000.0) : raw);
                dm.hasVoltage = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 3: {  // channel_utilization: float
            if (wire == 5)
            {
                uint32_t v = 0;
                if (!readFixed32(bytes, pos, v)) { return false; }
                dm.channelUtilization = fixed32ToFloat(v);
                dm.hasChannelUtilization = true;
            }
            else if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                dm.channelUtilization = static_cast<float>(v);
                dm.hasChannelUtilization = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 4: {  // air_util_tx: float
            if (wire == 5)
            {
                uint32_t v = 0;
                if (!readFixed32(bytes, pos, v)) { return false; }
                dm.airUtilTx = fixed32ToFloat(v);
                dm.hasAirUtilTx = true;
            }
            else if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                dm.airUtilTx = static_cast<float>(v);
                dm.hasAirUtilTx = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 5: {  // uptime_seconds: uint32
            if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                dm.uptimeSeconds = static_cast<uint32_t>(v);
                dm.hasUptimeSeconds = true;
            }
            else if (wire == 5)
            {
                uint32_t v = 0;
                if (!readFixed32(bytes, pos, v)) { return false; }
                dm.uptimeSeconds = v;
                dm.hasUptimeSeconds = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        default:
            if (!skipField(bytes, pos, wire)) { return false; }
            break;
        }
    }

    return true;
}

bool parseEnvironmentMetrics(const QByteArray& bytes, EnvironmentMetrics& em)
{
    int pos = 0;

    while (pos < bytes.size())
    {
        uint64_t rawTag = 0;
        if (!readVarint(bytes, pos, rawTag)) { return false; }

        const auto field = static_cast<uint32_t>(rawTag >> 3);
        const auto wire  = static_cast<uint32_t>(rawTag & 0x7);

        switch (field)
        {
        case 1: {  // temperature: float (°C)
            if (wire == 5)
            {
                uint32_t v = 0;
                if (!readFixed32(bytes, pos, v)) { return false; }
                em.temperature = fixed32ToFloat(v);
                em.hasTemperature = true;
            }
            else if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                em.temperature = static_cast<float>(static_cast<int32_t>(v));
                em.hasTemperature = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 2: {  // relative_humidity: float (%)
            if (wire == 5)
            {
                uint32_t v = 0;
                if (!readFixed32(bytes, pos, v)) { return false; }
                em.relativeHumidity = fixed32ToFloat(v);
                em.hasRelativeHumidity = true;
            }
            else if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                em.relativeHumidity = static_cast<float>(v);
                em.hasRelativeHumidity = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 3: {  // barometric_pressure: float (hPa)
            if (wire == 5)
            {
                uint32_t v = 0;
                if (!readFixed32(bytes, pos, v)) { return false; }
                em.barometricPressure = fixed32ToFloat(v);
                em.hasBarometricPressure = true;
            }
            else if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                em.barometricPressure = static_cast<float>(v);
                em.hasBarometricPressure = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 4: {  // gas_resistance: float (MOhm)
            if (wire != 5) { return false; }
            uint32_t v = 0;
            if (!readFixed32(bytes, pos, v)) { return false; }
            em.gasResistance    = fixed32ToFloat(v);
            em.hasGasResistance = true;
            break;
        }
        case 11: {  // iaq: float (indoor air quality, newer field)
            if (wire != 5) { return false; }
            uint32_t v = 0;
            if (!readFixed32(bytes, pos, v)) { return false; }
            em.iaq    = fixed32ToFloat(v);
            em.hasIaq = true;
            break;
        }
        default:
            if (!skipField(bytes, pos, wire)) { return false; }
            break;
        }
    }

    return true;
}

bool parseTelemetryPayload(const QByteArray& bytes, TelemetryFields& t)
{
    int pos = 0;

    while (pos < bytes.size())
    {
        uint64_t rawTag = 0;
        if (!readVarint(bytes, pos, rawTag)) { return false; }

        const auto field = static_cast<uint32_t>(rawTag >> 3);
        const auto wire  = static_cast<uint32_t>(rawTag & 0x7);

        switch (field)
        {
        case 1: {  // time: uint32
            if (wire == 0)
            {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                t.time = static_cast<uint32_t>(v);
                t.hasTime = true;
            }
            else if (wire == 5)
            {
                uint32_t v = 0;
                if (!readFixed32(bytes, pos, v)) { return false; }
                t.time = v;
                t.hasTime = true;
            }
            else
            {
                if (!skipField(bytes, pos, wire)) { return false; }
            }
            break;
        }
        case 2: {  // device_metrics: DeviceMetrics (embedded message)
            if (wire != 2) { return false; }
            uint64_t len = 0;
            if (!readVarint(bytes, pos, len)) { return false; }
            if (len > static_cast<uint64_t>(bytes.size() - pos)) { return false; }
            if (!parseDeviceMetrics(bytes.mid(pos, static_cast<int>(len)), t.deviceMetrics)) { return false; }
            t.hasDeviceMetrics = true;
            pos += static_cast<int>(len);
            break;
        }
        case 3: {  // environment_metrics: EnvironmentMetrics (embedded message)
            if (wire != 2) { return false; }
            uint64_t len = 0;
            if (!readVarint(bytes, pos, len)) { return false; }
            if (len > static_cast<uint64_t>(bytes.size() - pos)) { return false; }
            if (!parseEnvironmentMetrics(bytes.mid(pos, static_cast<int>(len)), t.environmentMetrics)) { return false; }
            t.hasEnvironmentMetrics = true;
            pos += static_cast<int>(len);
            break;
        }
        default:
            if (!skipField(bytes, pos, wire)) { return false; }
            break;
        }
    }

    return t.hasTime || t.hasDeviceMetrics || t.hasEnvironmentMetrics;
}

void appendTelemetryDecodeFields(const TelemetryFields& t, DecodeResult& result)
{
    if (t.hasTime)
    {
        addDecodeField(result, "telemetry.time", QString::number(t.time));
        const QDateTime dt = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t.time), Qt::UTC);
        addDecodeField(result, "telemetry.datetime_utc", dt.toString(Qt::ISODate));
    }

    if (t.hasDeviceMetrics)
    {
        const DeviceMetrics& dm = t.deviceMetrics;
        if (dm.hasBatteryLevel) {
            addDecodeField(result, "telemetry.device.battery_level_pct",
                           QString::number(static_cast<double>(dm.batteryLevel), 'f', 1));
        }
        if (dm.hasVoltage) {
            addDecodeField(result, "telemetry.device.voltage_v",
                           QString::number(static_cast<double>(dm.voltage), 'f', 3));
        }
        if (dm.hasChannelUtilization) {
            addDecodeField(result, "telemetry.device.channel_util_pct",
                           QString::number(static_cast<double>(dm.channelUtilization), 'f', 2));
        }
        if (dm.hasAirUtilTx) {
            addDecodeField(result, "telemetry.device.air_util_tx_pct",
                           QString::number(static_cast<double>(dm.airUtilTx), 'f', 2));
        }
        if (dm.hasUptimeSeconds) {
            addDecodeField(result, "telemetry.device.uptime_s",
                           QString::number(dm.uptimeSeconds));
        }
    }

    if (t.hasEnvironmentMetrics)
    {
        const EnvironmentMetrics& em = t.environmentMetrics;
        if (em.hasTemperature) {
            addDecodeField(result, "telemetry.env.temperature_c",
                           QString::number(static_cast<double>(em.temperature), 'f', 2));
        }
        if (em.hasRelativeHumidity) {
            addDecodeField(result, "telemetry.env.humidity_pct",
                           QString::number(static_cast<double>(em.relativeHumidity), 'f', 1));
        }
        if (em.hasBarometricPressure) {
            addDecodeField(result, "telemetry.env.pressure_hpa",
                           QString::number(static_cast<double>(em.barometricPressure), 'f', 2));
        }
        if (em.hasGasResistance) {
            addDecodeField(result, "telemetry.env.gas_resistance_moh",
                           QString::number(static_cast<double>(em.gasResistance), 'f', 3));
        }
        if (em.hasIaq) {
            addDecodeField(result, "telemetry.env.iaq",
                           QString::number(static_cast<double>(em.iaq), 'f', 1));
        }
    }
}

// =============================================================================
// TRACEROUTE_APP (portnum = 70)
// Proto: meshtastic/mesh.proto  message RouteDiscovery
// =============================================================================

struct RouteDiscoveryFields
{
    QVector<uint32_t> route;       // node IDs on forward path  (repeated fixed32 field 1)
    QVector<int32_t>  snrTowards;  // SNR values forward path in 0.25 dB steps (sint32 field 2)
    QVector<uint32_t> routeBack;   // node IDs on return path   (repeated fixed32 field 3)
    QVector<int32_t>  snrBack;     // SNR values return path in 0.25 dB steps  (sint32 field 4)
};

/** Parse a packed repeated fixed32 block (content only, length already consumed). */
void parsePackedFixed32(const QByteArray& bytes, QVector<uint32_t>& out)
{
    int pos = 0;
    while ((pos + 4) <= bytes.size())
    {
        out.append(readU32LE(bytes.constData() + pos));
        pos += 4;
    }
}

/** Parse a packed repeated sint32 block (content only, length already consumed). */
void parsePackedVarintSint32(const QByteArray& bytes, QVector<int32_t>& out)
{
    int pos = 0;
    while (pos < bytes.size())
    {
        uint64_t v = 0;
        if (!readVarint(bytes, pos, v)) { break; }
        out.append(zigzagDecode32(v));
    }
}

bool parseRouteDiscoveryPayload(const QByteArray& bytes, RouteDiscoveryFields& r)
{
    int pos = 0;

    while (pos < bytes.size())
    {
        uint64_t rawTag = 0;
        if (!readVarint(bytes, pos, rawTag)) { return false; }

        const auto field = static_cast<uint32_t>(rawTag >> 3);
        const auto wire  = static_cast<uint32_t>(rawTag & 0x7);

        switch (field)
        {
        case 1: {  // route: repeated fixed32  (packed wire=2 or individual wire=5)
            if (wire == 2) {
                uint64_t len = 0;
                if (!readVarint(bytes, pos, len)) { return false; }
                if (len > static_cast<uint64_t>(bytes.size() - pos)) { return false; }
                parsePackedFixed32(bytes.mid(pos, static_cast<int>(len)), r.route);
                pos += static_cast<int>(len);
            } else if (wire == 5) {
                uint32_t v = 0;
                if (!readFixed32(bytes, pos, v)) { return false; }
                r.route.append(v);
            } else {
                return false;
            }
            break;
        }
        case 2: {  // snr_towards: repeated sint32  (packed wire=2 or individual wire=0)
            if (wire == 2) {
                uint64_t len = 0;
                if (!readVarint(bytes, pos, len)) { return false; }
                if (len > static_cast<uint64_t>(bytes.size() - pos)) { return false; }
                parsePackedVarintSint32(bytes.mid(pos, static_cast<int>(len)), r.snrTowards);
                pos += static_cast<int>(len);
            } else if (wire == 0) {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                r.snrTowards.append(zigzagDecode32(v));
            } else {
                return false;
            }
            break;
        }
        case 3: {  // route_back: repeated fixed32
            if (wire == 2) {
                uint64_t len = 0;
                if (!readVarint(bytes, pos, len)) { return false; }
                if (len > static_cast<uint64_t>(bytes.size() - pos)) { return false; }
                parsePackedFixed32(bytes.mid(pos, static_cast<int>(len)), r.routeBack);
                pos += static_cast<int>(len);
            } else if (wire == 5) {
                uint32_t v = 0;
                if (!readFixed32(bytes, pos, v)) { return false; }
                r.routeBack.append(v);
            } else {
                return false;
            }
            break;
        }
        case 4: {  // snr_back: repeated sint32
            if (wire == 2) {
                uint64_t len = 0;
                if (!readVarint(bytes, pos, len)) { return false; }
                if (len > static_cast<uint64_t>(bytes.size() - pos)) { return false; }
                parsePackedVarintSint32(bytes.mid(pos, static_cast<int>(len)), r.snrBack);
                pos += static_cast<int>(len);
            } else if (wire == 0) {
                uint64_t v = 0;
                if (!readVarint(bytes, pos, v)) { return false; }
                r.snrBack.append(zigzagDecode32(v));
            } else {
                return false;
            }
            break;
        }
        default:
            if (!skipField(bytes, pos, wire)) { return false; }
            break;
        }
    }

    return !r.route.isEmpty() || !r.routeBack.isEmpty();
}

void appendRouteDiscoveryDecodeFields(const RouteDiscoveryFields& r, DecodeResult& result)
{
    addDecodeField(result, "traceroute.forward_hops", QString::number(r.route.size()));

    for (int i = 0; i < r.route.size(); ++i)
    {
        const QString prefix = QString("traceroute.route[%1]").arg(i);
        addDecodeField(result, prefix + ".node_id",
                       QString("!%1").arg(r.route[i], 8, 16, QChar('0')));
        if (i < r.snrTowards.size()) {
            addDecodeField(result, prefix + ".snr_towards_db",
                           QString::number(r.snrTowards[i] / 4.0, 'f', 2));
        }
    }

    if (!r.routeBack.isEmpty())
    {
        addDecodeField(result, "traceroute.back_hops", QString::number(r.routeBack.size()));
        for (int i = 0; i < r.routeBack.size(); ++i)
        {
            const QString prefix = QString("traceroute.route_back[%1]").arg(i);
            addDecodeField(result, prefix + ".node_id",
                           QString("!%1").arg(r.routeBack[i], 8, 16, QChar('0')));
            if (i < r.snrBack.size()) {
                addDecodeField(result, prefix + ".snr_back_db",
                               QString::number(r.snrBack[i] / 4.0, 'f', 2));
            }
        }
    }
}

QString summarizePortPayload(const DataFields& d)
{
    if (d.payload.isEmpty()) {
        return " payload=<empty>";
    }

    const auto appendPayloadHex = [&d]() {
        const int n = std::min<int>(32, static_cast<int>(d.payload.size()));
        QString text = QString(" payload_hex=%1").arg(QString(d.payload.left(n).toHex()));
        if (d.payload.size() > n) {
            text += "...";
        }
        return text;
    };

    const QString text = payloadToText(d.payload);

    switch (d.portnum)
    {
    case 1: {  // TEXT_MESSAGE_APP
        if (!text.isEmpty()) {
            return QString(" text=\"%1\"").arg(text);
        }
        return appendPayloadHex();
    }

    case 3: {  // POSITION_APP
        PositionFields p;
        if (!parsePositionPayload(d.payload, p)) {
            return appendPayloadHex();
        }

        QString out;
        if (p.hasLatitude && p.hasLongitude) {
            out += QString(" lat=%1 lon=%2")
                       .arg(p.latitude, 0, 'f', 5)
                       .arg(p.longitude, 0, 'f', 5);
        }
        if (p.hasAltitude) {
            out += QString(" alt=%1m").arg(p.altitude);
        }
        if (p.hasGroundSpeed) {
            out += QString(" spd=%1km/h").arg(p.groundSpeed);
        }
        if (p.hasSatsInView) {
            out += QString(" sats=%1").arg(p.satsInView);
        }

        return out.isEmpty() ? appendPayloadHex() : out;
    }

    case 4: {  // NODEINFO_APP
        UserFields u;
        if (!parseUserPayload(d.payload, u)) {
            return appendPayloadHex();
        }

        QString out;
        if (u.hasLongName) {
            out += QString(" name=\"%1\"").arg(u.longName);
        }
        if (u.hasShortName) {
            out += QString(" short=\"%1\"").arg(u.shortName);
        }
        if (u.hasId) {
            out += QString(" id=%1").arg(u.id);
        }
        if (u.hasHwModel) {
            out += QString(" hw=%1").arg(u.hwModel);
        }
        if (u.hasIsLicensed) {
            out += QString(" licensed=%1").arg(u.isLicensed ? 1 : 0);
        }
        if (u.hasMacaddr) {
            out += QString(" mac=%1").arg(QString(u.macaddr.left(8).toHex()));
            if (u.macaddr.size() > 8) {
                out += "...";
            }
        }

        return out.isEmpty() ? appendPayloadHex() : out;
    }

    case 67: {  // TELEMETRY_APP
        TelemetryFields t;
        if (!parseTelemetryPayload(d.payload, t))
        {
            DeviceMetrics dm;
            EnvironmentMetrics em;
            const bool hasDm = parseDeviceMetrics(d.payload, dm);
            const bool hasEm = parseEnvironmentMetrics(d.payload, em);

            if (!hasDm && !hasEm) {
                return appendPayloadHex();
            }

            t.hasDeviceMetrics = hasDm;
            t.deviceMetrics = dm;
            t.hasEnvironmentMetrics = hasEm;
            t.environmentMetrics = em;
        }

        QString out;
        if (t.hasDeviceMetrics)
        {
            const DeviceMetrics& dm = t.deviceMetrics;
            if (dm.hasBatteryLevel) {
                out += QString(" batt=%1%").arg(static_cast<double>(dm.batteryLevel), 0, 'f', 1);
            }
            if (dm.hasVoltage) {
                out += QString(" volt=%1V").arg(static_cast<double>(dm.voltage), 0, 'f', 2);
            }
        }
        if (t.hasEnvironmentMetrics)
        {
            const EnvironmentMetrics& em = t.environmentMetrics;
            if (em.hasTemperature) {
                out += QString(" temp=%1°C").arg(static_cast<double>(em.temperature), 0, 'f', 1);
            }
            if (em.hasRelativeHumidity) {
                out += QString(" hum=%1%").arg(static_cast<double>(em.relativeHumidity), 0, 'f', 0);
            }
            if (em.hasBarometricPressure) {
                out += QString(" pres=%1hPa").arg(static_cast<double>(em.barometricPressure), 0, 'f', 1);
            }
        }

        return out.isEmpty() ? appendPayloadHex() : out;
    }

    case 70: {  // TRACEROUTE_APP
        RouteDiscoveryFields r;
        if (!parseRouteDiscoveryPayload(d.payload, r)) {
            return appendPayloadHex();
        }

        QString out = QString(" fwd_hops=%1").arg(r.route.size());
        if (!r.routeBack.isEmpty()) {
            out += QString(" back_hops=%1").arg(r.routeBack.size());
        }

        return out;
    }

    default:
        if (!text.isEmpty()) {
            return QString(" text=\"%1\"").arg(text);
        }
        return appendPayloadHex();
    }
}

// =============================================================================
// appendDataDecodeFields — dispatches to per-port decoders for known types,
// falls back to generic text / hex for everything else.
// =============================================================================

void appendDataDecodeFields(const DataFields& d, DecodeResult& result)
{
    addDecodeField(result, "data.port_name", portToName(d.portnum));
    addDecodeField(result, "data.portnum", d.portnum);
    addDecodeField(result, "data.want_response", d.wantResponse);

    if (d.hasDest) {
        addDecodeField(result, "data.dest", formatNode(d.dest));
    }
    if (d.hasSource) {
        addDecodeField(result, "data.source", formatNode(d.source));
    }
    if (d.hasRequestId) {
        addDecodeField(result, "data.request_id", QString("0x%1").arg(d.requestId, 8, 16, QChar('0')));
    }
    if (d.hasReplyId) {
        addDecodeField(result, "data.reply_id", QString("0x%1").arg(d.replyId, 8, 16, QChar('0')));
    }
    if (d.hasEmoji) {
        addDecodeField(result, "data.emoji", QString::number(d.emoji));
    }
    if (d.hasBitfield) {
        addDecodeField(result, "data.bitfield", QString("0x%1").arg(d.bitfield, 0, 16));
    }

    addDecodeField(result, "data.payload_len", QString::number(d.payload.size()));

    if (d.payload.isEmpty()) {
        return;
    }

    switch (d.portnum)
    {
    case 1: {  // TEXT_MESSAGE_APP
        const QString text = payloadToText(d.payload);
        if (!text.isEmpty()) {
            addDecodeField(result, "data.text", text);
        } else {
            addDecodeField(result, "data.payload_hex", QString(d.payload.toHex()));
        }
        break;
    }
    case 3: {  // POSITION_APP
        PositionFields p;
        if (parsePositionPayload(d.payload, p)) {
            appendPositionDecodeFields(p, result);
        } else {
            addDecodeField(result, "data.payload_hex", QString(d.payload.toHex()));
        }
        break;
    }
    case 4: {  // NODEINFO_APP
        UserFields u;
        if (parseUserPayload(d.payload, u)) {
            appendUserDecodeFields(u, result);
        } else {
            addDecodeField(result, "data.payload_hex", QString(d.payload.toHex()));
        }
        break;
    }
    case 67: {  // TELEMETRY_APP
        TelemetryFields t;
        if (parseTelemetryPayload(d.payload, t)) {
            appendTelemetryDecodeFields(t, result);
        } else {
            DeviceMetrics dm;
            EnvironmentMetrics em;
            const bool hasDm = parseDeviceMetrics(d.payload, dm);
            const bool hasEm = parseEnvironmentMetrics(d.payload, em);

            if (hasDm || hasEm)
            {
                TelemetryFields direct;
                direct.hasDeviceMetrics = hasDm;
                direct.deviceMetrics = dm;
                direct.hasEnvironmentMetrics = hasEm;
                direct.environmentMetrics = em;
                appendTelemetryDecodeFields(direct, result);
                addDecodeField(result, "telemetry.decode_mode", QStringLiteral("direct_metrics_payload"));
            }
            else
            {
                addDecodeField(result, "data.payload_hex", QString(d.payload.toHex()));
            }
        }
        break;
    }
    case 70: {  // TRACEROUTE_APP
        RouteDiscoveryFields r;
        if (parseRouteDiscoveryPayload(d.payload, r)) {
            appendRouteDiscoveryDecodeFields(r, result);
        } else {
            addDecodeField(result, "data.payload_hex", QString(d.payload.toHex()));
        }
        break;
    }
    default: {
        const QString text = payloadToText(d.payload);
        if (!text.isEmpty()) {
            addDecodeField(result, "data.text", text);
        } else {
            addDecodeField(result, "data.payload_hex", QString(d.payload.toHex()));
        }
        break;
    }
    }
}

bool deriveTxRadioSettingsFromConfig(const CommandConfig& cfg, TxRadioSettings& settings, QString& error)
{
    settings = TxRadioSettings();
    settings.hasCommand = true;
    settings.syncWord = 0x2B;

    QString presetName = cfg.presetName;
    if (presetName.isEmpty()) {
        presetName = "LONG_FAST";
    }

    bool wideLora = false;
    const RegionBand* region = nullptr;
    if (cfg.hasRegion) {
        region = findRegionBand(cfg.regionName);
        if (!region) {
            error = "invalid region";
            return false;
        }
        wideLora = region->wideLora;
    }

    int bandwidthHz = 0;
    int spreadFactor = 0;
    int parityBits = 0;
    if (!presetToParams(presetName, wideLora, bandwidthHz, spreadFactor, parityBits)) {
        error = "invalid preset";
        return false;
    }

    if (wideLora) {
        error = "LORA_24 wide LoRa presets are not supported by ChirpChat";
        return false;
    }

    settings.hasLoRaParams = true;
    settings.bandwidthHz = bandwidthHz;
    settings.spreadFactor = spreadFactor;
    settings.parityBits = parityBits;
    settings.preambleChirps = wideLora ? 12 : 17;

    const double symbolTimeSec = static_cast<double>(1u << spreadFactor) / static_cast<double>(bandwidthHz);
    settings.deBits = (symbolTimeSec > 0.016) ? 2 : 0; // match classic LoRa low data rate optimization rule

    if (cfg.hasOverrideFrequencyMHz)
    {
        const double freqMHz = cfg.overrideFrequencyMHz + (cfg.hasFrequencyOffsetMHz ? cfg.frequencyOffsetMHz : 0.0);
        settings.hasCenterFrequency = true;
        settings.centerFrequencyHz = std::llround(freqMHz * 1000000.0);
    }
    else if (region)
    {
        const double bwMHz = static_cast<double>(bandwidthHz) / 1000000.0;
        const double slotWidthMHz = region->spacingMHz + bwMHz;
        const double spanMHz = region->freqEndMHz - region->freqStartMHz;
        const auto numChannels = static_cast<uint32_t>(std::floor(spanMHz / slotWidthMHz));

        if (numChannels == 0) {
            error = "region span too narrow for selected preset bandwidth";
            return false;
        }

        uint32_t channelIndex = 0;
        if (cfg.hasChannelNum)
        {
            if (cfg.channelNum < 1 || cfg.channelNum > numChannels) {
                error = QString("channel_num out of range 1..%1").arg(numChannels);
                return false;
            }
            channelIndex = cfg.channelNum - 1;
        }
        else
        {
            const QString displayName = presetToDisplayName(presetName);
            channelIndex = meshHashDjb2(displayName) % numChannels;
        }

        const double centerMHz = region->freqStartMHz + (bwMHz / 2.0) + (channelIndex * slotWidthMHz)
            + (cfg.hasFrequencyOffsetMHz ? cfg.frequencyOffsetMHz : 0.0);

        settings.hasCenterFrequency = true;
        settings.centerFrequencyHz = std::llround(centerMHz * 1000000.0);
    }

    settings.summary = QString("preset=%1 sf=%2 cr=4/%3 bw=%4kHz de=%5")
        .arg(presetName)
        .arg(settings.spreadFactor)
        .arg(settings.parityBits + 4)
        .arg(settings.bandwidthHz / 1000)
        .arg(settings.deBits);
    settings.summary += QString(" preamble=%1").arg(settings.preambleChirps);

    if (region) {
        settings.summary += QString(" region=%1").arg(region->name);
    }
    if (cfg.hasChannelNum) {
        settings.summary += QString(" channel_num=%1").arg(cfg.channelNum);
    }
    if (settings.hasCenterFrequency) {
        settings.summary += QString(" freq=%1MHz").arg(settings.centerFrequencyHz / 1000000.0, 0, 'f', 6);
    }

    return true;
}

} // namespace

bool Packet::isCommand(const QString& text)
{
    return text.trimmed().startsWith("MESH:", Qt::CaseInsensitive);
}

bool Packet::buildFrameFromCommand(const QString& command, QByteArray& frame, QString& summary, QString& error)
{
    CommandConfig cfg;

    if (!parseCommand(command, cfg, error)) {
        return false;
    }

    QByteArray payload = encodeData(cfg.data);

    if (cfg.encrypt) {
        payload = aesCtrCrypt(payload, cfg.key, cfg.header.from, cfg.header.id, CounterMode::BigEndian);
        if (payload.isEmpty()) {
            error = "failed to encrypt payload";
            return false;
        }
    }

    frame = encodeHeader(cfg.header);
    frame.append(payload);

    summary = QString("MESH TX|%1 key=%2 encrypt=%3 %4")
        .arg(summarizeHeader(cfg.header))
        .arg(cfg.keyLabel)
        .arg(cfg.encrypt ? 1 : 0)
        .arg(summarizeData(cfg.data));

    return true;
}

bool Packet::decodeFrame(const QByteArray& frame, DecodeResult& result)
{
    return decodeFrame(frame, result, QString());
}

bool Packet::decodeFrame(const QByteArray& frame, DecodeResult& result, const QString& keySpecList)
{
    result = DecodeResult();

    Header h;
    if (!parseHeader(frame, h)) {
        return false;
    }

    result.isFrame = true;
    appendHeaderDecodeFields(h, result);

    const QByteArray encryptedPayload = frame.mid(kHeaderLength);
    addDecodeField(result, "decode.payload_encrypted_len", QString::number(encryptedPayload.size()));

    // 1) Plain decode attempt first (unencrypted / key-none packets)
    DataFields data;
    if (parseData(encryptedPayload, data))
    {
        result.dataDecoded = true;
        result.decrypted = false;
        result.keyLabel = "none";
        result.summary = QString("MESH RX|%1 key=none %2")
            .arg(summarizeHeader(h))
            .arg(summarizeData(data));
        addDecodeField(result, "decode.path", QStringLiteral("plain"));
        addDecodeField(result, "decode.key_label", result.keyLabel);
        addDecodeField(result, "decode.decrypted", result.decrypted);
        appendDataDecodeFields(data, result);
        return true;
    }

    // 2) Try configured keys (hash-matched keys first)
    std::vector<KeyEntry> keys;

    if (!keySpecList.trimmed().isEmpty())
    {
        QString error;
        int keyCount = 0;

        if (!parseKeySpecList(keySpecList, keys, &error, &keyCount, true))
        {
            qWarning() << "Meshtastic::Packet::decodeFrame: invalid keySpecList:" << error;
            keys = defaultKeysFromEnv();
        }
        else if (keyCount == 0)
        {
            keys = defaultKeysFromEnv();
        }
    }
    else
    {
        keys = defaultKeysFromEnv();
    }

    std::stable_sort(keys.begin(), keys.end(), [h](const KeyEntry& a, const KeyEntry& b) {
        const int as = (a.hasExpectedHash && a.expectedHash == h.channel) ? 1 : 0;
        const int bs = (b.hasExpectedHash && b.expectedHash == h.channel) ? 1 : 0;
        return as > bs;
    });

    std::set<QString> tested;

    for (const KeyEntry& k : keys)
    {
        if (k.key.isEmpty()) {
            continue;
        }

        const QString fingerprint = QString("%1:%2").arg(QString(k.key.toHex()), k.label);

        if (tested.find(fingerprint) != tested.end()) {
            continue;
        }
        tested.insert(fingerprint);

        const QByteArray plainBe = aesCtrCrypt(encryptedPayload, k.key, h.from, h.id, CounterMode::BigEndian);
        if (!plainBe.isEmpty() && parseData(plainBe, data))
        {
            result.dataDecoded = true;
            result.decrypted = true;
            result.keyLabel = k.label;
            result.summary = QString("MESH RX|%1 key=%2 ctr=be %3")
                .arg(summarizeHeader(h))
                .arg(k.label)
                .arg(summarizeData(data));
            addDecodeField(result, "decode.path", QStringLiteral("aes_ctr_be"));
            addDecodeField(result, "decode.key_label", result.keyLabel);
            addDecodeField(result, "decode.decrypted", result.decrypted);
            appendDataDecodeFields(data, result);
            return true;
        }

        // Keep CTR mode strict to match Meshtastic reference decode path.
    }

    result.summary = QString("MESH RX|%1 undecoded payload_len=%2")
        .arg(summarizeHeader(h))
        .arg(encryptedPayload.size());
    addDecodeField(result, "decode.path", QStringLiteral("undecoded"));
    addDecodeField(result, "decode.key_label", QStringLiteral("none"));
    addDecodeField(result, "decode.decrypted", false);

    if (!encryptedPayload.isEmpty()) {
        addDecodeField(result, "decode.payload_encrypted_hex", QString(encryptedPayload.toHex()));
    }

    return true;
}

bool Packet::validateKeySpecList(const QString& keySpecList, QString& error, int* keyCount)
{
    std::vector<KeyEntry> keys;
    if (!parseKeySpecList(keySpecList, keys, &error, keyCount, true)) {
        return false;
    }

    if (keyCount && *keyCount == 0)
    {
        error = "no keys found";
        return false;
    }

    return true;
}

bool Packet::deriveTxRadioSettings(const QString& command, TxRadioSettings& settings, QString& error)
{
    CommandConfig cfg;

    if (!parseCommand(command, cfg, error)) {
        return false;
    }

    return deriveTxRadioSettingsFromConfig(cfg, settings, error);
}

} // namespace Meshtastic
