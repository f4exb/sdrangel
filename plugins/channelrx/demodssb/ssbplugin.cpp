///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2014 John Greb <hexameron>                                          //
// Copyright (C) 2015-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>              //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                                //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                          //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#include "ssbplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"
#ifndef SERVER_MODE
#include "ssbdemodgui.h"
#endif
#include "ssbdemod.h"
#include "ssbdemodwebapiadapter.h"
#include "ssbplugin.h"

const PluginDescriptor SSBPlugin::m_pluginDescriptor = {
    SSBDemod::m_channelId,
	QStringLiteral("SSB Demodulator"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

SSBPlugin::SSBPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& SSBPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void SSBPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register demodulator
	m_pluginAPI->registerRxChannel(SSBDemod::m_channelIdURI, SSBDemod::m_channelId, this);
}

void SSBPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		SSBDemod *instance = new SSBDemod(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

#ifdef SERVER_MODE
ChannelGUI* SSBPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
	(void) deviceUISet;
	(void) rxChannel;
    return nullptr;
}
#else
ChannelGUI* SSBPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return SSBDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* SSBPlugin::createChannelWebAPIAdapter() const
{
	return new SSBDemodWebAPIAdapter();
}
