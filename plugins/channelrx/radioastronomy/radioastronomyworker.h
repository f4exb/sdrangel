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

#ifndef INCLUDE_RADIOASTRONOMYWORKER_H
#define INCLUDE_RADIOASTRONOMYWORKER_H

#include <QObject>
#include <QTimer>

#include "util/message.h"
#include "util/messagequeue.h"
#include "util/visa.h"

#include "radioastronomysettings.h"

class RadioAstronomy;

class RadioAstronomyWorker : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureRadioAstronomyWorker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RadioAstronomySettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureRadioAstronomyWorker* create(const RadioAstronomySettings& settings, bool force)
        {
            return new MsgConfigureRadioAstronomyWorker(settings, force);
        }

    private:
        RadioAstronomySettings m_settings;
        bool m_force;

        MsgConfigureRadioAstronomyWorker(const RadioAstronomySettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    RadioAstronomyWorker(RadioAstronomy* radioAstronomy);
    ~RadioAstronomyWorker();
    void reset();
    bool startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_msgQueueToChannel = messageQueue; }
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_msgQueueToGUI = messageQueue; }

private:

    RadioAstronomy* m_radioAstronomy;
    MessageQueue m_inputMessageQueue;  //!< Queue for asynchronous inbound communication
    MessageQueue *m_msgQueueToChannel;
    MessageQueue *m_msgQueueToGUI;
    RadioAstronomySettings m_settings;
    bool m_running;
    QMutex m_mutex;

    VISA m_visa;
    ViSession m_session[RADIOASTRONOMY_SENSORS];
    QTimer m_sensorTimer;

    bool handleMessage(const Message& cmd);
    void applySettings(const RadioAstronomySettings& settings, bool force = false);
    MessageQueue *getMessageQueueToGUI() { return m_msgQueueToGUI; }

private slots:
    void handleInputMessages();
    void measureSensors();
};

#endif // INCLUDE_RADIOASTRONOMYWORKER_H
