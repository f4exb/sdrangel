///////////////////////////////////////////////////////////////////////////////////
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

#include <QDebug>

#include "feature/featureset.h"
#include "settings/serializable.h"

#include "remotecontrol.h"
#include "remotecontrolworker.h"

MESSAGE_CLASS_DEFINITION(RemoteControl::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(RemoteControl::MsgConfigureRemoteControl, Message)
MESSAGE_CLASS_DEFINITION(RemoteControl::MsgDeviceGetState, Message)
MESSAGE_CLASS_DEFINITION(RemoteControl::MsgDeviceSetState, Message)
MESSAGE_CLASS_DEFINITION(RemoteControl::MsgDeviceStatus, Message)
MESSAGE_CLASS_DEFINITION(RemoteControl::MsgDeviceError, Message)
MESSAGE_CLASS_DEFINITION(RemoteControl::MsgDeviceUnavailable, Message)

const char* const RemoteControl::m_featureIdURI = "sdrangel.feature.remotecontrol";
const char* const RemoteControl::m_featureId = "RemoteControl";

RemoteControl::RemoteControl(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface)
{
    qDebug("RemoteControl::RemoteControl: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "RemoteControl error";
    start();
}

RemoteControl::~RemoteControl()
{
    stop();
}

void RemoteControl::start()
{
    qDebug() << "RemoteControl::start";

    m_thread = new QThread();
    m_worker = new RemoteControlWorker();
    m_worker->moveToThread(m_thread);

    QObject::connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    QObject::connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    m_worker->setMessageQueueToFeature(getInputMessageQueue());
    m_state = StRunning;
    m_thread->start();
}

void RemoteControl::stop()
{
    qDebug() << "RemoteControl::stop";
    m_state = StIdle;
    m_thread->quit();
    m_thread->wait();
}

bool RemoteControl::handleMessage(const Message& cmd)
{
    if (MsgConfigureRemoteControl::match(cmd))
    {
        MsgConfigureRemoteControl& cfg = (MsgConfigureRemoteControl&) cmd;
        applySettings(cfg.getSettings(), cfg.getForce());
        // Ensure GUI message queue is set. setMessageQueueToGUI() isn't virtual, so can't hook in there.
        m_worker->setMessageQueueToGUI(getMessageQueueToGUI());
        // Forward to worker
        MsgConfigureRemoteControl *msgToWorker = new MsgConfigureRemoteControl(cfg);
        m_worker->getInputMessageQueue()->push(msgToWorker);
        return true;
    }
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        // Unlike most other plugins, our worker is always running.
        // Start/stop is used just to control automatic updating of device state
        // Forward to worker
        MsgStartStop *msgToWorker = new MsgStartStop(cfg);
        m_worker->getInputMessageQueue()->push(msgToWorker);
        return true;
    }
    else if (MsgDeviceGetState::match(cmd))
    {
        MsgDeviceGetState& get = (MsgDeviceGetState&)cmd;
        // Forward to worker
        MsgDeviceGetState *msgToWorker = new MsgDeviceGetState(get);
        m_worker->getInputMessageQueue()->push(msgToWorker);
        return true;
    }
    else if (MsgDeviceSetState::match(cmd))
    {
        MsgDeviceSetState& set = (MsgDeviceSetState&)cmd;
        // Forward to worker
        MsgDeviceSetState *msgToWorker = new MsgDeviceSetState(set);
        m_worker->getInputMessageQueue()->push(msgToWorker);
        return true;
    }
    else
    {
        return false;
    }
}

QByteArray RemoteControl::serialize() const
{
    return m_settings.serialize();
}

bool RemoteControl::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureRemoteControl *msg = MsgConfigureRemoteControl::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureRemoteControl *msg = MsgConfigureRemoteControl::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void RemoteControl::applySettings(const RemoteControlSettings& settings, bool force)
{
    qDebug() << "RemoteControl::applySettings:"
            << " force: " << force;

    m_settings = settings;
}
