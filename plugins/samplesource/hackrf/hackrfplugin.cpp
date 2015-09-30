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
#include "libhackrf/hackrf.h"

#include "hackrfplugin.h"

#include "hackrfgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"

const PluginDescriptor HackRFPlugin::m_pluginDescriptor = {
	QString("HackRF Input"),
	QString("---"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString HackRFPlugin::m_deviceTypeID = HACKRF_DEVICE_TYPE_ID;

HackRFPlugin::HackRFPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& HackRFPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void HackRFPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;
	m_pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SampleSourceDevices HackRFPlugin::enumSampleSources()
{
	SampleSourceDevices result;
	hackrf_device_list_t *hackrf_devices = hackrf_device_list();
	hackrf_device *hackrf_ptr;
	read_partid_serialno_t read_partid_serialno;
	hackrf_error rc;
	int i;

	rc = (hackrf_error) hackrf_init();

	if (rc != HACKRF_SUCCESS)
	{
		qCritical("HackRFPlugin::SampleSourceDevices: failed to initiate HackRF library: %s", hackrf_error_name(rc));
	}




	for (i=0; i < hackrf_devices->devicecount; i++)
	{
		rc = (hackrf_error) hackrf_device_list_open(hackrf_devices, i, &hackrf_ptr);

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
			uint64_t serial_num = (((uint64_t) serial_msb)<<32) + serial_lsb;
			QString displayedName(QString("HackRF #%1 0x%2").arg(i).arg(serial_str));
			SimpleSerializer s(1);
			s.writeS32(1, i);
			s.writeString(2, serial_str);
			s.writeU64(3, serial_num);
			result.append(SampleSourceDevice(displayedName, m_deviceTypeID, s.final()));
			qDebug("HackRFPlugin::enumSampleSources: enumerated HackRF device #%d", i);

			hackrf_close(hackrf_ptr);
		}
		else
		{
			qDebug("HackRFPlugin::enumSampleSources: failed to enumerate HackRF device #%d: %s", i, hackrf_error_name(rc));
		}
	}

	rc = (hackrf_error) hackrf_exit();
	qDebug("HackRFPlugin::enumSampleSources: hackrf_exit: %s", hackrf_error_name(rc));

	return result;
}

PluginGUI* HackRFPlugin::createSampleSourcePluginGUI(const QString& sourceName, const QByteArray& address)
{
	if (!m_pluginAPI)
	{
		return 0;
	}

	if(sourceName == m_deviceTypeID)
	{
		HackRFGui* gui = new HackRFGui(m_pluginAPI);
		m_pluginAPI->setInputGUI(gui);
		return gui;
	}
	else
	{
		return 0;
	}
}
