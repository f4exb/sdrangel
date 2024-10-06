///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
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
#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "nfmplugin.h"
#ifndef SERVER_MODE
#include "nfmdemodgui.h"
#endif
#include "nfmdemod.h"
#include "nfmdemodwebapiadapter.h"
#include "nfmplugin.h"

const PluginDescriptor NFMPlugin::m_pluginDescriptor = {
    NFMDemod::m_channelId,
	QStringLiteral("NFM Demodulator"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

NFMPlugin::NFMPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& NFMPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void NFMPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register NFM demodulator
	m_pluginAPI->registerRxChannel(NFMDemod::m_channelIdURI, NFMDemod::m_channelId, this);
}

void NFMPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		NFMDemod *instance = new NFMDemod(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

#ifdef SERVER_MODE
ChannelGUI* NFMPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
	(void) deviceUISet;
	(void) rxChannel;
    return nullptr;
}
#else
ChannelGUI* NFMPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return NFMDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* NFMPlugin::createChannelWebAPIAdapter() const
{
	return new NFMDemodWebAPIAdapter();
}
