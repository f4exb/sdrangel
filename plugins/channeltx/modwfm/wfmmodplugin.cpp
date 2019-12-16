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
#include "wfmmodgui.h"
#endif
#include "wfmmod.h"
#include "wfmmodwebapiadapter.h"
#include "wfmmodplugin.h"

const PluginDescriptor WFMModPlugin::m_pluginDescriptor = {
    WFMMod::m_channelId,
    QString("WFM Modulator"),
    QString("4.12.3"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

WFMModPlugin::WFMModPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& WFMModPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void WFMModPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register AM modulator
	m_pluginAPI->registerTxChannel(WFMMod::m_channelIdURI, WFMMod::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* WFMModPlugin::createTxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSource *txChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* WFMModPlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const
{
    return WFMModGUI::create(m_pluginAPI, deviceUISet, txChannel);
}
#endif

BasebandSampleSource* WFMModPlugin::createTxChannelBS(DeviceAPI *deviceAPI) const
{
    return new WFMMod(deviceAPI);
}

ChannelAPI* WFMModPlugin::createTxChannelCS(DeviceAPI *deviceAPI) const
{
    return new WFMMod(deviceAPI);
}

ChannelWebAPIAdapter* WFMModPlugin::createChannelWebAPIAdapter() const
{
	return new WFMModWebAPIAdapter();
}
