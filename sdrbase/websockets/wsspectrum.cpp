///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB.                                  //
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

#include <QtWebSockets>
#include <QHostAddress>
#include <QDebug>

#include "util/timeutil.h"
#include "wsspectrum.h"

WSSpectrum::WSSpectrum(QObject *parent) :
    QObject(parent),
    m_listeningAddress(QHostAddress::LocalHost),
    m_port(8887),
    m_webSocketServer(nullptr)
{
    connect(this,
            SIGNAL(payloadToSend(const QByteArray&)),
            this,
            SLOT(sendPayload(const QByteArray&)),
            Qt::QueuedConnection);
    m_timer.start();
}

WSSpectrum::~WSSpectrum()
{
    disconnect(this,
            SIGNAL(payloadToSend(const QByteArray&)),
            this,
            SLOT(sendPayload(const QByteArray&)));
    closeSocket();
}

void WSSpectrum::openSocket()
{
    m_webSocketServer = new QWebSocketServer(
        QStringLiteral("Spectrum Server"),
        QWebSocketServer::NonSecureMode,
        this);

    if (m_webSocketServer->listen(m_listeningAddress, m_port))
    {
        qDebug() << "WSSpectrum::openSocket: spectrum server listening at " << m_listeningAddress.toString() << " on port " << m_port;
        connect(m_webSocketServer, &QWebSocketServer::newConnection, this, &WSSpectrum::onNewConnection);
    }
    else
    {
        qInfo("WSSpectrum::openSocket: cannot start spectrum server at %s on port %u", qPrintable(m_listeningAddress.toString()), m_port);
    }
}

void WSSpectrum::closeSocket()
{
    if (m_webSocketServer)
    {
        qDebug() << "WSSpectrum::closeSocket: stopping spectrum server listening at " << m_listeningAddress.toString() << " on port " << m_port;
        delete m_webSocketServer;
        m_webSocketServer = nullptr;
    }
}

bool WSSpectrum::socketOpened() const
{
    return m_webSocketServer && m_webSocketServer->isListening();
}

void WSSpectrum::getPeers(QList<QHostAddress>& hosts, QList<quint16>& ports) const
{
    hosts.clear();
    ports.clear();

    for (auto pSocket : m_clients)
    {
        hosts.push_back(pSocket->peerAddress());
        ports.push_back(pSocket->peerPort());
    }
}

void WSSpectrum::setListeningAddress(const QString& address)
{
    if (address == "127.0.0.1") {
        m_listeningAddress.setAddress(QHostAddress::LocalHost);
    } else if (address == "0.0.0.0") {
        m_listeningAddress.setAddress(QHostAddress::Any);
    } else {
        m_listeningAddress.setAddress(address);
    }
}

QString WSSpectrum::getWebSocketIdentifier(QWebSocket *peer)
{
    return QStringLiteral("%1:%2").arg(peer->peerAddress().toString(), QString::number(peer->peerPort()));
}

void WSSpectrum::onNewConnection()
{
    auto pSocket = m_webSocketServer->nextPendingConnection();
    qDebug() << "WSSpectrum::onNewConnection: " << getWebSocketIdentifier(pSocket) << " connected";
    pSocket->setParent(this);

    connect(pSocket, &QWebSocket::textMessageReceived, this, &WSSpectrum::processClientMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &WSSpectrum::socketDisconnected);

    m_clients << pSocket;
}

void WSSpectrum::processClientMessage(const QString &message)
{
     qDebug() << "WSSpectrum::processClientMessage: " << message;
}

void WSSpectrum::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    qDebug() << getWebSocketIdentifier(pClient) << " disconnected";

    if (pClient)
    {
        m_clients.removeAll(pClient);
        pClient->deleteLater();
    }
}

QHostAddress WSSpectrum::getListeningAddress() const
{
    if (m_webSocketServer) {
        return m_webSocketServer->serverAddress();
    } else {
        return QHostAddress::Null;
    }
}

uint16_t WSSpectrum::getListeningPort() const
{
    if (m_webSocketServer) {
        return m_webSocketServer->serverPort();
    } else {
        return 0;
    }
}

void WSSpectrum::newSpectrum(
    const std::vector<Real>& spectrum,
    int fftSize,
    uint64_t centerFrequency,
    int bandwidth,
    bool linear,
    bool ssb,
    bool usb
)
{
    qint64 elapsed = m_timer.restart();
    uint64_t nowMs = TimeUtil::nowms();
    QByteArray payload;

    buildPayload(
        payload,
        spectrum,
        fftSize,
        elapsed,
        nowMs,
        centerFrequency,
        bandwidth,
        linear,
        ssb,
        usb
    );
    //qDebug() << "WSSpectrum::newSpectrum: " << payload.size() << " bytes in " << elapsed << " ms";
    emit payloadToSend(payload);
}

void WSSpectrum::sendPayload(const QByteArray& payload)
{
    //qDebug() << "WSSpectrum::sendPayload: " << payload.size() << " bytes";
    for (QWebSocket *pClient : qAsConst(m_clients)) {
        pClient->sendBinaryMessage(payload);
    }
}

void WSSpectrum::buildPayload(
    QByteArray& bytes,
    const std::vector<Real>& spectrum,
    int fftSize,
    int64_t fftTimeMs,
    uint64_t timestampMs,
    uint64_t centerFrequency,
    int bandwidth,
    bool linear,
    bool ssb,
    bool usb
)
{
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    buffer.write((char*) &centerFrequency, sizeof(uint64_t));    // 0
    buffer.write((char*) &fftTimeMs, sizeof(int64_t));           // 8
    buffer.write((char*) &timestampMs, sizeof(uint64_t));        // 16
    buffer.write((char*) &fftSize, sizeof(int));                 // 24
    buffer.write((char*) &bandwidth, sizeof(int));               // 28
    int indicators = (linear ? 1 : 0) + (ssb ? 2 : 0) + (usb ? 4 : 0);
    buffer.write((char*) &indicators, sizeof(int));              // 32
    buffer.write((char*) spectrum.data(), fftSize*sizeof(Real)); // 36
    buffer.close();
}
