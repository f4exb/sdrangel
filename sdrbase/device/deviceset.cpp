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

#include <algorithm>

#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "dsp/spectrumvis.h"
#include "plugin/pluginapi.h"
#include "plugin/plugininterface.h"
#include "settings/preset.h"
#include "channel/channelapi.h"
#include "channel/channelutils.h"
#include "settings/preset.h"
#include "maincore.h"

#include "deviceset.h"


DeviceSet::DeviceSet(int tabIndex, int deviceType)
{
    m_deviceAPI = nullptr;
    m_deviceSourceEngine = nullptr;
    m_deviceSinkEngine = nullptr;
    m_deviceMIMOEngine = nullptr;
    m_deviceTabIndex = tabIndex;

    if ((deviceType == 0) || (deviceType == 2)) { // Single Rx or MIMO
        m_spectrumVis = new SpectrumVis(SDR_RX_SCALEF);
    } else if (deviceType == 1) { // Single Tx
        m_spectrumVis = new SpectrumVis(SDR_TX_SCALEF);
    }
}

DeviceSet::~DeviceSet()
{
    delete m_spectrumVis;
}


void DeviceSet::freeChannels()
{
    for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
    {
        qDebug("DeviceSet::freeChannels: destroying channel [%s]", qPrintable(m_channelInstanceRegistrations[i]->getURI()));
        m_channelInstanceRegistrations[i]->destroy();
    }

    MainCore::instance()->clearChannels(this);
}

const ChannelAPI *DeviceSet::getChannelAt(int channelIndex) const
{
    if ((channelIndex >= 0) && (channelIndex < m_channelInstanceRegistrations.size())) {
        return m_channelInstanceRegistrations[channelIndex];
    } else {
        return nullptr;
    }
}

ChannelAPI *DeviceSet::getChannelAt(int channelIndex)
{
    if ((channelIndex >= 0) && (channelIndex < m_channelInstanceRegistrations.size())) {
        return m_channelInstanceRegistrations[channelIndex];
    } else {
        return nullptr;
    }
}

void DeviceSet::deleteChannel(int channelIndex)
{
    if (channelIndex < m_channelInstanceRegistrations.count())
    {
        m_channelInstanceRegistrations[channelIndex]->destroy();
        m_channelInstanceRegistrations.removeAt(channelIndex);
        MainCore::instance()->removeChannelInstanceAt(this, channelIndex);
        renameChannelInstances();
    }
}

ChannelAPI *DeviceSet::addRxChannel(int selectedChannelIndex, PluginAPI *pluginAPI)
{
    PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getRxChannelRegistrations(); // Available channel plugins
    ChannelAPI *rxChannel;
    (*channelRegistrations)[selectedChannelIndex].m_plugin->createRxChannel(m_deviceAPI, nullptr, &rxChannel);
    m_channelInstanceRegistrations.append(rxChannel);
    MainCore::instance()->addChannelInstance(this, rxChannel);
    renameChannelInstances();
    qDebug("DeviceSet::addRxChannel: %s", qPrintable(rxChannel->getName()));
    return rxChannel;
}

ChannelAPI *DeviceSet::addTxChannel(int selectedChannelIndex, PluginAPI *pluginAPI)
{
    PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getTxChannelRegistrations(); // Available channel plugins
    ChannelAPI *txChannel;
    (*channelRegistrations)[selectedChannelIndex].m_plugin->createTxChannel(m_deviceAPI, nullptr, &txChannel);
    m_channelInstanceRegistrations.append(txChannel);
    MainCore::instance()->addChannelInstance(this, txChannel);
    renameChannelInstances();
    qDebug("DeviceSet::addTxChannel: %s", qPrintable(txChannel->getName()));
    return txChannel;
}

ChannelAPI *DeviceSet::addMIMOChannel(int selectedChannelIndex, PluginAPI *pluginAPI)
{
    PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getMIMOChannelRegistrations(); // Available channel plugins
    ChannelAPI *mimoChannel;
    (*channelRegistrations)[selectedChannelIndex].m_plugin->createMIMOChannel(m_deviceAPI, nullptr, &mimoChannel);
    m_channelInstanceRegistrations.append(mimoChannel);
    MainCore::instance()->addChannelInstance(this, mimoChannel);
    renameChannelInstances();
    qDebug("DeviceSet::addMIMOChannel: %s", qPrintable(mimoChannel->getName()));
    return mimoChannel;
}

void DeviceSet::loadRxChannelSettings(const Preset *preset, PluginAPI *pluginAPI)
{
    if (preset->isSourcePreset())
    {
        MainCore *mainCore = MainCore::instance();
        qDebug("DeviceSet::loadChannelSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // Available channel plugins
        PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getRxChannelRegistrations();

        // copy currently open channels and clear list
        ChannelInstanceRegistrations openChannels = m_channelInstanceRegistrations;
        m_channelInstanceRegistrations.clear();
        mainCore->clearChannels(this);

        qDebug("DeviceSet::loadChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for (int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelAPI *channelAPI = nullptr;

            // if we have one instance available already, use it

            for (int i = 0; i < openChannels.count(); i++)
            {
                qDebug("DeviceSet::loadChannelSettings: channels compare [%s] vs [%s]",
                    qPrintable(openChannels[i]->getURI()), qPrintable(channelConfig.m_channelIdURI));

                //if(openChannels[i].m_channelName == channelConfig.m_channelIdURI)
                if (ChannelUtils::compareChannelURIs(openChannels[i]->getURI(), channelConfig.m_channelIdURI))
                {
                    qDebug("DeviceSet::loadChannelSettings: channel [%s] found", qPrintable(openChannels[i]->getURI()));
                    channelAPI = openChannels.takeAt(i);
                    m_channelInstanceRegistrations.append(channelAPI);
                    mainCore->addChannelInstance(this, channelAPI);
                    break;
                }
            }

            // if we haven't one already, create one

            if (!channelAPI)
            {
                for (int i = 0; i < channelRegistrations->count(); i++)
                {
                    //if((*channelRegistrations)[i].m_channelIdURI == channelConfig.m_channelIdURI)
                    if (ChannelUtils::compareChannelURIs((*channelRegistrations)[i].m_channelIdURI, channelConfig.m_channelIdURI))
                    {
                        qDebug("DeviceSet::loadChannelSettings: creating new channel [%s] from config [%s]",
                                qPrintable((*channelRegistrations)[i].m_channelIdURI),
                                qPrintable(channelConfig.m_channelIdURI));
                        ChannelAPI *rxChannel;
                        (*channelRegistrations)[i].m_plugin->createRxChannel(m_deviceAPI, nullptr, &rxChannel);
                        channelAPI = rxChannel;
                        m_channelInstanceRegistrations.append(channelAPI);
                        mainCore->addChannelInstance(this, channelAPI);
                        break;
                    }
                }
            }

            if (channelAPI)
            {
                qDebug("DeviceSet::loadChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                channelAPI->deserialize(channelConfig.m_config);
            }
        }

        // everything, that is still "available" is not needed anymore
        for (int i = 0; i < openChannels.count(); i++)
        {
            qDebug("DeviceSet::loadChannelSettings: destroying spare channel [%s]", qPrintable(openChannels[i]->getURI()));
            openChannels[i]->destroy();
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
        for (int i = 0; i < m_channelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceSet::saveChannelSettings: channel [%s] saved", qPrintable(m_channelInstanceRegistrations[i]->getURI()));
            preset->addChannel(m_channelInstanceRegistrations[i]->getURI(), m_channelInstanceRegistrations[i]->serialize());
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
        MainCore *mainCore = MainCore::instance();
        qDebug("DeviceSet::loadTxChannelSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // Available channel plugins
        PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getTxChannelRegistrations();

        // copy currently open channels and clear list
        ChannelInstanceRegistrations openChannels = m_channelInstanceRegistrations;
        m_channelInstanceRegistrations.clear();
        mainCore->clearChannels(this);

        qDebug("DeviceSet::loadTxChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for (int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelAPI *channelAPI = nullptr;

            // if we have one instance available already, use it

            for (int i = 0; i < openChannels.count(); i++)
            {
                qDebug("DeviceSet::loadTxChannelSettings: channels compare [%s] vs [%s]",
                    qPrintable(openChannels[i]->getURI()), qPrintable(channelConfig.m_channelIdURI));

                if (openChannels[i]->getURI() == channelConfig.m_channelIdURI)
                {
                    qDebug("DeviceSet::loadTxChannelSettings: channel [%s] found", qPrintable(openChannels[i]->getURI()));
                    channelAPI = openChannels.takeAt(i);
                    m_channelInstanceRegistrations.append(channelAPI);
                    mainCore->addChannelInstance(this, channelAPI);
                    break;
                }
            }

            // if we haven't one already, create one

            if (!channelAPI)
            {
                for (int i = 0; i < channelRegistrations->count(); i++)
                {
                    if ((*channelRegistrations)[i].m_channelIdURI == channelConfig.m_channelIdURI)
                    {
                        qDebug("DeviceSet::loadTxChannelSettings: creating new channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                        ChannelAPI *txChannel;
                        (*channelRegistrations)[i].m_plugin->createTxChannel(m_deviceAPI, nullptr, &txChannel);
                        channelAPI = txChannel;
                        m_channelInstanceRegistrations.append(channelAPI);
                        mainCore->addChannelInstance(this, channelAPI);
                        break;
                    }
                }
            }

            if (channelAPI)
            {
                qDebug("DeviceSet::loadTxChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                channelAPI->deserialize(channelConfig.m_config);
            }
        }

        // everything, that is still "available" is not needed anymore
        for (int i = 0; i < openChannels.count(); i++)
        {
            qDebug("DeviceSet::loadTxChannelSettings: destroying spare channel [%s]", qPrintable(openChannels[i]->getURI()));
            openChannels[i]->destroy();
        }

        renameChannelInstances();
    }
    else
    {
        qDebug("DeviceSet::loadTxChannelSettings: Loading preset [%s | %s] not a sink preset",
            qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }

}

void DeviceSet::saveTxChannelSettings(Preset *preset)
{
    if (preset->isSinkPreset())
    {
        for (int i = 0; i < m_channelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceSet::saveTxChannelSettings: channel [%s] saved", qPrintable(m_channelInstanceRegistrations[i]->getURI()));
            preset->addChannel(m_channelInstanceRegistrations[i]->getURI(), m_channelInstanceRegistrations[i]->serialize());
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
        MainCore *mainCore = MainCore::instance();
        qDebug("DeviceSet::loadMIMOChannelSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // Available channel plugins
        PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getMIMOChannelRegistrations();

        // copy currently open channels and clear list
        ChannelInstanceRegistrations openChannels = m_channelInstanceRegistrations;
        m_channelInstanceRegistrations.clear();
        mainCore->clearChannels(this);

        qDebug("DeviceSet::loadMIMOChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for (int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelAPI *channelAPI = nullptr;

            // if we have one instance available already, use it

            for (int i = 0; i < openChannels.count(); i++)
            {
                qDebug("DeviceSet::loadMIMOChannelSettings: channels compare [%s] vs [%s]",
                    qPrintable(openChannels[i]->getURI()), qPrintable(channelConfig.m_channelIdURI));

                //if(openChannels[i].m_channelName == channelConfig.m_channelIdURI)
                if (ChannelUtils::compareChannelURIs(openChannels[i]->getURI(), channelConfig.m_channelIdURI))
                {
                    qDebug("DeviceSet::loadMIMOChannelSettings: channel [%s] found", qPrintable(openChannels[i]->getURI()));
                    channelAPI = openChannels.takeAt(i);
                    m_channelInstanceRegistrations.append(channelAPI);
                    mainCore->addChannelInstance(this, channelAPI);
                    break;
                }
            }

            // if we haven't one already, create one

            if (!channelAPI)
            {
                for (int i = 0; i < channelRegistrations->count(); i++)
                {
                    //if((*channelRegistrations)[i].m_channelIdURI == channelConfig.m_channelIdURI)
                    if (ChannelUtils::compareChannelURIs((*channelRegistrations)[i].m_channelIdURI, channelConfig.m_channelIdURI))
                    {
                        qDebug("DeviceSet::loadMIMOChannelSettings: creating new channel [%s] from config [%s]",
                                qPrintable((*channelRegistrations)[i].m_channelIdURI),
                                qPrintable(channelConfig.m_channelIdURI));
                        ChannelAPI *mimoChannel;
                        (*channelRegistrations)[i].m_plugin->createMIMOChannel(m_deviceAPI, nullptr, &mimoChannel);
                        channelAPI = mimoChannel;
                        m_channelInstanceRegistrations.append(channelAPI);
                        mainCore->addChannelInstance(this, channelAPI);
                        break;
                    }
                }
            }

            if (channelAPI)
            {
                qDebug("DeviceSet::loadMIMOChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                channelAPI->deserialize(channelConfig.m_config);
            }
        }

        // everything, that is still "available" is not needed anymore
        for (int i = 0; i < openChannels.count(); i++)
        {
            qDebug("DeviceSet::loadMIMOChannelSettings: destroying spare channel [%s]", qPrintable(openChannels[i]->getURI()));
            openChannels[i]->destroy();
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
        for (int i = 0; i < m_channelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceSet::saveMIMOChannelSettings: channel [%s] saved", qPrintable(m_channelInstanceRegistrations[i]->getURI()));
            preset->addChannel(m_channelInstanceRegistrations[i]->getURI(), m_channelInstanceRegistrations[i]->serialize());
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
        m_channelInstanceRegistrations[i]->setName(QString("%1:%2").arg(m_channelInstanceRegistrations[i]->getURI()).arg(i));
        m_channelInstanceRegistrations[i]->setIndexInDeviceSet(i);
    }
}

// sort by increasing delta frequency and type (i.e. name)
bool DeviceSet::compareChannels(const ChannelAPI *channelA, const ChannelAPI *channelB)
{
    if (channelA && channelB)
    {
        if (channelA->getCenterFrequency() == channelB->getCenterFrequency()) {
            return channelA->getName() < channelB->getName();
        } else {
            return channelA->getCenterFrequency() < channelB->getCenterFrequency();
        }
    }
    else
    {
        return false;
    }
}

int DeviceSet::webapiSpectrumSettingsGet(SWGSDRangel::SWGGLSpectrum& response, QString& errorMessage) const
{
    return m_spectrumVis->webapiSpectrumSettingsGet(response, errorMessage);
}

int DeviceSet::webapiSpectrumSettingsPutPatch(
    bool force,
    const QStringList& spectrumSettingsKeys,
    SWGSDRangel::SWGGLSpectrum& response, // query + response
    QString& errorMessage)
{
    return m_spectrumVis->webapiSpectrumSettingsPutPatch(force, spectrumSettingsKeys, response, errorMessage);
}

int DeviceSet::webapiSpectrumServerGet(SWGSDRangel::SWGSpectrumServer& response, QString& errorMessage) const
{
    return m_spectrumVis->webapiSpectrumServerGet(response, errorMessage);
}

int DeviceSet::webapiSpectrumServerPost(SWGSDRangel::SWGSuccessResponse& response, QString& errorMessage)
{
    return m_spectrumVis->webapiSpectrumServerPost(response, errorMessage);
}

int DeviceSet::webapiSpectrumServerDelete(SWGSDRangel::SWGSuccessResponse& response, QString& errorMessage)
{
    return m_spectrumVis->webapiSpectrumServerDelete(response, errorMessage);
}

void DeviceSet::addChannelInstance(ChannelAPI *channelAPI)
{
    MainCore *mainCore = MainCore::instance();
    m_channelInstanceRegistrations.append(channelAPI);
    mainCore->addChannelInstance(this, channelAPI);
    renameChannelInstances();
}

void DeviceSet::removeChannelInstanceAt(int index)
{
    if (index < m_channelInstanceRegistrations.size())
    {
        MainCore *mainCore = MainCore::instance();
        m_channelInstanceRegistrations.removeAt(index);
        mainCore->removeChannelInstanceAt(this, index);
        renameChannelInstances();
    }
}

void DeviceSet::removeChannelInstance(ChannelAPI *channelAPI)
{
    MainCore *mainCore = MainCore::instance();

    for (int i = 0; i < m_channelInstanceRegistrations.count(); i++)
    {
        if (m_channelInstanceRegistrations.at(i) == channelAPI)
        {
            m_channelInstanceRegistrations.removeAt(i);
            mainCore->removeChannelInstance(channelAPI);
            break;
        }
    }

    renameChannelInstances();
}

void DeviceSet::clearChannels()
{
    MainCore *mainCore = MainCore::instance();
    m_channelInstanceRegistrations.clear();
    mainCore->clearChannels(this);
}
