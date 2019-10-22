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

#include "beamsteeringcwsourceplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "beamsteeringcwsourcegui.h"
#endif
#include "beamsteeringcwsource.h"
#include "beamsteeringcwsourcewebapiadapter.h"
#include "beamsteeringcwsourceplugin.h"

const PluginDescriptor BeamSteeringCWSourcePlugin::m_pluginDescriptor = {
    QString("BeamSteeringCWSource"),
    QString("5.0.0"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

BeamSteeringCWSourcePlugin::BeamSteeringCWSourcePlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& BeamSteeringCWSourcePlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void BeamSteeringCWSourcePlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    // register channel MIMO
    m_pluginAPI->registerMIMOChannel(BeamSteeringCWSource::m_channelIdURI, BeamSteeringCWSource::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* BeamSteeringCWSourcePlugin::createMIMOChannelGUI(
        DeviceUISet *deviceUISet,
        MIMOChannel *mimoChannel) const
{
    return nullptr;
}
#else
PluginInstanceGUI* BeamSteeringCWSourcePlugin::createMIMOChannelGUI(DeviceUISet *deviceUISet, MIMOChannel *mimoChannel) const
{
    return BeamSteeringCWSourceGUI::create(m_pluginAPI, deviceUISet, mimoChannel);
}
#endif

MIMOChannel* BeamSteeringCWSourcePlugin::createMIMOChannelBS(DeviceAPI *deviceAPI) const
{
    return new BeamSteeringCWSource(deviceAPI);
}

ChannelAPI* BeamSteeringCWSourcePlugin::createMIMOChannelCS(DeviceAPI *deviceAPI) const
{
    return new BeamSteeringCWSource(deviceAPI);
}

ChannelWebAPIAdapter* BeamSteeringCWSourcePlugin::createChannelWebAPIAdapter() const
{
	return new BeamSteeringCWSourceWebAPIAdapter();
}
