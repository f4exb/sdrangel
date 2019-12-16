///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "localsinkplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "localsinkgui.h"
#endif
#include "localsink.h"
#include "localsinkwebapiadapter.h"
#include "localsinkplugin.h"

const PluginDescriptor LocalSinkPlugin::m_pluginDescriptor = {
    LocalSink::m_channelId,
    QString("Local channel sink"),
    QString("4.12.3"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

LocalSinkPlugin::LocalSinkPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& LocalSinkPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void LocalSinkPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    // register channel Source
    m_pluginAPI->registerRxChannel(LocalSink::m_channelIdURI, LocalSink::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* LocalSinkPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* LocalSinkPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return LocalSinkGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

BasebandSampleSink* LocalSinkPlugin::createRxChannelBS(DeviceAPI *deviceAPI) const
{
    return new LocalSink(deviceAPI);
}

ChannelAPI* LocalSinkPlugin::createRxChannelCS(DeviceAPI *deviceAPI) const
{
    return new LocalSink(deviceAPI);
}

ChannelWebAPIAdapter* LocalSinkPlugin::createChannelWebAPIAdapter() const
{
	return new LocalSinkWebAPIAdapter();
}
