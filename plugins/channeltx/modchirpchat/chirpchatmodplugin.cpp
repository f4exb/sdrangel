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
#include "chirpchatmodgui.h"
#endif
#include "chirpchatmod.h"
#include "chirpchatmodwebapiadapter.h"
#include "chirpchatmodplugin.h"

const PluginDescriptor ChirpChatModPlugin::m_pluginDescriptor = {
    ChirpChatMod::m_channelId,
    QString("ChirpChat Modulator"),
    QString("5.3.0"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

ChirpChatModPlugin::ChirpChatModPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& ChirpChatModPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void ChirpChatModPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register LoRa modulator
	m_pluginAPI->registerTxChannel(ChirpChatMod::m_channelIdURI, ChirpChatMod::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* ChirpChatModPlugin::createTxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSource *txChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* ChirpChatModPlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const
{
    return ChirpChatModGUI::create(m_pluginAPI, deviceUISet, txChannel);
}
#endif

BasebandSampleSource* ChirpChatModPlugin::createTxChannelBS(DeviceAPI *deviceAPI) const
{
    return new ChirpChatMod(deviceAPI);
}

ChannelAPI* ChirpChatModPlugin::createTxChannelCS(DeviceAPI *deviceAPI) const
{
    return new ChirpChatMod(deviceAPI);
}

ChannelWebAPIAdapter* ChirpChatModPlugin::createChannelWebAPIAdapter() const
{
	return new ChirpChatModWebAPIAdapter();
}
