///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "m17modgui.h"
#endif
#include "m17mod.h"
#include "m17modwebapiadapter.h"
#include "m17modplugin.h"

const PluginDescriptor M17ModPlugin::m_pluginDescriptor = {
    M17Mod::m_channelId,
    QStringLiteral("M17 Modulator"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Edouard Griffiths, F4EXB"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

M17ModPlugin::M17ModPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& M17ModPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void M17ModPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register AM modulator
	m_pluginAPI->registerTxChannel(M17Mod::m_channelIdURI, M17Mod::m_channelId, this);
}

void M17ModPlugin::createTxChannel(DeviceAPI *deviceAPI, BasebandSampleSource **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		M17Mod *instance = new M17Mod(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

#ifdef SERVER_MODE
ChannelGUI* M17ModPlugin::createTxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSource *txChannel) const
{
	(void) deviceUISet;
	(void) txChannel;
    return nullptr;
}
#else
ChannelGUI* M17ModPlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const
{
    return M17ModGUI::create(m_pluginAPI, deviceUISet, txChannel);
}
#endif

ChannelWebAPIAdapter* M17ModPlugin::createChannelWebAPIAdapter() const
{
	return new M17ModWebAPIAdapter();
}
