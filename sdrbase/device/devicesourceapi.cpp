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

#include <device/devicesourceapi.h>
#include "plugin/plugingui.h"
#include "plugin/pluginapi.h"
#include "plugin/plugininterface.h"
#include "gui/glspectrum.h"
#include "gui/channelwindow.h"
#include "mainwindow.h"
#include "settings/preset.h"

DeviceSourceAPI::DeviceSourceAPI(MainWindow *mainWindow,
        int deviceTabIndex,
        DSPDeviceSourceEngine *deviceEngine,
        GLSpectrum *glSpectrum,
        ChannelWindow *channelWindow) :
    m_mainWindow(mainWindow),
    m_deviceTabIndex(deviceTabIndex),
    m_deviceEngine(deviceEngine),
    m_spectrum(glSpectrum),
    m_channelWindow(channelWindow),
    m_sampleSourceSequence(0),
    m_sampleSourcePluginGUI(0)
{
}

DeviceSourceAPI::~DeviceSourceAPI()
{
}

void DeviceSourceAPI::addSink(BasebandSampleSink *sink)
{
    m_deviceEngine->addSink(sink);
}

void DeviceSourceAPI::removeSink(BasebandSampleSink* sink)
{
    m_deviceEngine->removeSink(sink);
}

void DeviceSourceAPI::addThreadedSink(ThreadedBasebandSampleSink* sink)
{
    m_deviceEngine->addThreadedSink(sink);
}

void DeviceSourceAPI::removeThreadedSink(ThreadedBasebandSampleSink* sink)
{
    m_deviceEngine->removeThreadedSink(sink);
}

void DeviceSourceAPI::setSource(DeviceSampleSource* source)
{
    m_deviceEngine->setSource(source);
}

bool DeviceSourceAPI::initAcquisition()
{
    return m_deviceEngine->initAcquisition();
}

bool DeviceSourceAPI::startAcquisition()
{
    return m_deviceEngine->startAcquisition();
}

void DeviceSourceAPI::stopAcquisition()
{
    m_deviceEngine->stopAcquistion();
}

DSPDeviceSourceEngine::State DeviceSourceAPI::state() const
{
    return m_deviceEngine->state();
}

QString DeviceSourceAPI::errorMessage()
{
    return m_deviceEngine->errorMessage();
}

uint DeviceSourceAPI::getDeviceUID() const
{
    return m_deviceEngine->getUID();
}

MessageQueue *DeviceSourceAPI::getDeviceInputMessageQueue()
{
    return m_deviceEngine->getInputMessageQueue();
}

MessageQueue *DeviceSourceAPI::getDeviceOutputMessageQueue()
{
    return m_deviceEngine->getOutputMessageQueue();
}

void DeviceSourceAPI::configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection)
{
    m_deviceEngine->configureCorrections(dcOffsetCorrection, iqImbalanceCorrection);
}

GLSpectrum *DeviceSourceAPI::getSpectrum()
{
    return m_spectrum;
}

void DeviceSourceAPI::addChannelMarker(ChannelMarker* channelMarker)
{
    m_spectrum->addChannelMarker(channelMarker);
}

ChannelWindow *DeviceSourceAPI::getChannelWindow()
{
    return m_channelWindow;
}

void DeviceSourceAPI::addRollupWidget(QWidget *widget)
{
    m_channelWindow->addRollupWidget(widget);
}

void DeviceSourceAPI::setInputGUI(QWidget* inputGUI, const QString& sourceDisplayName)
{
    m_mainWindow->setInputGUI(m_deviceTabIndex, inputGUI, sourceDisplayName);
}

void DeviceSourceAPI::setSampleSourceId(const QString& id)
{
    m_sampleSourceId = id;
}

void DeviceSourceAPI::setSampleSourceSerial(const QString& serial)
{
    m_sampleSourceSerial = serial;
}

void DeviceSourceAPI::setSampleSourceSequence(int sequence)
{
    m_sampleSourceSequence = sequence;
    m_deviceEngine->setSourceSequence(sequence);
}

void DeviceSourceAPI::setSampleSourcePluginGUI(PluginGUI *gui)
{
    if (m_sampleSourcePluginGUI != 0)
    {
        m_sampleSourcePluginGUI->destroy();
        m_sampleSourceId.clear();
    }

    m_sampleSourcePluginGUI = gui;
}

void DeviceSourceAPI::registerChannelInstance(const QString& channelName, PluginGUI* pluginGUI)
{
    m_channelInstanceRegistrations.append(ChannelInstanceRegistration(channelName, pluginGUI));
    renameChannelInstances();
}

void DeviceSourceAPI::removeChannelInstance(PluginGUI* pluginGUI)
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

void DeviceSourceAPI::renameChannelInstances()
{
    for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
    {
        m_channelInstanceRegistrations[i].m_gui->setName(QString("%1:%2").arg(m_channelInstanceRegistrations[i].m_channelName).arg(i));
    }
}

void DeviceSourceAPI::freeAll()
{
//    while(!m_channelInstanceRegistrations.isEmpty())
//    {
//        ChannelInstanceRegistration reg(m_channelInstanceRegistrations.takeLast());
//        reg.m_gui->destroy();
//    }

    for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
    {
        qDebug("DeviceAPI::freeAll: destroying channel [%s]", qPrintable(m_channelInstanceRegistrations[i].m_channelName));
        m_channelInstanceRegistrations[i].m_gui->destroy();
    }


    if(m_sampleSourcePluginGUI != 0)
    {
        qDebug("DeviceAPI::freeAll: destroying m_sampleSourcePluginGUI");
        m_deviceEngine->setSource(0);
        m_sampleSourcePluginGUI->destroy();
        m_sampleSourcePluginGUI = 0;
        m_sampleSourceId.clear();
    }
}

void DeviceSourceAPI::loadSourceSettings(const Preset* preset)
{
    qDebug("DeviceAPI::loadSourceSettings: Loading preset [%s | %s]\n", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

    if(m_sampleSourcePluginGUI != 0)
    {
        const QByteArray* sourceConfig = preset->findBestSourceConfig(m_sampleSourceId, m_sampleSourceSerial, m_sampleSourceSequence);

        if (sourceConfig != 0)
        {
            qDebug() << "DeviceAPI::loadSettings: deserializing source " << qPrintable(m_sampleSourceId);
            m_sampleSourcePluginGUI->deserialize(*sourceConfig);
        }

        qint64 centerFrequency = preset->getCenterFrequency();
        m_sampleSourcePluginGUI->setCenterFrequency(centerFrequency);
    }
}

void DeviceSourceAPI::saveSourceSettings(Preset* preset)
{
    qDebug("DeviceAPI::saveSourceSettings");

    if(m_sampleSourcePluginGUI != NULL)
    {
        preset->addOrUpdateSourceConfig(m_sampleSourceId, m_sampleSourceSerial, m_sampleSourceSequence, m_sampleSourcePluginGUI->serialize());
        preset->setCenterFrequency(m_sampleSourcePluginGUI->getCenterFrequency());
    }
}

void DeviceSourceAPI::loadChannelSettings(const Preset *preset, PluginAPI *pluginAPI)
{
    qDebug("DeviceAPI::loadChannelSettings: Loading preset [%s | %s]\n", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

    // Available channel plugins
    PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getChannelRegistrations();

    // copy currently open channels and clear list
    ChannelInstanceRegistrations openChannels = m_channelInstanceRegistrations;
    m_channelInstanceRegistrations.clear();

    for(int i = 0; i < preset->getChannelCount(); i++)
    {
        const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
        ChannelInstanceRegistration reg;

        // if we have one instance available already, use it

        for(int i = 0; i < openChannels.count(); i++)
        {
            qDebug("PluginManager::loadSettings: channels compare [%s] vs [%s]", qPrintable(openChannels[i].m_channelName), qPrintable(channelConfig.m_channel));

            if(openChannels[i].m_channelName == channelConfig.m_channel)
            {
                qDebug("PluginManager::loadSettings: channel [%s] found", qPrintable(openChannels[i].m_channelName));
                reg = openChannels.takeAt(i);
                m_channelInstanceRegistrations.append(reg);
                break;
            }
        }

        // if we haven't one already, create one

        if(reg.m_gui == NULL)
        {
            for(int i = 0; i < channelRegistrations->count(); i++)
            {
                if((*channelRegistrations)[i].m_channelName == channelConfig.m_channel)
                {
                    qDebug("PluginManager::loadSettings: creating new channel [%s]", qPrintable(channelConfig.m_channel));
                    reg = ChannelInstanceRegistration(channelConfig.m_channel, (*channelRegistrations)[i].m_plugin->createChannel(channelConfig.m_channel, this));
                    break;
                }
            }
        }

        if(reg.m_gui != NULL)
        {
            qDebug("PluginManager::loadSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channel));
            reg.m_gui->deserialize(channelConfig.m_config);
        }
    }

    // everything, that is still "available" is not needed anymore
    for(int i = 0; i < openChannels.count(); i++)
    {
        qDebug("PluginManager::loadSettings: destroying spare channel [%s]", qPrintable(openChannels[i].m_channelName));
        openChannels[i].m_gui->destroy();
    }

    renameChannelInstances();
}

void DeviceSourceAPI::saveChannelSettings(Preset *preset)
{
    qDebug("DeviceAPI::saveChannelSettings");

    qSort(m_channelInstanceRegistrations.begin(), m_channelInstanceRegistrations.end()); // sort by increasing delta frequency and type

    for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
    {
        preset->addChannel(m_channelInstanceRegistrations[i].m_channelName, m_channelInstanceRegistrations[i].m_gui->serialize());
    }
}

// sort by increasing delta frequency and type (i.e. name)
bool DeviceSourceAPI::ChannelInstanceRegistration::operator<(const ChannelInstanceRegistration& other) const
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
