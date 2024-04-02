///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FEATURE_SIDWORKER_H_
#define INCLUDE_FEATURE_SIDWORKER_H_

#include <QObject>
#include <QTimer>

#include "util/message.h"
#include "util/messagequeue.h"

#include "sid.h"
#include "sidsettings.h"

class WebAPIAdapterInterface;
class SIDMain;

class SIDWorker : public QObject
{
    Q_OBJECT
public:

    SIDWorker(SIDMain *m_sid, WebAPIAdapterInterface *webAPIAdapterInterface);
    ~SIDWorker();
    void startWork();
    void stopWork();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToFeature(MessageQueue *messageQueue) { m_msgQueueToFeature = messageQueue; }
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_msgQueueToGUI = messageQueue; }

private:

    SIDMain *m_sid;
    WebAPIAdapterInterface *m_webAPIAdapterInterface;
    MessageQueue m_inputMessageQueue;  //!< Queue for asynchronous inbound communication
    MessageQueue *m_msgQueueToFeature; //!< Queue to report channel change to main feature object
    MessageQueue *m_msgQueueToGUI;
    SIDSettings m_settings;
    QRecursiveMutex m_mutex;
    QTimer m_pollTimer;

    bool handleMessage(const Message& cmd);
    void applySettings(const SIDSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    MessageQueue *getMessageQueueToGUI() { return m_msgQueueToGUI; }

private slots:
    void handleInputMessages();
    void update();
};

#endif // INCLUDE_FEATURE_SIDWORKER_H_
