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

#include <device/devicesourceapi.h>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"

const PluginDescriptor AirspyPlugin::m_pluginDescriptor = {
	QString("Airspy Input"),
	QString("3.8.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString AirspyPlugin::m_hardwareID = "Airspy";
const QString AirspyPlugin::m_deviceTypeID = AIRSPY_DEVICE_TYPE_ID;

AirspyPlugin::AirspyPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& AirspyPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void AirspyPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices AirspyPlugin::enumSampleSources()
{
	SamplingDevices result;
	airspy_read_partid_serialno_t read_partid_serialno;
	struct airspy_device *devinfo;
	uint32_t serial_msb = 0;
	uint32_t serial_lsb = 0;
	airspy_error rc;
	int i;

	rc = (airspy_error) airspy_init();

	if (rc != AIRSPY_SUCCESS)
	{
		qCritical("AirspyPlugin::enumSampleSources: failed to initiate Airspy library: %s", airspy_error_name(rc));
	}

	for (i=0; i < AIRSPY_MAX_DEVICE; i++)
	{
		rc = (airspy_error) airspy_open(&devinfo);

		if (rc == AIRSPY_SUCCESS)
		{
			qDebug("AirspyPlugin::enumSampleSources: try to enumerate Airspy device #%d", i);

			rc = (airspy_error) airspy_board_partid_serialno_read(devinfo, &read_partid_serialno);

			if (rc != AIRSPY_SUCCESS)
			{
				qDebug("AirspyPlugin::enumSampleSources: failed to read serial no: %s", airspy_error_name(rc));
				airspy_close(devinfo);
				continue; // next
			}

			if ((read_partid_serialno.serial_no[2] != serial_msb) && (read_partid_serialno.serial_no[3] != serial_lsb))
			{
				serial_msb = read_partid_serialno.serial_no[2];
				serial_lsb = read_partid_serialno.serial_no[3];

				QString serial_str = QString::number(serial_msb, 16) + QString::number(serial_lsb, 16);
				//uint64_t serial_num = (((uint64_t) serial_msb)<<32) + serial_lsb;
				QString displayedName(QString("Airspy[%1] %2").arg(i).arg(serial_str));

				result.append(SamplingDevice(displayedName,
				        m_hardwareID,
						m_deviceTypeID,
						serial_str,
						i,
						PluginInterface::SamplingDevice::PhysicalDevice,
						true,
						1,
						0));

				qDebug("AirspyPlugin::enumSampleSources: enumerated Airspy device #%d", i);
			}

			airspy_close(devinfo);
		}
		else
		{
			qDebug("AirspyPlugin::enumSampleSources: enumerated %d Airspy devices %s", i, airspy_error_name(rc));
			break; // finished
		}
	}

	rc = (airspy_error) airspy_exit();
	qDebug("AirspyPlugin::enumSampleSources: airspy_exit: %s", airspy_error_name(rc));

	return result;
}

PluginInstanceGUI* AirspyPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if (sourceId == m_deviceTypeID)
	{
		AirspyGui* gui = new AirspyGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}

DeviceSampleSource *AirspyPlugin::createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceSourceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        AirspyInput* input = new AirspyInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}
