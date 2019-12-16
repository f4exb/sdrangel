///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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
#include "freedvmodgui.h"
#endif
#include "freedvmod.h"
#include "freedvmodwebapiadapter.h"
#include "freedvmodplugin.h"

const PluginDescriptor FreeDVModPlugin::m_pluginDescriptor = {
    FreeDVMod::m_channelId,
    QString("FreeDV Modulator"),
    QString("4.12.3"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

FreeDVModPlugin::FreeDVModPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& FreeDVModPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void FreeDVModPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register FreeDV modulator
    m_pluginAPI->registerTxChannel(FreeDVMod::m_channelIdURI, FreeDVMod::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* FreeDVModPlugin::createTxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSource *txChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* FreeDVModPlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const
{
    return FreeDVModGUI::create(m_pluginAPI, deviceUISet, txChannel);
}
#endif

BasebandSampleSource* FreeDVModPlugin::createTxChannelBS(DeviceAPI *deviceAPI) const
{
    return new FreeDVMod(deviceAPI);
}

ChannelAPI* FreeDVModPlugin::createTxChannelCS(DeviceAPI *deviceAPI) const
{
    return new FreeDVMod(deviceAPI);
}

ChannelWebAPIAdapter* FreeDVModPlugin::createChannelWebAPIAdapter() const
{
	return new FreeDVModWebAPIAdapter();
}
