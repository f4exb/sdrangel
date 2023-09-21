///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2019 Edouard Griffiths, F4EXB                              //
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#include "remotetcpsinkplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "remotetcpsinkgui.h"
#endif
#include "remotetcpsink.h"
#include "remotetcpsinkwebapiadapter.h"
#include "remotetcpsinkplugin.h"

const PluginDescriptor RemoteTCPSinkPlugin::m_pluginDescriptor = {
    RemoteTCPSink::m_channelId,
    QStringLiteral("Remote TCP channel sink"),
    QStringLiteral("7.16.0"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

RemoteTCPSinkPlugin::RemoteTCPSinkPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& RemoteTCPSinkPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void RemoteTCPSinkPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    // register channel Source
    m_pluginAPI->registerRxChannel(RemoteTCPSink::m_channelIdURI, RemoteTCPSink::m_channelId, this);
}

void RemoteTCPSinkPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
    if (bs || cs)
    {
        RemoteTCPSink *instance = new RemoteTCPSink(deviceAPI);

        if (bs) {
            *bs = instance;
        }

        if (cs) {
            *cs = instance;
        }
    }
}

#ifdef SERVER_MODE
ChannelGUI* RemoteTCPSinkPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    (void) deviceUISet;
    (void) rxChannel;
    return nullptr;
}
#else
ChannelGUI* RemoteTCPSinkPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return RemoteTCPSinkGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* RemoteTCPSinkPlugin::createChannelWebAPIAdapter() const
{
    return new RemoteTCPSinkWebAPIAdapter();
}
