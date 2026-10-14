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

#ifndef INCLUDE_DTVUDP_H
#define INCLUDE_DTVUDP_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include <QHostAddress>
#include <QString>
#include <QUdpSocket>
#include "export.h"

// Sends transport stream packets over UDP, 7 to a datagram, as VLC and ffplay expect for udp:// URLs. Used in the
// demodulator's thread, where the socket is made, as a socket belongs to the thread that made it
class SDRBASE_API DTVUDP
{
public:
    DTVUDP();
    ~DTVUDP();     //!< Waits for a host name lookup under way

    //! address: an IP address, or a host name, looked up in a thread of its own (a lookup can take seconds, which
    //! would hold up demodulation): nothing's sent until it's found. The same name again isn't looked up again.
    //! A datagram part filled is dropped, as it was for the destination before
    void setDestination(const QString& address, uint16_t port);
    void write(const uint8_t *packet);
    //! count 188-byte packets
    void writePackets(const uint8_t *packets, int count);
    //! The datagram part filled is dropped (as when sending's turned off, so it isn't sent, stale, when it's on again)
    void discard() { m_datagramFill = 0; }
    uint64_t getDatagrams() const { return m_datagrams; } //!< Sent

    static constexpr int m_packetsPerDatagram = 7;

private:
    void lookup();          //!< The lookup thread's
    void send();

    std::unique_ptr<QUdpSocket> m_socket;
    QString m_name;                 //!< Address or host name set
    std::atomic<uint16_t> m_port;
    char m_datagram[m_packetsPerDatagram * 188];
    int m_datagramFill;
    std::atomic<uint64_t> m_datagrams;

    // The address to send to, and the lookup of a host name, in a thread of its own (joined when this is deleted)
    std::mutex m_mutex;
    QHostAddress m_address;
    uint64_t m_generation;          //!< Of the last destination set: a lookup for an earlier one is ignored
    QString m_lookupName;           //!< To look up, empty if none's waiting
    uint64_t m_lookupGeneration;
    bool m_lookupStop;
    std::condition_variable m_lookupWake;
    std::thread m_lookupThread;     //!< Started for the first host name
};

#endif // INCLUDE_DTVUDP_H
