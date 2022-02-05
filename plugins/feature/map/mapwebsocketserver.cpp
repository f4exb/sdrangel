///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <QJsonDocument>

#include "mapwebsocketserver.h"

MapWebSocketServer::MapWebSocketServer(QObject *parent) :
    QObject(parent),
    m_socket("", QWebSocketServer::NonSecureMode, this),
    m_client(nullptr)
{
    connect(&m_socket, &QWebSocketServer::newConnection, this, &MapWebSocketServer::onNewConnection);
    int port = 0;
    if (!m_socket.listen(QHostAddress::Any, port)) {
        qCritical() << "MapWebSocketServer - Unable to listen on port " << port;
    }
}

quint16 MapWebSocketServer::serverPort()
{
    return m_socket.serverPort();
}

void MapWebSocketServer::onNewConnection()
{
    QWebSocket *socket = m_socket.nextPendingConnection();

    connect(socket, &QWebSocket::textMessageReceived, this, &MapWebSocketServer::processTextMessage);
    connect(socket, &QWebSocket::binaryMessageReceived, this, &MapWebSocketServer::processBinaryMessage);
    connect(socket, &QWebSocket::disconnected, this, &MapWebSocketServer::socketDisconnected);

    m_client = socket;

    emit connected();
}

void MapWebSocketServer::processTextMessage(QString message)
{
    //QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    //qDebug() << "MapWebSocketServer::processTextMessage - Received text " << message;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);
    if (!doc.isNull() && doc.isObject()) {
        emit received(doc.object());
    } else {
        qDebug() << "MapWebSocketServer::processTextMessage: " << error.errorString();
    }
}

void MapWebSocketServer::processBinaryMessage(QByteArray message)
{
    //QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    // Shouldn't receive any binary messages for now
    qDebug() << "MapWebSocketServer::processBinaryMessage - Received binary " << message;
}

void MapWebSocketServer::socketDisconnected()
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (client) {
        client->deleteLater();
        m_client = nullptr;
    }
}

bool MapWebSocketServer::isConnected()
{
    return m_client != nullptr;
}

void MapWebSocketServer::send(const QJsonObject &obj)
{
    if (m_client)
    {
        QJsonDocument doc(obj);
        QByteArray bytes = doc.toJson();
        qint64 bytesSent = m_client->sendTextMessage(bytes);
        m_client->flush(); // Try to reduce latency
        if (bytesSent != bytes.size()) {
            qDebug() << "MapWebSocketServer::update - Sent only " << bytesSent << " bytes out of " << bytes.size();
        }
    }
}

