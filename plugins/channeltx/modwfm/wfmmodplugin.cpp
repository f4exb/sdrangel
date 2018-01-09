///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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
#include <QAction>
#include "plugin/pluginapi.h"

#include "wfmmodgui.h"
#include "wfmmod.h"
#include "wfmmodplugin.h"

const PluginDescriptor WFMModPlugin::m_pluginDescriptor = {
    QString("WFM Modulator"),
    QString("3.10.1"),
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

PluginInstanceGUI* WFMModPlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel)
{
    return WFMModGUI::create(m_pluginAPI, deviceUISet, txChannel);
}

BasebandSampleSource* WFMModPlugin::createTxChannelBS(DeviceSinkAPI *deviceAPI)
{
    return new WFMMod(deviceAPI);
}

ChannelSourceAPI* WFMModPlugin::createTxChannelCS(DeviceSinkAPI *deviceAPI)
{
    return new WFMMod(deviceAPI);
}


