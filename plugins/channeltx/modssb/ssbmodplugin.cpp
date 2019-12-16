///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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
#include "ssbmodgui.h"
#endif
#include "ssbmod.h"
#include "ssbmodwebapiadapter.h"
#include "ssbmodplugin.h"

const PluginDescriptor SSBModPlugin::m_pluginDescriptor = {
    SSBMod::m_channelId,
    QString("SSB Modulator"),
    QString("4.12.3"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

SSBModPlugin::SSBModPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& SSBModPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void SSBModPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register SSB modulator
    m_pluginAPI->registerTxChannel(SSBMod::m_channelIdURI, SSBMod::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* SSBModPlugin::createTxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSource *txChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* SSBModPlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const
{
    return SSBModGUI::create(m_pluginAPI, deviceUISet, txChannel);
}
#endif

BasebandSampleSource* SSBModPlugin::createTxChannelBS(DeviceAPI *deviceAPI) const
{
    return new SSBMod(deviceAPI);
}

ChannelAPI* SSBModPlugin::createTxChannelCS(DeviceAPI *deviceAPI) const
{
    return new SSBMod(deviceAPI);
}

ChannelWebAPIAdapter* SSBModPlugin::createChannelWebAPIAdapter() const
{
	return new SSBModWebAPIAdapter();
}

