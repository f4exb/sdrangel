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

#ifndef INCLUDE_IPTUNNELTYPES_H
#define INCLUDE_IPTUNNELTYPES_H

#include <QHostAddress>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

#include "export.h"

/**
 * The value types of IPTunnel's API, shared by IPTunnel, its platform backends, the routing
 * planner and the Windows helper's protocol. IPTunnel re-exports each of them under its own name
 * (IPTunnel::Configuration and so on).
 */
namespace IPTunnelTypes
{
    //! Identifies an asynchronous request (open, previewRouting, applyRouting) in operationFinished
    using RequestId = quint64;

    //! The tunnel's life cycle, as stateChanged reports it
    enum class State { Closed, Opening, Open, Closing, Error };
    //! How the OS interface is obtained: created, an existing one opened by name, or a descriptor adopted
    enum class OpenMode { CreateNew, OpenExisting, AdoptDescriptor };
    //! Managed: the tunnel sets its addresses and routes; External: it changes neither
    enum class ConfigurationMode { Managed, External };
    //! The outcome of IPTunnel::injectInboundPacket (Accepted is queued for the OS, not delivered)
    enum class EnqueueResult { Accepted, QueueFull, NotOpen, InvalidPacket, PacketTooLarge };
    //! The kind of an Error
    enum class ErrorCode {
        None, InvalidConfiguration, Busy, PermissionDenied, DependencyMissing,
        Unsupported, ConfigurationConflict, DeviceRemoved, QueueOverflow,
        NativeFailure
    };

    /**
     * An IPv4 or IPv6 prefix: an address and a prefix length (-1 until set). Two prefixes compare
     * equal when they cover the same addresses, whatever host bits their addresses have.
     */
    struct SDRBASE_API Prefix
    {
        QHostAddress address;
        int length = -1;

        bool isValid() const;
        bool isDefault() const { return length == 0; }
        QString toString() const;
        bool operator==(const Prefix& other) const;
    };

    //! An address given to the tunnel's interface, with its prefix length and, for a point-to-point link, its peer
    struct Address
    {
        QHostAddress address;
        int prefixLength = -1;
        QHostAddress peer;
    };

    /**
     * The bounds of one of the tunnel's two packet queues (from the OS to the consumer, and from the
     * consumer to the OS): packets and bytes held, and the age after which a packet is dropped (0
     * keeps packets however old they are)
     */
    struct QueueLimits
    {
        int packets = 64;
        qint64 bytes = 128 * 1024;
        int maximumAgeMs = 10000;
    };

    /**
     * The routes the tunnel carries: the included prefixes less the excluded ones (planned into
     * disjoint routes by iptunnelPlanRouting). A default route needs allowDefaultRoute. External mode
     * leaves the OS's routes alone.
     */
    struct RoutingConfiguration
    {
        ConfigurationMode mode = ConfigurationMode::Managed;
        QVector<Prefix> include;
        QVector<Prefix> exclude;
        bool allowDefaultRoute = false;
        int metric = 0;
    };

    /**
     * Everything IPTunnel::open needs: the interface's names and how it is obtained, its addresses,
     * the IP families it carries, its MTU, the two queues' limits and its routing. adoptedDescriptor
     * and takeDescriptorOwnership are for OpenMode::AdoptDescriptor (Linux and macOS);
     * wintunRingCapacity is the Wintun ring's size on Windows.
     */
    struct Configuration
    {
        QString logicalId;
        QString interfaceName;
        QString displayName = QStringLiteral("SDRangel IP Tunnel");
        OpenMode openMode = OpenMode::CreateNew;
        ConfigurationMode configurationMode = ConfigurationMode::Managed;
        QVector<Address> addresses;
        bool ipv4 = true;
        bool ipv6 = true;
        int mtu = 1280;
        QueueLimits outboundQueue;
        QueueLimits inboundQueue;
        RoutingConfiguration routing;
        qintptr adoptedDescriptor = -1;
        bool takeDescriptorOwnership = true;
        int wintunRingCapacity = 0x200000;
    };

    //! What this platform's backend supports (IPTunnel::capabilities), and its name
    struct Capabilities
    {
        bool available = false;
        bool ipv4 = false;
        bool ipv6 = false;
        bool managedAddresses = false;
        bool managedRoutes = false;
        bool routeMetric = false;
        bool namedCreate = false;
        bool namedAttach = false;
        bool descriptorAdoption = false;
        QString backend;
    };

    //! The OS interface the tunnel opened: its name, its index and its MTU
    struct InterfaceInfo
    {
        QString name;
        quint32 index = 0;
        int mtu = 0;
    };

    //! A failure: its kind, the operation it happened in, a message and the OS's own error code, if any
    struct Error
    {
        ErrorCode code = ErrorCode::None;
        QString operation;
        QString message;
        qint64 nativeCode = 0;

        bool isError() const { return code != ErrorCode::None; }
    };

    //! The disjoint routes a RoutingConfiguration produces, and warnings (an exclusion outside every included prefix)
    struct RoutePlan
    {
        QVector<Prefix> routes;
        QStringList warnings;
    };

    //! Whether a configuration or a packet is acceptable, and if not why
    struct ValidationResult
    {
        bool valid = true;
        Error error;
    };

    //! The result of an asynchronous request (operationFinished) or of planRouting, with its route plan
    struct OperationResult
    {
        bool success = true;
        Error error;
        RoutePlan routePlan;
    };

    //! The tunnel's counters since it was opened, and its queues' current depths
    struct Statistics
    {
        quint64 packetsReadFromOS = 0;
        quint64 bytesReadFromOS = 0;
        quint64 packetsTakenByConsumer = 0;
        quint64 bytesTakenByConsumer = 0;
        quint64 packetsAcceptedForInjection = 0;
        quint64 bytesAcceptedForInjection = 0;
        quint64 packetsWrittenToOS = 0;
        quint64 bytesWrittenToOS = 0;
        quint64 invalidPackets = 0;
        quint64 oversizedPackets = 0;
        quint64 outboundOverflow = 0;
        quint64 inboundOverflow = 0;
        quint64 outboundExpired = 0;
        quint64 inboundExpired = 0;
        quint64 nativeFailures = 0;
        int outboundQueuedPackets = 0;
        qint64 outboundQueuedBytes = 0;
        int inboundQueuedPackets = 0;
        qint64 inboundQueuedBytes = 0;
    };
}

Q_DECLARE_METATYPE(IPTunnelTypes::State)
Q_DECLARE_METATYPE(IPTunnelTypes::Error)
Q_DECLARE_METATYPE(IPTunnelTypes::OperationResult)
Q_DECLARE_METATYPE(IPTunnelTypes::InterfaceInfo)

#endif // INCLUDE_IPTUNNELTYPES_H
