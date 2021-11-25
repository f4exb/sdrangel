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

#ifndef INCLUDE_FEATURE_PERTESTERWORKER_H_
#define INCLUDE_FEATURE_PERTESTERWORKER_H_

#include <QObject>
#include <QTimer>
#include <QUdpSocket>

#include "util/message.h"
#include "util/messagequeue.h"

#include "pertestersettings.h"

class PERTesterWorker : public QObject
{
    Q_OBJECT
public:
    class MsgConfigurePERTesterWorker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const PERTesterSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigurePERTesterWorker* create(const PERTesterSettings& settings, bool force)
        {
            return new MsgConfigurePERTesterWorker(settings, force);
        }

    private:
        PERTesterSettings m_settings;
        bool m_force;

        MsgConfigurePERTesterWorker(const PERTesterSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    PERTesterWorker();
    ~PERTesterWorker();
    void reset();
    bool startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToFeature(MessageQueue *messageQueue) { m_msgQueueToFeature = messageQueue; }
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_msgQueueToGUI = messageQueue; }

private:

    MessageQueue m_inputMessageQueue;
    MessageQueue *m_msgQueueToFeature;
    MessageQueue *m_msgQueueToGUI;
    PERTesterSettings m_settings;
    bool m_running;
    QMutex m_mutex;
    QUdpSocket *m_rxUDPSocket;                  //!< UDP socket to receive packets on
    QUdpSocket m_txUDPSocket;
    QTimer m_txTimer;
    int m_tx;                                   //!< Number of packets we've transmitted
    int m_rxMatched;                            //!< Number of packets we've received that match a transmitted packet
    int m_rxUnmatched;                          //!< Number of packets received that didn't match a transmitted packet
    QList<QByteArray> m_txPackets;              //!< Packets we've transmitted, but not yet received

    bool handleMessage(const Message& cmd);
    void applySettings(const PERTesterSettings& settings, bool force = false);
    MessageQueue *getMessageQueueToGUI() { return m_msgQueueToGUI; }
    void openUDP(const PERTesterSettings& settings);
    void closeUDP();
    void resetStats();

private slots:
    void started();
    void finished();
    void handleInputMessages();
    void rx();
    void tx();
    void testComplete();
};

#endif // INCLUDE_FEATURE_PERTESTERWORKER_H_
