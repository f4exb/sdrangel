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
#include <libbladeRF.h>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "bladerfgui.h"
#include "bladerfplugin.h"

const PluginDescriptor BlderfPlugin::m_pluginDescriptor = {
	QString("BladerRF Input"),
	QString("1.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/rtl-sdrangelove/tree/f4exb"),
	true,
	QString("https://github.com/f4exb/rtl-sdrangelove/tree/f4exb")
};

BlderfPlugin::BlderfPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& BlderfPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void BlderfPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;
	m_pluginAPI->registerSampleSource("org.osmocom.sdr.samplesource.bladerf", this);
}

PluginInterface::SampleSourceDevices BlderfPlugin::enumSampleSources()
{
	SampleSourceDevices result;
	struct bladerf_devinfo *devinfo;
	int count = bladerf_get_device_list(&devinfo);

	for(int i = 0; i < count; i++)
	{
		QString displayedName(QString("BladeRF #%1 %2 (%3,%4)").arg(devinfo[i].instance).arg(devinfo[i].serial).arg(devinfo[i].usb_bus).arg(devinfo[i].usb_addr));
		SimpleSerializer s(1);
		s.writeS32(1, i);
		s.writeString(2, devinfo[i].serial);
		result.append(SampleSourceDevice(displayedName, "org.osmocom.sdr.samplesource.bladerf", s.final()));
	}
	return result;
}

PluginGUI* BlderfPlugin::createSampleSource(const QString& sourceName, const QByteArray& address)
{
	if(sourceName == "org.osmocom.sdr.samplesource.bladerf") {
		BladerfGui* gui = new BladerfGui(m_pluginAPI);
		m_pluginAPI->setInputGUI(gui);
		return gui;
	} else {
		return NULL;
	}
}
