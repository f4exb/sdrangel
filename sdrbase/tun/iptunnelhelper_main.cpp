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

#include <windows.h>

#include <QCoreApplication>
#include <QHash>
#include <QLocalSocket>
#include <QTimer>

#include "iptunnel.h"
#include "iptunnelbrokerprotocol.h"

using namespace IPTunnelTypes;
using namespace IPTunnelBrokerProtocol;

namespace {

bool elevated()
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

Error helperError(ErrorCode code, const QString& operation, const QString& message, qint64 nativeCode = 0)
{
    return {code, operation, message, nativeCode};
}

/**
 * The elevated side of a Windows tunnel (sdrangel-tunnel-helper.exe): it connects to the pipe the
 * unelevated SDRangel named on its command line, authenticates with the token it was given, opens an
 * IPTunnel (which owns Wintun) from the Configuration SDRangel sends, and then relays packets,
 * routing requests and errors between that tunnel and the pipe (IPTunnelBrokerProtocol). Anything
 * unexpected on the pipe, or the pipe closing, closes the tunnel and quits the application.
 */
class TunnelHelper
{
public:
    TunnelHelper(QCoreApplication& application, const QString& serverName, const QByteArray& token) :
        m_application(application),
        m_token(token)
    {
        QObject::connect(&m_socket, &QLocalSocket::connected, [&]() {
            send(Message::Authenticate, m_token);
        });
        QObject::connect(&m_socket, &QLocalSocket::readyRead, [&]() {
            m_input.append(m_socket.readAll());
            processInput();
        });
        QObject::connect(&m_socket, &QLocalSocket::disconnected, [&]() {
            shutdown();
        });
        QObject::connect(&m_tunnel, &IPTunnel::outboundPacketsAvailable, [&]() {
            QByteArray packet;

            while (m_tunnel.takeOutboundPacket(packet))
            {
                if (!send(Message::PacketFromOS, packet)) {
                    return;
                }
            }
        });
        QObject::connect(&m_tunnel, &IPTunnel::operationFinished,
            [&](RequestId id, const OperationResult& result) { operationFinished(id, result); });
        QObject::connect(&m_tunnel, &IPTunnel::errorOccurred, [&](const Error& error) {
            if (m_suppressOperationError && sameError(error, m_operationError))
            {
                m_suppressOperationError = false;
                return;
            }

            if (m_openReported) {
                send(Message::Error, encodeError(error, m_tunnel.state() == State::Error));
            }
        });
        m_socket.connectToServer(serverName, QIODevice::ReadWrite);
        QTimer::singleShot(30000, &m_socket, [&]() {
            if (m_socket.state() != QLocalSocket::ConnectedState) {
                shutdown();
            }
        });
    }

private:
    QCoreApplication& m_application;
    QByteArray m_token;
    QLocalSocket m_socket;
    IPTunnel m_tunnel;
    QByteArray m_input;
    RequestId m_openRequest = 0;
    QHash<RequestId, RequestId> m_routingRequests;
    bool m_configured = false;
    bool m_openReported = false;
    bool m_stopping = false;
    bool m_suppressOperationError = false;
    Error m_operationError;

    static bool sameError(const Error& left, const Error& right)
    {
        return (left.code == right.code) && (left.operation == right.operation)
            && (left.message == right.message) && (left.nativeCode == right.nativeCode);
    }

    bool send(Message message, const QByteArray& payload = {})
    {
        if (m_socket.state() != QLocalSocket::ConnectedState) {
            return false;
        }

        const QByteArray data = frame(message, payload);

        if (m_socket.bytesToWrite() + data.size() > qint64(MaximumPayload))
        {
            shutdown();
            return false;
        }

        return m_socket.write(data) == data.size();
    }

    void processInput()
    {
        for (;;)
        {
            Message message {};
            QByteArray payload;
            QString parseError;
            const ParseResult result = takeFrame(m_input, message, payload, parseError);

            if (result == ParseResult::NeedMore) {
                return;
            }

            if (result == ParseResult::Invalid)
            {
                shutdown();
                return;
            }

            if (!m_configured)
            {
                if ((message != Message::Configuration) || (payload.size() > 64 * 1024))
                {
                    shutdown();
                    return;
                }

                Configuration configuration;

                if (!decodeConfiguration(payload, configuration) || (configuration.openMode == OpenMode::AdoptDescriptor))
                {
                    const Error error = helperError(ErrorCode::InvalidConfiguration, QStringLiteral("open"),
                        QStringLiteral("The elevated tunnel helper rejected its configuration"));
                    send(Message::Opened, encodeOpened({}, error));
                    shutdown();
                    return;
                }

                const ValidationResult validation = IPTunnel::validate(configuration);

                if (!validation.valid)
                {
                    send(Message::Opened, encodeOpened({}, validation.error));
                    shutdown();
                    return;
                }

                m_configured = true;
                m_openRequest = m_tunnel.open(configuration);
                continue;
            }

            if (message == Message::PacketToOS)
            {
                if (payload.isEmpty() || (payload.size() > 65535))
                {
                    shutdown();
                    return;
                }

                const EnqueueResult enqueueResult = m_tunnel.injectInboundPacket(payload);

                if (enqueueResult == EnqueueResult::Accepted)
                {
                    QByteArray accepted;
                    QDataStream stream(&accepted, QIODevice::WriteOnly);
                    stream << qint64(payload.size());
                    send(Message::PacketAccepted, accepted);
                }
                else if (enqueueResult == EnqueueResult::QueueFull)
                {
                    send(Message::Error, encodeError(helperError(ErrorCode::QueueOverflow, QStringLiteral("write"),
                        QStringLiteral("Elevated tunnel helper input queue is full")), false));
                }
                else
                {
                    send(Message::Error, encodeError(helperError(ErrorCode::InvalidConfiguration, QStringLiteral("write"),
                        QStringLiteral("Elevated tunnel helper rejected an IP packet")), false));
                }
            }
            else if (message == Message::ApplyRouting)
            {
                RequestId remoteId = 0;
                RoutingConfiguration routing;

                if (!decodeRouting(payload, remoteId, routing))
                {
                    shutdown();
                    return;
                }

                const OperationResult plan = IPTunnel::planRouting(routing);

                if (!plan.success) {
                    send(Message::RoutingFinished, encodeRoutingResult(remoteId, plan));
                } else {
                    m_routingRequests.insert(m_tunnel.applyRouting(routing), remoteId);
                }
            }
            else if ((message == Message::Close) && payload.isEmpty())
            {
                shutdown();
                return;
            }
            else
            {
                shutdown();
                return;
            }
        }
    }

    void operationFinished(RequestId id, const OperationResult& result)
    {
        if (!result.success)
        {
            m_operationError = result.error;
            m_suppressOperationError = true;
        }

        if ((id == m_openRequest) && !m_openReported)
        {
            m_openReported = true;
            send(Message::Opened, encodeOpened(result.success ? m_tunnel.interfaceInfo() : InterfaceInfo {}, result.error));

            if (!result.success) {
                QTimer::singleShot(0, &m_application, [&]() { shutdown(); });
            }

            return;
        }

        auto it = m_routingRequests.find(id);

        if (it != m_routingRequests.end())
        {
            const RequestId remoteId = it.value();
            m_routingRequests.erase(it);
            send(Message::RoutingFinished, encodeRoutingResult(remoteId, result));
        }
    }

    void shutdown()
    {
        if (m_stopping) {
            return;
        }

        m_stopping = true;
        m_tunnel.close();

        if (m_socket.state() != QLocalSocket::UnconnectedState) {
            m_socket.disconnectFromServer();
        }

        QTimer::singleShot(0, &m_application, &QCoreApplication::quit);
    }
};

}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    QString serverName;
    QByteArray token;

    for (int i = 1; i + 1 < arguments.size(); i += 2)
    {
        if (arguments[i] == QStringLiteral("--pipe")) {
            serverName = arguments[i + 1];
        } else if (arguments[i] == QStringLiteral("--token")) {
            token = QByteArray::fromHex(arguments[i + 1].toLatin1());
        } else {
            return 2;
        }
    }

    if ((arguments.size() != 5) || serverName.isEmpty() || (token.size() != 32) || !elevated()) {
        return 2;
    }

    TunnelHelper helper(application, serverName, token);
    return application.exec();
}
