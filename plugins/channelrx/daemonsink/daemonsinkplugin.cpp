///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include "daemonsinkplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "daemonsinkgui.h"
#endif
#include "daemonsink.h"

const PluginDescriptor DaemonSinkPlugin::m_pluginDescriptor = {
    QString("Daemon Channel Sink"),
    QString("4.1.0"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

DaemonSinkPlugin::DaemonSinkPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& DaemonSinkPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void DaemonSinkPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    // register TCP Channel Source
    m_pluginAPI->registerRxChannel(DaemonSink::m_channelIdURI, DaemonSink::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* DaemonSinkPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet __attribute__((unused)),
        BasebandSampleSink *rxChannel __attribute__((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* DaemonSinkPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    return DaemonSinkGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

BasebandSampleSink* DaemonSinkPlugin::createRxChannelBS(DeviceSourceAPI *deviceAPI)
{
    return new DaemonSink(deviceAPI);
}

ChannelSinkAPI* DaemonSinkPlugin::createRxChannelCS(DeviceSourceAPI *deviceAPI)
{
    return new DaemonSink(deviceAPI);
}




