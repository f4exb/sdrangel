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

#include <QDebug>

#include "SWGDeviceState.h"
#include "SWGSuccessResponse.h"
#include "SWGErrorResponse.h"

#include "webapi/webapiadapterinterface.h"
#include "device/deviceset.h"
#include "channel/channelapi.h"
#include "maincore.h"

#include "vorlocalizerreport.h"
#include "vorlocalizerworker.h"

MESSAGE_CLASS_DEFINITION(VorLocalizerWorker::MsgConfigureVORLocalizerWorker, Message)
MESSAGE_CLASS_DEFINITION(VorLocalizerWorker::MsgRefreshChannels, Message)

class DSPDeviceSourceEngine;

VorLocalizerWorker::VorLocalizerWorker(WebAPIAdapterInterface *webAPIAdapterInterface) :
    m_webAPIAdapterInterface(webAPIAdapterInterface),
    m_msgQueueToGUI(nullptr),
    m_running(false),
    m_mutex(QMutex::Recursive)
{
    qDebug("VorLocalizerWorker::VorLocalizerWorker");
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
}

VorLocalizerWorker::~VorLocalizerWorker()
{
    m_inputMessageQueue.clear();
}

void VorLocalizerWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

bool VorLocalizerWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = true;
    return m_running;
}

void VorLocalizerWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = false;
}

void VorLocalizerWorker::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool VorLocalizerWorker::handleMessage(const Message& cmd)
{
    if (MsgConfigureVORLocalizerWorker::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureVORLocalizerWorker& cfg = (MsgConfigureVORLocalizerWorker&) cmd;
        qDebug() << "VorLocalizerWorker::handleMessage: MsgConfigureVORLocalizerWorker";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgRefreshChannels::match(cmd))
    {
        qDebug() << "VorLocalizerWorker::handleMessage: MsgRefreshChannels";
        updateChannels();
        return true;
    }
    else
    {
        return false;
    }
}

void VorLocalizerWorker::applySettings(const VORLocalizerSettings& settings, bool force)
{
    qDebug() << "VorLocalizerWorker::applySettings:"
            << " m_title: " << settings.m_title
            << " m_rgbColor: " << settings.m_rgbColor
            << " force: " << force;

    // Remove sub-channels no longer needed
    for (int i = 0; i < m_vorChannels.size(); i++)
    {
        if (!settings.m_subChannelSettings.contains(m_vorChannels[i].m_subChannelId))
        {
            qDebug() << "VorLocalizerWorker::applySettings: Removing sink " << m_vorChannels[i].m_subChannelId;
            removeVORChannel(m_vorChannels[i].m_subChannelId);
        }
    }

    // Add new sub channels
    QHash<int, VORLocalizerSubChannelSettings *>::const_iterator itr = settings.m_subChannelSettings.begin();
    while (itr != settings.m_subChannelSettings.end())
    {
        VORLocalizerSubChannelSettings *subChannelSettings = itr.value();
        int j = 0;

        for (; j < m_vorChannels.size(); j++)
        {
            if (subChannelSettings->m_id == m_vorChannels[j].m_subChannelId)
                break;
        }

        if (j == m_vorChannels.size())
        {
            // Add a sub-channel sink
            qDebug() << "VorLocalizerWorker::applySettings: Adding sink " << subChannelSettings->m_id;
            addVORChannel(subChannelSettings);
        }

        ++itr;
    }

    m_settings = settings;
}

void VorLocalizerWorker::updateHardware()
{
    SWGSDRangel::SWGSuccessResponse response;
    SWGSDRangel::SWGErrorResponse error;
    m_updateTimer.stop();
    m_mutex.unlock();
}

void VorLocalizerWorker::removeVORChannel(int navId)
{
    for (int i = 0; i < m_vorChannels.size(); i++)
    {
        if (m_vorChannels[i].m_subChannelId == navId)
        {
            m_vorChannels.removeAt(i);
            break;
        }
    }
}

void VorLocalizerWorker::addVORChannel(const VORLocalizerSubChannelSettings *subChannelSettings)
{
    VORChannel vorChannel = VORChannel{subChannelSettings->m_id, subChannelSettings->m_frequency, subChannelSettings->m_audioMute};
    m_vorChannels.push_back(vorChannel);
}

void VorLocalizerWorker::updateChannels()
{
    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();
    std::vector<DeviceSet*>::const_iterator it = deviceSets.begin();
    m_availableChannels.clear();

    int deviceIndex = 0;

    for (; it != deviceSets.end(); ++it, deviceIndex++)
    {
        DSPDeviceSourceEngine *deviceSourceEngine =  (*it)->m_deviceSourceEngine;

        if (deviceSourceEngine)
        {
            for (int chi = 0; chi < (*it)->getNumberOfChannels(); chi++)
            {
                ChannelAPI *channel = (*it)->getChannelAt(chi);

                if (channel->getURI() == "sdrangel.channel.vordemodsc")
                {
                    AvailableChannel availableChannel = AvailableChannel{deviceIndex, chi, channel};
                    m_availableChannels.push_back(availableChannel);
                }
            }
        }
    }

    if (m_msgQueueToGUI)
    {
        VORLocalizerReport::MsgReportChannels *msg = VORLocalizerReport::MsgReportChannels::create();
        std::vector<VORLocalizerReport::MsgReportChannels::Channel>& msgChannels = msg->getChannels();

        for (int i = 0; i < m_availableChannels.size(); i++)
        {
            VORLocalizerReport::MsgReportChannels::Channel msgChannel =
                VORLocalizerReport::MsgReportChannels::Channel{
                    m_availableChannels[i].m_deviceSetIndex,
                    m_availableChannels[i].m_channelIndex
                };
            msgChannels.push_back(msgChannel);
        }

        m_msgQueueToGUI->push(msg);
    }
}
