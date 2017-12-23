///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "plugin/pluginapi.h"
#include "plugin/plugininterface.h"
#include "settings/preset.h"
#include "channel/channelsinkapi.h"
#include "channel/channelsourceapi.h"
#include "settings/preset.h"

#include "deviceset.h"


DeviceSet::DeviceSet(int tabIndex)
{
    m_deviceSourceEngine = 0;
    m_deviceSourceAPI = 0;
    m_deviceSinkEngine = 0;
    m_deviceSinkAPI = 0;
    m_deviceTabIndex = tabIndex;
}

DeviceSet::~DeviceSet()
{
}

void DeviceSet::registerRxChannelInstance(const QString& channelName, ChannelSinkAPI* channelAPI)
{
    m_rxChannelInstanceRegistrations.append(ChannelInstanceRegistration(channelName, channelAPI));
    renameRxChannelInstances();
}

void DeviceSet::registerTxChannelInstance(const QString& channelName, ChannelSourceAPI* channelAPI)
{
    m_txChannelInstanceRegistrations.append(ChannelInstanceRegistration(channelName, channelAPI));
    renameTxChannelInstances();
}

void DeviceSet::removeRxChannelInstance(ChannelSinkAPI* channelAPI)
{
    for(ChannelInstanceRegistrations::iterator it = m_rxChannelInstanceRegistrations.begin(); it != m_rxChannelInstanceRegistrations.end(); ++it)
    {
        if(it->m_channelSinkAPI == channelAPI)
        {
            m_rxChannelInstanceRegistrations.erase(it);
            break;
        }
    }

    renameRxChannelInstances();
}

void DeviceSet::removeTxChannelInstance(ChannelSourceAPI* channelAPI)
{
    for(ChannelInstanceRegistrations::iterator it = m_txChannelInstanceRegistrations.begin(); it != m_txChannelInstanceRegistrations.end(); ++it)
    {
        if(it->m_channelSourceAPI == channelAPI)
        {
            m_txChannelInstanceRegistrations.erase(it);
            break;
        }
    }

    renameTxChannelInstances();
}

void DeviceSet::freeRxChannels()
{
    for(int i = 0; i < m_rxChannelInstanceRegistrations.count(); i++)
    {
        qDebug("DeviceSet::freeAll: destroying channel [%s]", qPrintable(m_rxChannelInstanceRegistrations[i].m_channelName));
        m_rxChannelInstanceRegistrations[i].m_channelSinkAPI->destroy();
    }
}

void DeviceSet::freeTxChannels()
{
    for(int i = 0; i < m_txChannelInstanceRegistrations.count(); i++)
    {
        qDebug("DeviceSet::freeAll: destroying channel [%s]", qPrintable(m_txChannelInstanceRegistrations[i].m_channelName));
        m_txChannelInstanceRegistrations[i].m_channelSourceAPI->destroy();
    }
}

void DeviceSet::deleteRxChannel(int channelIndex)
{
    if (channelIndex < m_rxChannelInstanceRegistrations.count())
    {
        m_rxChannelInstanceRegistrations[channelIndex].m_channelSinkAPI->destroy();
        m_rxChannelInstanceRegistrations.removeAt(channelIndex);
        renameRxChannelInstances();
    }
}

void DeviceSet::deleteTxChannel(int channelIndex)
{
    if (channelIndex < m_txChannelInstanceRegistrations.count())
    {
        m_txChannelInstanceRegistrations[channelIndex].m_channelSourceAPI->destroy();
        m_txChannelInstanceRegistrations.removeAt(channelIndex);
        renameTxChannelInstances();
    }
}

void DeviceSet::addRxChannel(int selectedChannelIndex, PluginAPI *pluginAPI)
{
    PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getRxChannelRegistrations(); // Available channel plugins
    ChannelSinkAPI *rxChannel =(*channelRegistrations)[selectedChannelIndex].m_plugin->createRxChannelCS(m_deviceSourceAPI);
    ChannelInstanceRegistration reg = ChannelInstanceRegistration(rxChannel->getName(), rxChannel);
    m_rxChannelInstanceRegistrations.append(reg);
    qDebug("DeviceSet::addRxChannel: %s", qPrintable(rxChannel->getName()));
}

void DeviceSet::addTxChannel(int selectedChannelIndex, PluginAPI *pluginAPI)
{
    PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getTxChannelRegistrations(); // Available channel plugins
    ChannelSourceAPI *txChannel = (*channelRegistrations)[selectedChannelIndex].m_plugin->createTxChannelCS(m_deviceSinkAPI);
    ChannelInstanceRegistration reg = ChannelInstanceRegistration(txChannel->getName(), txChannel);
    m_txChannelInstanceRegistrations.append(reg);
    qDebug("DeviceSet::addTxChannel: %s", qPrintable(txChannel->getName()));
}

void DeviceSet::loadRxChannelSettings(const Preset *preset, PluginAPI *pluginAPI)
{
    if (preset->isSourcePreset())
    {
        qDebug("DeviceSet::loadChannelSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // Available channel plugins
        PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getRxChannelRegistrations();

        // copy currently open channels and clear list
        ChannelInstanceRegistrations openChannels = m_rxChannelInstanceRegistrations;
        m_rxChannelInstanceRegistrations.clear();

        qDebug("DeviceSet::loadChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for(int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelInstanceRegistration reg;

            // if we have one instance available already, use it

            for(int i = 0; i < openChannels.count(); i++)
            {
                qDebug("DeviceSet::loadChannelSettings: channels compare [%s] vs [%s]", qPrintable(openChannels[i].m_channelName), qPrintable(channelConfig.m_channelIdURI));

                if(openChannels[i].m_channelName == channelConfig.m_channelIdURI)
                {
                    qDebug("DeviceSet::loadChannelSettings: channel [%s] found", qPrintable(openChannels[i].m_channelName));
                    reg = openChannels.takeAt(i);
                    m_rxChannelInstanceRegistrations.append(reg);
                    break;
                }
            }

            // if we haven't one already, create one

            if(reg.m_channelSinkAPI == 0)
            {
                for(int i = 0; i < channelRegistrations->count(); i++)
                {
                    if((*channelRegistrations)[i].m_channelIdURI == channelConfig.m_channelIdURI)
                    {
                        qDebug("DeviceSet::loadChannelSettings: creating new channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                        ChannelSinkAPI *rxChannel = (*channelRegistrations)[i].m_plugin->createRxChannelCS(m_deviceSourceAPI);
                        reg = ChannelInstanceRegistration(channelConfig.m_channelIdURI, rxChannel);
                        m_rxChannelInstanceRegistrations.append(reg);
                        break;
                    }
                }
            }

            if(reg.m_channelSinkAPI != 0)
            {
                qDebug("DeviceSet::loadChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                reg.m_channelSinkAPI->deserialize(channelConfig.m_config);
            }
        }

        // everything, that is still "available" is not needed anymore
        for(int i = 0; i < openChannels.count(); i++)
        {
            qDebug("DeviceSet::loadChannelSettings: destroying spare channel [%s]", qPrintable(openChannels[i].m_channelName));
            openChannels[i].m_channelSinkAPI->destroy();
        }

        renameRxChannelInstances();
    }
    else
    {
        qDebug("DeviceSet::loadChannelSettings: Loading preset [%s | %s] not a source preset", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }
}

void DeviceSet::saveRxChannelSettings(Preset *preset)
{
    if (preset->isSourcePreset())
    {
        qSort(m_rxChannelInstanceRegistrations.begin(), m_rxChannelInstanceRegistrations.end()); // sort by increasing delta frequency and type

        for(int i = 0; i < m_rxChannelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceSet::saveChannelSettings: channel [%s] saved", qPrintable(m_rxChannelInstanceRegistrations[i].m_channelName));
            preset->addChannel(m_rxChannelInstanceRegistrations[i].m_channelName, m_rxChannelInstanceRegistrations[i].m_channelSinkAPI->serialize());
        }
    }
    else
    {
        qDebug("DeviceSet::saveChannelSettings: not a source preset");
    }
}

void DeviceSet::loadTxChannelSettings(const Preset *preset, PluginAPI *pluginAPI)
{
    if (preset->isSourcePreset())
    {
        qDebug("DeviceSet::loadChannelSettings: Loading preset [%s | %s] not a sink preset", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }
    else
    {
        qDebug("DeviceSet::loadChannelSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // Available channel plugins
        PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getTxChannelRegistrations();

        // copy currently open channels and clear list
        ChannelInstanceRegistrations openChannels = m_txChannelInstanceRegistrations;
        m_txChannelInstanceRegistrations.clear();

        qDebug("DeviceSet::loadChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for(int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelInstanceRegistration reg;

            // if we have one instance available already, use it

            for(int i = 0; i < openChannels.count(); i++)
            {
                qDebug("DeviceSet::loadChannelSettings: channels compare [%s] vs [%s]", qPrintable(openChannels[i].m_channelName), qPrintable(channelConfig.m_channelIdURI));

                if(openChannels[i].m_channelName == channelConfig.m_channelIdURI)
                {
                    qDebug("DeviceSet::loadChannelSettings: channel [%s] found", qPrintable(openChannels[i].m_channelName));
                    reg = openChannels.takeAt(i);
                    m_txChannelInstanceRegistrations.append(reg);
                    break;
                }
            }

            // if we haven't one already, create one

            if(reg.m_channelSourceAPI == 0)
            {
                for(int i = 0; i < channelRegistrations->count(); i++)
                {
                    if((*channelRegistrations)[i].m_channelIdURI == channelConfig.m_channelIdURI)
                    {
                        qDebug("DeviceSet::loadChannelSettings: creating new channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                        ChannelSourceAPI *txChannel = (*channelRegistrations)[i].m_plugin->createTxChannelCS(m_deviceSinkAPI);
                        reg = ChannelInstanceRegistration(channelConfig.m_channelIdURI, txChannel);
                        m_txChannelInstanceRegistrations.append(reg);
                        break;
                    }
                }
            }

            if(reg.m_channelSourceAPI != 0)
            {
                qDebug("DeviceSet::loadChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                reg.m_channelSourceAPI->deserialize(channelConfig.m_config);
            }
        }

        // everything, that is still "available" is not needed anymore
        for(int i = 0; i < openChannels.count(); i++)
        {
            qDebug("DeviceUISet::loadChannelSettings: destroying spare channel [%s]", qPrintable(openChannels[i].m_channelName));
            openChannels[i].m_channelSourceAPI->destroy();
        }

        renameTxChannelInstances();
    }
}

void DeviceSet::saveTxChannelSettings(Preset *preset)
{
    if (preset->isSourcePreset())
    {
        qDebug("DeviceSet::saveChannelSettings: not a sink preset");
    }
    else
    {
        qSort(m_txChannelInstanceRegistrations.begin(), m_txChannelInstanceRegistrations.end()); // sort by increasing delta frequency and type

        for(int i = 0; i < m_txChannelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceSet::saveChannelSettings: channel [%s] saved", qPrintable(m_txChannelInstanceRegistrations[i].m_channelName));
            preset->addChannel(m_txChannelInstanceRegistrations[i].m_channelName, m_txChannelInstanceRegistrations[i].m_channelSourceAPI->serialize());
        }
    }
}

void DeviceSet::renameRxChannelInstances()
{
    for(int i = 0; i < m_rxChannelInstanceRegistrations.count(); i++)
    {
        m_rxChannelInstanceRegistrations[i].m_channelSinkAPI->setName(QString("%1:%2").arg(m_rxChannelInstanceRegistrations[i].m_channelName).arg(i));
    }
}

void DeviceSet::renameTxChannelInstances()
{
    for(int i = 0; i < m_txChannelInstanceRegistrations.count(); i++)
    {
        m_txChannelInstanceRegistrations[i].m_channelSourceAPI->setName(QString("%1:%2").arg(m_txChannelInstanceRegistrations[i].m_channelName).arg(i));
    }
}

// sort by increasing delta frequency and type (i.e. name)
bool DeviceSet::ChannelInstanceRegistration::operator<(const ChannelInstanceRegistration& other) const
{
    if (m_channelSinkAPI && other.m_channelSinkAPI)
    {
        if (m_channelSinkAPI->getCenterFrequency() == other.m_channelSinkAPI->getCenterFrequency())
        {
            return m_channelSinkAPI->getName() < other.m_channelSinkAPI->getName();
        }
        else
        {
            return m_channelSinkAPI->getCenterFrequency() < other.m_channelSinkAPI->getCenterFrequency();
        }
    }
    else if (m_channelSourceAPI && other.m_channelSourceAPI)
    {
        if (m_channelSourceAPI->getCenterFrequency() == other.m_channelSourceAPI->getCenterFrequency())
        {
            return m_channelSourceAPI->getName() < other.m_channelSourceAPI->getName();
        }
        else
        {
            return m_channelSourceAPI->getCenterFrequency() < other.m_channelSourceAPI->getCenterFrequency();
        }
    }
    else
    {
        return false;
    }
}

