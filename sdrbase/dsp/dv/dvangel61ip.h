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

#ifndef INCLUDE_DVANGEL61IP_H
#define INCLUDE_DVANGEL61IP_H

#include <cstdint>
#include <string>
#include <vector>

#include "export.h"

#include "dvangel61.h"

/**
 * Angel61 IP tunnelling (ANGEL61.md 3.5): the part that goes over the air. One station carries
 * IPv4 and IPv6 datagrams for another; the host side is `sdrbase/tun` (`IPTunnel`), which gives
 * a layer 3 interface and one complete datagram per buffer, and knows nothing about radio.
 *
 * A datagram is an ordinary message of content type 6 - the reassembler, its keying by source
 * callsign and message id, its duplicate rules and its expiry all apply unchanged - and the two
 * ends agree with content type 7, tunnel control. What is *not* here is any policy: what may be
 * sent, to whom, and whether a tunnel exists at all is the operator's, and the plugins'.
 */
class SDRBASE_API DVAngel61IP
{
public:
    //! A prefix as the advertise carries it
    struct Prefix
    {
        int m_family = 4;                 //!< 4 or 6
        int m_length = 0;                 //!< prefix length in bits
        std::vector<uint8_t> m_address;   //!< 4 or 16 bytes, network order

        bool operator==(const Prefix& other) const
        {
            return (m_family == other.m_family) && (m_length == other.m_length) && (m_address == other.m_address);
        }
        std::string toString() const;
    };

    /**
     * Tunnel control, content type 7: kind (8) then the kind's own payload. Kinds 4 and 5 are
     * the header compression's (3.6, DVAngel61HC): the decompressor's contexts gone stale, and
     * the ones it has established.
     */
    struct SDRBASE_API Control
    {
        enum Kind {
            Advertise = 0,   //!< this station will tunnel, and these are its terms
            Refuse = 1,
            Close = 2,
            Repair = 3,      //!< these fragments of a datagram did not arrive
            ContextState = 4, //!< 3.6: these header compression contexts are stale
            ContextAck = 5   //!< 3.6: these contexts are established
        };
        enum Reason {
            ReasonOperator = 0,      //!< refused by the operator
            ReasonNoTunnel = 1,      //!< no tunnel configured
            ReasonNoOverlap = 2,     //!< the prefixes do not overlap
            ReasonBusy = 3
        };
        enum Flags {
            FlagIPv4 = 0x01,
            FlagIPv6 = 0x02,
            FlagForwards = 0x04,     //!< the station forwards beyond itself
            FlagRepairs = 0x08       //!< it will answer a repair request
        };

        int m_kind = Advertise;
        int m_flags = FlagIPv4;
        int m_mtu = 576;
        int m_compression = 0;       //!< 3.6: what compressed datagrams it takes and how many contexts (DVAngel61HC::capability)
        std::vector<Prefix> m_prefixes;
        int m_reason = ReasonOperator;
        int m_messageId = 0;         //!< Repair: the datagram wanted
        std::vector<std::pair<int, int>> m_ranges;   //!< Repair: first fragment index and count
        std::vector<int> m_contexts; //!< ContextState, ContextAck: the context ids

        std::vector<uint8_t> encode() const;
        static bool decode(const std::vector<uint8_t>& bytes, Control& control);
        std::string toString() const;
    };

    static constexpr int MaxPrefixes = 16;
    static constexpr int MinMTU = 64;
    static constexpr int MaxMTU = 1500;

    //! The largest datagram message fields of fieldBytes payload bytes can carry (3.2: 255 fragments)
    static int maxDatagram(int fieldBytes) { return 255 * fieldBytes - 2; }

    /**
     * Whether a datagram is one this link will carry at all (3.5, "Filtering, and why it is not
     * optional"): it has to be IPv4 or IPv6, its length has to agree with its header, and
     * broadcast and multicast destinations are refused - a host with a route pointed at a tunnel
     * produces neighbour discovery, mDNS, SSDP and DHCP renewals of its own accord, and any one
     * of them fills 333 bytes a second. `reason` says which rule refused it, for the counters
     * the operator sees.
     */
    enum Refusal {
        Accepted = 0,
        NotIP = 1,
        BadLength = 2,
        Multicast = 3,
        Broadcast = 4,
        TooLong = 5,
        NotAllowed = 6,     //!< outside the operator's allow list
        NbRefusals
    };

    /**
     * The operator's part of the filter (3.5). The allow list says what may be sent - a list of what
     * may pass rather than of what may not, as the licence's rules suggest - and is empty for
     * everything; the groups are multicast destinations let through all the same (an mDNS group, a
     * routing protocol's), each still subject to the allow list.
     */
    struct SDRBASE_API Filter
    {
        struct Rule
        {
            int m_protocol = -1;        //!< IP protocol number (1 ICMP, 6 TCP, 17 UDP, 58 ICMPv6), -1 any; ICMP stands for both ICMPs
            int m_portLow = -1;         //!< TCP and UDP: a port range either port of the datagram is in, -1 any
            int m_portHigh = -1;
        };

        std::vector<Rule> m_allow;
        std::vector<std::vector<uint8_t>> m_groups;   //!< 4 or 16 bytes each

        /**
         * "icmp, tcp:22, udp:53, tcp:8000-8080, gre, 41": a protocol by name (icmp, tcp, udp, gre,
         * esp, ah, sctp) or number, with a port or a port range for TCP, UDP and SCTP. False on a
         * term it cannot read, `error` naming it
         */
        static bool parseAllow(const std::string& text, std::vector<Rule>& rules, std::string& error);
        std::string allowString() const;
        bool allows(int protocol, int sourcePort, int destinationPort) const;
    };

    static Refusal classify(const std::vector<uint8_t>& datagram, int mtu = MaxMTU);
    //! As above, then the operator's filter: a group of theirs passes the multicast rule, and what
    //! the allow list does not name is refused
    static Refusal classify(const std::vector<uint8_t>& datagram, int mtu, const Filter& filter);
    /**
     * The datagram's transport: its IP protocol (IPv6: after the hop by hop, routing and
     * destination option headers) and its TCP, UDP or SCTP ports (-1 when it has none, or is a
     * fragment after the first). False when it is not IP
     */
    static bool transport(const std::vector<uint8_t>& datagram, int& protocol, int& sourcePort, int& destinationPort);
    static const char *refusalName(Refusal refusal);
    //! The version of a datagram (4, 6 or 0)
    static int version(const std::vector<uint8_t>& datagram);
    //! Its source and destination addresses, empty when it is not one
    static bool addresses(const std::vector<uint8_t>& datagram, std::vector<uint8_t>& source,
        std::vector<uint8_t>& destination);
    //! Whether an address is inside a prefix
    static bool inPrefix(const std::vector<uint8_t>& address, const Prefix& prefix);
    //! Whether any of the prefixes holds the destination (none: nothing is refused on this count)
    static bool routable(const std::vector<uint8_t>& datagram, const std::vector<Prefix>& prefixes);
};

#endif // INCLUDE_DVANGEL61IP_H
