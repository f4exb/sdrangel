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

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x06010000
#endif

#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>

#include "wintun.h"
#include "iptunnelbackend.h"
#include "iptunnel.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QUuid>

#include "iptunnelbrokerprotocol.h"

using namespace IPTunnelTypes;

namespace {

Error windowsError(ErrorCode code, const QString& operation, const QString& message, DWORD nativeCode = GetLastError())
{
    Error error {code, operation, message, qint64(nativeCode)};
    wchar_t *buffer = nullptr;

    if (nativeCode && FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, nativeCode, 0, reinterpret_cast<wchar_t *>(&buffer), 0, nullptr) && buffer)
    {
        error.message += QStringLiteral(": %1").arg(QString::fromWCharArray(buffer).trimmed());
        LocalFree(buffer);
    }

    return error;
}

bool samePrefix(const Prefix& a, const Prefix& b)
{
    return a == b;
}

bool processIsElevated()
{
    HANDLE token = nullptr;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    TOKEN_ELEVATION elevation {};
    DWORD size = 0;
    const bool result = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) && elevation.TokenIsElevated;
    CloseHandle(token);
    return result;
}

/**
 * The Windows backend, on Wintun. Run elevated (or built into sdrangel-tunnel-helper.exe), it loads
 * the wintun.dll beside the executable, creates or opens the adapter, sets its MTU, addresses and
 * routes, and moves packets between the Wintun session and the IPTunnel queues on its thread.
 * Run unelevated, it starts the helper through a UAC prompt instead, and relays the configuration,
 * routing requests and packets to it over a private local pipe authenticated with a random token
 * (IPTunnelBrokerProtocol).
 */
class WindowsBackend final : public IPTunnelBackend
{
public:
    WindowsBackend(const Configuration& configuration, const Callbacks& callbacks) :
        IPTunnelBackend(configuration, callbacks),
        m_wakeEvent(CreateEventW(nullptr, FALSE, FALSE, nullptr))
    {}

    ~WindowsBackend() override
    {
        if (m_wakeEvent) {
            CloseHandle(m_wakeEvent);
        }
    }

protected:
    void wake() override
    {
        if (m_wakeEvent) {
            SetEvent(m_wakeEvent);
        }
    }

    void run() override
    {
#ifdef IPTUNNEL_ELEVATED_HELPER
        runDirect();
#else
        if (!processIsElevated()) {
            runBroker();
        } else {
            runDirect();
        }
#endif
    }

private:
    void runDirect()
    {
        Error error;
        InterfaceInfo info;

        if (!load(error) || !openAdapter(info, error) || !configure(error) || !startSession(error))
        {
            cleanup();
            m_callbacks.opened({}, error);
            return;
        }

        m_callbacks.opened(info, {});

        HANDLE handles[2] = {m_wakeEvent, m_getReadWaitEvent(m_session)};

        while (!stopping())
        {
            drainRouting();
            drainWrites();
            drainReads();

            if (stopping()) {
                break;
            }

            DWORD wait = WaitForMultipleObjects(2, handles, FALSE, m_pendingWrite.isEmpty() ? INFINITE : 10);

            if (wait == WAIT_TIMEOUT) {
                continue;
            }

            if (wait == WAIT_FAILED)
            {
                m_callbacks.error(windowsError(ErrorCode::NativeFailure, QStringLiteral("wait"), QStringLiteral("Wintun wait failed")), true);
                break;
            }
        }

        cleanup();
    }

    HMODULE m_library = nullptr;
    HANDLE m_wakeEvent = nullptr;
    WINTUN_ADAPTER_HANDLE m_adapter = nullptr;
    WINTUN_SESSION_HANDLE m_session = nullptr;
    NET_LUID m_luid {};
    QVector<MIB_UNICASTIPADDRESS_ROW> m_ownedAddresses;
    QVector<Prefix> m_ownedRoutes;
    QByteArray m_pendingWrite;

    WINTUN_CREATE_ADAPTER_FUNC *m_createAdapter = nullptr;
    WINTUN_OPEN_ADAPTER_FUNC *m_openAdapterFunction = nullptr;
    WINTUN_CLOSE_ADAPTER_FUNC *m_closeAdapter = nullptr;
    WINTUN_GET_ADAPTER_LUID_FUNC *m_getAdapterLuid = nullptr;
    WINTUN_START_SESSION_FUNC *m_startSessionFunction = nullptr;
    WINTUN_END_SESSION_FUNC *m_endSession = nullptr;
    WINTUN_GET_READ_WAIT_EVENT_FUNC *m_getReadWaitEvent = nullptr;
    WINTUN_RECEIVE_PACKET_FUNC *m_receivePacket = nullptr;
    WINTUN_RELEASE_RECEIVE_PACKET_FUNC *m_releaseReceivePacket = nullptr;
    WINTUN_ALLOCATE_SEND_PACKET_FUNC *m_allocateSendPacket = nullptr;
    WINTUN_SEND_PACKET_FUNC *m_sendPacket = nullptr;

    bool writeBrokerMessage(QLocalSocket& socket, IPTunnelBrokerProtocol::Message message, const QByteArray& payload = {})
    {
        const QByteArray data = IPTunnelBrokerProtocol::frame(message, payload);

        if ((socket.state() != QLocalSocket::ConnectedState) || (socket.write(data) != data.size())) {
            return false;
        }

        socket.flush();
        return true;
    }

    bool readBrokerMessage(QLocalSocket& socket, QByteArray& input, IPTunnelBrokerProtocol::Message& message,
        QByteArray& payload, int timeoutMs, HANDLE process = nullptr)
    {
        QElapsedTimer timer;
        timer.start();

        while (!stopping() && (timer.elapsed() < timeoutMs))
        {
            QString parseError;
            const auto parsed = IPTunnelBrokerProtocol::takeFrame(input, message, payload, parseError);

            if (parsed == IPTunnelBrokerProtocol::ParseResult::Ready) {
                return true;
            }

            if (parsed == IPTunnelBrokerProtocol::ParseResult::Invalid) {
                return false;
            }

            if (socket.state() == QLocalSocket::UnconnectedState) {
                return false;
            }

            if (process)
            {
                DWORD exitCode = STILL_ACTIVE;

                if (!GetExitCodeProcess(process, &exitCode) || (exitCode != STILL_ACTIVE)) {
                    return false;
                }
            }

            socket.waitForReadyRead(std::min(50, timeoutMs - int(timer.elapsed())));
            input.append(socket.readAll());
        }

        return false;
    }

    void stopBrokerProcess(QLocalSocket *socket, HANDLE process)
    {
        if (socket && (socket->state() == QLocalSocket::ConnectedState))
        {
            writeBrokerMessage(*socket, IPTunnelBrokerProtocol::Message::Close);
            socket->waitForBytesWritten(500);
            socket->disconnectFromServer();
            socket->waitForDisconnected(500);
        }

        if (process)
        {
            if (WaitForSingleObject(process, 3000) == WAIT_TIMEOUT)
            {
                TerminateProcess(process, 1);
                WaitForSingleObject(process, 1000);
            }

            CloseHandle(process);
        }
    }

    bool launchBroker(const QString& serverName, const QByteArray& token, HANDLE& process, Error& error)
    {
        const QString helperPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("sdrangel-tunnel-helper.exe"));

        if (!QFileInfo::exists(helperPath))
        {
            error = windowsError(ErrorCode::DependencyMissing, QStringLiteral("elevation"),
                QStringLiteral("Unable to find the elevated tunnel helper at %1").arg(helperPath), ERROR_FILE_NOT_FOUND);
            return false;
        }

        const QString parameters = QStringLiteral("--pipe %1 --token %2")
            .arg(serverName, QString::fromLatin1(token.toHex()));
        const QString directory = QCoreApplication::applicationDirPath();
        SHELLEXECUTEINFOW execute {};
        execute.cbSize = sizeof(execute);
        execute.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
        execute.lpVerb = L"runas";
        execute.lpFile = reinterpret_cast<LPCWSTR>(helperPath.utf16());
        execute.lpParameters = reinterpret_cast<LPCWSTR>(parameters.utf16());
        execute.lpDirectory = reinterpret_cast<LPCWSTR>(directory.utf16());
        execute.nShow = SW_HIDE;

        if (!ShellExecuteExW(&execute))
        {
            const DWORD code = GetLastError();
            error = windowsError(ErrorCode::PermissionDenied, QStringLiteral("elevation"),
                (code == ERROR_CANCELLED) ? QStringLiteral("Windows permission elevation was cancelled") :
                                            QStringLiteral("Unable to start the elevated tunnel helper"), code);
            return false;
        }

        process = execute.hProcess;

        if (!process)
        {
            error = windowsError(ErrorCode::NativeFailure, QStringLiteral("elevation"),
                QStringLiteral("Windows did not return a tunnel-helper process handle"), 0);
            return false;
        }

        return true;
    }

    QLocalSocket *acceptBroker(QLocalServer& server, HANDLE process, const QByteArray& token, Error& error)
    {
        QElapsedTimer timer;
        timer.start();

        while (!stopping() && (timer.elapsed() < 60000))
        {
            DWORD exitCode = STILL_ACTIVE;

            if (!GetExitCodeProcess(process, &exitCode) || (exitCode != STILL_ACTIVE))
            {
                error = windowsError(ErrorCode::NativeFailure, QStringLiteral("elevation"),
                    QStringLiteral("The elevated tunnel helper exited before connecting"), exitCode);
                return nullptr;
            }

            if (!server.hasPendingConnections()) {
                server.waitForNewConnection(100);
            }

            QLocalSocket *candidate = server.nextPendingConnection();

            if (!candidate) {
                continue;
            }

            QByteArray input;
            IPTunnelBrokerProtocol::Message message {};
            QByteArray payload;

            if (readBrokerMessage(*candidate, input, message, payload, 5000, process)
                && (message == IPTunnelBrokerProtocol::Message::Authenticate) && (payload == token))
            {
                candidate->setParent(nullptr);
                return candidate;
            }

            candidate->disconnectFromServer();
            delete candidate;
        }

        error = windowsError(ErrorCode::PermissionDenied, QStringLiteral("elevation"),
            QStringLiteral("Timed out waiting for the elevated tunnel helper"), WAIT_TIMEOUT);
        return nullptr;
    }

    bool processBrokerInput(QLocalSocket& socket, QByteArray& input)
    {
        input.append(socket.readAll());

        for (;;)
        {
            IPTunnelBrokerProtocol::Message message {};
            QByteArray payload;
            QString parseError;
            const auto parsed = IPTunnelBrokerProtocol::takeFrame(input, message, payload, parseError);

            if (parsed == IPTunnelBrokerProtocol::ParseResult::NeedMore) {
                return true;
            }

            if (parsed == IPTunnelBrokerProtocol::ParseResult::Invalid)
            {
                m_callbacks.error(windowsError(ErrorCode::NativeFailure, QStringLiteral("elevation"), parseError, 0), true);
                return false;
            }

            if (message == IPTunnelBrokerProtocol::Message::PacketFromOS)
            {
                if (payload.isEmpty() || (payload.size() > 65535)) {
                    return false;
                }

                m_callbacks.packetReceived(payload);
            }
            else if (message == IPTunnelBrokerProtocol::Message::PacketAccepted)
            {
                QDataStream stream(payload);
                qint64 size = 0;
                stream >> size;

                if ((stream.status() != QDataStream::Ok) || !stream.atEnd() || (size <= 0) || (size > 65535)) {
                    return false;
                }

                m_callbacks.packetWritten(size);
            }
            else if (message == IPTunnelBrokerProtocol::Message::RoutingFinished)
            {
                RequestId id = 0;
                OperationResult result;

                if (!IPTunnelBrokerProtocol::decodeRoutingResult(payload, id, result)) {
                    return false;
                }

                m_callbacks.routingFinished(id, result);
            }
            else if (message == IPTunnelBrokerProtocol::Message::Error)
            {
                Error brokerError;
                bool fatal = false;

                if (!IPTunnelBrokerProtocol::decodeError(payload, brokerError, fatal)) {
                    return false;
                }

                m_callbacks.error(brokerError, fatal);

                if (fatal) {
                    return false;
                }
            }
            else
            {
                m_callbacks.error(windowsError(ErrorCode::NativeFailure, QStringLiteral("elevation"),
                    QStringLiteral("The elevated tunnel helper sent an unexpected message"), 0), true);
                return false;
            }
        }
    }

    bool drainBrokerWrites(QLocalSocket& socket)
    {
        for (;;)
        {
            QByteArray packet;
            const bool retrying = !m_pendingWrite.isEmpty();

            if (retrying) {
                packet = std::move(m_pendingWrite);
            } else if (!m_callbacks.takePacketToOS(packet)) {
                return true;
            }

            const QByteArray data = IPTunnelBrokerProtocol::frame(IPTunnelBrokerProtocol::Message::PacketToOS, packet);

            if (socket.bytesToWrite() + data.size() > qint64(IPTunnelBrokerProtocol::MaximumPayload))
            {
                m_pendingWrite = std::move(packet);

                if (!retrying)
                {
                    m_callbacks.error(windowsError(ErrorCode::QueueOverflow, QStringLiteral("write"),
                        QStringLiteral("Elevated tunnel helper pipe is full"), ERROR_BUFFER_OVERFLOW), false);
                }

                return true;
            }

            if (socket.write(data) != data.size()) {
                return false;
            }

            socket.flush();
        }
    }

    bool drainBrokerRouting(QLocalSocket& socket)
    {
        RoutingCommand command;

        while (takeRoutingCommand(command))
        {
            if (!writeBrokerMessage(socket, IPTunnelBrokerProtocol::Message::ApplyRouting,
                    IPTunnelBrokerProtocol::encodeRouting(command.id, command.routing)))
            {
                return false;
            }
        }

        return true;
    }

    void runBroker()
    {
        if (m_configuration.openMode == OpenMode::AdoptDescriptor)
        {
            m_callbacks.opened({}, windowsError(ErrorCode::Unsupported, QStringLiteral("elevation"),
                QStringLiteral("Descriptor adoption cannot be delegated to the elevated tunnel helper"), 0));
            return;
        }

        QLocalServer server;
        server.setSocketOptions(QLocalServer::UserAccessOption);
        const QString serverName = QStringLiteral("sdrangel-tunnel-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

        if (!server.listen(serverName))
        {
            m_callbacks.opened({}, windowsError(ErrorCode::NativeFailure, QStringLiteral("elevation"),
                QStringLiteral("Unable to create the private tunnel-helper pipe: %1").arg(server.errorString()), 0));
            return;
        }

        QByteArray token = QUuid::createUuid().toRfc4122() + QUuid::createUuid().toRfc4122();
        HANDLE process = nullptr;
        Error error;

        if (!launchBroker(serverName, token, process, error))
        {
            m_callbacks.opened({}, error);
            return;
        }

        QLocalSocket *socket = acceptBroker(server, process, token, error);

        if (!socket)
        {
            stopBrokerProcess(nullptr, process);
            m_callbacks.opened({}, error);
            return;
        }

        if (!writeBrokerMessage(*socket, IPTunnelBrokerProtocol::Message::Configuration,
                IPTunnelBrokerProtocol::encodeConfiguration(m_configuration)))
        {
            stopBrokerProcess(socket, process);
            delete socket;
            m_callbacks.opened({}, windowsError(ErrorCode::NativeFailure, QStringLiteral("elevation"),
                QStringLiteral("Unable to send configuration to the elevated tunnel helper"), 0));
            return;
        }

        QByteArray input;
        IPTunnelBrokerProtocol::Message message {};
        QByteArray payload;

        if (!readBrokerMessage(*socket, input, message, payload, 60000, process)
            || (message != IPTunnelBrokerProtocol::Message::Opened))
        {
            stopBrokerProcess(socket, process);
            delete socket;
            m_callbacks.opened({}, windowsError(ErrorCode::NativeFailure, QStringLiteral("elevation"),
                QStringLiteral("The elevated tunnel helper did not report an open result"), 0));
            return;
        }

        InterfaceInfo info;

        if (!IPTunnelBrokerProtocol::decodeOpened(payload, info, error))
        {
            stopBrokerProcess(socket, process);
            delete socket;
            m_callbacks.opened({}, windowsError(ErrorCode::NativeFailure, QStringLiteral("elevation"),
                QStringLiteral("The elevated tunnel helper returned an invalid open result"), 0));
            return;
        }

        m_callbacks.opened(info, error);

        if (error.isError())
        {
            stopBrokerProcess(socket, process);
            delete socket;
            return;
        }

        bool healthy = true;

        while (!stopping() && healthy)
        {
            healthy = drainBrokerRouting(*socket) && drainBrokerWrites(*socket);

            if (!healthy) {
                break;
            }

            socket->waitForReadyRead(10);
            healthy = processBrokerInput(*socket, input);
            DWORD exitCode = STILL_ACTIVE;

            if ((socket->state() == QLocalSocket::UnconnectedState)
                || !GetExitCodeProcess(process, &exitCode) || (exitCode != STILL_ACTIVE))
            {
                healthy = false;
            }
        }

        if (!stopping() && !healthy)
        {
            m_callbacks.error(windowsError(ErrorCode::NativeFailure, QStringLiteral("elevation"),
                QStringLiteral("The elevated tunnel helper disconnected unexpectedly"), 0), true);
        }

        stopBrokerProcess(socket, process);
        delete socket;
    }

    template<typename T>
    bool resolve(T *&function, const char *name)
    {
        function = reinterpret_cast<T *>(GetProcAddress(m_library, name));
        return function != nullptr;
    }

    bool load(Error& error)
    {
        const QString fileName = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("wintun.dll"));
        m_library = LoadLibraryExW(reinterpret_cast<LPCWSTR>(fileName.utf16()), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);

        if (!m_library)
        {
            error = windowsError(ErrorCode::DependencyMissing, QStringLiteral("load"),
                QStringLiteral("Unable to load %1").arg(fileName));
            return false;
        }

#define RESOLVE_WINTUN(member, name) \
        if (!resolve(member, name)) \
        { \
            error = windowsError(ErrorCode::DependencyMissing, QStringLiteral("load"), QStringLiteral("Wintun is missing %1").arg(QStringLiteral(name))); \
            return false; \
        }
        RESOLVE_WINTUN(m_createAdapter, "WintunCreateAdapter")
        RESOLVE_WINTUN(m_openAdapterFunction, "WintunOpenAdapter")
        RESOLVE_WINTUN(m_closeAdapter, "WintunCloseAdapter")
        RESOLVE_WINTUN(m_getAdapterLuid, "WintunGetAdapterLUID")
        RESOLVE_WINTUN(m_startSessionFunction, "WintunStartSession")
        RESOLVE_WINTUN(m_endSession, "WintunEndSession")
        RESOLVE_WINTUN(m_getReadWaitEvent, "WintunGetReadWaitEvent")
        RESOLVE_WINTUN(m_receivePacket, "WintunReceivePacket")
        RESOLVE_WINTUN(m_releaseReceivePacket, "WintunReleaseReceivePacket")
        RESOLVE_WINTUN(m_allocateSendPacket, "WintunAllocateSendPacket")
        RESOLVE_WINTUN(m_sendPacket, "WintunSendPacket")
#undef RESOLVE_WINTUN

        return true;
    }

    bool openAdapter(InterfaceInfo& info, Error& error)
    {
        QString name = m_configuration.interfaceName.trimmed();

        if (name.isEmpty()) {
            name = QStringLiteral("SDRangel");
        }

        if (m_configuration.openMode == OpenMode::AdoptDescriptor)
        {
            error = windowsError(ErrorCode::Unsupported, QStringLiteral("open"), QStringLiteral("Descriptor adoption is unavailable with Wintun"), 0);
            return false;
        }

        m_adapter = (m_configuration.openMode == OpenMode::OpenExisting) ?
            m_openAdapterFunction(reinterpret_cast<LPCWSTR>(name.utf16())) :
            m_createAdapter(reinterpret_cast<LPCWSTR>(name.utf16()), L"SDRangel", nullptr);

        if (!m_adapter)
        {
            const DWORD code = GetLastError();
            error = windowsError((code == ERROR_ACCESS_DENIED) ? ErrorCode::PermissionDenied : ErrorCode::NativeFailure,
                QStringLiteral("open"), QStringLiteral("Unable to open Wintun adapter '%1'").arg(name), code);
            return false;
        }

        m_getAdapterLuid(m_adapter, &m_luid);
        NET_IFINDEX index = 0;
        DWORD rc = ConvertInterfaceLuidToIndex(&m_luid, &index);

        if (rc != NO_ERROR)
        {
            error = windowsError(ErrorCode::NativeFailure, QStringLiteral("open"), QStringLiteral("Unable to obtain Wintun interface index"), rc);
            return false;
        }

        info.name = name;
        info.index = index;
        info.mtu = m_configuration.mtu;
        return true;
    }

    bool sockaddr(const QHostAddress& address, SOCKADDR_INET& result)
    {
        std::memset(&result, 0, sizeof(result));

        if (address.protocol() == QAbstractSocket::IPv4Protocol)
        {
            result.si_family = AF_INET;
            return InetPtonW(AF_INET, reinterpret_cast<LPCWSTR>(address.toString().utf16()), &result.Ipv4.sin_addr) == 1;
        }

        if (address.protocol() == QAbstractSocket::IPv6Protocol)
        {
            result.si_family = AF_INET6;
            return InetPtonW(AF_INET6, reinterpret_cast<LPCWSTR>(address.toString().utf16()), &result.Ipv6.sin6_addr) == 1;
        }

        return false;
    }

    bool configure(Error& error)
    {
        if (m_configuration.configurationMode == ConfigurationMode::External) {
            return true;
        }

        for (ADDRESS_FAMILY family : {AF_INET, AF_INET6})
        {
            if (((family == AF_INET) && !m_configuration.ipv4) || ((family == AF_INET6) && !m_configuration.ipv6)) {
                continue;
            }

            MIB_IPINTERFACE_ROW row;
            InitializeIpInterfaceEntry(&row);
            row.Family = family;
            row.InterfaceLuid = m_luid;
            DWORD rc = GetIpInterfaceEntry(&row);

            if (rc == NO_ERROR)
            {
                // SetIpInterfaceEntry rejects an IPv4 row unless this field is
                // zero, even when GetIpInterfaceEntry returned another value.
                // See the SetIpInterfaceEntry contract in the Windows IP Helper
                // API documentation.
                if (family == AF_INET) {
                    row.SitePrefixLength = 0;
                }

                row.NlMtu = ULONG(m_configuration.mtu);
                rc = SetIpInterfaceEntry(&row);
            }

            if (rc != NO_ERROR)
            {
                error = windowsError((rc == ERROR_ACCESS_DENIED) ? ErrorCode::PermissionDenied : ErrorCode::NativeFailure,
                    QStringLiteral("mtu"), QStringLiteral("Unable to configure tunnel MTU"), rc);
                return false;
            }
        }

        for (const Address& address : m_configuration.addresses)
        {
            MIB_UNICASTIPADDRESS_ROW row;
            InitializeUnicastIpAddressEntry(&row);
            row.InterfaceLuid = m_luid;
            row.OnLinkPrefixLength = UINT8(address.prefixLength);
            row.PrefixOrigin = IpPrefixOriginManual;
            row.SuffixOrigin = IpSuffixOriginManual;

            if (!sockaddr(address.address, row.Address))
            {
                error = windowsError(ErrorCode::InvalidConfiguration, QStringLiteral("address"), QStringLiteral("Invalid tunnel address"), 0);
                return false;
            }

            DWORD rc = CreateUnicastIpAddressEntry(&row);

            if (rc == NO_ERROR)
            {
                m_ownedAddresses.append(row);
            }
            else if (rc != ERROR_OBJECT_ALREADY_EXISTS)
            {
                error = windowsError((rc == ERROR_ACCESS_DENIED) ? ErrorCode::PermissionDenied : ErrorCode::NativeFailure,
                    QStringLiteral("address"), QStringLiteral("Unable to add tunnel address %1").arg(address.address.toString()), rc);
                return false;
            }
        }

        OperationResult routes = replaceRoutes(m_configuration.routing);

        if (!routes.success)
        {
            error = routes.error;
            return false;
        }

        return true;
    }

    bool startSession(Error& error)
    {
        DWORD capacity = DWORD(m_configuration.wintunRingCapacity);

        if ((capacity < WINTUN_MIN_RING_CAPACITY) || (capacity > WINTUN_MAX_RING_CAPACITY) || (capacity & (capacity - 1)))
        {
            error = windowsError(ErrorCode::InvalidConfiguration, QStringLiteral("open"), QStringLiteral("Wintun ring capacity must be a power of two from 128 KiB to 64 MiB"), 0);
            return false;
        }

        m_session = m_startSessionFunction(m_adapter, capacity);

        if (!m_session)
        {
            error = windowsError(ErrorCode::NativeFailure, QStringLiteral("open"), QStringLiteral("Unable to start Wintun session"));
            return false;
        }

        return true;
    }

    OperationResult replaceRoutes(const RoutingConfiguration& routing)
    {
        OperationResult result = IPTunnel::planRouting(routing);

        if (!result.success || (routing.mode == ConfigurationMode::External))
        {
            if (result.success)
            {
                removeRoutes(m_ownedRoutes);
                m_ownedRoutes.clear();
            }

            return result;
        }

        QVector<Prefix> added;

        for (const Prefix& prefix : result.routePlan.routes)
        {
            bool alreadyOwned = std::any_of(m_ownedRoutes.cbegin(), m_ownedRoutes.cend(), [&](const Prefix& p) { return samePrefix(p, prefix); });

            if (alreadyOwned) {
                continue;
            }

            bool owned = false;
            Error error;

            if (!addRoute(prefix, routing.metric, owned, error))
            {
                removeRoutes(added);
                result.success = false;
                result.error = error;
                return result;
            }

            if (owned) {
                added.append(prefix);
            }
        }

        QVector<Prefix> removed;

        for (const Prefix& old : m_ownedRoutes)
        {
            bool wanted = std::any_of(result.routePlan.routes.cbegin(), result.routePlan.routes.cend(), [&](const Prefix& p) { return samePrefix(p, old); });

            if (!wanted) {
                removeRoute(old);
            } else {
                removed.append(old);
            }
        }

        removed += added;
        m_ownedRoutes = removed;
        return result;
    }

    bool addRoute(const Prefix& prefix, int metric, bool& owned, Error& error)
    {
        MIB_IPFORWARD_ROW2 row;
        InitializeIpForwardEntry(&row);
        row.InterfaceLuid = m_luid;
        row.DestinationPrefix.PrefixLength = UINT8(prefix.length);
        row.Metric = ULONG(metric);
        row.Protocol = MIB_IPPROTO_NETMGMT;

        if (!sockaddr(prefix.address, row.DestinationPrefix.Prefix))
        {
            error = windowsError(ErrorCode::InvalidConfiguration, QStringLiteral("route"), QStringLiteral("Invalid route prefix"), 0);
            return false;
        }

        row.NextHop.si_family = row.DestinationPrefix.Prefix.si_family;
        DWORD rc = CreateIpForwardEntry2(&row);
        owned = (rc == NO_ERROR);

        if ((rc == NO_ERROR) || (rc == ERROR_OBJECT_ALREADY_EXISTS)) {
            return true;
        }

        error = windowsError((rc == ERROR_ACCESS_DENIED) ? ErrorCode::PermissionDenied : ErrorCode::ConfigurationConflict,
            QStringLiteral("route"), QStringLiteral("Unable to add route %1").arg(prefix.toString()), rc);
        return false;
    }

    void removeRoute(const Prefix& prefix)
    {
        MIB_IPFORWARD_ROW2 row;
        InitializeIpForwardEntry(&row);
        row.InterfaceLuid = m_luid;
        row.DestinationPrefix.PrefixLength = UINT8(prefix.length);

        if (!sockaddr(prefix.address, row.DestinationPrefix.Prefix)) {
            return;
        }

        row.NextHop.si_family = row.DestinationPrefix.Prefix.si_family;
        DeleteIpForwardEntry2(&row);
    }

    void removeRoutes(const QVector<Prefix>& routes)
    {
        for (const Prefix& prefix : routes) {
            removeRoute(prefix);
        }
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
            const bool retrying = !m_pendingWrite.isEmpty();

            if (retrying) {
                packet = std::move(m_pendingWrite);
            } else if (!m_callbacks.takePacketToOS(packet)) {
                return;
            }

            BYTE *destination = m_allocateSendPacket(m_session, DWORD(packet.size()));

            if (!destination)
            {
                DWORD code = GetLastError();

                if (code == ERROR_BUFFER_OVERFLOW)
                {
                    if (!retrying) {
                        m_callbacks.error(windowsError(ErrorCode::QueueOverflow, QStringLiteral("write"), QStringLiteral("Wintun send ring is full"), code), false);
                    }

                    m_pendingWrite = std::move(packet);
                    return;
                }

                m_callbacks.error(windowsError(ErrorCode::NativeFailure,
                    QStringLiteral("write"), QStringLiteral("Wintun packet injection failed"), code), code == ERROR_HANDLE_EOF);
                continue;
            }

            std::memcpy(destination, packet.constData(), size_t(packet.size()));
            m_sendPacket(m_session, destination);
            m_callbacks.packetWritten(packet.size());
        }
    }

    void drainReads()
    {
        for (;;)
        {
            DWORD size = 0;
            BYTE *packet = m_receivePacket(m_session, &size);

            if (!packet)
            {
                DWORD code = GetLastError();

                if (code != ERROR_NO_MORE_ITEMS) {
                    m_callbacks.error(windowsError(ErrorCode::NativeFailure, QStringLiteral("read"), QStringLiteral("Wintun receive failed"), code), code == ERROR_HANDLE_EOF);
                }

                return;
            }

            QByteArray copy(reinterpret_cast<const char *>(packet), int(size));
            m_releaseReceivePacket(m_session, packet);
            m_callbacks.packetReceived(copy);
        }
    }

    void cleanup()
    {
        if (m_session)
        {
            m_endSession(m_session);
            m_session = nullptr;
        }

        removeRoutes(m_ownedRoutes);
        m_ownedRoutes.clear();

        for (const MIB_UNICASTIPADDRESS_ROW& row : m_ownedAddresses) {
            DeleteUnicastIpAddressEntry(&row);
        }

        m_ownedAddresses.clear();

        if (m_adapter)
        {
            m_closeAdapter(m_adapter);
            m_adapter = nullptr;
        }

        if (m_library)
        {
            FreeLibrary(m_library);
            m_library = nullptr;
        }
    }
};

}

IPTunnelBackend *iptunnelCreateBackend(const Configuration& configuration, const IPTunnelBackend::Callbacks& callbacks)
{
    return new WindowsBackend(configuration, callbacks);
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
    c.backend = QStringLiteral("Wintun");
    return c;
}
