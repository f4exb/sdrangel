///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
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

#include <QtPlugin>
#include "plugin/pluginapi.h"
#include "dsddemodplugin.h"
#include "dsddemodgui.h"

const PluginDescriptor DSDDemodPlugin::m_pluginDescriptor = {
	QString("DSD Demodulator"),
	QString("---"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

DSDDemodPlugin::DSDDemodPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& DSDDemodPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void DSDDemodPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register DSD demodulator
	m_pluginAPI->registerChannel("sdrangel.channel.dsddemod", this);
}

PluginGUI* DSDDemodPlugin::createChannel(const QString& channelName)
{
	if(channelName == "sdrangel.channel.dsddemod") {
		DSDDemodGUI* gui = DSDDemodGUI::create(m_pluginAPI);
		m_pluginAPI->registerChannelInstance("sdrangel.channel.dsddemod", gui);
		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return NULL;
	}
}

void DSDDemodPlugin::createInstanceDSDDemod()
{
    DSDDemodGUI* gui = DSDDemodGUI::create(m_pluginAPI);
	m_pluginAPI->registerChannelInstance("sdrangel.channel.dsddemod", gui);
	m_pluginAPI->addChannelRollup(gui);
}
