///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
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

#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"

#ifdef SERVER_MODE
#include "hackrfinput.h"
#else
#include "hackrfinputgui.h"
#endif
#include "hackrfinputplugin.h"
#include "hackrfinputwebapiadapter.h"

const PluginDescriptor HackRFInputPlugin::m_pluginDescriptor = {
	QString("HackRF Input"),
	QString("4.11.10"),
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

void HackRFInputPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

	hackrf_device_list_t *hackrf_devices = hackrf_device_list();
	hackrf_device *hackrf_ptr;
	read_partid_serialno_t read_partid_serialno;
	int i;

	for (i=0; i < hackrf_devices->devicecount; i++)
	{
	    hackrf_error rc = (hackrf_error) hackrf_device_list_open(hackrf_devices, i, &hackrf_ptr);

		if (rc == HACKRF_SUCCESS)
		{
			qDebug("HackRFInputPlugin::enumOriginDevices: try to enumerate HackRF device #%d", i);

			rc = (hackrf_error) hackrf_board_partid_serialno_read(hackrf_ptr, &read_partid_serialno);

			if (rc != HACKRF_SUCCESS)
			{
				qDebug("HackRFInputPlugin::enumOriginDevices: failed to read serial no: %s", hackrf_error_name(rc));
				hackrf_close(hackrf_ptr);
				continue; // next
			}

			uint32_t serial_msb = read_partid_serialno.serial_no[2];
			uint32_t serial_lsb = read_partid_serialno.serial_no[3];

			QString serial_str = QString::number(serial_msb, 16) + QString::number(serial_lsb, 16);
			//uint64_t serial_num = (((uint64_t) serial_msb)<<32) + serial_lsb;
			QString displayedName(QString("HackRF[%1] %2").arg(i).arg(serial_str));

			originDevices.append(OriginDevice(
				displayedName,
				m_hardwareID,
				serial_str,
				i,
				1,
				1
			));

			qDebug("HackRFInputPlugin::enumOriginDevices: enumerated HackRF device #%d", i);

			hackrf_close(hackrf_ptr);
		}
		else
		{
			qDebug("HackRFOutputPlugin::enumOriginDevices: failed to enumerate HackRF device #%d: %s", i, hackrf_error_name(rc));
		}
	}

	hackrf_device_list_free(hackrf_devices);
	listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices HackRFInputPlugin::enumSampleSources(const OriginDevices& originDevices)
{
	SamplingDevices result;

	for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
			result.append(SamplingDevice(
				it->displayableName,
				it->hardwareId,
				m_deviceTypeID,
				it->serial,
				it->sequence,
				PluginInterface::SamplingDevice::PhysicalDevice,
				PluginInterface::SamplingDevice::StreamSingleRx,
				1,
				0
			));
		}
	}

	return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* HackRFInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    (void) sourceId;
    (void) widget;
    (void) deviceUISet;
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

DeviceSampleSource *HackRFInputPlugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
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

DeviceWebAPIAdapter *HackRFInputPlugin::createDeviceWebAPIAdapter() const
{
    return new HackRFInputWebAPIAdapter();
}
