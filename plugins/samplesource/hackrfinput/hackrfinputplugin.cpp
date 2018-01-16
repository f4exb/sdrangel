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
#include "libhackrf/hackrf.h"

#include <device/devicesourceapi.h>

#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"

#ifdef SERVER_MODE
#include "hackrfinput.h"
#else
#include "hackrfinputgui.h"
#endif
#include "hackrfinputplugin.h"

const PluginDescriptor HackRFInputPlugin::m_pluginDescriptor = {
	QString("HackRF Input"),
	QString("3.11.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString HackRFInputPlugin::m_hardwareID = "HackRF";
const QString HackRFInputPlugin::m_deviceTypeID = HACKRF_DEVICE_TYPE_ID;

HackRFInputPlugin::HackRFInputPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& HackRFInputPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void HackRFInputPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices HackRFInputPlugin::enumSampleSources()
{
    DeviceHackRF::instance();
//	hackrf_error rc = (hackrf_error) hackrf_init();
//
//	if (rc != HACKRF_SUCCESS)
//	{
//		qCritical("HackRFPlugin::SampleSourceDevices: failed to initiate HackRF library: %s", hackrf_error_name(rc));
//	}

	SamplingDevices result;
	hackrf_device_list_t *hackrf_devices = hackrf_device_list();
	hackrf_device *hackrf_ptr;
	read_partid_serialno_t read_partid_serialno;
	int i;

	for (i=0; i < hackrf_devices->devicecount; i++)
	{
	    hackrf_error rc = (hackrf_error) hackrf_device_list_open(hackrf_devices, i, &hackrf_ptr);

		if (rc == HACKRF_SUCCESS)
		{
			qDebug("HackRFPlugin::enumSampleSources: try to enumerate HackRF device #%d", i);

			rc = (hackrf_error) hackrf_board_partid_serialno_read(hackrf_ptr, &read_partid_serialno);

			if (rc != HACKRF_SUCCESS)
			{
				qDebug("HackRFPlugin::enumSampleSources: failed to read serial no: %s", hackrf_error_name(rc));
				hackrf_close(hackrf_ptr);
				continue; // next
			}

			uint32_t serial_msb = read_partid_serialno.serial_no[2];
			uint32_t serial_lsb = read_partid_serialno.serial_no[3];

			QString serial_str = QString::number(serial_msb, 16) + QString::number(serial_lsb, 16);
			//uint64_t serial_num = (((uint64_t) serial_msb)<<32) + serial_lsb;
			QString displayedName(QString("HackRF[%1] %2").arg(i).arg(serial_str));

			result.append(SamplingDevice(displayedName,
			        m_hardwareID,
			        m_deviceTypeID,
					serial_str,
					i,
					PluginInterface::SamplingDevice::PhysicalDevice,
					true,
					1,
					0));

			qDebug("HackRFPlugin::enumSampleSources: enumerated HackRF device #%d", i);

			hackrf_close(hackrf_ptr);
		}
		else
		{
			qDebug("HackRFPlugin::enumSampleSources: failed to enumerate HackRF device #%d: %s", i, hackrf_error_name(rc));
		}
	}

	hackrf_device_list_free(hackrf_devices);
//	rc = (hackrf_error) hackrf_exit();
//	qDebug("HackRFPlugin::enumSampleSources: hackrf_exit: %s", hackrf_error_name(rc));

	return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* HackRFInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId __attribute__((unused)),
        QWidget **widget __attribute__((unused)),
        DeviceUISet *deviceUISet __attribute__((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* HackRFInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sourceId == m_deviceTypeID)
	{
		HackRFInputGui* gui = new HackRFInputGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
#endif

DeviceSampleSource *HackRFInputPlugin::createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceSourceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        HackRFInput* input = new HackRFInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}
