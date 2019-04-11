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
    hackrf_device_list_t *hackrf_devices = hackrf_device_list();
    hackrf_device *hackrf_ptr;
    hackrf_error rc;

    instance();

    rc = (hackrf_error) hackrf_device_list_open(hackrf_devices, sequence, &hackrf_ptr);

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



