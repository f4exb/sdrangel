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
#include <libairspy/airspy.h>

#include "airspygui.h"
#include "airspyplugin.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"

const PluginDescriptor AirspyPlugin::m_pluginDescriptor = {
	QString("Airspy Input"),
	QString("---"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

AirspyPlugin::AirspyPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& AirspyPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void AirspyPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;
	m_pluginAPI->registerSampleSource("org.osmocom.sdr.samplesource.airspy", this);
}

PluginInterface::SampleSourceDevices AirspyPlugin::enumSampleSources()
{
	SampleSourceDevices result;
	airspy_read_partid_serialno_t read_partid_serialno;
	struct airspy_device *devinfo = 0;
	airspy_error rc;

	rc = (airspy_error) airspy_init();

	if (rc != AIRSPY_SUCCESS)
	{
		qCritical("AirspyPlugin::enumSampleSources: failed to initiate Airspy library: %s", airspy_error_name(rc));
	}

	for (int i=0; i < AIRSPY_MAX_DEVICE; i++)
	{
		rc = (airspy_error) airspy_open(&devinfo);

		if (rc == AIRSPY_SUCCESS)
		{
			rc = (airspy_error) airspy_board_partid_serialno_read(devinfo, &read_partid_serialno);

			if (rc != AIRSPY_SUCCESS)
			{
				qDebug("AirspyPlugin::enumSampleSources: failed to read serial no: %s", airspy_error_name(rc));
				continue; // next
			}

			QString serial_str = QString::number(read_partid_serialno.serial_no[2], 16) + QString::number(read_partid_serialno.serial_no[3], 16);
			uint64_t serial_num = (((uint64_t) read_partid_serialno.serial_no[2])<<32) + read_partid_serialno.serial_no[3];
			QString displayedName(QString("Airspy #%1 0x%2").arg(i+1).arg(serial_str));
			SimpleSerializer s(1);
			s.writeS32(1, i);
			s.writeString(2, serial_str);
			s.writeU64(3, serial_num);

			result.append(SampleSourceDevice(displayedName, "org.osmocom.sdr.samplesource.airspy", s.final()));
		}
		else
		{
			qDebug("AirspyPlugin::enumSampleSources: enumerated %d Airspy devices %s", i, airspy_error_name(rc));
			break; // finished
		}
	}

	airspy_exit();

	return result;
}

PluginGUI* AirspyPlugin::createSampleSourcePluginGUI(const QString& sourceName, const QByteArray& address)
{
	if (!m_pluginAPI)
	{
		return 0;
	}

	if(sourceName == "org.osmocom.sdr.samplesource.airspy")
	{
		AirspyGui* gui = new AirspyGui(m_pluginAPI);
		m_pluginAPI->setInputGUI(gui);
		return gui;
	}
	else
	{
		return 0;
	}
}
