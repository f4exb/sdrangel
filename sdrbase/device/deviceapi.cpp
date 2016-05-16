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

#include "device/deviceapi.h"
#include "plugin/plugingui.h"
#include "plugin/pluginapi.h"
#include "plugin/plugininterface.h"
#include "gui/glspectrum.h"
#include "gui/channelwindow.h"
#include "mainwindow.h"
#include "settings/preset.h"

DeviceAPI::DeviceAPI(MainWindow *mainWindow,
        int deviceTabIndex,
        DSPDeviceEngine *deviceEngine,
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

DeviceAPI::~DeviceAPI()
{
}

void DeviceAPI::addSink(SampleSink *sink)
{
    m_deviceEngine->addSink(sink);
}

void DeviceAPI::removeSink(SampleSink* sink)
{
    m_deviceEngine->removeSink(sink);
}

void DeviceAPI::addThreadedSink(ThreadedSampleSink* sink)
{
    m_deviceEngine->addThreadedSink(sink);
}

void DeviceAPI::removeThreadedSink(ThreadedSampleSink* sink)
{
    m_deviceEngine->removeThreadedSink(sink);
}

void DeviceAPI::setSource(SampleSource* source)
{
    m_deviceEngine->setSource(source);
}

bool DeviceAPI::initAcquisition()
{
    return m_deviceEngine->initAcquisition();
}

bool DeviceAPI::startAcquisition()
{
    return m_deviceEngine->startAcquisition();
}

void DeviceAPI::stopAcquisition()
{
    m_deviceEngine->stopAcquistion();
}

DSPDeviceEngine::State DeviceAPI::state() const
{
    return m_deviceEngine->state();
}

QString DeviceAPI::errorMessage()
{
    return m_deviceEngine->errorMessage();
}

uint DeviceAPI::getDeviceUID() const
{
    return m_deviceEngine->getUID();
}

MessageQueue *DeviceAPI::getDeviceInputMessageQueue()
{
    return m_deviceEngine->getInputMessageQueue();
}

MessageQueue *DeviceAPI::getDeviceOutputMessageQueue()
{
    return m_deviceEngine->getOutputMessageQueue();
}

void DeviceAPI::configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection)
{
    m_deviceEngine->configureCorrections(dcOffsetCorrection, iqImbalanceCorrection);
}

GLSpectrum *DeviceAPI::getSpectrum()
{
    return m_spectrum;
}

void DeviceAPI::addChannelMarker(ChannelMarker* channelMarker)
{
    m_spectrum->addChannelMarker(channelMarker);
}

ChannelWindow *DeviceAPI::getChannelWindow()
{
    return m_channelWindow;
}

void DeviceAPI::addRollupWidget(QWidget *widget)
{
    m_channelWindow->addRollupWidget(widget);
}

void DeviceAPI::setInputGUI(QWidget* inputGUI, const QString& sourceDisplayName)
{
    m_mainWindow->setInputGUI(m_deviceTabIndex, inputGUI, sourceDisplayName);
}

void DeviceAPI::setSampleSourceId(const QString& id)
{
    m_sampleSourceId = id;
}

void DeviceAPI::setSampleSourceSerial(const QString& serial)
{
    m_sampleSourceSerial = serial;
}

void DeviceAPI::setSampleSourceSequence(int sequence)
{
    m_sampleSourceSequence = sequence;
    m_deviceEngine->setSourceSequence(sequence);
}

void DeviceAPI::setSampleSourcePluginGUI(PluginGUI *gui)
{
    m_sampleSourcePluginGUI = gui;
}

void DeviceAPI::registerChannelInstance(const QString& channelName, PluginGUI* pluginGUI)
{
    m_channelInstanceRegistrations.append(ChannelInstanceRegistration(channelName, pluginGUI));
    renameChannelInstances();
}

void DeviceAPI::removeChannelInstance(PluginGUI* pluginGUI)
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

void DeviceAPI::renameChannelInstances()
{
    for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
    {
        m_channelInstanceRegistrations[i].m_gui->setName(QString("%1:%2").arg(m_channelInstanceRegistrations[i].m_channelName).arg(i));
    }
}

void DeviceAPI::freeAll()
{
//    while(!m_channelInstanceRegistrations.isEmpty())
//    {
//        ChannelInstanceRegistration reg(m_channelInstanceRegistrations.takeLast());
//        reg.m_gui->destroy();
//    }

    for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
    {
        qDebug("PluginManager::loadSettings: destroying channel [%s]", qPrintable(m_channelInstanceRegistrations[i].m_channelName));
        m_channelInstanceRegistrations[i].m_gui->destroy();
    }


//    if(m_sampleSourcePluginGUI != 0)
//    {
//        m_deviceEngine->setSource(0);
//        m_sampleSourcePluginGUI->destroy();
//        m_sampleSourcePluginGUI = 0;
//        m_sampleSourceId.clear();
//    }
}

void DeviceAPI::loadSourceSettings(const Preset* preset)
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

void DeviceAPI::saveSourceSettings(Preset* preset)
{
    qDebug("DeviceAPI::saveSourceSettings");

    if(m_sampleSourcePluginGUI != NULL)
    {
        preset->addOrUpdateSourceConfig(m_sampleSourceId, m_sampleSourceSerial, m_sampleSourceSequence, m_sampleSourcePluginGUI->serialize());
        preset->setCenterFrequency(m_sampleSourcePluginGUI->getCenterFrequency());
    }
}

void DeviceAPI::loadChannelSettings(const Preset *preset, PluginAPI *pluginAPI)
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

void DeviceAPI::saveChannelSettings(Preset *preset)
{
    qDebug("DeviceAPI::saveChannelSettings");

    qSort(m_channelInstanceRegistrations.begin(), m_channelInstanceRegistrations.end()); // sort by increasing delta frequency and type

    for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
    {
        preset->addChannel(m_channelInstanceRegistrations[i].m_channelName, m_channelInstanceRegistrations[i].m_gui->serialize());
    }
}

// sort by increasing delta frequency and type (i.e. name)
bool DeviceAPI::ChannelInstanceRegistration::operator<(const ChannelInstanceRegistration& other) const
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
