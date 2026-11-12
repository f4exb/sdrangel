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

#ifndef INCLUDE_IPTUNNELBACKEND_H
#define INCLUDE_IPTUNNELBACKEND_H

#include <atomic>
#include <functional>

#include <QMutex>
#include <QQueue>
#include <QThread>

#include "iptunneltypes.h"

/**
 * The platform side of IPTunnel: a thread that owns the operating system's tunnel device
 * (Wintun, or the privileged helper, on Windows; a TUN device on Linux; utun on macOS) and
 * moves packets between it and IPTunnel's queues. Each platform implements a subclass, created
 * by iptunnelCreateBackend; IPTunnel starts it on open and stops and waits for it on close.
 *
 * The backend reports through the callbacks it is given and never touches IPTunnel's state
 * directly. Calls from IPTunnel (stop, packetsAvailable, applyRouting) only set a flag or queue
 * a command and then wake() the thread, which does the work.
 */
class IPTunnelBackend : public QThread
{
public:
    /**
     * How a backend reports to IPTunnel, called from the backend's thread: the device opened
     * (or failed to), a packet read from the OS, the next packet to write to the OS (false when
     * there is none), a packet written, a routing request finished, and an error (fatal or not).
     */
    struct Callbacks
    {
        std::function<void(const IPTunnelTypes::InterfaceInfo&, const IPTunnelTypes::Error&)> opened;
        std::function<void(const QByteArray&)> packetReceived;
        std::function<bool(QByteArray&)> takePacketToOS;
        std::function<void(qint64)> packetWritten;
        std::function<void(IPTunnelTypes::RequestId, const IPTunnelTypes::OperationResult&)> routingFinished;
        std::function<void(const IPTunnelTypes::Error&, bool)> error;
    };

    IPTunnelBackend(const IPTunnelTypes::Configuration& configuration, const Callbacks& callbacks);
    ~IPTunnelBackend() override = default;

    void stop();
    void packetsAvailable();
    void applyRouting(IPTunnelTypes::RequestId id, const IPTunnelTypes::RoutingConfiguration& routing);

protected:
    /** A routing change requested by IPTunnel::applyRouting, queued for the backend's thread */
    struct RoutingCommand
    {
        IPTunnelTypes::RequestId id;
        IPTunnelTypes::RoutingConfiguration routing;
    };

    bool stopping() const { return m_stopping.load(); }
    bool takeRoutingCommand(RoutingCommand& command);
    virtual void wake() = 0;

    IPTunnelTypes::Configuration m_configuration;
    Callbacks m_callbacks;

private:
    std::atomic_bool m_stopping {false};
    QMutex m_commandMutex;
    QQueue<RoutingCommand> m_routingCommands;
};

IPTunnelBackend *iptunnelCreateBackend(
    const IPTunnelTypes::Configuration& configuration,
    const IPTunnelBackend::Callbacks& callbacks);
IPTunnelTypes::Capabilities iptunnelBackendCapabilities();

#endif // INCLUDE_IPTUNNELBACKEND_H
