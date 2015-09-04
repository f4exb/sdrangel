///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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
#include "util/simpleserializer.h"
#include "fcdproplugin.h"
#include "fcdprogui.h"

const PluginDescriptor FCDProPlugin::m_pluginDescriptor = {
	QString("FunCube Pro Input"),
	QString("---"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

FCDProPlugin::FCDProPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& FCDProPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void FCDProPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	m_pluginAPI->registerSampleSource("org.osmocom.sdr.samplesource.fcdpro", this);
}

PluginInterface::SampleSourceDevices FCDProPlugin::enumSampleSources()
{
	SampleSourceDevices result;

	QString displayedName(QString("Funcube Dongle Pro #1"));
	SimpleSerializer s(1);
	s.writeS32(1, 0);
	result.append(SampleSourceDevice(displayedName, "org.osmocom.sdr.samplesource.fcdpro", s.final()));

	return result;
}

PluginGUI* FCDProPlugin::createSampleSourcePluginGUI(const QString& sourceName, const QByteArray& address)
{
	if(sourceName == "org.osmocom.sdr.samplesource.fcdpro")
	{
		FCDProGui* gui = new FCDProGui(m_pluginAPI);
		m_pluginAPI->setInputGUI(gui);
		return gui;
	}
	else
	{
		return NULL;
	}
}
