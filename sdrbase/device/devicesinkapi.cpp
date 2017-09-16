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

#include <plugin/plugininstanceui.h>
#include "device/devicesinkapi.h"
#include "device/devicesourceapi.h"
#include "plugin/pluginapi.h"
#include "plugin/plugininterface.h"
#include "gui/glspectrum.h"
#include "gui/channelwindow.h"
#include "mainwindow.h"
#include "settings/preset.h"

// TODO: extract GUI dependencies in a separate object
DeviceSinkAPI::DeviceSinkAPI(MainWindow *mainWindow,
        int deviceTabIndex,
        DSPDeviceSinkEngine *deviceSinkEngine,
        GLSpectrum *glSpectrum,
        ChannelWindow *channelWindow) :
    m_mainWindow(mainWindow),
    m_deviceTabIndex(deviceTabIndex),
    m_deviceSinkEngine(deviceSinkEngine),
    m_spectrum(glSpectrum),
    m_channelWindow(channelWindow),
    m_sampleSinkSequence(0),
    m_pluginInterface(0),
    m_sampleSinkPluginInstanceUI(0),
    m_buddySharedPtr(0),
    m_isBuddyLeader(false),
    m_masterTimer(mainWindow->getMasterTimer()) // TODO: get master timer directly not from main window
{
}

DeviceSinkAPI::~DeviceSinkAPI()
{
}

void DeviceSinkAPI::addSpectrumSink(BasebandSampleSink *spectrumSink)
{
    m_deviceSinkEngine->addSpectrumSink(spectrumSink);
}

void DeviceSinkAPI::removeSpectrumSink(BasebandSampleSink* spectrumSink)
{
    m_deviceSinkEngine->removeSpectrumSink(spectrumSink);
}

void DeviceSinkAPI::addSource(BasebandSampleSource *source)
{
    m_deviceSinkEngine->addSource(source);
}

void DeviceSinkAPI::removeSource(BasebandSampleSource* source)
{
    m_deviceSinkEngine->removeSource(source);
}

void DeviceSinkAPI::addThreadedSource(ThreadedBasebandSampleSource* source)
{
    m_deviceSinkEngine->addThreadedSource(source);
}

void DeviceSinkAPI::removeThreadedSource(ThreadedBasebandSampleSource* source)
{
    m_deviceSinkEngine->removeThreadedSource(source);
}

uint32_t DeviceSinkAPI::getNumberOfSources()
{
    return m_deviceSinkEngine->getNumberOfSources();
}

void DeviceSinkAPI::setSampleSink(DeviceSampleSink* sink)
{
    m_deviceSinkEngine->setSink(sink);
}

DeviceSampleSink *DeviceSinkAPI::getSampleSink()
{
    return m_deviceSinkEngine->getSink();
}

bool DeviceSinkAPI::initGeneration()
{
    return m_deviceSinkEngine->initGeneration();
}

bool DeviceSinkAPI::startGeneration()
{
    return m_deviceSinkEngine->startGeneration();
}

void DeviceSinkAPI::stopGeneration()
{
    m_deviceSinkEngine->stopGeneration();
}
DSPDeviceSinkEngine::State DeviceSinkAPI::state() const
{
    return m_deviceSinkEngine->state();
}

QString DeviceSinkAPI::errorMessage()
{
    return m_deviceSinkEngine->errorMessage();
}

uint DeviceSinkAPI::getDeviceUID() const
{
    return m_deviceSinkEngine->getUID();
}

MessageQueue *DeviceSinkAPI::getDeviceEngineInputMessageQueue()
{
    return m_deviceSinkEngine->getInputMessageQueue();
}

MessageQueue *DeviceSinkAPI::getDeviceEngineOutputMessageQueue()
{
    return m_deviceSinkEngine->getOutputMessageQueue();
}

GLSpectrum *DeviceSinkAPI::getSpectrum()
{
    return m_spectrum;
}

void DeviceSinkAPI::addChannelMarker(ChannelMarker* channelMarker)
{
    m_spectrum->addChannelMarker(channelMarker);
}

ChannelWindow *DeviceSinkAPI::getChannelWindow()
{
    return m_channelWindow;
}

void DeviceSinkAPI::addRollupWidget(QWidget *widget)
{
    m_channelWindow->addRollupWidget(widget);
}

void DeviceSinkAPI::setHardwareId(const QString& id)
{
    m_hardwareId = id;
}

void DeviceSinkAPI::setSampleSinkId(const QString& id)
{
    m_sampleSinkId = id;
}

void DeviceSinkAPI::resetSampleSinkId()
{
    m_sampleSinkId.clear();
}

void DeviceSinkAPI::setSampleSinkSerial(const QString& serial)
{
    m_sampleSinkSerial = serial;
}

void DeviceSinkAPI::setSampleSinkDisplayName(const QString& name)
{
    m_sampleSinkDisplayName = name;
}

void DeviceSinkAPI::setSampleSinkSequence(int sequence)
{
    m_sampleSinkSequence = sequence;
    m_deviceSinkEngine->setSinkSequence(sequence);
}

void DeviceSinkAPI::setSampleSinkPluginInterface(PluginInterface *iface)
{
    m_pluginInterface = iface;
}

void DeviceSinkAPI::setSampleSinkPluginInstanceUI(PluginInstanceUI *gui)
{
    m_sampleSinkPluginInstanceUI = gui;
}

void DeviceSinkAPI::registerChannelInstance(const QString& channelName, PluginInstanceUI* pluginGUI)
{
    m_channelInstanceRegistrations.append(ChannelInstanceRegistration(channelName, pluginGUI));
    renameChannelInstances();
}

void DeviceSinkAPI::removeChannelInstance(PluginInstanceUI* pluginGUI)
{
    for(ChannelInstanceRegistrations::iterator it = m_channelInstanceRegistrations.begin(); it != m_channelInstanceRegistrations.end(); ++it)
    {
        if(it->m_gui == pluginGUI)
        {
            m_channelInstanceRegistrations.erase(it);
            break;
        }
    }

    renameChannelInstances();
}

void DeviceSinkAPI::renameChannelInstances()
{
    for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
    {
        m_channelInstanceRegistrations[i].m_gui->setName(QString("%1:%2").arg(m_channelInstanceRegistrations[i].m_channelName).arg(i));
    }
}

void DeviceSinkAPI::freeChannels()
{
    for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
    {
        qDebug("DeviceSinkAPI::freeAll: destroying channel [%s]", qPrintable(m_channelInstanceRegistrations[i].m_channelName));
        m_channelInstanceRegistrations[i].m_gui->destroy();
    }
}

void DeviceSinkAPI::loadSinkSettings(const Preset* preset)
{
    if (preset->isSourcePreset())
    {
        qDebug("DeviceSinkAPI::loadSinkSettings: Preset [%s | %s] is not a sink preset", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }
    else
    {
        qDebug("DeviceSinkAPI::loadSinkSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        if(m_sampleSinkPluginInstanceUI != 0)
        {
            const QByteArray* sourceConfig = preset->findBestDeviceConfig(m_sampleSinkId, m_sampleSinkSerial, m_sampleSinkSequence);

            if (sourceConfig != 0)
            {
                qDebug("DeviceSinkAPI::loadSinkSettings: deserializing sink: %s", qPrintable(m_sampleSinkId));
                m_sampleSinkPluginInstanceUI->deserialize(*sourceConfig);
            }

            qint64 centerFrequency = preset->getCenterFrequency();
            m_sampleSinkPluginInstanceUI->setCenterFrequency(centerFrequency);
        }
    }
}

void DeviceSinkAPI::saveSinkSettings(Preset* preset)
{
    if (preset->isSourcePreset())
    {
        qDebug("DeviceSinkAPI::saveSinkSettings: not a sink preset");
    }
    else
    {
        if(m_sampleSinkPluginInstanceUI != NULL)
        {
            qDebug("DeviceSinkAPI::saveSourceSettings: %s saved", qPrintable(m_sampleSinkPluginInstanceUI->getName()));
            preset->addOrUpdateDeviceConfig(m_sampleSinkId, m_sampleSinkSerial, m_sampleSinkSequence, m_sampleSinkPluginInstanceUI->serialize());
            preset->setCenterFrequency(m_sampleSinkPluginInstanceUI->getCenterFrequency());
        }
        else
        {
            qDebug("DeviceSinkAPI::saveSinkSettings: no sink");
        }
    }
}

void DeviceSinkAPI::loadChannelSettings(const Preset *preset, PluginAPI *pluginAPI)
{
    if (preset->isSourcePreset())
    {
        qDebug("DeviceSinkAPI::loadChannelSettings: Loading preset [%s | %s] not a sink preset", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }
    else
    {
        qDebug("DeviceSinkAPI::loadChannelSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // Available channel plugins
        PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getTxChannelRegistrations();

        // copy currently open channels and clear list
        ChannelInstanceRegistrations openChannels = m_channelInstanceRegistrations;
        m_channelInstanceRegistrations.clear();

        qDebug("DeviceSinkAPI::loadChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for(int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelInstanceRegistration reg;

            // if we have one instance available already, use it

            for(int i = 0; i < openChannels.count(); i++)
            {
                qDebug("DeviceSinkAPI::loadChannelSettings: channels compare [%s] vs [%s]", qPrintable(openChannels[i].m_channelName), qPrintable(channelConfig.m_channel));

                if(openChannels[i].m_channelName == channelConfig.m_channel)
                {
                    qDebug("DeviceSinkAPI::loadChannelSettings: channel [%s] found", qPrintable(openChannels[i].m_channelName));
                    reg = openChannels.takeAt(i);
                    m_channelInstanceRegistrations.append(reg);
                    break;
                }
            }

            // if we haven't one already, create one

            if(reg.m_gui == 0)
            {
                for(int i = 0; i < channelRegistrations->count(); i++)
                {
                    if((*channelRegistrations)[i].m_channelName == channelConfig.m_channel)
                    {
                        qDebug("DeviceSinkAPI::loadChannelSettings: creating new channel [%s]", qPrintable(channelConfig.m_channel));
                        reg = ChannelInstanceRegistration(channelConfig.m_channel, (*channelRegistrations)[i].m_plugin->createTxChannel(channelConfig.m_channel, this));
                        break;
                    }
                }
            }

            if(reg.m_gui != 0)
            {
                qDebug("DeviceSinkAPI::loadChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channel));
                reg.m_gui->deserialize(channelConfig.m_config);
            }
        }

        // everything, that is still "available" is not needed anymore
        for(int i = 0; i < openChannels.count(); i++)
        {
            qDebug("DeviceSinkAPI::loadChannelSettings: destroying spare channel [%s]", qPrintable(openChannels[i].m_channelName));
            openChannels[i].m_gui->destroy();
        }

        renameChannelInstances();
    }
}

void DeviceSinkAPI::saveChannelSettings(Preset *preset)
{
    if (preset->isSourcePreset())
    {
        qDebug("DeviceSinkAPI::saveChannelSettings: not a sink preset");
    }
    else
    {
        qSort(m_channelInstanceRegistrations.begin(), m_channelInstanceRegistrations.end()); // sort by increasing delta frequency and type

        for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceSinkAPI::saveChannelSettings: channel [%s] saved", qPrintable(m_channelInstanceRegistrations[i].m_channelName));
            preset->addChannel(m_channelInstanceRegistrations[i].m_channelName, m_channelInstanceRegistrations[i].m_gui->serialize());
        }
    }
}

// sort by increasing delta frequency and type (i.e. name)
bool DeviceSinkAPI::ChannelInstanceRegistration::operator<(const ChannelInstanceRegistration& other) const
{
    if (m_gui && other.m_gui)
    {
        if (m_gui->getCenterFrequency() == other.m_gui->getCenterFrequency())
        {
            return m_gui->getName() < other.m_gui->getName();
        }
        else
        {
            return m_gui->getCenterFrequency() < other.m_gui->getCenterFrequency();
        }
    }
    else
    {
        return false;
    }
}

void DeviceSinkAPI::addSourceBuddy(DeviceSourceAPI* buddy)
{
    std::vector<DeviceSourceAPI*>::iterator it = m_sourceBuddies.begin();

    m_sourceBuddies.push_back(buddy);
    buddy->m_sinkBuddies.push_back(this);
    qDebug("DeviceSinkAPI::addSourceBuddy: added buddy %s(%s) to the list [%lx] <-> [%lx]",
            qPrintable(buddy->getHardwareId()),
            qPrintable(buddy->getSampleSourceSerial()),
            (uint64_t) buddy,
            (uint64_t) this);
}

void DeviceSinkAPI::addSinkBuddy(DeviceSinkAPI* buddy)
{
    std::vector<DeviceSinkAPI*>::iterator it = m_sinkBuddies.begin();

    m_sinkBuddies.push_back(buddy);
    buddy->m_sinkBuddies.push_back(this);
    qDebug("DeviceSinkAPI::addSinkBuddy: added buddy %s(%s) to the list [%lx] <-> [%lx]",
            qPrintable(buddy->getHardwareId()),
            qPrintable(buddy->getSampleSinkSerial()),
            (uint64_t) buddy,
            (uint64_t) this);
}

void DeviceSinkAPI::removeSourceBuddy(DeviceSourceAPI* buddy)
{
    std::vector<DeviceSourceAPI*>::iterator it = m_sourceBuddies.begin();

    for (;it != m_sourceBuddies.end(); ++it)
    {
        if (*it == buddy)
        {
            qDebug("DeviceSinkAPI::removeSourceBuddy: buddy %s(%s) [%lx] removed from the list of [%lx]",
                    qPrintable(buddy->getHardwareId()),
                    qPrintable(buddy->getSampleSourceSerial()),
                    (uint64_t) (*it),
                    (uint64_t) this);
            m_sourceBuddies.erase(it);
            return;
        }
    }

    qDebug("DeviceSinkAPI::removeSourceBuddy: buddy %s(%s) [%lx] not found in the list of [%lx]",
            qPrintable(buddy->getHardwareId()),
            qPrintable(buddy->getSampleSourceSerial()),
            (uint64_t) buddy,
            (uint64_t) this);
}

void DeviceSinkAPI::removeSinkBuddy(DeviceSinkAPI* buddy)
{
    std::vector<DeviceSinkAPI*>::iterator it = m_sinkBuddies.begin();

    for (;it != m_sinkBuddies.end(); ++it)
    {
        if (*it == buddy)
        {
            qDebug("DeviceSinkAPI::removeSinkBuddy: buddy %s(%s) [%lx] removed from the list of [%lx]",
                    qPrintable(buddy->getHardwareId()),
                    qPrintable(buddy->getSampleSinkSerial()),
                    (uint64_t) (*it),
                    (uint64_t) this);
            m_sinkBuddies.erase(it);
            return;
        }
    }

    qDebug("DeviceSinkAPI::removeSinkBuddy: buddy %s(%s) [%lx] not found in the list of [%lx]",
            qPrintable(buddy->getHardwareId()),
            qPrintable(buddy->getSampleSinkSerial()),
            (uint64_t) buddy,
            (uint64_t) this);
}

void DeviceSinkAPI::clearBuddiesLists()
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

        (*itSource)->removeSinkBuddy(this);
    }

    m_sourceBuddies.clear();

    for (;itSink != m_sinkBuddies.end(); ++itSink)
    {
        if (isBuddyLeader() && !leaderElected)
        {
            (*itSink)->setBuddyLeader(true);
            leaderElected = true;
        }

        (*itSink)->removeSinkBuddy(this);
    }

    m_sinkBuddies.clear();
}

