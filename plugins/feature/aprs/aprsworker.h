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

#ifndef INCLUDE_FEATURE_APRSWORKER_H_
#define INCLUDE_FEATURE_APRSWORKER_H_

#include <QObject>
#include <QTcpSocket>

#include "util/message.h"
#include "util/messagequeue.h"

#include "aprs.h"
#include "aprssettings.h"

class WebAPIAdapterInterface;

class APRSWorker : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureAPRSWorker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const APRSSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureAPRSWorker* create(const APRSSettings& settings, const QList<QString>& settingsKeys, bool force)
        {
            return new MsgConfigureAPRSWorker(settings, settingsKeys, force);
        }

    private:
        APRSSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureAPRSWorker(const APRSSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    APRSWorker(APRS *m_aprs, WebAPIAdapterInterface *webAPIAdapterInterface);
    ~APRSWorker();
    void startWork();
    void stopWork();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToFeature(MessageQueue *messageQueue) { m_msgQueueToFeature = messageQueue; }
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_msgQueueToGUI = messageQueue; }

private:

    APRS *m_aprs;
    WebAPIAdapterInterface *m_webAPIAdapterInterface;
    MessageQueue m_inputMessageQueue;  //!< Queue for asynchronous inbound communication
    MessageQueue *m_msgQueueToFeature; //!< Queue to report channel change to main feature object
    MessageQueue *m_msgQueueToGUI;
    APRSSettings m_settings;
    QRecursiveMutex m_mutex;
    QTcpSocket m_socket;
    bool m_loggedIn;

    bool handleMessage(const Message& cmd);
    void applySettings(const APRSSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    MessageQueue *getMessageQueueToGUI() { return m_msgQueueToGUI; }
    void send(const char *data, int length);

private slots:
    void handleInputMessages();
    void connected();
    void disconnected();
    void errorOccurred(QAbstractSocket::SocketError socketError);
    void recv();
};

#endif // INCLUDE_FEATURE_APRSWORKER_H_
