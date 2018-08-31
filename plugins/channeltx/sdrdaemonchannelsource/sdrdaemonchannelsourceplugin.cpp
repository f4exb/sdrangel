///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "sdrdaemonchannelsourcegui.h"
#endif
#include "sdrdaemonchannelsource.h"
#include "sdrdaemonchannelsourceplugin.h"

const PluginDescriptor SDRDaemonChannelSourcePlugin::m_pluginDescriptor = {
    QString("SDRDaemon source"),
    QString("4.1.0"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

SDRDaemonChannelSourcePlugin::SDRDaemonChannelSourcePlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& SDRDaemonChannelSourcePlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void SDRDaemonChannelSourcePlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register 
	m_pluginAPI->registerTxChannel(SDRDaemonChannelSource::m_channelIdURI, SDRDaemonChannelSource::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* SDRDaemonChannelSourcePlugin::createTxChannelGUI(
        DeviceUISet *deviceUISet __attribute__((unused)),
        BasebandSampleSource *txChannel __attribute__((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* SDRDaemonChannelSourcePlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel)
{
	return SDRDaemonChannelSourceGUI::create(m_pluginAPI, deviceUISet, txChannel);
}
#endif

BasebandSampleSource* SDRDaemonChannelSourcePlugin::createTxChannelBS(DeviceSinkAPI *deviceAPI)
{
    return new SDRDaemonChannelSource(deviceAPI);
}

ChannelSourceAPI* SDRDaemonChannelSourcePlugin::createTxChannelCS(DeviceSinkAPI *deviceAPI)
{
    return new SDRDaemonChannelSource(deviceAPI);
}

