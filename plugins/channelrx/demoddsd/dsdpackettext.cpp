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

#include <cstdio>

#include "dsdpackettext.h"

void DSDPacketText::appendUTF8(std::string& text, uint32_t codePoint)
{
    if ((codePoint < 0x20) || (codePoint == 0x7f)) {
        codePoint = ' '; // Keep status/event rows on one printable line.
    } else if ((codePoint > 0x10ffff) || ((codePoint >= 0xd800) && (codePoint <= 0xdfff))) {
        codePoint = 0xfffd;
    }

    if (codePoint < 0x80) {
        text.push_back((char) codePoint);
    } else if (codePoint < 0x800) {
        text.push_back((char) (0xc0 | (codePoint >> 6)));
        text.push_back((char) (0x80 | (codePoint & 0x3f)));
    } else if (codePoint < 0x10000) {
        text.push_back((char) (0xe0 | (codePoint >> 12)));
        text.push_back((char) (0x80 | ((codePoint >> 6) & 0x3f)));
        text.push_back((char) (0x80 | (codePoint & 0x3f)));
    } else {
        text.push_back((char) (0xf0 | (codePoint >> 18)));
        text.push_back((char) (0x80 | ((codePoint >> 12) & 0x3f)));
        text.push_back((char) (0x80 | ((codePoint >> 6) & 0x3f)));
        text.push_back((char) (0x80 | (codePoint & 0x3f)));
    }
}

void DSDPacketText::decodeUTF16(const uint8_t *bytes, int nbBytes, bool bigEndian, std::string& text)
{
    auto unit = [&](int i) { return bigEndian ? (((uint32_t) bytes[i] << 8) | bytes[i + 1]) : (((uint32_t) bytes[i + 1] << 8) | bytes[i]); };
    int i = 0;

    while (i + 1 < nbBytes)
    {
        uint32_t codePoint = unit(i);
        i += 2;

        if ((codePoint == 0xfeff) && text.empty()) {
            continue;
        }

        if (codePoint == 0) {
            break; // NUL: the end of the text (padding follows)
        }

        if ((codePoint >= 0xd800) && (codePoint <= 0xdbff))
        {
            if (i + 1 >= nbBytes) {
                appendUTF8(text, 0xfffd);
                break;
            }

            uint32_t low = unit(i);

            if ((low < 0xdc00) || (low > 0xdfff)) {
                codePoint = 0xfffd;
            } else {
                codePoint = 0x10000 + ((codePoint - 0xd800) << 10) + (low - 0xdc00);
                i += 2;
            }
        }

        appendUTF8(text, codePoint);
    }
}

uint16_t DSDPacketText::internetChecksum(const uint8_t *bytes, int nbBytes)
{
    uint32_t sum = 0;

    for (int i = 0; i < nbBytes; i += 2)
    {
        sum += ((uint16_t) bytes[i] << 8) | (i + 1 < nbBytes ? bytes[i + 1] : 0);
        sum = (sum & 0xffff) + (sum >> 16);
    }

    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    return (uint16_t) ~sum;
}

bool DSDPacketText::fromIPv4(const uint8_t *ip, int nbBytes, std::string& text, std::string& kind)
{
    if (nbBytes < 28) {
        return false;
    }

    const int headerBytes = (ip[0] & 0x0f) * 4;
    const int totalBytes = ((int) ip[2] << 8) | ip[3];
    const int fragment = ((ip[6] & 0x3f) << 8) | ip[7];

    if (((ip[0] >> 4) != 4) || (headerBytes < 20) || (headerBytes + 8 > nbBytes) || (totalBytes < headerBytes + 8)
        || (totalBytes > nbBytes) || (ip[9] != 17) || (fragment != 0) || (internetChecksum(ip, headerBytes) != 0))
    {
        return false;
    }

    const uint8_t *udp = ip + headerBytes;
    const int sourcePort = ((int) udp[0] << 8) | udp[1];
    const int destinationPort = ((int) udp[2] << 8) | udp[3];
    const int udpBytes = ((int) udp[4] << 8) | udp[5];

    if ((udpBytes < 8) || (headerBytes + udpBytes > totalBytes)) {
        return false;
    }

    return fromUdpPayload(sourcePort, destinationPort, udp + 8, udpBytes - 8, text, kind);
}

bool DSDPacketText::fromUdpPayload(int sourcePort, int destinationPort, const uint8_t *payload, int nbBytes, std::string& text, std::string& kind)
{
    text.clear();

    if ((sourcePort == 4007) || (destinationPort == 4007))
    {
        kind = "TMS";
        return fromTMS(payload, nbBytes, text);
    }

    if ((sourcePort == 5016) || (destinationPort == 5016) || (sourcePort == 5017) || (destinationPort == 5017))
    {
        if ((nbBytes < 2) || (nbBytes & 1)) {
            return false;
        }

        kind = "UTF-16BE";
        decodeUTF16(payload, nbBytes, true, text);
        return !text.empty();
    }

    return false;
}

bool DSDPacketText::fromTMS(const uint8_t *pdu, int nbBytes, std::string& text)
{
    // | length (2) | header (1) | address length (1) | address | options | text |: the header's
    // bit 7 says options follow, bit 4 a control message (no text); its low nibble is the PDU
    // type, 0 for a simple text message; an option byte's bit 7 says another follows, and the
    // second option byte's low 5 bits give the encoding (4 = UCS-2 little endian, the default)
    if (nbBytes < 4) {
        return false;
    }

    int length = ((int) pdu[0] << 8) | pdu[1];

    if ((length < 2) || (length + 2 > nbBytes)) {
        length = nbBytes - 2; // a truncated or overstated length: take what is there
    }

    const uint8_t header = pdu[2];
    const bool control = (header & 0x10) != 0;
    const int type = header & 0x0f;
    int position = 4 + pdu[3];

    if (control || (type != 0) || (position > length + 2)) {
        return false;
    }

    int encoding = 4;

    if (header & 0x80)
    {
        bool more = true;
        int option = 0;

        while (more && (position < length + 2))
        {
            uint8_t byte = pdu[position++];
            more = (byte & 0x80) != 0;

            if (option == 1) {
                encoding = byte & 0x1f;
            }

            option++;
        }
    }

    const int textBytes = length + 2 - position;

    if (textBytes < 2) {
        return false;
    }

    // UCS-2: little endian as Motorola sends it; when the encoding is not stated, the byte
    // order that leaves the high bytes of Latin text zero
    bool bigEndian = false;

    if (encoding != 4)
    {
        int zeroHigh = 0, zeroLow = 0;

        for (int i = 0; i + 1 < textBytes; i += 2)
        {
            zeroHigh += pdu[position + i] == 0;
            zeroLow += pdu[position + i + 1] == 0;
        }

        bigEndian = zeroHigh > zeroLow;
    }

    decodeUTF16(pdu + position, textBytes & ~1, bigEndian, text);
    return !text.empty();
}

bool DSDPacketText::printable(const uint8_t *bytes, int nbBytes, std::string& text, double minFraction)
{
    while ((nbBytes > 0) && (bytes[nbBytes - 1] == 0)) {
        nbBytes--;
    }

    if (nbBytes <= 0) {
        return false;
    }

    int printableCount = 0;
    text.clear();

    for (int i = 0; i < nbBytes; i++)
    {
        uint8_t c = bytes[i];
        bool ok = ((c >= 0x20) && (c < 0x7f)) || (c == 13) || (c == 10) || (c == 9);
        printableCount += ok;
        text.push_back(ok ? (c < 0x20 ? ' ' : (char) c) : '.');
    }

    return printableCount >= minFraction * nbBytes;
}

std::string DSDPacketText::hex(const uint8_t *bytes, int nbBytes)
{
    std::string s;
    char buffer[4];

    for (int i = 0; i < nbBytes; i++)
    {
        snprintf(buffer, sizeof(buffer), "%02X", bytes[i]);
        s += buffer;
    }

    return s;
}
