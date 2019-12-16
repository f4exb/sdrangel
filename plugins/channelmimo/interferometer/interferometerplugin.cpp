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

#include "interferometerplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "interferometergui.h"
#endif
#include "interferometer.h"
#include "interferometerwebapiadapter.h"
#include "interferometerplugin.h"

const PluginDescriptor InterferometerPlugin::m_pluginDescriptor = {
    QString(Interferometer::m_channelId),
    QString("Interferometer"),
    QString("5.0.0"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

InterferometerPlugin::InterferometerPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& InterferometerPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void InterferometerPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    // register channel MIMO
    m_pluginAPI->registerMIMOChannel(Interferometer::m_channelIdURI, Interferometer::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* InterferometerPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        MIMOChannel *mimoChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* InterferometerPlugin::createMIMOChannelGUI(DeviceUISet *deviceUISet, MIMOChannel *mimoChannel) const
{
    return InterferometerGUI::create(m_pluginAPI, deviceUISet, mimoChannel);
}
#endif

MIMOChannel* InterferometerPlugin::createMIMOChannelBS(DeviceAPI *deviceAPI) const
{
    return new Interferometer(deviceAPI);
}

ChannelAPI* InterferometerPlugin::createMIMOChannelCS(DeviceAPI *deviceAPI) const
{
    return new Interferometer(deviceAPI);
}

ChannelWebAPIAdapter* InterferometerPlugin::createChannelWebAPIAdapter() const
{
	return new InterferometerWebAPIAdapter();
}
