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
#include "sigmffilesinkgui.h"
#endif
#include "sigmffilesink.h"
#include "sigmffilesinkwebapiadapter.h"
#include "sigmffilesinkplugin.h"

const PluginDescriptor SigMFFileSinkPlugin::m_pluginDescriptor = {
    SigMFFileSink::m_channelId,
    QString("SigMF File Sink"),
    QString("5.8.1"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

SigMFFileSinkPlugin::SigMFFileSinkPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& SigMFFileSinkPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void SigMFFileSinkPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    // register channel Source
    m_pluginAPI->registerRxChannel(SigMFFileSink::m_channelIdURI, SigMFFileSink::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* SigMFFileSinkPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* SigMFFileSinkPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return SigMFFileSinkGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

BasebandSampleSink* SigMFFileSinkPlugin::createRxChannelBS(DeviceAPI *deviceAPI) const
{
    return new SigMFFileSink(deviceAPI);
}

ChannelAPI* SigMFFileSinkPlugin::createRxChannelCS(DeviceAPI *deviceAPI) const
{
    return new SigMFFileSink(deviceAPI);
}

ChannelWebAPIAdapter* SigMFFileSinkPlugin::createChannelWebAPIAdapter() const
{
	return new SigMFFileSinkWebAPIAdapter();
}
