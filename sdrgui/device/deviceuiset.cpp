///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <QFont>

#include "gui/glspectrum.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrumgui.h"
#include "gui/channelwindow.h"
#include "gui/samplingdevicecontrol.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "plugin/plugininstancegui.h"
#include "plugin/pluginapi.h"
#include "plugin/plugininterface.h"
#include "channel/channelutils.h"
#include "settings/preset.h"

#include "deviceuiset.h"

DeviceUISet::DeviceUISet(int tabIndex, int deviceType, QTimer& timer)
{
    m_spectrum = new GLSpectrum;
    if ((deviceType == 0) || (deviceType == 2)) { // Single Rx or MIMO
        m_spectrumVis = new SpectrumVis(SDR_RX_SCALEF, m_spectrum);
    } else if (deviceType == 1) { // Single Tx
        m_spectrumVis = new SpectrumVis(SDR_TX_SCALEF, m_spectrum);
    }
    m_spectrum->connectTimer(timer);
    m_spectrumGUI = new GLSpectrumGUI;
    m_spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, m_spectrum);
    m_channelWindow = new ChannelWindow;
    m_samplingDeviceControl = new SamplingDeviceControl(tabIndex, deviceType);
    m_deviceAPI = nullptr;
    m_deviceSourceEngine = nullptr;
    m_deviceSinkEngine = nullptr;
    m_deviceMIMOEngine = nullptr;
    m_deviceTabIndex = tabIndex;
    m_nbAvailableRxChannels = 0;   // updated at enumeration for UI selector
    m_nbAvailableTxChannels = 0;   // updated at enumeration for UI selector
    m_nbAvailableMIMOChannels = 0; // updated at enumeration for UI selector

    // m_spectrum needs to have its font to be set since it cannot be inherited from the main window
    QFont font;
    font.setFamily(QStringLiteral("Liberation Sans"));
    font.setPointSize(9);
    m_spectrum->setFont(font);
}

DeviceUISet::~DeviceUISet()
{
    delete m_samplingDeviceControl;
    delete m_channelWindow;
    delete m_spectrumGUI;
    delete m_spectrumVis;
    delete m_spectrum;
}

void DeviceUISet::setSpectrumScalingFactor(float scalef)
{
    m_spectrumVis->setScalef(m_spectrumVis->getInputMessageQueue(), scalef);
}

void DeviceUISet::addChannelMarker(ChannelMarker* channelMarker)
{
    m_spectrum->addChannelMarker(channelMarker);
}

void DeviceUISet::addRollupWidget(QWidget *widget)
{
    m_channelWindow->addRollupWidget(widget);
}

void DeviceUISet::registerRxChannelInstance(const QString& channelName, PluginInstanceGUI* pluginGUI)
{
    m_channelInstanceRegistrations.append(ChannelInstanceRegistration(channelName, pluginGUI, 0));
    renameChannelInstances();
}

void DeviceUISet::registerTxChannelInstance(const QString& channelName, PluginInstanceGUI* pluginGUI)
{
    m_channelInstanceRegistrations.append(ChannelInstanceRegistration(channelName, pluginGUI, 1));
    renameChannelInstances();
}

void DeviceUISet::registerChannelInstance(const QString& channelName, PluginInstanceGUI* pluginGUI)
{
    m_channelInstanceRegistrations.append(ChannelInstanceRegistration(channelName, pluginGUI, 2));
    renameChannelInstances();
}

void DeviceUISet::removeRxChannelInstance(PluginInstanceGUI* pluginGUI)
{
    for (ChannelInstanceRegistrations::iterator it = m_channelInstanceRegistrations.begin(); it != m_channelInstanceRegistrations.end(); ++it)
    {
        if (it->m_gui == pluginGUI)
        {
            m_channelInstanceRegistrations.erase(it);
            break;
        }
    }

    renameChannelInstances();
}

void DeviceUISet::removeTxChannelInstance(PluginInstanceGUI* pluginGUI)
{
    for (ChannelInstanceRegistrations::iterator it = m_channelInstanceRegistrations.begin(); it != m_channelInstanceRegistrations.end(); ++it)
    {
        if (it->m_gui == pluginGUI)
        {
            m_channelInstanceRegistrations.erase(it);
            break;
        }
    }

    renameChannelInstances();
}

void DeviceUISet::removeChannelInstance(PluginInstanceGUI* pluginGUI)
{
    for (ChannelInstanceRegistrations::iterator it = m_channelInstanceRegistrations.begin(); it != m_channelInstanceRegistrations.end(); ++it)
    {
        if (it->m_gui == pluginGUI)
        {
            m_channelInstanceRegistrations.erase(it);
            break;
        }
    }

    renameChannelInstances();
}

void DeviceUISet::freeChannels()
{
    for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
    {
        qDebug("DeviceUISet::freeChannels: destroying channel [%s]", qPrintable(m_channelInstanceRegistrations[i].m_channelName));
        m_channelInstanceRegistrations[i].m_gui->destroy();
    }
}

void DeviceUISet::deleteChannel(int channelIndex)
{
    if ((channelIndex >= 0) && (channelIndex < m_channelInstanceRegistrations.count()))
    {
        qDebug("DeviceUISet::deleteChannel: delete channel [%s] at %d",
                qPrintable(m_channelInstanceRegistrations[channelIndex].m_channelName),
                channelIndex);
        m_channelInstanceRegistrations.removeAt(channelIndex);
        renameChannelInstances();
    }
}

void DeviceUISet::loadRxChannelSettings(const Preset *preset, PluginAPI *pluginAPI)
{
    if (preset->isSourcePreset())
    {
        qDebug("DeviceUISet::loadRxChannelSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // Available channel plugins
        PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getRxChannelRegistrations();

        // copy currently open channels and clear list
        ChannelInstanceRegistrations openChannels = m_channelInstanceRegistrations;
        m_channelInstanceRegistrations.clear();

        for(int i = 0; i < openChannels.count(); i++)
        {
            qDebug("DeviceUISet::loadRxChannelSettings: destroying old channel [%s]", qPrintable(openChannels[i].m_channelName));
            openChannels[i].m_gui->destroy(); // FIXME: stop channel before
        }

        qDebug("DeviceUISet::loadRxChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for (int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelInstanceRegistration reg;

            // create channel instance

            for(int i = 0; i < channelRegistrations->count(); i++)
            {
                //if((*channelRegistrations)[i].m_channelIdURI == channelConfig.m_channelIdURI)
                if (ChannelUtils::compareChannelURIs((*channelRegistrations)[i].m_channelIdURI, channelConfig.m_channelIdURI))
                {
                    qDebug("DeviceUISet::loadRxChannelSettings: creating new channel [%s] from config [%s]",
                            qPrintable((*channelRegistrations)[i].m_channelIdURI),
                            qPrintable(channelConfig.m_channelIdURI));
                    BasebandSampleSink *rxChannel =
                            (*channelRegistrations)[i].m_plugin->createRxChannelBS(m_deviceAPI);
                    PluginInstanceGUI *rxChannelGUI =
                            (*channelRegistrations)[i].m_plugin->createRxChannelGUI(this, rxChannel);
                    reg = ChannelInstanceRegistration(channelConfig.m_channelIdURI, rxChannelGUI, 0);
                    break;
                }
            }

            if (reg.m_gui != 0)
            {
                qDebug("DeviceUISet::loadRxChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                reg.m_gui->deserialize(channelConfig.m_config);
            }
        }

        renameChannelInstances();
    }
    else
    {
        qDebug("DeviceUISet::loadRxChannelSettings: Loading preset [%s | %s] not a source preset", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }
}

void DeviceUISet::saveRxChannelSettings(Preset *preset)
{
    if (preset->isSourcePreset())
    {
        qSort(m_channelInstanceRegistrations.begin(), m_channelInstanceRegistrations.end()); // sort by increasing delta frequency and type

        for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceUISet::saveRxChannelSettings: saving channel [%s]", qPrintable(m_channelInstanceRegistrations[i].m_channelName));
            preset->addChannel(m_channelInstanceRegistrations[i].m_channelName, m_channelInstanceRegistrations[i].m_gui->serialize());
        }
    }
    else
    {
        qDebug("DeviceUISet::saveRxChannelSettings: not a source preset");
    }
}

void DeviceUISet::loadTxChannelSettings(const Preset *preset, PluginAPI *pluginAPI)
{
    if (preset->isSinkPreset())
    {
        qDebug("DeviceUISet::loadTxChannelSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // Available channel plugins
        PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getTxChannelRegistrations();

        // copy currently open channels and clear list
        ChannelInstanceRegistrations openChannels = m_channelInstanceRegistrations;
        m_channelInstanceRegistrations.clear();

        for(int i = 0; i < openChannels.count(); i++)
        {
            qDebug("DeviceUISet::loadTxChannelSettings: destroying old channel [%s]", qPrintable(openChannels[i].m_channelName));
            openChannels[i].m_gui->destroy();
        }

        qDebug("DeviceUISet::loadTxChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for(int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelInstanceRegistration reg;

            // create channel instance

            for(int i = 0; i < channelRegistrations->count(); i++)
            {
                if ((*channelRegistrations)[i].m_channelIdURI == channelConfig.m_channelIdURI)
                {
                    qDebug("DeviceUISet::loadTxChannelSettings: creating new channel [%s] from config [%s]",
                            qPrintable((*channelRegistrations)[i].m_channelIdURI),
                            qPrintable(channelConfig.m_channelIdURI));
                    BasebandSampleSource *txChannel =
                            (*channelRegistrations)[i].m_plugin->createTxChannelBS(m_deviceAPI);
                    PluginInstanceGUI *txChannelGUI =
                            (*channelRegistrations)[i].m_plugin->createTxChannelGUI(this, txChannel);
                    reg = ChannelInstanceRegistration(channelConfig.m_channelIdURI, txChannelGUI, 1);
                    break;
                }
            }

            if(reg.m_gui != 0)
            {
                qDebug("DeviceUISet::loadTxChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                reg.m_gui->deserialize(channelConfig.m_config);
            }
        }

        renameChannelInstances();
    }
    else
    {
        qDebug("DeviceUISet::loadTxChannelSettings: Loading preset [%s | %s] not a sink preset", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }

}

void DeviceUISet::saveTxChannelSettings(Preset *preset)
{
    if (preset->isSinkPreset())
    {
        qSort(m_channelInstanceRegistrations.begin(), m_channelInstanceRegistrations.end()); // sort by increasing delta frequency and type

        for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceUISet::saveTxChannelSettings: saving channel [%s]", qPrintable(m_channelInstanceRegistrations[i].m_channelName));
            preset->addChannel(m_channelInstanceRegistrations[i].m_channelName, m_channelInstanceRegistrations[i].m_gui->serialize());
        }
    }
    else
    {
        qDebug("DeviceUISet::saveTxChannelSettings: not a sink preset");
    }
}

void DeviceUISet::loadMIMOChannelSettings(const Preset *preset, PluginAPI *pluginAPI)
{
    if (preset->isMIMOPreset())
    {
        qDebug("DeviceUISet::loadMIMOChannelSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // Available channel plugins
        PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getMIMOChannelRegistrations();

        // copy currently open channels and clear list
        ChannelInstanceRegistrations openChannels = m_channelInstanceRegistrations;
        m_channelInstanceRegistrations.clear();

        for(int i = 0; i < openChannels.count(); i++)
        {
            qDebug("DeviceUISet::loadMIMOChannelSettings: destroying old channel [%s]", qPrintable(openChannels[i].m_channelName));
            openChannels[i].m_gui->destroy(); // FIXME: stop channel before
        }

        qDebug("DeviceUISet::loadMIMOChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for (int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelInstanceRegistration reg;

            // create channel instance

            for(int i = 0; i < channelRegistrations->count(); i++)
            {
                //if((*channelRegistrations)[i].m_channelIdURI == channelConfig.m_channelIdURI)
                if (ChannelUtils::compareChannelURIs((*channelRegistrations)[i].m_channelIdURI, channelConfig.m_channelIdURI))
                {
                    qDebug("DeviceUISet::loadMIMOChannelSettings: creating new channel [%s] from config [%s]",
                            qPrintable((*channelRegistrations)[i].m_channelIdURI),
                            qPrintable(channelConfig.m_channelIdURI));
                    MIMOChannel *mimoChannel =
                            (*channelRegistrations)[i].m_plugin->createMIMOChannelBS(m_deviceAPI);
                    PluginInstanceGUI *mimoChannelGUI =
                            (*channelRegistrations)[i].m_plugin->createMIMOChannelGUI(this, mimoChannel);
                    reg = ChannelInstanceRegistration(channelConfig.m_channelIdURI, mimoChannelGUI, 2);
                    break;
                }
            }

            if (reg.m_gui != 0)
            {
                qDebug("DeviceUISet::loadMIMOChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                reg.m_gui->deserialize(channelConfig.m_config);
            }
        }

        renameChannelInstances();
    }
    else
    {
        qDebug("DeviceUISet::loadMIMOChannelSettings: Loading preset [%s | %s] not a MIMO preset", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }
}

void DeviceUISet::saveMIMOChannelSettings(Preset *preset)
{
    if (preset->isMIMOPreset())
    {
        qSort(m_channelInstanceRegistrations.begin(), m_channelInstanceRegistrations.end()); // sort by increasing delta frequency and type

        for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceUISet::saveMIMOChannelSettings: saving channel [%s]", qPrintable(m_channelInstanceRegistrations[i].m_channelName));
            preset->addChannel(m_channelInstanceRegistrations[i].m_channelName, m_channelInstanceRegistrations[i].m_gui->serialize());
        }
    }
    else
    {
        qDebug("DeviceUISet::saveMIMOChannelSettings: not a MIMO preset");
    }
}

void DeviceUISet::renameChannelInstances()
{
    for (int i = 0; i < m_channelInstanceRegistrations.count(); i++) {
        m_channelInstanceRegistrations[i].m_gui->setName(QString("%1:%2").arg(m_channelInstanceRegistrations[i].m_channelName).arg(i));
    }
}

// sort by increasing delta frequency and type (i.e. name)
bool DeviceUISet::ChannelInstanceRegistration::operator<(const ChannelInstanceRegistration& other) const
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

