///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2019 Edouard Griffiths, F4EXB                              //
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

#include "remotesinkplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "remotesinkgui.h"
#endif
#include "remotesink.h"
#include "remotesinkwebapiadapter.h"
#include "remotesinkplugin.h"

const PluginDescriptor RemoteSinkPlugin::m_pluginDescriptor = {
    RemoteSink::m_channelId,
    QString("Remote channel sink"),
    QString("4.12.3"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

RemoteSinkPlugin::RemoteSinkPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& RemoteSinkPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void RemoteSinkPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    // register channel Source
    m_pluginAPI->registerRxChannel(RemoteSink::m_channelIdURI, RemoteSink::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* RemoteSinkPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* RemoteSinkPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return RemoteSinkGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

BasebandSampleSink* RemoteSinkPlugin::createRxChannelBS(DeviceAPI *deviceAPI) const
{
    return new RemoteSink(deviceAPI);
}

ChannelAPI* RemoteSinkPlugin::createRxChannelCS(DeviceAPI *deviceAPI) const
{
    return new RemoteSink(deviceAPI);
}

ChannelWebAPIAdapter* RemoteSinkPlugin::createChannelWebAPIAdapter() const
{
	return new RemoteSinkWebAPIAdapter();
}
