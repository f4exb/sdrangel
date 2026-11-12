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

#ifndef INCLUDE_DSDPACKETTEXT_H
#define INCLUDE_DSDPACKETTEXT_H

#include <cstdint>
#include <string>

/**
 * Text messages carried as packet data by the digital voice protocols (DMR and P25 send IP
 * datagrams over their packet data channels, NXDN raw user data). What is decoded:
 *
 *  - UDP port 5016 (or 5017): the ETSI DMR text message service, UTF-16BE text;
 *  - UDP port 4007: the Motorola Text Messaging Service (TMS), used on DMR and P25 radios:
 *    a length, a header, an optional address and options, then UCS-2 text;
 *  - printable text: what NXDN short data calls carry when the radio sends plain characters.
 *
 * All text comes out as UTF-8 on one line (control characters become spaces).
 */
class DSDPacketText
{
public:
    static void appendUTF8(std::string& text, uint32_t codePoint);
    static void decodeUTF16(const uint8_t *bytes, int nbBytes, bool bigEndian, std::string& text);
    static uint16_t internetChecksum(const uint8_t *bytes, int nbBytes);

    /**
     * An IPv4 datagram (from its header): a UDP payload on a known text port decodes to text.
     * kind names the service ("TMS", "UTF-16BE"). Returns false when it is not a text message.
     */
    static bool fromIPv4(const uint8_t *ip, int nbBytes, std::string& text, std::string& kind);
    //! A UDP payload with its ports (the DMR compressed UDP/IPv4 header path)
    static bool fromUdpPayload(int sourcePort, int destinationPort, const uint8_t *payload, int nbBytes, std::string& text, std::string& kind);
    //! Motorola TMS PDU (a length then the header): the text of a simple text message
    static bool fromTMS(const uint8_t *pdu, int nbBytes, std::string& text);
    /**
     * Plain characters: the bytes as text when at least minFraction of them (trailing NULs
     * ignored) are printable ASCII, the rest shown as '.'; false otherwise.
     */
    static bool printable(const uint8_t *bytes, int nbBytes, std::string& text, double minFraction = 0.8);
    //! The bytes as hexadecimal
    static std::string hex(const uint8_t *bytes, int nbBytes);
};

#endif // INCLUDE_DSDPACKETTEXT_H
