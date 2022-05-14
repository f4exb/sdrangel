///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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
#include "udpsinkgui.h"
#endif
#include "udpsink.h"
#include "udpsinkwebapiadapter.h"
#include "udpsinkplugin.h"

const PluginDescriptor UDPSinkPlugin::m_pluginDescriptor = {
    UDPSink::m_channelId,
	QStringLiteral("UDP Channel Sink"),
    QStringLiteral("7.0.0"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

UDPSinkPlugin::UDPSinkPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& UDPSinkPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void UDPSinkPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register TCP Channel Source
	m_pluginAPI->registerRxChannel(UDPSink::m_channelIdURI, UDPSink::m_channelId, this);
}

void UDPSinkPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		UDPSink *instance = new UDPSink(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

#ifdef SERVER_MODE
ChannelGUI* UDPSinkPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
	(void) deviceUISet;
	(void) rxChannel;
    return nullptr;
}
#else
ChannelGUI* UDPSinkPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return UDPSinkGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* UDPSinkPlugin::createChannelWebAPIAdapter() const
{
	return new UDPSinkWebAPIAdapter();
}
