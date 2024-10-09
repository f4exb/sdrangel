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

#ifndef INCLUDE_SOCKET_H_
#define INCLUDE_SOCKET_H_

#include <QTcpSocket>
#include <QWebSocket>

#include "export.h"

// Class to allow easy use of either QTCPSocket or QWebSocket
class SDRBASE_API Socket : public QObject {
    Q_OBJECT
protected:
    Socket(QObject *socket, QObject *parent=nullptr);

public:
    virtual ~Socket();
    virtual qint64 write(const char *data, qint64 length) = 0;
    virtual void flush() = 0;
    virtual qint64 read(char *data, qint64 length) = 0;
    virtual qint64 bytesAvailable() = 0;
    virtual QByteArray readAll() = 0;
    virtual void close() = 0;
    virtual QHostAddress peerAddress() = 0;
    virtual quint16 peerPort() = 0;
    virtual bool isConnected() = 0;

    QObject *socket() { return m_socket; }

protected:

    QObject *m_socket;

};

class SDRBASE_API TCPSocket : public Socket {
    Q_OBJECT

public:

    explicit TCPSocket(QTcpSocket *socket) ;
    qint64 write(const char *data, qint64 length) override;
    void flush() override;
    qint64 read(char *data, qint64 length) override;
    qint64 bytesAvailable() override;
    QByteArray readAll() override;
    void close() override;
    QHostAddress peerAddress() override;
    quint16 peerPort() override;
    bool isConnected() override;

};

class SDRBASE_API WebSocket : public Socket {
    Q_OBJECT

public:

    explicit WebSocket(QWebSocket *socket);
    qint64 write(const char *data, qint64 length) override;
    void flush() override;
    qint64 read(char *data, qint64 length) override;
    qint64 bytesAvailable() override;
    QByteArray readAll() override;
    void close() override;
    QHostAddress peerAddress() override;
    quint16 peerPort() override;
    bool isConnected() override;

private slots:

    void binaryFrameReceived(const QByteArray &frame, bool isLastFrame);

private:

    QByteArray m_rxBuffer;
    QByteArray m_txBuffer;

};

#endif // INCLUDE_SOCKET_H_
