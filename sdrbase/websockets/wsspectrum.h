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

#ifndef SDRBASE_WEBSOCKETS_WSSPECTRUM_H_
#define SDRBASE_WEBSOCKETS_WSSPECTRUM_H_

#include <vector>

#include <QObject>
#include <QList>
#include <QElapsedTimer>
#include <QHostAddress>

#include "dsp/dsptypes.h"

#include "export.h"

class QWebSocketServer;
class QWebSocket;

class SDRBASE_API WSSpectrum : public QObject
{
    Q_OBJECT
public:
    explicit WSSpectrum(QObject *parent = nullptr);
    ~WSSpectrum() override;

    void openSocket();
    void closeSocket();
    bool socketOpened() const;
    void getPeers(QList<QHostAddress>& hosts, QList<quint16>& ports) const;
    void setListeningAddress(const QString& address);
    void setPort(quint16 port) { m_port = port; }
    QHostAddress getListeningAddress() const;
    uint16_t getListeningPort() const;
    void newSpectrum(
        const std::vector<Real>& spectrum,
        int fftSize,
        uint64_t centerFrequency,
        int bandwidth,
        bool linear,
        bool ssb = false,
        bool usb = true
    );

signals:
    void payloadToSend(const QByteArray& payload);

private slots:
    void onNewConnection();
    void processClientMessage(const QString &message);
    void socketDisconnected();
    void sendPayload(const QByteArray& payload);

private:
    QHostAddress m_listeningAddress;
    quint16 m_port;
    QWebSocketServer* m_webSocketServer;
    QList<QWebSocket*> m_clients;
    QElapsedTimer m_timer;

    static QString getWebSocketIdentifier(QWebSocket *peer);
    void buildPayload(
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
    );
};

#endif // SDRBASE_WEBSOCKETS_WSSPECTRUM_H_
