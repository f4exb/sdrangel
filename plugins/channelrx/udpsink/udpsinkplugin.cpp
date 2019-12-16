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
	QString("UDP Channel Sink"),
	QString("4.12.3"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
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

#ifdef SERVER_MODE
PluginInstanceGUI* UDPSinkPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* UDPSinkPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return UDPSinkGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

BasebandSampleSink* UDPSinkPlugin::createRxChannelBS(DeviceAPI *deviceAPI) const
{
    return new UDPSink(deviceAPI);
}

ChannelAPI* UDPSinkPlugin::createRxChannelCS(DeviceAPI *deviceAPI) const
{
    return new UDPSink(deviceAPI);
}

ChannelWebAPIAdapter* UDPSinkPlugin::createChannelWebAPIAdapter() const
{
	return new UDPSinkWebAPIAdapter();
}
