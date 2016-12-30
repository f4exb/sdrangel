///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2017 Edouard Griffiths, F4EXB                              //
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

#include <cstdio>
#include <cstring>
#include "devicebladerf.h"

bool DeviceBladeRF::open_bladerf(struct bladerf **dev, const char *serial)
{
    int res;
    int fpga_loaded;

    if ((*dev = open_bladerf_from_serial(0)) == 0)
    {
        fprintf(stderr, "DeviceBladeRF::open_bladerf: could not open BladeRF");
        return false;
    }

    fpga_loaded = bladerf_is_fpga_configured(*dev);

    if (fpga_loaded < 0)
    {
        fprintf(stderr, "DeviceBladeRF::open_bladerf: failed to check FPGA state: %s",
                  bladerf_strerror(fpga_loaded));
        return false;
    }
    else if (fpga_loaded == 0)
    {
        fprintf(stderr, "BladerfOutput::start: the device's FPGA is not loaded.");
        return false;
    }

    return true;
}

struct bladerf *DeviceBladeRF::open_bladerf_from_serial(const char *serial)
{
    int status;
    struct bladerf *dev;
    struct bladerf_devinfo info;

    /* Initialize all fields to "don't care" wildcard values.
     *
     * Immediately passing this to bladerf_open_with_devinfo() would cause
     * libbladeRF to open any device on any available backend. */
    bladerf_init_devinfo(&info);

    /* Specify the desired device's serial number, while leaving all other
     * fields in the info structure wildcard values */
    if (serial != 0)
    {
        strncpy(info.serial, serial, BLADERF_SERIAL_LENGTH - 1);
        info.serial[BLADERF_SERIAL_LENGTH - 1] = '\0';
    }

    status = bladerf_open_with_devinfo(&dev, &info);

    if (status == BLADERF_ERR_NODEV)
    {
        fprintf(stderr, "DeviceBladeRF::open_bladerf_from_serial: No devices available with serial=%s\n", serial);
        return 0;
    }
    else if (status != 0)
    {
        fprintf(stderr, "DeviceBladeRF::open_bladerf_from_serial: Failed to open device with serial=%s (%s)\n",
                serial, bladerf_strerror(status));
        return 0;
    }
    else
    {
        return dev;
    }
}


