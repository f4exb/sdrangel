///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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
#include "aismodgui.h"
#endif
#include "aismod.h"
#include "aismodwebapiadapter.h"
#include "aismodplugin.h"

const PluginDescriptor AISModPlugin::m_pluginDescriptor = {
    AISMod::m_channelId,
    QStringLiteral("AIS Modulator"),
    QStringLiteral("7.0.0"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

AISModPlugin::AISModPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& AISModPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void AISModPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerTxChannel(AISMod::m_channelIdURI, AISMod::m_channelId, this);
}

void AISModPlugin::createTxChannel(DeviceAPI *deviceAPI, BasebandSampleSource **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		AISMod *instance = new AISMod(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

#ifdef SERVER_MODE
ChannelGUI* AISModPlugin::createTxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSource *txChannel) const
{
	(void) deviceUISet;
	(void) txChannel;
    return nullptr;
}
#else
ChannelGUI* AISModPlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const
{
    return AISModGUI::create(m_pluginAPI, deviceUISet, txChannel);
}
#endif

ChannelWebAPIAdapter* AISModPlugin::createChannelWebAPIAdapter() const
{
    return new AISModWebAPIAdapter();
}
