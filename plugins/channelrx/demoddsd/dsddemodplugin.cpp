///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include "dsddemodplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"
#ifndef SERVER_MODE
#include "dsddemodgui.h"
#endif
#include "dsddemod.h"
#include "dsddemodwebapiadapter.h"
#include "dsddemodplugin.h"

const PluginDescriptor DSDDemodPlugin::m_pluginDescriptor = {
    DSDDemod::m_channelId,
	QStringLiteral("DSD Demodulator"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

DSDDemodPlugin::DSDDemodPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& DSDDemodPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void DSDDemodPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register DSD demodulator
	m_pluginAPI->registerRxChannel(DSDDemod::m_channelIdURI, DSDDemod::m_channelId, this);
}

void DSDDemodPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		DSDDemod *instance = new DSDDemod(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

#ifdef SERVER_MODE
ChannelGUI* DSDDemodPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
	(void) deviceUISet;
	(void) rxChannel;
    return nullptr;
}
#else
ChannelGUI* DSDDemodPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return DSDDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* DSDDemodPlugin::createChannelWebAPIAdapter() const
{
	return new DSDDemodWebAPIAdapter();
}
