///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FEATURE_STARTRACKERWORKER_H_
#define INCLUDE_FEATURE_STARTRACKERWORKER_H_

#include <QObject>
#include <QTimer>
#include <QAbstractSocket>

#include "util/message.h"
#include "util/messagequeue.h"
#include "util/astronomy.h"

#include "startrackersettings.h"

class WebAPIAdapterInterface;
class QTcpServer;
class QTcpSocket;
class StarTracker;
class QDateTime;
class ObjectPipe;

class StarTrackerWorker : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureStarTrackerWorker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const StarTrackerSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureStarTrackerWorker* create(const StarTrackerSettings& settings, const QList<QString>& settingsKeys, bool force)
        {
            return new MsgConfigureStarTrackerWorker(settings, settingsKeys, force);
        }

    private:
        StarTrackerSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureStarTrackerWorker(const StarTrackerSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    StarTrackerWorker(StarTracker* starTracker, WebAPIAdapterInterface *webAPIAdapterInterface);
    ~StarTrackerWorker();
    void startWork();
    void stopWork();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToFeature(MessageQueue *messageQueue) { m_msgQueueToFeature = messageQueue; }
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_msgQueueToGUI = messageQueue; }

private:

    StarTracker* m_starTracker;
    WebAPIAdapterInterface *m_webAPIAdapterInterface;
    MessageQueue m_inputMessageQueue;  //!< Queue for asynchronous inbound communication
    MessageQueue *m_msgQueueToFeature; //!< Queue to report channel change to main feature object
    MessageQueue *m_msgQueueToGUI;
    StarTrackerSettings m_settings;
    QRecursiveMutex m_mutex;
    QTimer m_pollTimer;
    QTcpServer *m_tcpServer;
    QTcpSocket *m_clientConnection;
    float m_solarFlux;

    bool handleMessage(const Message& cmd);
    void applySettings(const StarTrackerSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void restartServer(bool enabled, uint32_t port);
    MessageQueue *getMessageQueueToGUI() { return m_msgQueueToGUI; }
    void updateRaDec(RADec rd, QDateTime dt, bool lbTarget);
    void writeStellariumTarget(double ra, double dec);
    void removeFromMap(QString id);
    void sendToMap(
        const QList<ObjectPipe*>& mapMessagePipes,
        QString id,
        QString image,
        QString text,
        double lat,
        double lon,
        double rotation = 0.0
    );

private slots:
    void handleInputMessages();
    void update();
    void acceptConnection();
    void disconnected();
    void errorOccurred(QAbstractSocket::SocketError socketError);
    void readStellariumCommand();
};

#endif // INCLUDE_FEATURE_STARTRACKERWORKER_H_
