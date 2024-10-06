///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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
    Interferometer::m_channelId,
    QStringLiteral("Interferometer"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Edouard Griffiths, F4EXB"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
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

void InterferometerPlugin::createMIMOChannel(DeviceAPI *deviceAPI, MIMOChannel **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		Interferometer *instance = new Interferometer(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

#ifdef SERVER_MODE
ChannelGUI* InterferometerPlugin::createMIMOChannelGUI(
        DeviceUISet *deviceUISet,
        MIMOChannel *mimoChannel) const
{
    return 0;
}
#else
ChannelGUI* InterferometerPlugin::createMIMOChannelGUI(DeviceUISet *deviceUISet, MIMOChannel *mimoChannel) const
{
    return InterferometerGUI::create(m_pluginAPI, deviceUISet, mimoChannel);
}
#endif

ChannelWebAPIAdapter* InterferometerPlugin::createChannelWebAPIAdapter() const
{
	return new InterferometerWebAPIAdapter();
}
