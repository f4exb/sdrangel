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

#include "ssbmodgui.h"
#include "ssbmod.h"
#include "ssbmodplugin.h"

const PluginDescriptor SSBModPlugin::m_pluginDescriptor = {
    QString("SSB Modulator"),
    QString("3.9.0"),
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

PluginInstanceGUI* SSBModPlugin::createTxChannelGUI(const QString& channelName, DeviceUISet *deviceUISet, BasebandSampleSource *txChannel)
{
    if(channelName == SSBMod::m_channelIdURI)
	{
	    SSBModGUI* gui = SSBModGUI::create(m_pluginAPI, deviceUISet, txChannel);
		return gui;
	} else {
		return 0;
	}
}

BasebandSampleSource* SSBModPlugin::createTxChannelBS(const QString& channelName, DeviceSinkAPI *deviceAPI)
{
    if(channelName == SSBMod::m_channelIdURI)
    {
        SSBMod* source = new SSBMod(deviceAPI);
        return source;
    } else {
        return 0;
    }
}

ChannelSourceAPI* SSBModPlugin::createTxChannelCS(DeviceSinkAPI *deviceAPI)
{
    return new SSBMod(deviceAPI);
}


