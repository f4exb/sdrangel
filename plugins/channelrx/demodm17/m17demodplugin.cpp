///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
// Copyright (C) 2022 unknown <highvoltagegenerator@hotmail.com>                 //
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

//REMOVED REPEATED INCLUDE...
#include <QtPlugin>
#include "plugin/pluginapi.h"
#ifndef SERVER_MODE
#include "m17demodgui.h"
#endif
#include "m17demod.h"
#include "m17demodwebapiadapter.h"
#include "m17demodplugin.h"

const PluginDescriptor M17DemodPlugin::m_pluginDescriptor = {
    M17Demod::m_channelId,
	QStringLiteral("M17 Demodulator"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

M17DemodPlugin::M17DemodPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(nullptr)
{
}

const PluginDescriptor& M17DemodPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void M17DemodPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register DSD demodulator
	m_pluginAPI->registerRxChannel(M17Demod::m_channelIdURI, M17Demod::m_channelId, this);
}

void M17DemodPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		M17Demod *instance = new M17Demod(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

#ifdef SERVER_MODE
ChannelGUI* M17DemodPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
	(void) deviceUISet;
	(void) rxChannel;
    return nullptr;
}
#else
ChannelGUI* M17DemodPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return M17DemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* M17DemodPlugin::createChannelWebAPIAdapter() const
{
	return new M17DemodWebAPIAdapter();
}
