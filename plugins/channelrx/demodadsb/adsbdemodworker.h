///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_ADSBDEMODWORKER_H
#define INCLUDE_ADSBDEMODWORKER_H

#include <QObject>
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QTextStream>

#include "util/message.h"
#include "util/messagequeue.h"

#include "adsbdemodsettings.h"

// Beast binary server for sending ADS-B data to OpenSky Network (and others)
class ADSBBeastServer : public QTcpServer
{
    Q_OBJECT

private:
    QList<QTcpSocket*> m_clients;

public:
    ADSBBeastServer();
    void listen(quint16 port = 30005);
    void incomingConnection(qintptr socket);
    void send(const char *data, int length);
    void close();

private slots:
    void readClient();
    void discardClient();
};

// Worker that forwards ADS-B frames to various aggregators
class ADSBDemodWorker : public QObject
{
    Q_OBJECT
public:
     class MsgConfigureADSBDemodWorker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ADSBDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureADSBDemodWorker* create(const ADSBDemodSettings& settings, bool force)
        {
            return new MsgConfigureADSBDemodWorker(settings, force);
        }

    private:
        ADSBDemodSettings m_settings;
        bool m_force;

        MsgConfigureADSBDemodWorker(const ADSBDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    ADSBDemodWorker();
    ~ADSBDemodWorker();
    void reset();
    bool startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:

    MessageQueue m_inputMessageQueue;
    ADSBDemodSettings m_settings;
    bool m_running;
    QMutex m_mutex;
    QTimer m_heartbeatTimer;
    QTcpSocket m_socket;
    QFile m_logFile;
    QTextStream m_logStream;
    qint64 m_startTime;
    ADSBBeastServer m_beastServer;

    bool handleMessage(const Message& cmd);
    void applySettings(const ADSBDemodSettings& settings, bool force = false);
    void send(const char *data, int length);
    char *escape(char *p, char c);
    void handleADSB(QByteArray data, const QDateTime dateTime, float correlation);

private slots:
    void handleInputMessages();
    void connected();
    void disconnected();
    void errorOccurred(QAbstractSocket::SocketError socketError);
    void recv();
    void heartbeat();
};

#endif // INCLUDE_ADSBDEMODWORKER_H
