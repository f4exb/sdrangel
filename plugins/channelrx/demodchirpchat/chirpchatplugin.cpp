///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2020 Edouard Griffiths, F4EXB                              //
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

#include "chirpchatplugin.h"
#ifndef SERVER_MODE
#include "chirpchatdemodgui.h"
#endif
#include "chirpchatdemod.h"

const PluginDescriptor ChirpChatPlugin::m_pluginDescriptor = {
    ChirpChatDemod::m_channelId,
	QString("ChirpChat Demodulator"),
	QString("5.3.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

ChirpChatPlugin::ChirpChatPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(nullptr)
{
}

const PluginDescriptor& ChirpChatPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void ChirpChatPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register demodulator
	m_pluginAPI->registerRxChannel(ChirpChatDemod::m_channelIdURI, ChirpChatDemod::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* ChirpChatPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* ChirpChatPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return ChirpChatDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

BasebandSampleSink* ChirpChatPlugin::createRxChannelBS(DeviceAPI *deviceAPI) const
{
    return new ChirpChatDemod(deviceAPI);
}

ChannelAPI* ChirpChatPlugin::createRxChannelCS(DeviceAPI *deviceAPI) const
{
    return new ChirpChatDemod(deviceAPI);
}

