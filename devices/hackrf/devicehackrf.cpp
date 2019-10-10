///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <stdio.h>
#include <QtGlobal>
#include "devicehackrf.h"

DeviceHackRF::DeviceHackRF()
{
    hackrf_error rc = (hackrf_error) hackrf_init();

    if (rc != HACKRF_SUCCESS) {
        qCritical("DeviceHackRF::open_hackrf: failed to initiate HackRF library %s", hackrf_error_name(rc));
    }
}

DeviceHackRF::~DeviceHackRF()
{
    hackrf_exit();
}

DeviceHackRF& DeviceHackRF::instance()
{
    static DeviceHackRF inst;
    return inst;
}

hackrf_device *DeviceHackRF::open_hackrf(int sequence)
{
    instance();

    return open_hackrf_from_sequence(sequence);
}

hackrf_device *DeviceHackRF::open_hackrf(const char * const serial)
{
    hackrf_error rc;
    hackrf_device *hackrf_ptr;

    instance();

    rc = (hackrf_error) hackrf_open_by_serial(serial, &hackrf_ptr);

    if (rc == HACKRF_SUCCESS)
    {
        return hackrf_ptr;
    }
    else
    {
        qCritical("DeviceHackRF::open_hackrf: error #%d: %s", (int) rc, hackrf_error_name(rc));
        return 0;
    }
}

hackrf_device *DeviceHackRF::open_hackrf_from_sequence(int sequence)
{
    instance();

    hackrf_device_list_t *hackrf_devices = hackrf_device_list();

    if (hackrf_devices == nullptr) {
        return nullptr;
    }

    hackrf_device *hackrf_ptr;
    hackrf_error rc;

    rc = (hackrf_error) hackrf_device_list_open(hackrf_devices, sequence, &hackrf_ptr);
    hackrf_device_list_free(hackrf_devices);

    if (rc == HACKRF_SUCCESS)
    {
        return hackrf_ptr;
    }
    else
    {
        qCritical("DeviceHackRF::open_hackrf_from_sequence: error #%d: %s", (int) rc, hackrf_error_name(rc));
        return 0;
    }
}

void DeviceHackRF::enumOriginDevices(const QString& hardwareId, PluginInterface::OriginDevices& originDevices)
{
    instance();

	hackrf_device_list_t *hackrf_devices = hackrf_device_list();

    if (hackrf_devices == nullptr) {
        return;
    }

	hackrf_device *hackrf_ptr;
	read_partid_serialno_t read_partid_serialno;
	int i;

	for (i=0; i < hackrf_devices->devicecount; i++)
	{
	    hackrf_error rc = (hackrf_error) hackrf_device_list_open(hackrf_devices, i, &hackrf_ptr);

		if (rc == HACKRF_SUCCESS)
		{
			qDebug("DeviceHackRF::enumOriginDevices: try to enumerate HackRF device #%d", i);

			rc = (hackrf_error) hackrf_board_partid_serialno_read(hackrf_ptr, &read_partid_serialno);

			if (rc != HACKRF_SUCCESS)
			{
				qDebug("DeviceHackRF::enumOriginDevices: failed to read serial no: %s", hackrf_error_name(rc));
				hackrf_close(hackrf_ptr);
				continue; // next
			}

			uint32_t serial_msb = read_partid_serialno.serial_no[2];
			uint32_t serial_lsb = read_partid_serialno.serial_no[3];

			QString serial_str = QString::number(serial_msb, 16) + QString::number(serial_lsb, 16);
			//uint64_t serial_num = (((uint64_t) serial_msb)<<32) + serial_lsb;
			QString displayedName(QString("HackRF[%1] %2").arg(i).arg(serial_str));

			originDevices.append(PluginInterface::OriginDevice(
				displayedName,
				hardwareId,
				serial_str,
				i,
				1,
				1
			));

			qDebug("DeviceHackRF::enumOriginDevices: enumerated HackRF device #%d", i);

			hackrf_close(hackrf_ptr);
		}
		else
		{
			qDebug("DeviceHackRF::enumOriginDevices: failed to enumerate HackRF device #%d: %s", i, hackrf_error_name(rc));
		}
	}

	hackrf_device_list_free(hackrf_devices);
}

