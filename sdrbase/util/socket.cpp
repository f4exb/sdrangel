///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include "socket.h"

Socket::Socket(QObject *socket, QObject *parent) :
    QObject(parent),
    m_socket(socket)
{
}

Socket::~Socket()
{
    delete m_socket;
}

TCPSocket::TCPSocket(QTcpSocket *socket) :
    Socket(socket)
{
}

qint64 TCPSocket::write(const char *data, qint64 length)
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(m_socket);

    return socket->write(data, length);
}

qint64 TCPSocket::read(char *data, qint64 length)
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(m_socket);

    return socket->read(data, length);
}

QByteArray TCPSocket::readAll()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(m_socket);

    return socket->readAll();
}

void TCPSocket::close()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(m_socket);

    socket->close();
}

qint64 TCPSocket::bytesAvailable()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(m_socket);

    return socket->bytesAvailable();
}

void TCPSocket::flush()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(m_socket);

    socket->flush();
}

QHostAddress TCPSocket::peerAddress()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(m_socket);

    return socket->peerAddress();
}

quint16 TCPSocket::peerPort()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(m_socket);

    return socket->peerPort();
}

bool TCPSocket::isConnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(m_socket);

    return socket->state() == QAbstractSocket::ConnectedState;
}

WebSocket::WebSocket(QWebSocket *socket) :
    Socket(socket)
{
    m_rxBuffer.reserve(64000);
    m_txBuffer.reserve(64000);
    connect(socket, &QWebSocket::binaryFrameReceived, this, &WebSocket::binaryFrameReceived);
}

 qint64 WebSocket::write(const char *data, qint64 length)
{
    //QWebSocket *socket = qobject_cast<QWebSocket *>(m_socket);
    //return socket->sendBinaryMessage(QByteArray(data, length));

    m_txBuffer.append(data, length);
    return length;
}

void WebSocket::flush()
{
    QWebSocket *socket = qobject_cast<QWebSocket *>(m_socket);
    if (m_txBuffer.size() > 0)
    {
        qint64 len = socket->sendBinaryMessage(m_txBuffer);
        if (len != m_txBuffer.size()) {
            qDebug() << "WebSocket::flush: Failed to send all of message" << len << "/" << m_txBuffer.size();
        }
        m_txBuffer.clear();
    }
    socket->flush();
}

qint64 WebSocket::read(char *data, qint64 length)
{
    length = std::min(length, (qint64)m_rxBuffer.size());

    memcpy(data, m_rxBuffer.constData(), length);

    m_rxBuffer = m_rxBuffer.mid(length); // Yep, not very efficient

    return length;
}

QByteArray WebSocket::readAll()
{
    QByteArray b = m_rxBuffer;

    m_rxBuffer.clear();

    return b;
}

void WebSocket::close()
{
    QWebSocket *socket = qobject_cast<QWebSocket *>(m_socket);

    // Will crash if we call close on unopened socket
    if (socket->state() != QAbstractSocket::UnconnectedState) {
        socket->close();
    }
}

qint64 WebSocket::bytesAvailable()
{
    return m_rxBuffer.size();
}

void WebSocket::binaryFrameReceived(const QByteArray &frame, bool isLastFrame)
{
    (void) isLastFrame;

    m_rxBuffer.append(frame);
}

QHostAddress WebSocket::peerAddress()
{
    QWebSocket *socket = qobject_cast<QWebSocket *>(m_socket);

    return socket->peerAddress();
}

quint16 WebSocket::peerPort()
{
    QWebSocket *socket = qobject_cast<QWebSocket *>(m_socket);

    return socket->peerPort();
}

bool WebSocket::isConnected()
{
    QWebSocket *socket = qobject_cast<QWebSocket *>(m_socket);

    return socket->state() == QAbstractSocket::ConnectedState;
}
