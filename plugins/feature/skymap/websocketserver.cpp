///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include "websocketserver.h"

WebSocketServer::WebSocketServer(QObject *parent) :
    QObject(parent),
    m_socket("", QWebSocketServer::NonSecureMode, this),
    m_client(nullptr)
{
    connect(&m_socket, &QWebSocketServer::newConnection, this, &WebSocketServer::onNewConnection);
    int port = 0;
    if (!m_socket.listen(QHostAddress::Any, port)) {
        qCritical() << "WebSocketServer - Unable to listen on port " << port;
    }
}

quint16 WebSocketServer::serverPort()
{
    return m_socket.serverPort();
}

void WebSocketServer::onNewConnection()
{
    QWebSocket *socket = m_socket.nextPendingConnection();

    connect(socket, &QWebSocket::textMessageReceived, this, &WebSocketServer::processTextMessage);
    connect(socket, &QWebSocket::binaryMessageReceived, this, &WebSocketServer::processBinaryMessage);
    connect(socket, &QWebSocket::disconnected, this, &WebSocketServer::socketDisconnected);

    m_client = socket;

    emit connected();
}

void WebSocketServer::processTextMessage(QString message)
{
    //QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    //qDebug() << "WebSocketServer::processTextMessage - Received text " << message;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);
    if (!doc.isNull() && doc.isObject()) {
        emit received(doc.object());
    } else {
        qDebug() << "WebSocketServer::processTextMessage: " << error.errorString();
    }
}

void WebSocketServer::processBinaryMessage(QByteArray message)
{
    //QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    // Shouldn't receive any binary messages for now
    qDebug() << "WebSocketServer::processBinaryMessage - Received binary " << message;
}

void WebSocketServer::socketDisconnected()
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (client) {
        client->deleteLater();
        m_client = nullptr;
    }
}

bool WebSocketServer::isConnected()
{
    return m_client != nullptr;
}

void WebSocketServer::send(const QJsonObject &obj)
{
    if (m_client)
    {
        QJsonDocument doc(obj);
        QByteArray bytes = doc.toJson();
        qint64 bytesSent = m_client->sendTextMessage(bytes);
        m_client->flush(); // Try to reduce latency
        if (bytesSent != bytes.size()) {
            qDebug() << "WebSocketServer::update - Sent only " << bytesSent << " bytes out of " << bytes.size();
        }
        //qDebug() << obj;
    }
}

