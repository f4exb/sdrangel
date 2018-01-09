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

#include "ammodgui.h"
#include "ammod.h"
#include "ammodplugin.h"

const PluginDescriptor AMModPlugin::m_pluginDescriptor = {
    QString("AM Modulator"),
    QString("3.10.1"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

AMModPlugin::AMModPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& AMModPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void AMModPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register AM modulator
	m_pluginAPI->registerTxChannel(AMMod::m_channelIdURI, AMMod::m_channelId, this);
}

PluginInstanceGUI* AMModPlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel)
{
	return AMModGUI::create(m_pluginAPI, deviceUISet, txChannel);
}

BasebandSampleSource* AMModPlugin::createTxChannelBS(DeviceSinkAPI *deviceAPI)
{
    return new AMMod(deviceAPI);
}

ChannelSourceAPI* AMModPlugin::createTxChannelCS(DeviceSinkAPI *deviceAPI)
{
    return new AMMod(deviceAPI);
}

