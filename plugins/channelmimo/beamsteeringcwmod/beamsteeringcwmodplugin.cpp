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


#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "beamsteeringcwmodgui.h"
#endif
#include "beamsteeringcwmod.h"
#include "beamsteeringcwmodwebapiadapter.h"
#include "beamsteeringcwmodplugin.h"

const PluginDescriptor BeamSteeringCWModPlugin::m_pluginDescriptor = {
    QString("BeamSteeringCWMod"),
    QString("5.0.0"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

BeamSteeringCWModPlugin::BeamSteeringCWModPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& BeamSteeringCWModPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void BeamSteeringCWModPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    // register channel MIMO
    m_pluginAPI->registerMIMOChannel(BeamSteeringCWMod::m_channelIdURI, BeamSteeringCWMod::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* BeamSteeringCWModPlugin::createMIMOChannelGUI(
        DeviceUISet *deviceUISet,
        MIMOChannel *mimoChannel) const
{
    return nullptr;
}
#else
PluginInstanceGUI* BeamSteeringCWModPlugin::createMIMOChannelGUI(DeviceUISet *deviceUISet, MIMOChannel *mimoChannel) const
{
    return BeamSteeringCWModGUI::create(m_pluginAPI, deviceUISet, mimoChannel);
}
#endif

MIMOChannel* BeamSteeringCWModPlugin::createMIMOChannelBS(DeviceAPI *deviceAPI) const
{
    return new BeamSteeringCWMod(deviceAPI);
}

ChannelAPI* BeamSteeringCWModPlugin::createMIMOChannelCS(DeviceAPI *deviceAPI) const
{
    return new BeamSteeringCWMod(deviceAPI);
}

ChannelWebAPIAdapter* BeamSteeringCWModPlugin::createChannelWebAPIAdapter() const
{
	return new BeamSteeringCWModWebAPIAdapter();
}
