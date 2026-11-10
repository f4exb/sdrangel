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

#ifndef INCLUDE_IPTUNNEL_H
#define INCLUDE_IPTUNNEL_H

#include <QObject>
#include <QByteArray>

#include "export.h"
#include "iptunneltypes.h"

class IPTunnelPrivate;

/**
 * A layer 3 network interface on the host (Wintun on Windows, TUN on Linux, utun on macOS),
 * for plugins that carry IP over a radio link (see README.md). Packets are whole IPv4 or IPv6
 * datagrams, one QByteArray each, in both directions:
 *  - what the host sends through the interface is queued, and outboundPacketsAvailable says
 *    there is something for takeOutboundPacket;
 *  - what the radio receives goes to injectInboundPacket, queued for the host (QueueFull until
 *    inboundCapacityAvailable when the queue is full).
 * open, previewRouting and applyRouting are asynchronous: each returns a RequestId answered by
 * operationFinished. injectInboundPacket needs the Open state. The queues and the state are
 * guarded by a mutex; the platform's backend runs in its own thread (IPTunnelBackend).
 */
class SDRBASE_API IPTunnel : public QObject
{
    Q_OBJECT
public:
    using State = IPTunnelTypes::State;
    using EnqueueResult = IPTunnelTypes::EnqueueResult;
    using RequestId = IPTunnelTypes::RequestId;
    using Configuration = IPTunnelTypes::Configuration;
    using RoutingConfiguration = IPTunnelTypes::RoutingConfiguration;
    using Capabilities = IPTunnelTypes::Capabilities;
    using ValidationResult = IPTunnelTypes::ValidationResult;
    using OperationResult = IPTunnelTypes::OperationResult;
    using InterfaceInfo = IPTunnelTypes::InterfaceInfo;
    using Statistics = IPTunnelTypes::Statistics;
    using Error = IPTunnelTypes::Error;
    using RoutePlan = IPTunnelTypes::RoutePlan;

    explicit IPTunnel(QObject *parent = nullptr);
    ~IPTunnel() override;

    static Capabilities capabilities();
    static ValidationResult validate(const Configuration& configuration);
    static ValidationResult validatePacket(const QByteArray& packet, int mtu = 65535);
    static OperationResult planRouting(const RoutingConfiguration& routing);

    RequestId open(const Configuration& configuration);
    RequestId previewRouting(const RoutingConfiguration& routing);
    RequestId applyRouting(const RoutingConfiguration& routing);
    void close();

    State state() const;
    InterfaceInfo interfaceInfo() const;
    Statistics statistics() const;
    bool takeOutboundPacket(QByteArray& packet);
    EnqueueResult injectInboundPacket(const QByteArray& packet);

signals:
    void outboundPacketsAvailable();
    void inboundCapacityAvailable();
    void stateChanged(IPTunnelTypes::State state);
    void operationFinished(IPTunnelTypes::RequestId id, IPTunnelTypes::OperationResult result);
    void errorOccurred(IPTunnelTypes::Error error);

private:
    IPTunnelPrivate *m_private;
    friend class IPTunnelPrivate;
};

#endif // INCLUDE_IPTUNNEL_H
