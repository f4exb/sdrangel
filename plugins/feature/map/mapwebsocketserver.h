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

#ifndef INCLUDE_FEATURE_MAPWEBSOCKERSERVER_H_
#define INCLUDE_FEATURE_MAPWEBSOCKERSERVER_H_

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonObject>

class MapWebSocketServer : public QObject
{
    Q_OBJECT

private:

    QWebSocketServer m_socket;
    QWebSocket *m_client;

public:

    MapWebSocketServer(QObject *parent = nullptr);
    quint16 serverPort();

    bool isConnected();
    void send(const QJsonObject &obj);

signals:
    void connected();
    void received(const QJsonObject &obj);

public slots:

    void onNewConnection();
    void processTextMessage(QString message);
    void processBinaryMessage(QByteArray message);
    void socketDisconnected();

};

#endif // INCLUDE_FEATURE_MAPWEBSOCKERSERVER_H_
