///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "plugin/pluginapi.h"
#include "plugin/plugininterface.h"
#include "settings/preset.h"
#include "channel/channelapi.h"
#include "channel/channelutils.h"
#include "settings/preset.h"

#include "deviceset.h"


DeviceSet::ChannelInstanceRegistration::ChannelInstanceRegistration(const QString& channelName, ChannelAPI* channelAPI) :
    m_channelName(channelName),
    m_channelAPI(channelAPI)
{}

DeviceSet::DeviceSet(int tabIndex)
{
    m_deviceAPI = nullptr;
    m_deviceSourceEngine = nullptr;
    m_deviceSinkEngine = nullptr;
    m_deviceMIMOEngine = nullptr;
    m_deviceTabIndex = tabIndex;
}

DeviceSet::~DeviceSet()
{
}

void DeviceSet::registerRxChannelInstance(const QString& channelName, ChannelAPI* channelAPI)
{
    m_channelInstanceRegistrations.append(ChannelInstanceRegistration(channelName, channelAPI));
    renameChannelInstances();
}

void DeviceSet::registerTxChannelInstance(const QString& channelName, ChannelAPI* channelAPI)
{
    m_channelInstanceRegistrations.append(ChannelInstanceRegistration(channelName, channelAPI));
    renameChannelInstances();
}

void DeviceSet::registerChannelInstance(const QString& channelName, ChannelAPI* channelAPI)
{
    m_channelInstanceRegistrations.append(ChannelInstanceRegistration(channelName, channelAPI));
    renameChannelInstances();
}

void DeviceSet::removeRxChannelInstance(ChannelAPI* channelAPI)
{
    for (ChannelInstanceRegistrations::iterator it = m_channelInstanceRegistrations.begin(); it != m_channelInstanceRegistrations.end(); ++it)
    {
        if (it->m_channelAPI == channelAPI)
        {
            m_channelInstanceRegistrations.erase(it);
            break;
        }
    }

    renameChannelInstances();
}

void DeviceSet::removeTxChannelInstance(ChannelAPI* channelAPI)
{
    for(ChannelInstanceRegistrations::iterator it = m_channelInstanceRegistrations.begin(); it != m_channelInstanceRegistrations.end(); ++it)
    {
        if(it->m_channelAPI == channelAPI)
        {
            m_channelInstanceRegistrations.erase(it);
            break;
        }
    }

    renameChannelInstances();
}

void DeviceSet::removeChannelInstance(ChannelAPI* channelAPI)
{
    for(ChannelInstanceRegistrations::iterator it = m_channelInstanceRegistrations.begin(); it != m_channelInstanceRegistrations.end(); ++it)
    {
        if(it->m_channelAPI == channelAPI)
        {
            m_channelInstanceRegistrations.erase(it);
            break;
        }
    }

    renameChannelInstances();
}

void DeviceSet::freeChannels()
{
    for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
    {
        qDebug("DeviceSet::freeChannels: destroying channel [%s]", qPrintable(m_channelInstanceRegistrations[i].m_channelName));
        m_channelInstanceRegistrations[i].m_channelAPI->destroy();
    }
}

void DeviceSet::deleteChannel(int channelIndex)
{
    if (channelIndex < m_channelInstanceRegistrations.count())
    {
        m_channelInstanceRegistrations[channelIndex].m_channelAPI->destroy();
        m_channelInstanceRegistrations.removeAt(channelIndex);
        renameChannelInstances();
    }
}

void DeviceSet::addRxChannel(int selectedChannelIndex, PluginAPI *pluginAPI)
{
    PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getRxChannelRegistrations(); // Available channel plugins
    ChannelAPI *rxChannel =(*channelRegistrations)[selectedChannelIndex].m_plugin->createRxChannelCS(m_deviceAPI);
    ChannelInstanceRegistration reg = ChannelInstanceRegistration(rxChannel->getName(), rxChannel);
    m_channelInstanceRegistrations.append(reg);
    qDebug("DeviceSet::addRxChannel: %s", qPrintable(rxChannel->getName()));
}

void DeviceSet::addTxChannel(int selectedChannelIndex, PluginAPI *pluginAPI)
{
    PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getTxChannelRegistrations(); // Available channel plugins
    ChannelAPI *txChannel = (*channelRegistrations)[selectedChannelIndex].m_plugin->createTxChannelCS(m_deviceAPI);
    ChannelInstanceRegistration reg = ChannelInstanceRegistration(txChannel->getName(), txChannel);
    m_channelInstanceRegistrations.append(reg);
    qDebug("DeviceSet::addTxChannel: %s", qPrintable(txChannel->getName()));
}

void DeviceSet::addMIMOChannel(int selectedChannelIndex, PluginAPI *pluginAPI)
{
    PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getMIMOChannelRegistrations(); // Available channel plugins
    ChannelAPI *mimoChannel = (*channelRegistrations)[selectedChannelIndex].m_plugin->createMIMOChannelCS(m_deviceAPI);
    ChannelInstanceRegistration reg = ChannelInstanceRegistration(mimoChannel->getName(), mimoChannel);
    m_channelInstanceRegistrations.append(reg);
    qDebug("DeviceSet::addMIMOChannel: %s", qPrintable(mimoChannel->getName()));
}

void DeviceSet::loadRxChannelSettings(const Preset *preset, PluginAPI *pluginAPI)
{
    if (preset->isSourcePreset())
    {
        qDebug("DeviceSet::loadChannelSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // Available channel plugins
        PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getRxChannelRegistrations();

        // copy currently open channels and clear list
        ChannelInstanceRegistrations openChannels = m_channelInstanceRegistrations;
        m_channelInstanceRegistrations.clear();

        qDebug("DeviceSet::loadChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for (int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelInstanceRegistration reg;

            // if we have one instance available already, use it

            for (int i = 0; i < openChannels.count(); i++)
            {
                qDebug("DeviceSet::loadChannelSettings: channels compare [%s] vs [%s]", qPrintable(openChannels[i].m_channelName), qPrintable(channelConfig.m_channelIdURI));

                //if(openChannels[i].m_channelName == channelConfig.m_channelIdURI)
                if (ChannelUtils::compareChannelURIs(openChannels[i].m_channelName, channelConfig.m_channelIdURI))
                {
                    qDebug("DeviceSet::loadChannelSettings: channel [%s] found", qPrintable(openChannels[i].m_channelName));
                    reg = openChannels.takeAt(i);
                    m_channelInstanceRegistrations.append(reg);
                    break;
                }
            }

            // if we haven't one already, create one

            if (reg.m_channelAPI == nullptr)
            {
                for (int i = 0; i < channelRegistrations->count(); i++)
                {
                    //if((*channelRegistrations)[i].m_channelIdURI == channelConfig.m_channelIdURI)
                    if (ChannelUtils::compareChannelURIs((*channelRegistrations)[i].m_channelIdURI, channelConfig.m_channelIdURI))
                    {
                        qDebug("DeviceSet::loadChannelSettings: creating new channel [%s] from config [%s]",
                                qPrintable((*channelRegistrations)[i].m_channelIdURI),
                                qPrintable(channelConfig.m_channelIdURI));
                        ChannelAPI *rxChannel = (*channelRegistrations)[i].m_plugin->createRxChannelCS(m_deviceAPI);
                        reg = ChannelInstanceRegistration(channelConfig.m_channelIdURI, rxChannel);
                        m_channelInstanceRegistrations.append(reg);
                        break;
                    }
                }
            }

            if (reg.m_channelAPI != nullptr)
            {
                qDebug("DeviceSet::loadChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                reg.m_channelAPI->deserialize(channelConfig.m_config);
            }
        }

        // everything, that is still "available" is not needed anymore
        for (int i = 0; i < openChannels.count(); i++)
        {
            qDebug("DeviceSet::loadChannelSettings: destroying spare channel [%s]", qPrintable(openChannels[i].m_channelName));
            openChannels[i].m_channelAPI->destroy();
        }

        renameChannelInstances();
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
        qSort(m_channelInstanceRegistrations.begin(), m_channelInstanceRegistrations.end()); // sort by increasing delta frequency and type

        for (int i = 0; i < m_channelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceSet::saveChannelSettings: channel [%s] saved", qPrintable(m_channelInstanceRegistrations[i].m_channelName));
            preset->addChannel(m_channelInstanceRegistrations[i].m_channelName, m_channelInstanceRegistrations[i].m_channelAPI->serialize());
        }
    }
    else
    {
        qDebug("DeviceSet::saveChannelSettings: not a source preset");
    }
}

void DeviceSet::loadTxChannelSettings(const Preset *preset, PluginAPI *pluginAPI)
{
    if (preset->isSinkPreset())
    {
        qDebug("DeviceSet::loadTxChannelSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // Available channel plugins
        PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getTxChannelRegistrations();

        // copy currently open channels and clear list
        ChannelInstanceRegistrations openChannels = m_channelInstanceRegistrations;
        m_channelInstanceRegistrations.clear();

        qDebug("DeviceSet::loadTxChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for (int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelInstanceRegistration reg;

            // if we have one instance available already, use it

            for (int i = 0; i < openChannels.count(); i++)
            {
                qDebug("DeviceSet::loadTxChannelSettings: channels compare [%s] vs [%s]", qPrintable(openChannels[i].m_channelName), qPrintable(channelConfig.m_channelIdURI));

                if (openChannels[i].m_channelName == channelConfig.m_channelIdURI)
                {
                    qDebug("DeviceSet::loadTxChannelSettings: channel [%s] found", qPrintable(openChannels[i].m_channelName));
                    reg = openChannels.takeAt(i);
                    m_channelInstanceRegistrations.append(reg);
                    break;
                }
            }

            // if we haven't one already, create one

            if (reg.m_channelAPI == nullptr)
            {
                for (int i = 0; i < channelRegistrations->count(); i++)
                {
                    if ((*channelRegistrations)[i].m_channelIdURI == channelConfig.m_channelIdURI)
                    {
                        qDebug("DeviceSet::loadTxChannelSettings: creating new channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                        ChannelAPI *txChannel = (*channelRegistrations)[i].m_plugin->createTxChannelCS(m_deviceAPI);
                        reg = ChannelInstanceRegistration(channelConfig.m_channelIdURI, txChannel);
                        m_channelInstanceRegistrations.append(reg);
                        break;
                    }
                }
            }

            if (reg.m_channelAPI != nullptr)
            {
                qDebug("DeviceSet::loadTxChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                reg.m_channelAPI->deserialize(channelConfig.m_config);
            }
        }

        // everything, that is still "available" is not needed anymore
        for (int i = 0; i < openChannels.count(); i++)
        {
            qDebug("DeviceSet::loadTxChannelSettings: destroying spare channel [%s]", qPrintable(openChannels[i].m_channelName));
            openChannels[i].m_channelAPI->destroy();
        }

        renameChannelInstances();
    }
    else
    {
        qDebug("DeviceSet::loadTxChannelSettings: Loading preset [%s | %s] not a sink preset", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }

}

void DeviceSet::saveTxChannelSettings(Preset *preset)
{
    if (preset->isSinkPreset())
    {
        qSort(m_channelInstanceRegistrations.begin(), m_channelInstanceRegistrations.end()); // sort by increasing delta frequency and type

        for (int i = 0; i < m_channelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceSet::saveTxChannelSettings: channel [%s] saved", qPrintable(m_channelInstanceRegistrations[i].m_channelName));
            preset->addChannel(m_channelInstanceRegistrations[i].m_channelName, m_channelInstanceRegistrations[i].m_channelAPI->serialize());
        }
    }
    else
    {
        qDebug("DeviceSet::saveTxChannelSettings: not a sink preset");
    }
}

void DeviceSet::loadMIMOChannelSettings(const Preset *preset, PluginAPI *pluginAPI)
{
    if (preset->isMIMOPreset())
    {
        qDebug("DeviceSet::loadMIMOChannelSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // Available channel plugins
        PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getMIMOChannelRegistrations();

        // copy currently open channels and clear list
        ChannelInstanceRegistrations openChannels = m_channelInstanceRegistrations;
        m_channelInstanceRegistrations.clear();

        qDebug("DeviceSet::loadMIMOChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for (int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelInstanceRegistration reg;

            // if we have one instance available already, use it

            for (int i = 0; i < openChannels.count(); i++)
            {
                qDebug("DeviceSet::loadMIMOChannelSettings: channels compare [%s] vs [%s]", qPrintable(openChannels[i].m_channelName), qPrintable(channelConfig.m_channelIdURI));

                //if(openChannels[i].m_channelName == channelConfig.m_channelIdURI)
                if (ChannelUtils::compareChannelURIs(openChannels[i].m_channelName, channelConfig.m_channelIdURI))
                {
                    qDebug("DeviceSet::loadMIMOChannelSettings: channel [%s] found", qPrintable(openChannels[i].m_channelName));
                    reg = openChannels.takeAt(i);
                    m_channelInstanceRegistrations.append(reg);
                    break;
                }
            }

            // if we haven't one already, create one

            if (reg.m_channelAPI == nullptr)
            {
                for (int i = 0; i < channelRegistrations->count(); i++)
                {
                    //if((*channelRegistrations)[i].m_channelIdURI == channelConfig.m_channelIdURI)
                    if (ChannelUtils::compareChannelURIs((*channelRegistrations)[i].m_channelIdURI, channelConfig.m_channelIdURI))
                    {
                        qDebug("DeviceSet::loadMIMOChannelSettings: creating new channel [%s] from config [%s]",
                                qPrintable((*channelRegistrations)[i].m_channelIdURI),
                                qPrintable(channelConfig.m_channelIdURI));
                        ChannelAPI *mimoChannel = (*channelRegistrations)[i].m_plugin->createMIMOChannelCS(m_deviceAPI);
                        reg = ChannelInstanceRegistration(channelConfig.m_channelIdURI, mimoChannel);
                        m_channelInstanceRegistrations.append(reg);
                        break;
                    }
                }
            }

            if (reg.m_channelAPI != nullptr)
            {
                qDebug("DeviceSet::loadMIMOChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                reg.m_channelAPI->deserialize(channelConfig.m_config);
            }
        }

        // everything, that is still "available" is not needed anymore
        for (int i = 0; i < openChannels.count(); i++)
        {
            qDebug("DeviceSet::loadMIMOChannelSettings: destroying spare channel [%s]", qPrintable(openChannels[i].m_channelName));
            openChannels[i].m_channelAPI->destroy();
        }

        renameChannelInstances();
    }
    else
    {
        qDebug("DeviceSet::loadChannelSettings: Loading preset [%s | %s] not a MIMO preset", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }
}

void DeviceSet::saveMIMOChannelSettings(Preset *preset)
{
    if (preset->isMIMOPreset())
    {
        qSort(m_channelInstanceRegistrations.begin(), m_channelInstanceRegistrations.end()); // sort by increasing delta frequency and type

        for (int i = 0; i < m_channelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceSet::saveMIMOChannelSettings: channel [%s] saved", qPrintable(m_channelInstanceRegistrations[i].m_channelName));
            preset->addChannel(m_channelInstanceRegistrations[i].m_channelName, m_channelInstanceRegistrations[i].m_channelAPI->serialize());
        }
    }
    else
    {
        qDebug("DeviceSet::saveMIMOChannelSettings: not a MIMO preset");
    }
}

void DeviceSet::renameChannelInstances()
{
    for (int i = 0; i < m_channelInstanceRegistrations.count(); i++)
    {
        m_channelInstanceRegistrations[i].m_channelAPI->setName(QString("%1:%2").arg(m_channelInstanceRegistrations[i].m_channelName).arg(i));
    }
}

// sort by increasing delta frequency and type (i.e. name)
bool DeviceSet::ChannelInstanceRegistration::operator<(const ChannelInstanceRegistration& other) const
{
    if (m_channelAPI && other.m_channelAPI)
    {
        if (m_channelAPI->getCenterFrequency() == other.m_channelAPI->getCenterFrequency())
        {
            return m_channelAPI->getName() < other.m_channelAPI->getName();
        }
        else
        {
            return m_channelAPI->getCenterFrequency() < other.m_channelAPI->getCenterFrequency();
        }
    }
    else
    {
        return false;
    }
}

