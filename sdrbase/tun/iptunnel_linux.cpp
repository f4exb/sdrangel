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

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>

#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "iptunnelbackend.h"
#include "iptunnel.h"

using namespace IPTunnelTypes;

namespace {

Error linuxError(ErrorCode code, const QString& operation, const QString& message, int nativeCode = errno)
{
    return {code, operation, nativeCode ? message + QStringLiteral(": ") + QString::fromLocal8Bit(std::strerror(nativeCode)) : message, nativeCode};
}

QByteArray addressBytes(const QHostAddress& address)
{
    QByteArray result;

    if (address.protocol() == QAbstractSocket::IPv4Protocol)
    {
        result.resize(4);
        quint32 value = htonl(address.toIPv4Address());
        std::memcpy(result.data(), &value, 4);
    }
    else
    {
        result.resize(16);
        Q_IPV6ADDR value = address.toIPv6Address();
        std::memcpy(result.data(), value.c, 16);
    }

    return result;
}

bool addAttribute(nlmsghdr *header, size_t capacity, int type, const void *data, size_t length)
{
    const size_t aligned = NLMSG_ALIGN(header->nlmsg_len);
    const size_t attributeLength = RTA_LENGTH(length);

    if (aligned + RTA_ALIGN(attributeLength) > capacity) {
        return false;
    }

    auto *attribute = reinterpret_cast<rtattr *>(reinterpret_cast<char *>(header) + aligned);
    attribute->rta_type = type;
    attribute->rta_len = static_cast<unsigned short>(attributeLength);
    std::memcpy(RTA_DATA(attribute), data, length);
    header->nlmsg_len = static_cast<unsigned int>(aligned + RTA_ALIGN(attributeLength));
    return true;
}

/**
 * The Linux TUN backend. Opens /dev/net/tun (creating or attaching to a named interface) or
 * adopts an already open TUN descriptor, and, unless the configuration is external, brings the
 * interface up, sets its MTU, and adds the configured addresses and routes over a routing netlink
 * socket. Its thread polls the TUN descriptor and an eventfd that wake() signals, moving packets
 * between the OS and the backend's queues. Only the addresses and routes it added itself are
 * removed when it closes.
 */
class LinuxBackend final : public IPTunnelBackend
{
public:
    LinuxBackend(const Configuration& configuration, const Callbacks& callbacks) :
        IPTunnelBackend(configuration, callbacks),
        m_wakeFd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))
    {
    }

    ~LinuxBackend() override
    {
        if (m_wakeFd >= 0) {
            ::close(m_wakeFd);
        }
    }

protected:
    void wake() override
    {
        uint64_t one = 1;

        if (m_wakeFd >= 0) {
            (void) ::write(m_wakeFd, &one, sizeof(one));
        }
    }

    void run() override
    {
        Error error;
        InterfaceInfo info;

        if (!openTunnel(info, error) || !openNetlink(error) || !configure(error))
        {
            cleanup();
            m_callbacks.opened({}, error);
            return;
        }

        m_callbacks.opened(info, {});
        pollfd descriptors[2] {{m_tunFd, POLLIN, 0}, {m_wakeFd, POLLIN, 0}};

        while (!stopping())
        {
            drainRouting();
            drainWrites();
            descriptors[0].events = short(POLLIN | (m_pendingWrite.isEmpty() ? 0 : POLLOUT));
            int rc;

            do {
                rc = ::poll(descriptors, 2, -1);
            } while ((rc < 0) && (errno == EINTR));

            if (rc < 0)
            {
                m_callbacks.error(linuxError(ErrorCode::NativeFailure, QStringLiteral("poll"), QStringLiteral("Tunnel poll failed")), true);
                break;
            }

            if (descriptors[1].revents & POLLIN)
            {
                uint64_t value;

                while (::read(m_wakeFd, &value, sizeof(value)) > 0) {
                }
            }

            if (descriptors[0].revents & POLLIN) {
                drainReads();
            }

            if (descriptors[0].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                m_callbacks.error(linuxError(ErrorCode::DeviceRemoved, QStringLiteral("read"), QStringLiteral("Tunnel device was closed"), 0), true);
                break;
            }
        }

        cleanup();
    }

private:
    int m_tunFd = -1;
    int m_wakeFd = -1;
    int m_netlinkFd = -1;
    quint32 m_sequence = 0;
    int m_index = 0;
    QString m_name;
    QVector<Address> m_ownedAddresses;
    QVector<Prefix> m_ownedRoutes;
    QByteArray m_pendingWrite;

    bool openTunnel(InterfaceInfo& info, Error& error)
    {
        if (m_configuration.openMode == OpenMode::AdoptDescriptor)
        {
            m_tunFd = m_configuration.takeDescriptorOwnership ? int(m_configuration.adoptedDescriptor) : ::dup(int(m_configuration.adoptedDescriptor));

            if (m_tunFd < 0)
            {
                error = linuxError(ErrorCode::NativeFailure, QStringLiteral("open"), QStringLiteral("Unable to adopt TUN descriptor"));
                return false;
            }

            int flags = fcntl(m_tunFd, F_GETFL, 0);
            fcntl(m_tunFd, F_SETFL, flags | O_NONBLOCK);
            flags = fcntl(m_tunFd, F_GETFD, 0);
            fcntl(m_tunFd, F_SETFD, flags | FD_CLOEXEC);
            ifreq request {};

            if (ioctl(m_tunFd, TUNGETIFF, &request) < 0)
            {
                error = linuxError(ErrorCode::NativeFailure, QStringLiteral("open"), QStringLiteral("Descriptor is not an attached TUN device"));
                return false;
            }

            m_name = QString::fromLocal8Bit(request.ifr_name);
        }
        else
        {
            QByteArray requested = m_configuration.interfaceName.toLocal8Bit();

            if (requested.size() >= IFNAMSIZ)
            {
                error = linuxError(ErrorCode::InvalidConfiguration, QStringLiteral("open"), QStringLiteral("TUN interface name is too long"), 0);
                return false;
            }

            if ((m_configuration.openMode == OpenMode::OpenExisting) && (if_nametoindex(requested.constData()) == 0))
            {
                error = linuxError(ErrorCode::ConfigurationConflict, QStringLiteral("open"), QStringLiteral("Requested TUN interface does not exist"), 0);
                return false;
            }

            m_tunFd = ::open("/dev/net/tun", O_RDWR | O_NONBLOCK | O_CLOEXEC);

            if (m_tunFd < 0)
            {
                error = linuxError(((errno == EACCES) || (errno == EPERM)) ? ErrorCode::PermissionDenied : ErrorCode::DependencyMissing,
                    QStringLiteral("open"), QStringLiteral("Unable to open /dev/net/tun"));
                return false;
            }

            ifreq request {};
            request.ifr_flags = IFF_TUN | IFF_NO_PI;

            if (!requested.isEmpty()) {
                std::strncpy(request.ifr_name, requested.constData(), IFNAMSIZ - 1);
            }

            if (ioctl(m_tunFd, TUNSETIFF, &request) < 0)
            {
                error = linuxError(((errno == EACCES) || (errno == EPERM)) ? ErrorCode::PermissionDenied : ErrorCode::NativeFailure,
                    QStringLiteral("open"), QStringLiteral("Unable to attach TUN interface"));
                return false;
            }

            m_name = QString::fromLocal8Bit(request.ifr_name);
        }

        m_index = int(if_nametoindex(m_name.toLocal8Bit().constData()));

        if (!m_index)
        {
            error = linuxError(ErrorCode::NativeFailure, QStringLiteral("open"), QStringLiteral("Unable to resolve TUN interface index"));
            return false;
        }

        info.name = m_name;
        info.index = quint32(m_index);
        info.mtu = m_configuration.mtu;
        return true;
    }

    bool openNetlink(Error& error)
    {
        if (m_configuration.configurationMode == ConfigurationMode::External) {
            return true;
        }

        m_netlinkFd = ::socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);

        if (m_netlinkFd < 0)
        {
            error = linuxError(ErrorCode::NativeFailure, QStringLiteral("netlink"), QStringLiteral("Unable to open routing netlink socket"));
            return false;
        }

        sockaddr_nl address {};
        address.nl_family = AF_NETLINK;

        if (::bind(m_netlinkFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
        {
            error = linuxError(ErrorCode::NativeFailure, QStringLiteral("netlink"), QStringLiteral("Unable to bind routing netlink socket"));
            return false;
        }

        return true;
    }

    int transact(nlmsghdr *header)
    {
        header->nlmsg_seq = ++m_sequence;
        sockaddr_nl kernel {};
        kernel.nl_family = AF_NETLINK;
        iovec iov {header, header->nlmsg_len};
        msghdr message {&kernel, sizeof(kernel), &iov, 1, nullptr, 0, 0};

        if (::sendmsg(m_netlinkFd, &message, 0) < 0) {
            return errno;
        }

        char response[8192];

        for (;;)
        {
            ssize_t length = ::recv(m_netlinkFd, response, sizeof(response), 0);

            if (length < 0)
            {
                if (errno == EINTR) {
                    continue;
                }

                return errno;
            }

            for (nlmsghdr *item = reinterpret_cast<nlmsghdr *>(response); NLMSG_OK(item, length); item = NLMSG_NEXT(item, length))
            {
                if (item->nlmsg_seq != header->nlmsg_seq) {
                    continue;
                }

                if (item->nlmsg_type == NLMSG_ERROR)
                {
                    auto *reply = reinterpret_cast<nlmsgerr *>(NLMSG_DATA(item));
                    return reply->error ? -reply->error : 0;
                }
            }
        }
    }

    bool setLink(Error& error)
    {
        struct
        {
            nlmsghdr header;
            ifinfomsg info;
            char attributes[64];
        } request {};

        request.header.nlmsg_len = NLMSG_LENGTH(sizeof(ifinfomsg));
        request.header.nlmsg_type = RTM_NEWLINK;
        request.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
        request.info.ifi_family = AF_UNSPEC;
        request.info.ifi_index = m_index;
        request.info.ifi_flags = IFF_UP;
        request.info.ifi_change = IFF_UP;
        quint32 mtu = quint32(m_configuration.mtu);
        addAttribute(&request.header, sizeof(request), IFLA_MTU, &mtu, sizeof(mtu));
        int rc = transact(&request.header);

        if (rc)
        {
            error = linuxError(((rc == EPERM) || (rc == EACCES)) ? ErrorCode::PermissionDenied : ErrorCode::NativeFailure,
                QStringLiteral("link"), QStringLiteral("Unable to bring up TUN interface or set its MTU"), rc);
            return false;
        }

        return true;
    }

    bool changeAddress(const Address& address, bool add, bool& owned, Error& error)
    {
        struct
        {
            nlmsghdr header;
            ifaddrmsg info;
            char attributes[128];
        } request {};

        request.header.nlmsg_len = NLMSG_LENGTH(sizeof(ifaddrmsg));
        request.header.nlmsg_type = add ? RTM_NEWADDR : RTM_DELADDR;
        request.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | (add ? (NLM_F_CREATE | NLM_F_EXCL) : 0);
        request.info.ifa_family = (address.address.protocol() == QAbstractSocket::IPv4Protocol) ? AF_INET : AF_INET6;
        request.info.ifa_prefixlen = quint8(address.prefixLength);
        request.info.ifa_scope = RT_SCOPE_UNIVERSE;
        request.info.ifa_index = quint32(m_index);
        QByteArray local = addressBytes(address.address);
        QByteArray peer = address.peer.isNull() ? local : addressBytes(address.peer);
        addAttribute(&request.header, sizeof(request), IFA_LOCAL, local.constData(), size_t(local.size()));
        addAttribute(&request.header, sizeof(request), IFA_ADDRESS, peer.constData(), size_t(peer.size()));
        int rc = transact(&request.header);
        owned = add && (rc == 0);

        if ((add && (rc == EEXIST)) || (!add && ((rc == EADDRNOTAVAIL) || (rc == ENOENT)))) {
            return true;
        }

        if (rc)
        {
            error = linuxError(((rc == EPERM) || (rc == EACCES)) ? ErrorCode::PermissionDenied : ErrorCode::NativeFailure,
                QStringLiteral("address"), QStringLiteral("Unable to %1 address %2").arg(add ? QStringLiteral("add") : QStringLiteral("remove"), address.address.toString()), rc);
            return false;
        }

        return true;
    }

    bool changeRoute(const Prefix& prefix, int metric, bool add, bool& owned, Error& error)
    {
        struct
        {
            nlmsghdr header;
            rtmsg route;
            char attributes[128];
        } request {};

        request.header.nlmsg_len = NLMSG_LENGTH(sizeof(rtmsg));
        request.header.nlmsg_type = add ? RTM_NEWROUTE : RTM_DELROUTE;
        request.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | (add ? (NLM_F_CREATE | NLM_F_EXCL) : 0);
        request.route.rtm_family = (prefix.address.protocol() == QAbstractSocket::IPv4Protocol) ? AF_INET : AF_INET6;
        request.route.rtm_dst_len = quint8(prefix.length);
        request.route.rtm_table = RT_TABLE_MAIN;
        request.route.rtm_protocol = RTPROT_STATIC;
        request.route.rtm_scope = RT_SCOPE_LINK;
        request.route.rtm_type = RTN_UNICAST;

        if (prefix.length)
        {
            QByteArray destination = addressBytes(prefix.address);
            addAttribute(&request.header, sizeof(request), RTA_DST, destination.constData(), size_t(destination.size()));
        }

        quint32 index = quint32(m_index);
        addAttribute(&request.header, sizeof(request), RTA_OIF, &index, sizeof(index));

        if (metric > 0)
        {
            quint32 value = quint32(metric);
            addAttribute(&request.header, sizeof(request), RTA_PRIORITY, &value, sizeof(value));
        }

        int rc = transact(&request.header);
        owned = add && (rc == 0);

        if ((add && (rc == EEXIST)) || (!add && ((rc == ESRCH) || (rc == ENOENT)))) {
            return true;
        }

        if (rc)
        {
            error = linuxError(((rc == EPERM) || (rc == EACCES)) ? ErrorCode::PermissionDenied : ErrorCode::ConfigurationConflict,
                QStringLiteral("route"), QStringLiteral("Unable to %1 route %2").arg(add ? QStringLiteral("add") : QStringLiteral("remove"), prefix.toString()), rc);
            return false;
        }

        return true;
    }

    bool configure(Error& error)
    {
        if (m_configuration.configurationMode == ConfigurationMode::External) {
            return true;
        }

        if (!setLink(error)) {
            return false;
        }

        for (const Address& address : m_configuration.addresses)
        {
            bool owned = false;

            if (!changeAddress(address, true, owned, error)) {
                return false;
            }

            if (owned) {
                m_ownedAddresses.append(address);
            }
        }

        OperationResult result = replaceRoutes(m_configuration.routing);

        if (!result.success)
        {
            error = result.error;
            return false;
        }

        return true;
    }

    OperationResult replaceRoutes(const RoutingConfiguration& routing)
    {
        OperationResult result = IPTunnel::planRouting(routing);

        if (!result.success) {
            return result;
        }

        QVector<Prefix> desired = (routing.mode == ConfigurationMode::External) ? QVector<Prefix>() : result.routePlan.routes;
        QVector<Prefix> added;

        for (const Prefix& prefix : desired)
        {
            if (std::any_of(m_ownedRoutes.cbegin(), m_ownedRoutes.cend(), [&](const Prefix& p) { return p == prefix; })) {
                continue;
            }

            bool owned = false;
            Error error;

            if (!changeRoute(prefix, routing.metric, true, owned, error))
            {
                for (const Prefix& item : added)
                {
                    bool ignored;
                    Error e;
                    changeRoute(item, routing.metric, false, ignored, e);
                }

                result.success = false;
                result.error = error;
                return result;
            }

            if (owned) {
                added.append(prefix);
            }
        }

        QVector<Prefix> kept;

        for (const Prefix& old : m_ownedRoutes)
        {
            if (std::any_of(desired.cbegin(), desired.cend(), [&](const Prefix& p) { return p == old; }))
            {
                kept.append(old);
            }
            else
            {
                bool ignored;
                Error e;
                changeRoute(old, routing.metric, false, ignored, e);
            }
        }

        kept += added;
        m_ownedRoutes = kept;
        return result;
    }

    void drainRouting()
    {
        RoutingCommand command;

        while (takeRoutingCommand(command)) {
            m_callbacks.routingFinished(command.id, replaceRoutes(command.routing));
        }
    }

    void drainWrites()
    {
        for (;;)
        {
            QByteArray packet;

            if (!m_pendingWrite.isEmpty()) {
                packet = std::move(m_pendingWrite);
            } else if (!m_callbacks.takePacketToOS(packet)) {
                return;
            }

            ssize_t length;

            do {
                length = ::write(m_tunFd, packet.constData(), size_t(packet.size()));
            } while ((length < 0) && (errno == EINTR));

            if ((length < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
            {
                m_pendingWrite = std::move(packet);
                return;
            }

            if (length != packet.size())
            {
                m_callbacks.error(linuxError(ErrorCode::NativeFailure,
                    QStringLiteral("write"), QStringLiteral("Unable to inject complete TUN packet"), (length < 0) ? errno : EIO), false);
            }
            else
            {
                m_callbacks.packetWritten(length);
            }
        }
    }

    void drainReads()
    {
        QByteArray buffer(65535, Qt::Uninitialized);

        for (;;)
        {
            ssize_t length = ::read(m_tunFd, buffer.data(), size_t(buffer.size()));

            if ((length < 0) && (errno == EINTR)) {
                continue;
            }

            if ((length < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
                return;
            }

            if (length <= 0)
            {
                m_callbacks.error(linuxError(ErrorCode::DeviceRemoved, QStringLiteral("read"), QStringLiteral("TUN read failed"), (length < 0) ? errno : 0), true);
                return;
            }

            m_callbacks.packetReceived(QByteArray(buffer.constData(), int(length)));
        }
    }

    void cleanup()
    {
        if (m_netlinkFd >= 0)
        {
            for (const Prefix& route : m_ownedRoutes)
            {
                bool ignored;
                Error error;
                changeRoute(route, m_configuration.routing.metric, false, ignored, error);
            }

            for (const Address& address : m_ownedAddresses)
            {
                bool ignored;
                Error error;
                changeAddress(address, false, ignored, error);
            }

            ::close(m_netlinkFd);
            m_netlinkFd = -1;
        }

        m_ownedRoutes.clear();
        m_ownedAddresses.clear();

        if (m_tunFd >= 0)
        {
            ::close(m_tunFd);
            m_tunFd = -1;
        }
    }
};

}

IPTunnelBackend *iptunnelCreateBackend(const Configuration& configuration, const IPTunnelBackend::Callbacks& callbacks)
{
    return new LinuxBackend(configuration, callbacks);
}

Capabilities iptunnelBackendCapabilities()
{
    Capabilities c;
    c.available = true;
    c.ipv4 = true;
    c.ipv6 = true;
    c.managedAddresses = true;
    c.managedRoutes = true;
    c.routeMetric = true;
    c.namedCreate = true;
    c.namedAttach = true;
    c.descriptorAdoption = true;
    c.backend = QStringLiteral("Linux TUN");
    return c;
}
