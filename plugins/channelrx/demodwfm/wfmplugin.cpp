///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2014 John Greb <hexameron>                                          //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>              //
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
#include "wfmplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "wfmdemodgui.h"
#endif
#include "wfmdemod.h"
#include "wfmdemodwebapiadapter.h"
#include "wfmplugin.h"

const PluginDescriptor WFMPlugin::m_pluginDescriptor = {
    WFMDemod::m_channelId,
	QStringLiteral("WFM Demodulator"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

WFMPlugin::WFMPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& WFMPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void WFMPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register WFM demodulator
	m_pluginAPI->registerRxChannel(WFMDemod::m_channelIdURI, WFMDemod::m_channelId, this);
}

void WFMPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		WFMDemod *instance = new WFMDemod(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

#ifdef SERVER_MODE
ChannelGUI* WFMPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
	(void) deviceUISet;
	(void) rxChannel;
    return nullptr;
}
#else
ChannelGUI* WFMPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return WFMDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* WFMPlugin::createChannelWebAPIAdapter() const
{
	return new WFMDemodWebAPIAdapter();
}
