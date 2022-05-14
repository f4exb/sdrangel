///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "bfmplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "bfmdemodgui.h"
#endif
#include "bfmdemod.h"
#include "bfmdemodwebapiadapter.h"
#include "bfmplugin.h"

const PluginDescriptor BFMPlugin::m_pluginDescriptor = {
    BFMDemod::m_channelId,
	QStringLiteral("Broadcast FM Demodulator"),
    QStringLiteral("7.0.0"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

BFMPlugin::BFMPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& BFMPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void BFMPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register BFM demodulator
	m_pluginAPI->registerRxChannel(BFMDemod::m_channelIdURI, BFMDemod::m_channelId, this);
}

void BFMPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		BFMDemod *instance = new BFMDemod(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

#ifdef SERVER_MODE
ChannelGUI* BFMPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
	(void) deviceUISet;
	(void) rxChannel;
    return nullptr;
}
#else
ChannelGUI* BFMPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return BFMDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* BFMPlugin::createChannelWebAPIAdapter() const
{
	return new BFMDemodWebAPIAdapter();
}
