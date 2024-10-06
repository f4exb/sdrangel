///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>                   //
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
#include "ft8plugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"
#ifndef SERVER_MODE
#include "ft8demodgui.h"
#endif
#include "ft8demod.h"
#include "ft8demodwebapiadapter.h"
#include "ft8plugin.h"

const PluginDescriptor FT8Plugin::m_pluginDescriptor = {
    FT8Demod::m_channelId,
	QStringLiteral("FT8 Demodulator"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

FT8Plugin::FT8Plugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(nullptr)
{
}

const PluginDescriptor& FT8Plugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void FT8Plugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register demodulator
	m_pluginAPI->registerRxChannel(FT8Demod::m_channelIdURI, FT8Demod::m_channelId, this);
}

void FT8Plugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		FT8Demod *instance = new FT8Demod(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

#ifdef SERVER_MODE
ChannelGUI* FT8Plugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
	(void) deviceUISet;
	(void) rxChannel;
    return nullptr;
}
#else
ChannelGUI* FT8Plugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return FT8DemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* FT8Plugin::createChannelWebAPIAdapter() const
{
	return new FT8DemodWebAPIAdapter();
}
