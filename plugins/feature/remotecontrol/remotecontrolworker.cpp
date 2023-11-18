///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon <jon@beniston.com>                                     //
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

#include "util/iot/device.h"

#include "remotecontrol.h"
#include "remotecontrolworker.h"

RemoteControlWorker::RemoteControlWorker() :
    m_msgQueueToFeature(nullptr),
    m_msgQueueToGUI(nullptr),
    m_timer(this)
{
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(update()));
}

RemoteControlWorker::~RemoteControlWorker()
{
    m_timer.stop();
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_inputMessageQueue.clear();
    qDeleteAll(m_devices);
    m_devices.clear();
}

void RemoteControlWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

Device *RemoteControlWorker::getDevice(const QString &protocol, const QString deviceId) const
{
    for (auto device : m_devices)
    {
        if ((device->getProtocol() == protocol) && (device->getDeviceId() == deviceId)) {
            return device;
        }
    }
    return nullptr;
}

bool RemoteControlWorker::handleMessage(const Message& cmd)
{
    if (RemoteControl::MsgConfigureRemoteControl::match(cmd))
    {
        RemoteControl::MsgConfigureRemoteControl& cfg = (RemoteControl::MsgConfigureRemoteControl&) cmd;

        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (RemoteControl::MsgStartStop::match(cmd))
    {
        RemoteControl::MsgStartStop& cfg = (RemoteControl::MsgStartStop&) cmd;

        // Start/stop automatic state updates
        if (cfg.getStartStop()) {
            m_timer.start(m_settings.m_updatePeriod * 1000.0);
        } else {
            m_timer.stop();
        }
        return true;
    }
    else if (RemoteControl::MsgDeviceGetState::match(cmd))
    {
        // Get state for all devices
        update();
        return true;
    }
    else if (RemoteControl::MsgDeviceSetState::match(cmd))
    {
        RemoteControl::MsgDeviceSetState& msg = (RemoteControl::MsgDeviceSetState&) cmd;
        QString protocol = msg.getProtocol();
        QString deviceId = msg.getDeviceId();
        Device *device = getDevice(protocol, deviceId);
        if (device)
        {
            QString id = msg.getId();
            QVariant variant = msg.getValue();

            if ((QMetaType::Type)variant.type() == QMetaType::Bool)
            {
                bool b = variant.toBool();
                device->setState(id, b);
            }
            else if ((QMetaType::Type)variant.type() == QMetaType::Int)
            {
                int i = variant.toInt();
                device->setState(id, i);
            }
            else if ((QMetaType::Type)variant.type() == QMetaType::Float)
            {
                float f = variant.toFloat();
                device->setState(id, f);
            }
            else if ((QMetaType::Type)variant.type() == QMetaType::QString)
            {
                QString s = variant.toString();
                device->setState(id, s);
            }
            else
            {
                qDebug() << "RemoteControlWorker::handleMessage: Unsupported type: " << variant.typeName();
            }
        }
        else
        {
            qDebug() << "RemoteControlWorker::handleMessage: Device not found: " << protocol << " " << deviceId;
        }
        return true;
    }
    else
    {
        return false;
    }
}

void RemoteControlWorker::applySettings(const RemoteControlSettings& settings, bool force)
{
    qDebug() << "RemoteControlWorker::applySettings:"
            << " m_updatePeriod: " << settings.m_updatePeriod
            << " m_visaLogIO: " << settings.m_visaLogIO
            << " force: " << force;

    if ((settings.m_updatePeriod != m_settings.m_updatePeriod) || force) {
        m_timer.setInterval(settings.m_updatePeriod * 1000.0);
    }

    // Always recreate all devices, as DeviceInfo may have changed
    qDeleteAll(m_devices);
    m_devices.clear();
    for (auto rcDevice : settings.m_devices)
    {
        QHash<QString, QVariant> devSettings;
        if (rcDevice->m_protocol == "TPLink")
        {
            devSettings.insert("username", settings.m_tpLinkUsername);
            devSettings.insert("password", settings.m_tpLinkPassword);
        }
        else if (rcDevice->m_protocol == "HomeAssistant")
        {
            devSettings.insert("apiKey", settings.m_homeAssistantToken);
            devSettings.insert("url", settings.m_homeAssistantHost);
        }
        else if (rcDevice->m_protocol == "VISA")
        {
            devSettings.insert("logIO", settings.m_visaLogIO);
        }
        devSettings.insert("deviceId", rcDevice->m_info.m_id);
        QStringList controlIDs;
        for (auto control : rcDevice->m_controls) {
            controlIDs.append(control.m_id);
        }
        QStringList sensorIDs;
        for (auto sensor : rcDevice->m_sensors) {
            sensorIDs.append(sensor.m_id);
        }
        devSettings.insert("controlIds", controlIDs);
        devSettings.insert("sensorIds", sensorIDs);
        Device *device = Device::create(devSettings, rcDevice->m_protocol, &rcDevice->m_info);
        m_devices.append(device);
        connect(device, &Device::deviceUpdated, this, &RemoteControlWorker::deviceUpdated);
        connect(device, &Device::deviceUnavailable, this, &RemoteControlWorker::deviceUnavailable);
        connect(device, &Device::error, this, &RemoteControlWorker::deviceError);
    }

    m_settings = settings;
}

void RemoteControlWorker::update()
{
    for (auto device : m_devices) {
         device->getState();
    }
}

void RemoteControlWorker::deviceUpdated(QHash<QString, QVariant> status)
{
    QObject *device = this->sender();

    for (int i = 0; i < m_devices.size(); i++)
    {
        if (device == m_devices[i])
        {
            if (getMessageQueueToGUI())
            {
                getMessageQueueToGUI()->push(RemoteControl::MsgDeviceStatus::create(m_devices[i]->getProtocol(),
                                                                                    m_devices[i]->getDeviceId(),
                                                                                    status));
            }
        }
    }
}

void RemoteControlWorker::deviceUnavailable()
{
    if (getMessageQueueToGUI())
    {
        Device *device = qobject_cast<Device *>(this->sender());

        getMessageQueueToGUI()->push(RemoteControl::MsgDeviceUnavailable::create(device->getProtocol(), device->getDeviceId()));
    }
}

void RemoteControlWorker::deviceError(const QString &error)
{
    if (getMessageQueueToGUI()) {
        getMessageQueueToGUI()->push(RemoteControl::MsgDeviceError::create(error));
    }
}
