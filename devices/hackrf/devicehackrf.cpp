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
#include <QDebug>

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

void DeviceHackRF::setDevicePPMCorrection(hackrf_device *dev, qint32 loPPMTenths)
{
    if (!dev) {
        return;
    }

    hackrf_error rc = HACKRF_SUCCESS;
    const uint32_t msnaRegBase = 26; // Multisynth NA config register base
    const int32_t msnaFreq = 800000000; // Multisynth NA target frequency
    int32_t xo = 25000000; //Crystal frequency
    int32_t a; // Multisynth NA XTAL multiplier integer 32 * 25mhz = 800mhz
    int32_t b; // Multisynth NA XTAL multiplier fractional numerator  0 to 1048575
    int32_t c; // Multisynth NA XTAL multiplier fractional denominator 1048575 max resolution
    int64_t rem;
    int32_t p1, p2, p3; // raw register values

    xo = xo - xo/1000000*loPPMTenths/10; //adjust crystal freq by ppm error
    a = msnaFreq / xo; //multiplier integer
    rem = msnaFreq % xo; // multiplier remainder

    if (rem)
    { //fraction mode
        b = ((rem * 10485750)/xo +5) /10; //multiplier fractional numerator with rounding
        c = 1048575; //multiplier fractional divisor
        rc = (hackrf_error) hackrf_si5351c_write(dev, 22, 128); // MSNA set fractional mode
        qDebug() << "DeviceHackRF::setDevicePPMCorrection: si5351c MSNA set to fraction mode.";
    }
    else
    { //integer mode
        b = 0;
        c = 1;
        rc = (hackrf_error) hackrf_si5351c_write(dev, 22, 0); // MSNA set integer mode
        qDebug() << "DeviceHackRF::setDevicePPMCorrection: si5351c MSNA set to integer mode.";
    }

    qDebug() << "DeviceHackRF::setDevicePPMCorrection: si5351c MSNA rem" << rem;
    qDebug() << "DeviceHackRF::setDevicePPMCorrection: si5351c MSNA xoppm" << loPPMTenths / 10.0f;
    qDebug() << "DeviceHackRF::setDevicePPMCorrection: si5351c MSNA xo" << xo;
    qDebug() << "DeviceHackRF::setDevicePPMCorrection: si5351c MSNA a" << a;
    qDebug() << "DeviceHackRF::setDevicePPMCorrection: si5351c MSNA b" << b;
    qDebug() << "DeviceHackRF::setDevicePPMCorrection: si5351c MSNA c" << c;

    p1 = 128*a + (128 * b/c) - 512;
    p2 = (128*b) % c;
    p3 = c;

    if (rc == HACKRF_SUCCESS) {
        rc = (hackrf_error) hackrf_si5351c_write(dev,msnaRegBase, (p3 >> 8) & 0xFF); // reg 26 MSNA_P3[15:8]
    }
    if (rc == HACKRF_SUCCESS) {
        rc = (hackrf_error) hackrf_si5351c_write(dev, msnaRegBase + 1, p3 & 0xFF); // reg 27 MSNA_P3[7:0]
    }
    if (rc == HACKRF_SUCCESS) {
        rc = (hackrf_error) hackrf_si5351c_write(dev, msnaRegBase + 2, (p1 >> 16) & 0x3); // reg28 bits 1:0  MSNA_P1[17:16]
    }
    if (rc == HACKRF_SUCCESS) {
        rc = (hackrf_error) hackrf_si5351c_write(dev, msnaRegBase + 3, (p1 >> 8) & 0xFF); // reg 29 MSNA_P1[15:8]
    }
    if (rc == HACKRF_SUCCESS) {
        rc = (hackrf_error) hackrf_si5351c_write(dev, msnaRegBase + 4, p1 & 0xFF); // reg 30 MSNA_P1[7:0]
    }
    if (rc == HACKRF_SUCCESS) {
        rc = (hackrf_error) hackrf_si5351c_write(dev, msnaRegBase + 5, ((p3 & 0xF0000) >> 12) | ((p2 >> 16) & 0xF)); // bits 7:4 MSNA_P3[19:16], reg31 bits 3:0 MSNA_P2[19:16]
    }
    if (rc == HACKRF_SUCCESS) {
        rc = (hackrf_error) hackrf_si5351c_write(dev, msnaRegBase + 6, (p2 >> 8) & 0xFF); // reg 32 MSNA_P2[15:8]
    }
    if (rc == HACKRF_SUCCESS) {
        rc = (hackrf_error) hackrf_si5351c_write(dev, msnaRegBase + 7, p2 & 0xFF); // reg 33 MSNA_P2[7:0]
    }

    if (rc != HACKRF_SUCCESS)
    {
        qDebug("DeviceHackRF::setDevicePPMCorrection: XTAL error adjust failed: %s", hackrf_error_name(rc));
    }
    else
    {
        qDebug() << "DeviceHackRF::setDevicePPMCorrection: si5351c MSNA registers"
        << msnaRegBase << "<-"  << ((p3 >> 8) & 0xFF)
        << (msnaRegBase + 1) << "<-" << (p3 & 0xFF)
        << (msnaRegBase + 2) << "<-" << ((p1 >> 16) & 0x3)
        << (msnaRegBase + 3) << "<-" << ((p1 >> 8) & 0xFF)
        << (msnaRegBase + 4) << "<-" << (p1 & 0xFF)
        << (msnaRegBase + 5) << "<-" << (((p3 & 0xF0000) >> 12) | ((p2 >> 16) & 0xF))
        << (msnaRegBase + 6) << "<-" << ((p2 >> 8) & 0xFF)
        << (msnaRegBase + 7) << "<-" << (p2 & 0xFF);
        qDebug() << "DeviceHackRF::setDevicePPMCorrection: XTAL error adjusted by" << (loPPMTenths / 10.0f) << "PPM.";
    }
}
