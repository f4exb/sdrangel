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
#include "fcdproplusplugin.h"
#include "fcdproplusgui.h"

const PluginDescriptor FCDProPlusPlugin::m_pluginDescriptor = {
	QString("FunCube Pro+ Input"),
	QString("---"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

FCDProPlusPlugin::FCDProPlusPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& FCDProPlusPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void FCDProPlusPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	m_pluginAPI->registerSampleSource("org.osmocom.sdr.samplesource.fcdproplus", this);
}

PluginInterface::SampleSourceDevices FCDProPlusPlugin::enumSampleSources()
{
	SampleSourceDevices result;

	int i = 1;
	struct hid_device_info *device_info = hid_enumerate(0x04D8, 0xFB31);

	while (device_info != 0)
	{
		QString serialNumber = QString::fromWCharArray(device_info->serial_number);
		QString displayedName(QString("FunCube Dongle Pro+ #%1 ").arg(i) + serialNumber);
		SimpleSerializer s(1);
		s.writeS32(1, 0);
		result.append(SampleSourceDevice(displayedName, "org.osmocom.sdr.samplesource.fcdproplus", s.final()));

		device_info = device_info->next;
		i++;
	}

	return result;
}

PluginGUI* FCDProPlusPlugin::createSampleSourcePluginGUI(const QString& sourceName, const QByteArray& address)
{
	if(sourceName == "org.osmocom.sdr.samplesource.fcdproplus")
	{
		FCDProPlusGui* gui = new FCDProPlusGui(m_pluginAPI);
		m_pluginAPI->setInputGUI(gui);
		return gui;
	}
	else
	{
		return NULL;
	}
}
