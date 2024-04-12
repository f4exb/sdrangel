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

#include <QDebug>

#include "webapi/webapiadapterinterface.h"
#include "channel/channelwebapiutils.h"
#include "device/deviceset.h"
#include "device/deviceapi.h"
#include "maincore.h"

#include "sid.h"
#include "sidworker.h"

SIDWorker::SIDWorker(SIDMain *sid, WebAPIAdapterInterface *webAPIAdapterInterface) :
    m_sid(sid),
    m_webAPIAdapterInterface(webAPIAdapterInterface),
    m_msgQueueToFeature(nullptr),
    m_msgQueueToGUI(nullptr),
    m_pollTimer(this)
{
}

SIDWorker::~SIDWorker()
{
    stopWork();
    m_inputMessageQueue.clear();
}

void SIDWorker::startWork()
{
    qDebug("SIDWorker::startWork");
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_pollTimer, &QTimer::timeout, this, &SIDWorker::update);
    m_pollTimer.start(1000);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    // Handle any messages already on the queue
    handleInputMessages();
}

void SIDWorker::stopWork()
{
    qDebug("SIDWorker::stopWork");
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_pollTimer.stop();
    disconnect(&m_pollTimer, &QTimer::timeout, this, &SIDWorker::update);
}

void SIDWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool SIDWorker::handleMessage(const Message& cmd)
{
    if (SIDMain::MsgConfigureSID::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        SIDMain::MsgConfigureSID& cfg = (SIDMain::MsgConfigureSID&) cmd;

        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());
        return true;
    }
    else
    {
        return false;
    }
}

void SIDWorker::applySettings(const SIDSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "SIDWorker::applySettings:" << settings.getDebugString(settingsKeys, force) << force;

    if (settingsKeys.contains("period") || force)
    {
        m_pollTimer.stop();
        m_pollTimer.start(settings.m_period * 1000);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void SIDWorker::update()
{
    // Get powers from each channel
    QDateTime dateTime = QDateTime::currentDateTime();
    QStringList ids;
    QList<double> measurements;

    for (const auto& channelSettings : m_settings.m_channelSettings)
    {
        if (channelSettings.m_enabled)
        {
            unsigned int deviceSetIndex, channelIndex;

            if (MainCore::getDeviceAndChannelIndexFromId(channelSettings.m_id, deviceSetIndex, channelIndex))
            {
                // Check device is running
                std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
                if (deviceSetIndex < deviceSets.size())
                {
                    DeviceSet *deviceSet = deviceSets[deviceSetIndex];
                    if (deviceSet && (deviceSet->m_deviceAPI->state() == DeviceAPI::StRunning))
                    {
                        double power;
                        if (ChannelWebAPIUtils::getChannelReportValue(deviceSetIndex, channelIndex, "channelPowerDB", power))
                        {
                            if (getMessageQueueToGUI())
                            {
                                ids.append(channelSettings.m_id);
                                measurements.append(power);
                            }
                        }
                        else
                        {
                            qDebug() << "SIDWorker::update: Failed to get power for channel " << channelSettings.m_id;
                        }
                    }
                }
            }
            else
            {
                qDebug() << "SIDWorker::update: Malformed channel id: " << channelSettings.m_id;
            }
        }
    }

    if (getMessageQueueToGUI() && (ids.size() > 0))
    {
        SIDMain::MsgMeasurement *msgToGUI = SIDMain::MsgMeasurement::create(dateTime, ids, measurements);
        getMessageQueueToGUI()->push(msgToGUI);
    }
}
