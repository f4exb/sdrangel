///////////////////////////////////////////////////////////////////////////////////
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

#ifndef INCLUDE_FEATURE_SIMPLEPTTWORKER_H_
#define INCLUDE_FEATURE_SIMPLEPTTWORKER_H_

#include <QObject>
#include <QTimer>

#include "util/message.h"
#include "util/messagequeue.h"
#include "audio/audiofifo.h"

#include "simplepttsettings.h"
#include "simplepttcommand.h"

class WebAPIAdapterInterface;

class SimplePTTWorker : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureSimplePTTWorker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SimplePTTSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureSimplePTTWorker* create(const SimplePTTSettings& settings, const QList<QString>& settingsKeys, bool force)
        {
            return new MsgConfigureSimplePTTWorker(settings, settingsKeys, force);
        }

    private:
        SimplePTTSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureSimplePTTWorker(const SimplePTTSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgPTT : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getTx() const { return m_tx; }

        static MsgPTT* create(bool tx) {
            return new MsgPTT(tx);
        }

    private:
        bool m_tx;

        MsgPTT(bool tx) :
            Message(),
            m_tx(tx)
        { }
    };

    SimplePTTWorker(WebAPIAdapterInterface *webAPIAdapterInterface);
    ~SimplePTTWorker();
    void reset();
    void startWork();
    void stopWork();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

    void setMessageQueueToGUI(MessageQueue *messageQueue) {
        m_msgQueueToGUI = messageQueue;
        m_command.setMessageQueueToGUI(messageQueue);
    }

    void getAudioPeak(float& peak)
    {
        peak = m_audioMagsqPeak;
        m_audioMagsqPeak = 0;
    }

private:
    WebAPIAdapterInterface *m_webAPIAdapterInterface;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    MessageQueue *m_msgQueueToGUI; //!< Queue to report state to GUI
    SimplePTTSettings m_settings;
    bool m_tx;
    AudioFifo m_audioFifo;
    AudioVector m_audioReadBuffer;
    unsigned int m_audioReadBufferFill;
    int m_audioSampleRate;
	float m_audioMagsqPeak;
    float m_voxLevel;
    int m_voxHoldCount;
    bool m_voxState;
    SimplePTTCommand m_command;
	QTimer m_updateTimer;
    QRecursiveMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const SimplePTTSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void sendPTT(bool tx);
    bool turnDevice(bool on);
    void preSwitch(bool tx);

private slots:
    void handleInputMessages();
	void updateHardware();
    void handleAudio();
};

#endif // INCLUDE_FEATURE_SIMPLEPTTWORKER_H_
