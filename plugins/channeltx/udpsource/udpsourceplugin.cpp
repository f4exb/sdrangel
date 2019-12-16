///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
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

#include "udpsourceplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "udpsourcegui.h"
#endif
#include "udpsource.h"
#include "udpsourcewebapiadapter.h"
#include "udpsourceplugin.h"

const PluginDescriptor UDPSourcePlugin::m_pluginDescriptor = {
    UDPSource::m_channelId,
	QString("UDP Channel Source"),
	QString("4.12.3"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

UDPSourcePlugin::UDPSourcePlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& UDPSourcePlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void UDPSourcePlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register TCP Channel Source
    m_pluginAPI->registerTxChannel(UDPSource::m_channelIdURI, UDPSource::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* UDPSourcePlugin::createTxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSource *txChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* UDPSourcePlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const
{
    return UDPSourceGUI::create(m_pluginAPI, deviceUISet, txChannel);
}
#endif

BasebandSampleSource* UDPSourcePlugin::createTxChannelBS(DeviceAPI *deviceAPI) const
{
    return new UDPSource(deviceAPI);
}

ChannelAPI* UDPSourcePlugin::createTxChannelCS(DeviceAPI *deviceAPI) const
{
    return new UDPSource(deviceAPI);
}

ChannelWebAPIAdapter* UDPSourcePlugin::createChannelWebAPIAdapter() const
{
	return new UDPSourceWebAPIAdapter();
}
