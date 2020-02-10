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

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "loramodgui.h"
#endif
#include "loramod.h"
#include "loramodwebapiadapter.h"
#include "loramodplugin.h"

const PluginDescriptor LoRaModPlugin::m_pluginDescriptor = {
    LoRaMod::m_channelId,
    QString("LoRa Modulator"),
    QString("5.2.0"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

LoRaModPlugin::LoRaModPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& LoRaModPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void LoRaModPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register LoRa modulator
	m_pluginAPI->registerTxChannel(LoRaMod::m_channelIdURI, LoRaMod::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* LoRaModPlugin::createTxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSource *txChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* LoRaModPlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const
{
    return LoRaModGUI::create(m_pluginAPI, deviceUISet, txChannel);
}
#endif

BasebandSampleSource* LoRaModPlugin::createTxChannelBS(DeviceAPI *deviceAPI) const
{
    return new LoRaMod(deviceAPI);
}

ChannelAPI* LoRaModPlugin::createTxChannelCS(DeviceAPI *deviceAPI) const
{
    return new LoRaMod(deviceAPI);
}

ChannelWebAPIAdapter* LoRaModPlugin::createChannelWebAPIAdapter() const
{
	return new LoRaModWebAPIAdapter();
}
