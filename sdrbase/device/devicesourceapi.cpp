///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <plugin/plugininstancegui.h>
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "dsp/devicesamplesource.h"
#include "plugin/plugininterface.h"
#include "settings/preset.h"
#include "dsp/dspengine.h"
#include "channel/channelsinkapi.h"

DeviceSourceAPI::DeviceSourceAPI(int deviceTabIndex,
        DSPDeviceSourceEngine *deviceSourceEngine) :
    m_deviceTabIndex(deviceTabIndex),
    m_deviceSourceEngine(deviceSourceEngine),
    m_sampleSourceSequence(0),
    m_nbItems(1),
    m_itemIndex(0),
    m_pluginInterface(0),
    m_sampleSourcePluginInstanceUI(0),
    m_buddySharedPtr(0),
    m_isBuddyLeader(false),
    m_masterTimer(DSPEngine::instance()->getMasterTimer())
{
}

DeviceSourceAPI::~DeviceSourceAPI()
{
}

void DeviceSourceAPI::addSink(BasebandSampleSink *sink)
{
    m_deviceSourceEngine->addSink(sink);
}

void DeviceSourceAPI::removeSink(BasebandSampleSink* sink)
{
    m_deviceSourceEngine->removeSink(sink);
}

void DeviceSourceAPI::addThreadedSink(ThreadedBasebandSampleSink* sink)
{
    m_deviceSourceEngine->addThreadedSink(sink);
}

void DeviceSourceAPI::removeThreadedSink(ThreadedBasebandSampleSink* sink)
{
    m_deviceSourceEngine->removeThreadedSink(sink);
}

void DeviceSourceAPI::addChannelAPI(ChannelSinkAPI* channelAPI)
{
    m_channelAPIs.append(channelAPI);
    renumerateChannels();
}

void DeviceSourceAPI::removeChannelAPI(ChannelSinkAPI* channelAPI)
{
    if (m_channelAPIs.removeOne(channelAPI)) {
        renumerateChannels();
    }

    channelAPI->setIndexInDeviceSet(-1);
}

ChannelSinkAPI *DeviceSourceAPI::getChanelAPIAt(int index)
{
    if (index < m_channelAPIs.size()) {
        return m_channelAPIs.at(index);
    } else {
        return 0;
    }
}

void DeviceSourceAPI::renumerateChannels()
{
    for (int i = 0; i < m_channelAPIs.size(); ++i) {
        m_channelAPIs.at(i)->setIndexInDeviceSet(i);
    }
}

void DeviceSourceAPI::setSampleSource(DeviceSampleSource* source)
{
    m_deviceSourceEngine->setSource(source);
}

DeviceSampleSource *DeviceSourceAPI::getSampleSource()
{
    return m_deviceSourceEngine->getSource();
}

bool DeviceSourceAPI::initAcquisition()
{
    return m_deviceSourceEngine->initAcquisition();
}

bool DeviceSourceAPI::startAcquisition()
{
    return m_deviceSourceEngine->startAcquisition();
}

void DeviceSourceAPI::stopAcquisition()
{
    m_deviceSourceEngine->stopAcquistion();
}

DSPDeviceSourceEngine::State DeviceSourceAPI::state() const
{
    return m_deviceSourceEngine->state();
}

QString DeviceSourceAPI::errorMessage()
{
    return m_deviceSourceEngine->errorMessage();
}

uint DeviceSourceAPI::getDeviceUID() const
{
    return m_deviceSourceEngine->getUID();
}

MessageQueue *DeviceSourceAPI::getDeviceEngineInputMessageQueue()
{
    return m_deviceSourceEngine->getInputMessageQueue();
}

MessageQueue *DeviceSourceAPI::getSampleSourceInputMessageQueue()
{
    return getSampleSource()->getInputMessageQueue();
}

MessageQueue *DeviceSourceAPI::getSampleSourceGUIMessageQueue()
{
    return getSampleSource()->getMessageQueueToGUI();
}


void DeviceSourceAPI::configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection)
{
    m_deviceSourceEngine->configureCorrections(dcOffsetCorrection, iqImbalanceCorrection);
}

void DeviceSourceAPI::setHardwareId(const QString& id)
{
    m_hardwareId = id;
}

void DeviceSourceAPI::setSampleSourceId(const QString& id)
{
    m_sampleSourceId = id;
}

void DeviceSourceAPI::resetSampleSourceId()
{
    m_sampleSourceId.clear();
}

void DeviceSourceAPI::setSampleSourceSerial(const QString& serial)
{
    m_sampleSourceSerial = serial;
}

void DeviceSourceAPI::setSampleSourceDisplayName(const QString& name)
{
    m_sampleSourceDisplayName = name;
}

void DeviceSourceAPI::setSampleSourceSequence(int sequence)
{
    m_sampleSourceSequence = sequence;
    m_deviceSourceEngine->setSourceSequence(sequence);
}

void DeviceSourceAPI::setNbItems(uint32_t nbItems)
{
    m_nbItems = nbItems;
}

void DeviceSourceAPI::setItemIndex(uint32_t index)
{
    m_itemIndex = index;
}

void DeviceSourceAPI::setSampleSourcePluginInterface(PluginInterface *iface)
{
    m_pluginInterface = iface;
}

void DeviceSourceAPI::setSampleSourcePluginInstanceGUI(PluginInstanceGUI *gui)
{
    m_sampleSourcePluginInstanceUI = gui;
}

void DeviceSourceAPI::getDeviceEngineStateStr(QString& state)
{
    if (m_deviceSourceEngine)
    {
        switch(m_deviceSourceEngine->state())
        {
        case DSPDeviceSourceEngine::StNotStarted:
            state = "notStarted";
            break;
        case DSPDeviceSourceEngine::StIdle:
            state = "idle";
            break;
        case DSPDeviceSourceEngine::StReady:
            state = "ready";
            break;
        case DSPDeviceSourceEngine::StRunning:
            state = "running";
            break;
        case DSPDeviceSourceEngine::StError:
            state = "error";
            break;
        default:
            state = "notStarted";
            break;
        }
    }
    else
    {
        state = "notStarted";
    }
}

void DeviceSourceAPI::loadSourceSettings(const Preset* preset)
{
    if (preset->isSourcePreset())
    {
        qDebug("DeviceSourceAPI::loadSourceSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        if(m_sampleSourcePluginInstanceUI != 0)
        {
            const QByteArray* sourceConfig = preset->findBestDeviceConfig(m_sampleSourceId, m_sampleSourceSerial, m_sampleSourceSequence);

            if (sourceConfig != 0)
            {
                qDebug("DeviceSourceAPI::loadSettings: deserializing source %s[%d]: %s", qPrintable(m_sampleSourceId), m_sampleSourceSequence, qPrintable(m_sampleSourceSerial));
                m_sampleSourcePluginInstanceUI->deserialize(*sourceConfig);
            }
            else
            {
                qDebug("DeviceSourceAPI::loadSettings: source %s[%d]: %s not found", qPrintable(m_sampleSourceId), m_sampleSourceSequence, qPrintable(m_sampleSourceSerial));
            }

            qint64 centerFrequency = preset->getCenterFrequency();
            qDebug("DeviceSourceAPI::loadSettings: center frequency: %llu Hz", centerFrequency);
            m_sampleSourcePluginInstanceUI->setCenterFrequency(centerFrequency);
        }
    }
    else
    {
        qDebug("DeviceSourceAPI::loadSourceSettings: Loading preset [%s | %s] is not a source preset", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }
}

void DeviceSourceAPI::saveSourceSettings(Preset* preset)
{
    if (preset->isSourcePreset())
    {
        if(m_sampleSourcePluginInstanceUI != NULL)
        {
            qDebug("DeviceSourceAPI::saveSourceSettings: %s saved", qPrintable(m_sampleSourcePluginInstanceUI->getName()));
            preset->addOrUpdateDeviceConfig(m_sampleSourceId, m_sampleSourceSerial, m_sampleSourceSequence, m_sampleSourcePluginInstanceUI->serialize());
            preset->setCenterFrequency(m_sampleSourcePluginInstanceUI->getCenterFrequency());
        }
        else
        {
            qDebug("DeviceSourceAPI::saveSourceSettings: no source");
        }
    }
    else
    {
        qDebug("DeviceSourceAPI::saveSourceSettings: not a source preset");
    }
}

void DeviceSourceAPI::addSourceBuddy(DeviceSourceAPI* buddy)
{
    m_sourceBuddies.push_back(buddy);
    buddy->m_sourceBuddies.push_back(this);
    qDebug("DeviceSourceAPI::addSourceBuddy: added buddy %s(%s) [%lx] <-> [%lx]",
            qPrintable(buddy->getHardwareId()),
            qPrintable(buddy->getSampleSourceSerial()),
            (uint64_t) buddy,
            (uint64_t) this);
}

void DeviceSourceAPI::addSinkBuddy(DeviceSinkAPI* buddy)
{
    m_sinkBuddies.push_back(buddy);
    buddy->m_sourceBuddies.push_back(this);
    qDebug("DeviceSourceAPI::addSinkBuddy: added buddy %s(%s) [%lx] <-> [%lx]",
            qPrintable(buddy->getHardwareId()),
            qPrintable(buddy->getSampleSinkSerial()),
            (uint64_t) buddy,
            (uint64_t) this);
}

void DeviceSourceAPI::removeSourceBuddy(DeviceSourceAPI* buddy)
{
    std::vector<DeviceSourceAPI*>::iterator it = m_sourceBuddies.begin();

    for (;it != m_sourceBuddies.end(); ++it)
    {
        if (*it == buddy)
        {
            qDebug("DeviceSourceAPI::removeSourceBuddy: buddy %s(%s) [%lx] removed from the list of [%lx]",
                    qPrintable(buddy->getHardwareId()),
                    qPrintable(buddy->getSampleSourceSerial()),
                    (uint64_t) (*it),
                    (uint64_t) this);
            m_sourceBuddies.erase(it);
            return;
        }
    }

    qDebug("DeviceSourceAPI::removeSourceBuddy: buddy %s(%s) [%lx] not found in the list of [%lx]",
            qPrintable(buddy->getHardwareId()),
            qPrintable(buddy->getSampleSourceSerial()),
            (uint64_t) buddy,
            (uint64_t) this);
}

void DeviceSourceAPI::removeSinkBuddy(DeviceSinkAPI* buddy)
{
    std::vector<DeviceSinkAPI*>::iterator it = m_sinkBuddies.begin();

    for (;it != m_sinkBuddies.end(); ++it)
    {
        if (*it == buddy)
        {
            qDebug("DeviceSourceAPI::removeSinkBuddy: buddy %s(%s) [%lx] removed from the list of [%lx]",
                    qPrintable(buddy->getHardwareId()),
                    qPrintable(buddy->getSampleSinkSerial()),
                    (uint64_t) (*it),
                    (uint64_t) this);
            m_sinkBuddies.erase(it);
            return;
        }
    }

    qDebug("DeviceSourceAPI::removeSinkBuddy: buddy %s(%s) [%lx] not found in the list of [%lx]",
            qPrintable(buddy->getHardwareId()),
            qPrintable(buddy->getSampleSinkSerial()),
            (uint64_t) buddy,
            (uint64_t) this);
}

void DeviceSourceAPI::clearBuddiesLists()
{
    std::vector<DeviceSourceAPI*>::iterator itSource = m_sourceBuddies.begin();
    std::vector<DeviceSinkAPI*>::iterator itSink = m_sinkBuddies.begin();
    bool leaderElected = false;

    for (;itSource != m_sourceBuddies.end(); ++itSource)
    {
        if (isBuddyLeader() && !leaderElected)
        {
            (*itSource)->setBuddyLeader(true);
            leaderElected = true;
        }

        (*itSource)->removeSourceBuddy(this);
    }

    m_sourceBuddies.clear();

    for (;itSink != m_sinkBuddies.end(); ++itSink)
    {
        if (isBuddyLeader() && !leaderElected)
        {
            (*itSink)->setBuddyLeader(true);
            leaderElected = true;
        }

        (*itSink)->removeSourceBuddy(this);
    }

    m_sinkBuddies.clear();
}
