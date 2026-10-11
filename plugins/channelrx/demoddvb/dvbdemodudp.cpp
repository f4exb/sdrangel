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

#include <cstring>

#include <QDebug>
#include <QHostInfo>

#include "dvbdemodudp.h"

DVBDemodUDP::DVBDemodUDP() :
    m_port(5004),
    m_datagramFill(0),
    m_datagrams(0),
    m_address(QHostAddress::LocalHost),
    m_generation(0),
    m_lookupGeneration(0),
    m_lookupStop(false)
{
}

DVBDemodUDP::~DVBDemodUDP()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lookupStop = true;
    }

    m_lookupWake.notify_one();

    if (m_lookupThread.joinable()) {
        m_lookupThread.join();
    }
}

void DVBDemodUDP::setDestination(const QString& address, uint16_t port)
{
    if ((address == m_name) && (port == m_port)) {
        return; // As forced settings set it again
    }

    m_datagramFill = 0;
    m_port = port;

    if (address == m_name) {
        return;
    }

    m_name = address;
    QHostAddress literal;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_generation++;

    if (literal.setAddress(address))
    {
        m_address = literal;
        m_lookupName.clear();
        return;
    }

    // Until it's found. Only the latest name's looked up, after one under way
    m_address = QHostAddress();
    m_lookupName = address;
    m_lookupGeneration = m_generation;

    if (!m_lookupThread.joinable()) {
        m_lookupThread = std::thread(&DVBDemodUDP::lookup, this);
    } else {
        m_lookupWake.notify_one();
    }
}

void DVBDemodUDP::lookup()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    while (true)
    {
        m_lookupWake.wait(lock, [this]() { return m_lookupStop || !m_lookupName.isEmpty(); });

        if (m_lookupStop) {
            return;
        }

        const QString name = m_lookupName;
        const uint64_t generation = m_lookupGeneration;
        m_lookupName.clear();
        lock.unlock();

        const QHostInfo info = QHostInfo::fromName(name);
        const QHostAddress found = info.addresses().isEmpty() ? QHostAddress() : info.addresses().first();

        if (found.isNull()) {
            qWarning() << "DVBDemodUDP::lookup: can't find" << name;
        }

        lock.lock();

        if (m_generation == generation) {
            m_address = found;
        }
    }
}

void DVBDemodUDP::write(const uint8_t *packet)
{
    writePackets(packet, 1);
}

void DVBDemodUDP::writePackets(const uint8_t *packets, int count)
{
    for (int i = 0; i < count; i++)
    {
        memcpy(&m_datagram[m_datagramFill], packets + i * 188, 188);
        m_datagramFill += 188;

        if (m_datagramFill == (int) sizeof(m_datagram)) {
            send();
        }
    }
}

void DVBDemodUDP::send()
{
    m_datagramFill = 0;

    if (!m_socket) {
        m_socket.reset(new QUdpSocket());
    }

    QHostAddress address;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        address = m_address;
    }

    if (!address.isNull() && (m_socket->writeDatagram(m_datagram, sizeof(m_datagram), address, m_port) >= 0)) {
        m_datagrams++;
    }
}
