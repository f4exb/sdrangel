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
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/kern_control.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <QProcess>

#include "iptunnelbackend.h"
#include "iptunnel.h"

using namespace IPTunnelTypes;

namespace {

constexpr int UtunOptionInterfaceName = 2;

Error macError(ErrorCode code, const QString& operation, const QString& message, int nativeCode = errno)
{
    return {code, operation, nativeCode ? message + QStringLiteral(": ") + QString::fromLocal8Bit(std::strerror(nativeCode)) : message, nativeCode};
}

QString ipv4Mask(int prefix)
{
    quint32 value = (prefix == 0) ? 0 : 0xffffffffu << (32 - prefix);
    return QStringLiteral("%1.%2.%3.%4").arg((value >> 24) & 255).arg((value >> 16) & 255).arg((value >> 8) & 255).arg(value & 255);
}

/**
 * The macOS utun backend. Creates a utun interface through the com.apple.net.utun_control kernel
 * control, or adopts an already open utun descriptor (attaching to an existing utun by name is not
 * possible). Unless the configuration is external, it configures the interface's MTU, addresses
 * and routes by running /sbin/ifconfig and /sbin/route, and removes only those it added when it
 * closes. Its thread polls the utun descriptor and a pipe that wake() writes to; utun frames each
 * packet with a 4 byte address family, added on write and removed on read.
 */
class MacBackend final : public IPTunnelBackend
{
public:
    MacBackend(const Configuration& configuration, const Callbacks& callbacks) :
        IPTunnelBackend(configuration, callbacks)
    {
        if (::pipe(m_wakePipe) == 0)
        {
            for (int fd : m_wakePipe)
            {
                fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
                fcntl(fd, F_SETFD, FD_CLOEXEC);
            }
        }
        else
        {
            m_wakePipe[0] = -1;
            m_wakePipe[1] = -1;
        }
    }

    ~MacBackend() override
    {
        for (int fd : m_wakePipe)
        {
            if (fd >= 0) {
                ::close(fd);
            }
        }
    }

protected:
    void wake() override
    {
        const char byte = 1;

        if (m_wakePipe[1] >= 0) {
            (void) ::write(m_wakePipe[1], &byte, 1);
        }
    }

    void run() override
    {
        Error error;
        InterfaceInfo info;

        if (!openTunnel(info, error) || !configure(error))
        {
            cleanup();
            m_callbacks.opened({}, error);
            return;
        }

        m_callbacks.opened(info, {});
        pollfd descriptors[2] {{m_fd, POLLIN, 0}, {m_wakePipe[0], POLLIN, 0}};

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
                m_callbacks.error(macError(ErrorCode::NativeFailure, QStringLiteral("poll"), QStringLiteral("utun poll failed")), true);
                break;
            }

            if (descriptors[1].revents & POLLIN)
            {
                char bytes[64];

                while (::read(m_wakePipe[0], bytes, sizeof(bytes)) > 0) {
                }
            }

            if (descriptors[0].revents & POLLIN) {
                drainReads();
            }

            if (descriptors[0].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                m_callbacks.error(macError(ErrorCode::DeviceRemoved, QStringLiteral("read"), QStringLiteral("utun device closed"), 0), true);
                break;
            }
        }

        cleanup();
    }

private:
    int m_fd = -1;
    int m_wakePipe[2] {-1, -1};
    QString m_name;
    QVector<Address> m_ownedAddresses;
    QVector<Prefix> m_ownedRoutes;
    QByteArray m_pendingWrite;

    bool openTunnel(InterfaceInfo& info, Error& error)
    {
        if (m_configuration.openMode == OpenMode::OpenExisting)
        {
            error = macError(ErrorCode::Unsupported, QStringLiteral("open"), QStringLiteral("macOS cannot attach to an existing utun by name"), 0);
            return false;
        }

        if (m_configuration.openMode == OpenMode::AdoptDescriptor)
        {
            m_fd = m_configuration.takeDescriptorOwnership ? int(m_configuration.adoptedDescriptor) : ::dup(int(m_configuration.adoptedDescriptor));

            if (m_fd < 0)
            {
                error = macError(ErrorCode::NativeFailure, QStringLiteral("open"), QStringLiteral("Unable to adopt utun descriptor"));
                return false;
            }
        }
        else
        {
            m_fd = ::socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);

            if (m_fd < 0)
            {
                error = macError(((errno == EPERM) || (errno == EACCES)) ? ErrorCode::PermissionDenied : ErrorCode::NativeFailure,
                    QStringLiteral("open"), QStringLiteral("Unable to open utun control socket"));
                return false;
            }

            ctl_info control {};
            std::strncpy(control.ctl_name, "com.apple.net.utun_control", sizeof(control.ctl_name) - 1);

            if (ioctl(m_fd, CTLIOCGINFO, &control) < 0)
            {
                error = macError(ErrorCode::DependencyMissing, QStringLiteral("open"), QStringLiteral("Unable to resolve utun kernel control"));
                return false;
            }

            sockaddr_ctl address {};
            address.sc_len = sizeof(address);
            address.sc_family = AF_SYSTEM;
            address.ss_sysaddr = AF_SYS_CONTROL;
            address.sc_id = control.ctl_id;
            address.sc_unit = 0;

            if (::connect(m_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
            {
                error = macError(((errno == EPERM) || (errno == EACCES)) ? ErrorCode::PermissionDenied : ErrorCode::NativeFailure,
                    QStringLiteral("open"), QStringLiteral("Unable to create utun interface"));
                return false;
            }
        }

        char name[IFNAMSIZ] {};
        socklen_t nameLength = sizeof(name);

        if (getsockopt(m_fd, SYSPROTO_CONTROL, UtunOptionInterfaceName, name, &nameLength) < 0)
        {
            error = macError(ErrorCode::NativeFailure, QStringLiteral("open"), QStringLiteral("Unable to obtain utun name"));
            return false;
        }

        m_name = QString::fromLocal8Bit(name);
        int flags = fcntl(m_fd, F_GETFL, 0);
        fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);
        flags = fcntl(m_fd, F_GETFD, 0);
        fcntl(m_fd, F_SETFD, flags | FD_CLOEXEC);
        info.name = m_name;
        info.index = if_nametoindex(name);
        info.mtu = m_configuration.mtu;
        return true;
    }

    bool command(const QString& program, const QStringList& arguments, bool allowExists, Error& error, bool *changed = nullptr)
    {
        if (changed) {
            *changed = false;
        }

        QProcess process;
        process.start(program, arguments);

        if (!process.waitForStarted() || !process.waitForFinished(10000))
        {
            process.kill();
            error = macError(ErrorCode::NativeFailure, QStringLiteral("configure"), QStringLiteral("Unable to run %1").arg(program), 0);
            return false;
        }

        if ((process.exitStatus() == QProcess::NormalExit) && (process.exitCode() == 0))
        {
            if (changed) {
                *changed = true;
            }

            return true;
        }

        QString detail = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();

        if (allowExists && (detail.contains(QStringLiteral("File exists"), Qt::CaseInsensitive) || detail.contains(QStringLiteral("already exists"), Qt::CaseInsensitive))) {
            return true;
        }

        ErrorCode code = detail.contains(QStringLiteral("not permitted"), Qt::CaseInsensitive) ? ErrorCode::PermissionDenied : ErrorCode::NativeFailure;
        error = macError(code, QStringLiteral("configure"), detail.isEmpty() ? QStringLiteral("%1 failed").arg(program) : detail, 0);
        return false;
    }

    bool configure(Error& error)
    {
        if (m_configuration.configurationMode == ConfigurationMode::External) {
            return true;
        }

        if (!command(QStringLiteral("/sbin/ifconfig"), {m_name, QStringLiteral("mtu"), QString::number(m_configuration.mtu), QStringLiteral("up")}, false, error)) {
            return false;
        }

        for (const Address& address : m_configuration.addresses)
        {
            QStringList args {m_name};

            if (address.address.protocol() == QAbstractSocket::IPv4Protocol)
            {
                args << QStringLiteral("inet") << address.address.toString();

                if (!address.peer.isNull()) {
                    args << address.peer.toString();
                }

                args << QStringLiteral("netmask") << ipv4Mask(address.prefixLength) << QStringLiteral("alias");
            }
            else
            {
                args << QStringLiteral("inet6") << QStringLiteral("%1/%2").arg(address.address.toString()).arg(address.prefixLength) << QStringLiteral("alias");
            }

            bool added = false;

            if (!command(QStringLiteral("/sbin/ifconfig"), args, true, error, &added)) {
                return false;
            }

            if (added) {
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

    bool changeRoute(const Prefix& prefix, bool add, bool allowExists, Error& error, bool *changed = nullptr)
    {
        QStringList args {QStringLiteral("-n"), add ? QStringLiteral("add") : QStringLiteral("delete")};

        if (prefix.address.protocol() == QAbstractSocket::IPv6Protocol) {
            args << QStringLiteral("-inet6");
        }

        args << QStringLiteral("-net") << prefix.toString() << QStringLiteral("-interface") << m_name;
        return command(QStringLiteral("/sbin/route"), args, allowExists, error, changed);
    }

    OperationResult replaceRoutes(const RoutingConfiguration& routing)
    {
        OperationResult result = IPTunnel::planRouting(routing);

        if (!result.success) {
            return result;
        }

        if ((routing.mode != ConfigurationMode::External) && (routing.metric != 0))
        {
            result.success = false;
            result.error = macError(ErrorCode::Unsupported, QStringLiteral("routing"), QStringLiteral("Portable route metrics are not supported by the macOS backend"), 0);
            return result;
        }

        QVector<Prefix> desired = (routing.mode == ConfigurationMode::External) ? QVector<Prefix>() : result.routePlan.routes;
        QVector<Prefix> added;

        for (const Prefix& prefix : desired)
        {
            if (std::any_of(m_ownedRoutes.cbegin(), m_ownedRoutes.cend(), [&](const Prefix& p) { return p == prefix; })) {
                continue;
            }

            Error error;
            bool addedRoute = false;

            if (!changeRoute(prefix, true, true, error, &addedRoute))
            {
                for (const Prefix& item : added)
                {
                    Error ignored;
                    changeRoute(item, false, true, ignored);
                }

                result.success = false;
                result.error = error;
                return result;
            }

            if (addedRoute) {
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
                Error ignored;
                changeRoute(old, false, true, ignored);
            }
        }

        kept += added;
        m_ownedRoutes = kept;
        return result;
    }

    void drainRouting()
    {
        RoutingCommand c;

        while (takeRoutingCommand(c)) {
            m_callbacks.routingFinished(c.id, replaceRoutes(c.routing));
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

            quint32 family = htonl(((quint8(packet[0]) >> 4) == 4) ? AF_INET : AF_INET6);
            QByteArray framed(reinterpret_cast<const char *>(&family), 4);
            framed += packet;
            ssize_t length;

            do {
                length = ::write(m_fd, framed.constData(), size_t(framed.size()));
            } while ((length < 0) && (errno == EINTR));

            if ((length < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
            {
                m_pendingWrite = std::move(packet);
                return;
            }

            if (length != framed.size()) {
                m_callbacks.error(macError(ErrorCode::NativeFailure, QStringLiteral("write"), QStringLiteral("Unable to inject utun packet"), (length < 0) ? errno : EIO), false);
            } else {
                m_callbacks.packetWritten(packet.size());
            }
        }
    }

    void drainReads()
    {
        QByteArray buffer(65539, Qt::Uninitialized);

        for (;;)
        {
            ssize_t length = ::read(m_fd, buffer.data(), size_t(buffer.size()));

            if ((length < 0) && (errno == EINTR)) {
                continue;
            }

            if ((length < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
                return;
            }

            if (length <= 4)
            {
                if (length < 0) {
                    m_callbacks.error(macError(ErrorCode::NativeFailure, QStringLiteral("read"), QStringLiteral("utun read failed")), false);
                }

                return;
            }

            quint32 family;
            std::memcpy(&family, buffer.constData(), 4);
            family = ntohl(family);

            if ((family != AF_INET) && (family != AF_INET6))
            {
                m_callbacks.error(macError(ErrorCode::NativeFailure, QStringLiteral("read"), QStringLiteral("utun returned an unknown address family"), 0), false);
                continue;
            }

            m_callbacks.packetReceived(QByteArray(buffer.constData() + 4, int(length - 4)));
        }
    }

    void cleanup()
    {
        for (const Prefix& route : m_ownedRoutes)
        {
            Error ignored;
            changeRoute(route, false, true, ignored);
        }

        for (const Address& address : m_ownedAddresses)
        {
            QStringList args {m_name, (address.address.protocol() == QAbstractSocket::IPv4Protocol) ? QStringLiteral("inet") : QStringLiteral("inet6"), address.address.toString(), QStringLiteral("-alias")};
            Error ignored;
            command(QStringLiteral("/sbin/ifconfig"), args, true, ignored);
        }

        m_ownedRoutes.clear();
        m_ownedAddresses.clear();

        if (m_fd >= 0)
        {
            ::close(m_fd);
            m_fd = -1;
        }
    }
};

}

IPTunnelBackend *iptunnelCreateBackend(const Configuration& configuration, const IPTunnelBackend::Callbacks& callbacks)
{
    return new MacBackend(configuration, callbacks);
}

Capabilities iptunnelBackendCapabilities()
{
    Capabilities c;
    c.available = true;
    c.ipv4 = true;
    c.ipv6 = true;
    c.managedAddresses = true;
    c.managedRoutes = true;
    c.namedCreate = false;
    c.namedAttach = false;
    c.descriptorAdoption = true;
    c.routeMetric = false;
    c.backend = QStringLiteral("macOS utun");
    return c;
}
