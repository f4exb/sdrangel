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

#include "wsspectrum.h"

WSSpectrum::WSSpectrum(QObject *parent) :
    QObject(parent),
    m_listeningAddress(QHostAddress::LocalHost),
    m_port(8887),
    m_webSocketServer(nullptr)
{
    m_timer.start();
}

WSSpectrum::~WSSpectrum()
{
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
        delete m_webSocketServer;
        m_webSocketServer = nullptr;
    }
}

bool WSSpectrum::socketOpened()
{
    return m_webSocketServer && m_webSocketServer->isListening();
}

QString WSSpectrum::getWebSocketIdentifier(QWebSocket *peer)
{
    return QStringLiteral("%1:%2").arg(peer->peerAddress().toString(), QString::number(peer->peerPort()));
}

void WSSpectrum::onNewConnection()
{
    auto pSocket = m_webSocketServer->nextPendingConnection();
    qDebug() << " WSSpectrum::onNewConnection: " << getWebSocketIdentifier(pSocket) << " connected";
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

void WSSpectrum::newSpectrum(
    const std::vector<Real>& spectrum,
    int fftSize,
    float refLevel,
    float powerRange,
    uint64_t centerFrequency,
    int bandwidth,
    bool linear
)
{
    if (m_timer.elapsed() < 200) { // Max 5 frames per second
        return;
    }

    qint64 elapsed = m_timer.restart();
    QWebSocket *pSender = qobject_cast<QWebSocket *>(sender());
    QByteArray payload;

    buildPayload(
        payload,
        spectrum,
        fftSize,
        elapsed,
        refLevel,
        powerRange,
        centerFrequency,
        bandwidth,
        linear
    );
    //qDebug() << "WSSpectrum::newSpectrum: " << payload.size() << " bytes in " << elapsed << " ms";

    for (QWebSocket *pClient : qAsConst(m_clients)) {
        pClient->sendBinaryMessage(payload);
    }
}

void WSSpectrum::buildPayload(
    QByteArray& bytes,
    const std::vector<Real>& spectrum,
    int fftSize,
    int64_t fftTimeMs,
    float refLevel,
    float powerRange,
    uint64_t centerFrequency,
    int bandwidth,
    bool linear
)
{
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    buffer.write((char*) &fftSize, sizeof(int));
    buffer.write((char*) &fftTimeMs, sizeof(int64_t));
    buffer.write((char*) &refLevel, sizeof(float));
    buffer.write((char*) &powerRange, sizeof(float));
    buffer.write((char*) &centerFrequency, sizeof(uint64_t));
    buffer.write((char*) &bandwidth, sizeof(int));
    int linearInt = linear ? 1 : 0;
    buffer.write((char*) &linearInt, sizeof(int));
    buffer.write((char*) spectrum.data(), fftSize*sizeof(Real));
    buffer.close();
}
