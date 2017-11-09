///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "udpsrcplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "udpsrcgui.h"
#include "udpsrc.h"

const PluginDescriptor UDPSrcPlugin::m_pluginDescriptor = {
	QString("UDP Channel Source"),
	QString("3.8.2"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

UDPSrcPlugin::UDPSrcPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& UDPSrcPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void UDPSrcPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register TCP Channel Source
	m_pluginAPI->registerRxChannel(UDPSrc::m_channelID, this);
}

PluginInstanceGUI* UDPSrcPlugin::createRxChannelGUI(const QString& channelName, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	if(channelName == UDPSrc::m_channelID)
	{
		UDPSrcGUI* gui = UDPSrcGUI::create(m_pluginAPI, deviceUISet, rxChannel);
//		deviceAPI->registerChannelInstance("sdrangel.channel.udpsrc", gui);
//		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return 0;
	}
}

BasebandSampleSink* UDPSrcPlugin::createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
    if(channelName == UDPSrc::m_channelID)
    {
        UDPSrc* sink = new UDPSrc(deviceAPI);
        return sink;
    } else {
        return 0;
    }
}
