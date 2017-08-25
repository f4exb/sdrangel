///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "bfmplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "bfmdemodgui.h"

const PluginDescriptor BFMPlugin::m_pluginDescriptor = {
	QString("Broadcast FM Demodulator"),
	QString("3.6.1"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

BFMPlugin::BFMPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& BFMPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void BFMPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register BFM demodulator
	m_pluginAPI->registerRxChannel(BFMDemodGUI::m_channelID, this);
}

PluginGUI* BFMPlugin::createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
	if(channelName == BFMDemodGUI::m_channelID)
	{
		BFMDemodGUI* gui = BFMDemodGUI::create(m_pluginAPI, deviceAPI);
		return gui;
	} else {
		return 0;
	}
}

void BFMPlugin::createInstanceBFM(DeviceSourceAPI *deviceAPI)
{
	BFMDemodGUI::create(m_pluginAPI, deviceAPI);
}
