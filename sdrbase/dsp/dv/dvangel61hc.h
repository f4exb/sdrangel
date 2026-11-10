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

#ifndef INCLUDE_DVANGEL61HC_H
#define INCLUDE_DVANGEL61HC_H

#include <cstdint>
#include <vector>

#include "export.h"

/**
 * Angel61 header compression (ANGEL61.md 3.6): the IPv4 or IPv6 and TCP or UDP headers of a
 * tunnelled datagram (3.5) sent as what changed since the last datagram of its flow. RFC 1144's
 * scheme - a context per flow, deltas against its reference - with RFC 2507's IPv6, UDP and
 * explicit context state, and TCP timestamps carried as deltas so that Linux and macOS hosts,
 * which change them in every segment, compress too.
 *
 * A datagram goes in one of three forms, the four flag bits of its message field (content type
 * 6): verbatim; full header (context id, context sequence, the datagram), which sets the
 * context's reference; compressed (context id, context sequence, change mask, an extension
 * mask when bit 7 is set, the transport checksum, the changed fields, the payload). The
 * decompressor applies a compressed datagram only to the reference of the sequence before it,
 * and checks the transport checksum of what it rebuilt; anything else leaves the context stale
 * and discarded until a full header comes, which it asks for with tunnel control kind 4.
 */
class SDRBASE_API DVAngel61HC
{
public:
    enum Form { FormVerbatim = 0, FormFull = 1, FormCompressed = 2 };

    //! The advertise's compression byte (3.5, 3.6): bits 0 to 2, and the contexts kept as a power of two in bits 4 to 7
    enum Capability { CapabilityTCP = 0x01, CapabilityUDP = 0x02, CapabilitySendsState = 0x04 };

    //! The change mask: what follows the transport checksum, in this order
    enum Mask {
        MaskSequence = 0x01,        //!< TCP sequence number: delta
        MaskAck = 0x02,             //!< TCP acknowledgement number: delta
        MaskWindow = 0x04,          //!< TCP window: delta
        MaskUrgent = 0x08,          //!< URG set, and the urgent pointer: 16 bits
        MaskPush = 0x10,            //!< PSH set: nothing follows
        MaskIdentification = 0x20,  //!< IPv4 identification changed by other than 1: delta
        MaskTTL = 0x40,             //!< TTL or hop limit: 8 bits
        MaskExtended = 0x80         //!< the extension mask follows the change mask
    };
    //! The extension mask: its fields follow the change mask's
    enum MaskExtension {
        ExtensionTSval = 0x01,      //!< TCP timestamp value: delta
        ExtensionTSecr = 0x02,      //!< TCP timestamp echo reply: delta
        ExtensionAckRequest = 0x04  //!< the compressor asks for tunnel control kind 5
    };

    static const int DefaultContexts = 16;
    static const int RefreshPackets = 16;   //!< a full header at least every 16 packets of a context
    static const int RefreshMs = 30000;     //!< and every 30 s

    //! The compression byte of an advertise
    static int capability(bool tcp, bool udp, bool sendsState, int contexts);
    //! The contexts a compression byte offers (0: it takes no compressed datagram)
    static int contextsOf(int capability);

    struct Statistics
    {
        int m_verbatim = 0;
        int m_full = 0;
        int m_compressed = 0;
        int64_t m_bytesIn = 0;     //!< datagrams
        int64_t m_bytesOut = 0;    //!< what goes in the message field
    };

    class SDRBASE_API Compressor
    {
    public:
        Compressor();
        //! What the peer takes: its advertise's compression byte (0: nothing is compressed)
        void setPeer(int capability);
        int getPeer() const { return m_peer; }
        /**
         * A datagram to send: the message for content type 6 and its form (the fragment flags).
         * timeMs: the sender's clock, for the 30 s refresh.
         */
        void compress(const std::vector<uint8_t>& datagram, int64_t timeMs, std::vector<uint8_t>& message, int& form);
        //! The peer's context state (tunnel control kind 4): these contexts go as full headers next
        void refresh(const std::vector<int>& contexts);
        void reset();
        const Statistics& getStatistics() const { return m_statistics; }

    private:
        struct Context
        {
            bool m_used = false;
            std::vector<uint8_t> m_key;
            std::vector<uint8_t> m_reference;   //!< the headers of the last datagram sent
            int m_sequence = 0;                 //!< of the next datagram
            int m_sinceFull = 0;
            int64_t m_fullMs = 0;
            bool m_needsFull = true;
            int64_t m_lastUsed = 0;
        };

        int m_peer;
        std::vector<Context> m_contexts;
        int64_t m_uses;
        Statistics m_statistics;
    };

    class SDRBASE_API Decompressor
    {
    public:
        Decompressor();
        /**
         * A message of content type 6 and its form: true and the datagram, or false when it is
         * discarded (a compressed datagram whose context is stale, or does not follow its
         * reference, or whose rebuilt checksum fails)
         */
        bool decompress(const std::vector<uint8_t>& message, int form, std::vector<uint8_t>& datagram);
        //! The contexts that went stale since the last call: for tunnel control kind 4
        std::vector<int> takeStale();
        //! The contexts whose compressor asked to hear they are established: for kind 5
        std::vector<int> takeAckRequests();
        void reset();
        int getDiscarded() const { return m_discarded; }
        int getRebuilt() const { return m_rebuilt; }

    private:
        struct Context
        {
            bool m_valid = false;
            bool m_stale = false;
            int m_sequence = 0;                 //!< of the last datagram accepted
            std::vector<uint8_t> m_reference;
        };

        std::vector<Context> m_contexts;
        std::vector<int> m_stale;
        std::vector<int> m_acks;
        int m_discarded;
        int m_rebuilt;

        void markStale(int id);
    };

    //! Whether a datagram can be compressed at all: IPv4 without options and not a fragment, or
    //! IPv6 without extension headers, carrying TCP or UDP
    static bool compressible(const std::vector<uint8_t>& datagram);
    //! The TCP or UDP checksum of a datagram checks (a UDP checksum of zero over IPv4: none sent)
    static bool transportChecksumOk(const std::vector<uint8_t>& datagram);
    //! The TCP or UDP checksum, and the IPv4 header checksum, written into a datagram
    static void setChecksums(std::vector<uint8_t>& datagram);
};

#endif // INCLUDE_DVANGEL61HC_H
