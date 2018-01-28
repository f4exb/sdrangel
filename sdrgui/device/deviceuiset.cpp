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

#include <QFont>

#include "gui/glspectrum.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrumgui.h"
#include "gui/channelwindow.h"
#include "gui/samplingdevicecontrol.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "plugin/plugininstancegui.h"
#include "plugin/pluginapi.h"
#include "plugin/plugininterface.h"
#include "settings/preset.h"

#include "deviceuiset.h"

DeviceUISet::DeviceUISet(int tabIndex, bool rxElseTx, QTimer& timer)
{
    m_spectrum = new GLSpectrum;
    if (rxElseTx) {
        m_spectrumVis = new SpectrumVis(SDR_RX_SCALEF, m_spectrum);
    } else {
        m_spectrumVis = new SpectrumVis(SDR_TX_SCALEF, m_spectrum);
    }
    m_spectrum->connectTimer(timer);
    m_spectrumGUI = new GLSpectrumGUI;
    m_spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, m_spectrum);
    m_channelWindow = new ChannelWindow;
    m_samplingDeviceControl = new SamplingDeviceControl(tabIndex, rxElseTx);
    m_deviceSourceEngine = 0;
    m_deviceSourceAPI = 0;
    m_deviceSinkEngine = 0;
    m_deviceSinkAPI = 0;
    m_deviceTabIndex = tabIndex;

    // m_spectrum needs to have its font to be set since it cannot be inherited from the main window
    QFont font;
    font.setFamily(QStringLiteral("Sans Serif"));
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
    m_rxChannelInstanceRegistrations.append(ChannelInstanceRegistration(channelName, pluginGUI));
    renameRxChannelInstances();
}

void DeviceUISet::registerTxChannelInstance(const QString& channelName, PluginInstanceGUI* pluginGUI)
{
    m_txChannelInstanceRegistrations.append(ChannelInstanceRegistration(channelName, pluginGUI));
    renameTxChannelInstances();
}

void DeviceUISet::removeRxChannelInstance(PluginInstanceGUI* pluginGUI)
{
    for(ChannelInstanceRegistrations::iterator it = m_rxChannelInstanceRegistrations.begin(); it != m_rxChannelInstanceRegistrations.end(); ++it)
    {
        if(it->m_gui == pluginGUI)
        {
            m_rxChannelInstanceRegistrations.erase(it);
            break;
        }
    }

    renameRxChannelInstances();
}

void DeviceUISet::removeTxChannelInstance(PluginInstanceGUI* pluginGUI)
{
    for(ChannelInstanceRegistrations::iterator it = m_txChannelInstanceRegistrations.begin(); it != m_txChannelInstanceRegistrations.end(); ++it)
    {
        if(it->m_gui == pluginGUI)
        {
            m_txChannelInstanceRegistrations.erase(it);
            break;
        }
    }

    renameTxChannelInstances();
}

void DeviceUISet::freeRxChannels()
{
    for(int i = 0; i < m_rxChannelInstanceRegistrations.count(); i++)
    {
        qDebug("DeviceUISet::freeAll: destroying channel [%s]", qPrintable(m_rxChannelInstanceRegistrations[i].m_channelName));
        m_rxChannelInstanceRegistrations[i].m_gui->destroy();
    }
}

void DeviceUISet::freeTxChannels()
{
    for(int i = 0; i < m_txChannelInstanceRegistrations.count(); i++)
    {
        qDebug("DeviceUISet::freeAll: destroying channel [%s]", qPrintable(m_txChannelInstanceRegistrations[i].m_channelName));
        m_txChannelInstanceRegistrations[i].m_gui->destroy();
    }
}

void DeviceUISet::deleteRxChannel(int channelIndex)
{
    if ((channelIndex >= 0) && (channelIndex < m_rxChannelInstanceRegistrations.count()))
    {
        qDebug("DeviceUISet::deleteRxChennel: delete channel [%s] at %d",
                qPrintable(m_rxChannelInstanceRegistrations[channelIndex].m_channelName),
                channelIndex);
        m_rxChannelInstanceRegistrations[channelIndex].m_gui->destroy();
        m_rxChannelInstanceRegistrations.removeAt(channelIndex);
        renameRxChannelInstances();
    }
}

void DeviceUISet::deleteTxChannel(int channelIndex)
{
    if ((channelIndex >= 0) && (channelIndex < m_txChannelInstanceRegistrations.count()))
    {
        qDebug("DeviceUISet::deleteTxChennel: delete channel [%s] at %d",
                qPrintable(m_txChannelInstanceRegistrations[channelIndex].m_channelName),
                channelIndex);
        m_txChannelInstanceRegistrations[channelIndex].m_gui->destroy();
        m_txChannelInstanceRegistrations.removeAt(channelIndex);
        renameTxChannelInstances();
    }
}

void DeviceUISet::loadRxChannelSettings(const Preset *preset, PluginAPI *pluginAPI)
{
    if (preset->isSourcePreset())
    {
        qDebug("DeviceUISet::loadChannelSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // Available channel plugins
        PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getRxChannelRegistrations();

        // copy currently open channels and clear list
        ChannelInstanceRegistrations openChannels = m_rxChannelInstanceRegistrations;
        m_rxChannelInstanceRegistrations.clear();

        qDebug("DeviceUISet::loadChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for(int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelInstanceRegistration reg;

            // if we have one instance available already, use it

            for(int i = 0; i < openChannels.count(); i++)
            {
                qDebug("DeviceUISet::loadChannelSettings: channels compare [%s] vs [%s]", qPrintable(openChannels[i].m_channelName), qPrintable(channelConfig.m_channelIdURI));

                if(openChannels[i].m_channelName == channelConfig.m_channelIdURI)
                {
                    qDebug("DeviceSourceAPI::loadChannelSettings: channel [%s] found", qPrintable(openChannels[i].m_channelName));
                    reg = openChannels.takeAt(i);
                    m_rxChannelInstanceRegistrations.append(reg);
                    break;
                }
            }

            // if we haven't one already, create one

            if(reg.m_gui == NULL)
            {
                for(int i = 0; i < channelRegistrations->count(); i++)
                {
                    if((*channelRegistrations)[i].m_channelIdURI == channelConfig.m_channelIdURI)
                    {
                        qDebug("DeviceUISet::loadChannelSettings: creating new channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                        BasebandSampleSink *rxChannel =
                                (*channelRegistrations)[i].m_plugin->createRxChannelBS(m_deviceSourceAPI);
                        PluginInstanceGUI *rxChannelGUI =
                                (*channelRegistrations)[i].m_plugin->createRxChannelGUI(this, rxChannel);
                        reg = ChannelInstanceRegistration(channelConfig.m_channelIdURI, rxChannelGUI);
                        break;
                    }
                }
            }

            if(reg.m_gui != NULL)
            {
                qDebug("DeviceUISet::loadChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                reg.m_gui->deserialize(channelConfig.m_config);
            }
        }

        // everything, that is still "available" is not needed anymore
        for(int i = 0; i < openChannels.count(); i++)
        {
            qDebug("DeviceUISet::loadChannelSettings: destroying spare channel [%s]", qPrintable(openChannels[i].m_channelName));
            openChannels[i].m_gui->destroy();
        }

        renameRxChannelInstances();
    }
    else
    {
        qDebug("DeviceUISet::loadChannelSettings: Loading preset [%s | %s] not a source preset", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }
}

void DeviceUISet::saveRxChannelSettings(Preset *preset)
{
    if (preset->isSourcePreset())
    {
        qSort(m_rxChannelInstanceRegistrations.begin(), m_rxChannelInstanceRegistrations.end()); // sort by increasing delta frequency and type

        for(int i = 0; i < m_rxChannelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceUISet::saveChannelSettings: channel [%s] saved", qPrintable(m_rxChannelInstanceRegistrations[i].m_channelName));
            preset->addChannel(m_rxChannelInstanceRegistrations[i].m_channelName, m_rxChannelInstanceRegistrations[i].m_gui->serialize());
        }
    }
    else
    {
        qDebug("DeviceUISet::saveChannelSettings: not a source preset");
    }
}

void DeviceUISet::loadTxChannelSettings(const Preset *preset, PluginAPI *pluginAPI)
{
    if (preset->isSourcePreset())
    {
        qDebug("DeviceUISet::loadChannelSettings: Loading preset [%s | %s] not a sink preset", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }
    else
    {
        qDebug("DeviceUISet::loadChannelSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // Available channel plugins
        PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getTxChannelRegistrations();

        // copy currently open channels and clear list
        ChannelInstanceRegistrations openChannels = m_txChannelInstanceRegistrations;
        m_txChannelInstanceRegistrations.clear();

        qDebug("DeviceUISet::loadChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for(int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelInstanceRegistration reg;

            // if we have one instance available already, use it

            for(int i = 0; i < openChannels.count(); i++)
            {
                qDebug("DeviceUISet::loadChannelSettings: channels compare [%s] vs [%s]", qPrintable(openChannels[i].m_channelName), qPrintable(channelConfig.m_channelIdURI));

                if(openChannels[i].m_channelName == channelConfig.m_channelIdURI)
                {
                    qDebug("DeviceUISet::loadChannelSettings: channel [%s] found", qPrintable(openChannels[i].m_channelName));
                    reg = openChannels.takeAt(i);
                    m_txChannelInstanceRegistrations.append(reg);
                    break;
                }
            }

            // if we haven't one already, create one

            if(reg.m_gui == 0)
            {
                for(int i = 0; i < channelRegistrations->count(); i++)
                {
                    if((*channelRegistrations)[i].m_channelIdURI == channelConfig.m_channelIdURI)
                    {
                        qDebug("DeviceUISet::loadChannelSettings: creating new channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                        BasebandSampleSource *txChannel =
                                (*channelRegistrations)[i].m_plugin->createTxChannelBS(m_deviceSinkAPI);
                        PluginInstanceGUI *txChannelGUI =
                                (*channelRegistrations)[i].m_plugin->createTxChannelGUI(this, txChannel);
                        reg = ChannelInstanceRegistration(channelConfig.m_channelIdURI, txChannelGUI);
                        break;
                    }
                }
            }

            if(reg.m_gui != 0)
            {
                qDebug("DeviceUISet::loadChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                reg.m_gui->deserialize(channelConfig.m_config);
            }
        }

        // everything, that is still "available" is not needed anymore
        for(int i = 0; i < openChannels.count(); i++)
        {
            qDebug("DeviceUISet::loadChannelSettings: destroying spare channel [%s]", qPrintable(openChannels[i].m_channelName));
            openChannels[i].m_gui->destroy();
        }

        renameTxChannelInstances();
    }
}

void DeviceUISet::saveTxChannelSettings(Preset *preset)
{
    if (preset->isSourcePreset())
    {
        qDebug("DeviceUISet::saveChannelSettings: not a sink preset");
    }
    else
    {
        qSort(m_txChannelInstanceRegistrations.begin(), m_txChannelInstanceRegistrations.end()); // sort by increasing delta frequency and type

        for(int i = 0; i < m_txChannelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceUISet::saveChannelSettings: channel [%s] saved", qPrintable(m_txChannelInstanceRegistrations[i].m_channelName));
            preset->addChannel(m_txChannelInstanceRegistrations[i].m_channelName, m_txChannelInstanceRegistrations[i].m_gui->serialize());
        }
    }
}

void DeviceUISet::renameRxChannelInstances()
{
    for(int i = 0; i < m_rxChannelInstanceRegistrations.count(); i++)
    {
        m_rxChannelInstanceRegistrations[i].m_gui->setName(QString("%1:%2").arg(m_rxChannelInstanceRegistrations[i].m_channelName).arg(i));
    }
}

void DeviceUISet::renameTxChannelInstances()
{
    for(int i = 0; i < m_txChannelInstanceRegistrations.count(); i++)
    {
        m_txChannelInstanceRegistrations[i].m_gui->setName(QString("%1:%2").arg(m_txChannelInstanceRegistrations[i].m_channelName).arg(i));
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


