///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_FEATURE_REMOTECONTROLWORKER_H_
#define INCLUDE_FEATURE_REMOTECONTROLWORKER_H_

#include <QObject>
#include <QTimer>
#include <QUdpSocket>

#include "util/message.h"
#include "util/messagequeue.h"

#include "remotecontrolsettings.h"

class RemoteControlWorker : public QObject
{
    Q_OBJECT
public:

    RemoteControlWorker();
    ~RemoteControlWorker();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToFeature(MessageQueue *messageQueue) { m_msgQueueToFeature = messageQueue; }
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_msgQueueToGUI = messageQueue; }

private:

    MessageQueue m_inputMessageQueue;
    MessageQueue *m_msgQueueToFeature;
    MessageQueue *m_msgQueueToGUI;
    RemoteControlSettings m_settings;
    bool m_running;
    QTimer m_timer;
    QList<Device *> m_devices;

    bool handleMessage(const Message& cmd);
    void applySettings(const RemoteControlSettings& settings, bool force = false);
    MessageQueue *getMessageQueueToGUI() { return m_msgQueueToGUI; }
    Device *getDevice(const QString &protocol, const QString deviceId) const;

private slots:
    void handleInputMessages();
    void update();
    void deviceUpdated(QHash<QString, QVariant> status);
    void deviceUnavailable();
    void deviceError(const QString &error);
};

#endif // INCLUDE_FEATURE_REMOTECONTROLWORKER_H_
