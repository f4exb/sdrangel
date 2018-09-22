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

#include <QtGlobal>

#include "devicebladerf2.h"

DeviceBladeRF2::DeviceBladeRF2() :
    m_dev(0)
{}

DeviceBladeRF2::~DeviceBladeRF2()
{
    if (m_dev)
    {
        bladerf_close(m_dev);
        m_dev = 0;
    }
}

bool DeviceBladeRF2::open(const char *serial)
{
    int fpga_loaded;

    if ((m_dev = open_bladerf_from_serial(serial)) == 0)
    {
        qCritical("DeviceBladeRF2::open: could not open BladeRF");
        return false;
    }

    fpga_loaded = bladerf_is_fpga_configured(m_dev);

    if (fpga_loaded < 0)
    {
        qCritical("DeviceBladeRF2::open: failed to check FPGA state: %s",
                bladerf_strerror(fpga_loaded));
        return false;
    }
    else if (fpga_loaded == 0)
    {
        qCritical("DeviceBladeRF2::open: the device's FPGA is not loaded.");
        return false;
    }

    return true;
}

struct bladerf *DeviceBladeRF2::open_bladerf_from_serial(const char *serial)
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
        qCritical("DeviceBladeRF2::open_bladerf_from_serial: No devices available with serial %s", serial);
        return 0;
    }
    else if (status != 0)
    {
        qCritical("DeviceBladeRF2::open_bladerf_from_serial: Failed to open device with serial %s (%s)",
                serial, bladerf_strerror(status));
        return 0;
    }
    else
    {
        return dev;
    }
}

void DeviceBladeRF2::getFrequencyRangeRx(int& min, int& max, int& step)
{
    if (m_dev)
    {
        const struct bladerf_range *range;
        int status;

        status = bladerf_get_frequency_range(m_dev, BLADERF_CHANNEL_RX(0), &range);

        if (status < 0)
        {
            qCritical("DeviceBladeRF2::getFrequencyRangeRx: Failed to get Rx frequency range: %s",
                    bladerf_strerror(status));
        }
        else
        {
            min = range->min;
            max = range->max;
            step = range->step;
        }
    }
}

void DeviceBladeRF2::getFrequencyRangeTx(int& min, int& max, int& step)
{
    if (m_dev)
    {
        const struct bladerf_range *range;
        int status;

        status = bladerf_get_frequency_range(m_dev, BLADERF_CHANNEL_TX(0), &range);

        if (status < 0)
        {
            qCritical("DeviceBladeRF2::getFrequencyRangeTx: Failed to get Tx frequency range: %s",
                    bladerf_strerror(status));
        }
        else
        {
            min = range->min;
            max = range->max;
            step = range->step;
        }
    }
}

void DeviceBladeRF2::getSampleRateRangeRx(int& min, int& max, int& step)
{
    if (m_dev)
    {
        const struct bladerf_range *range;
        int status;

        status = bladerf_get_sample_rate_range(m_dev, BLADERF_CHANNEL_RX(0), &range);

        if (status < 0)
        {
            qCritical("DeviceBladeRF2::getSampleRateRangeRx: Failed to get Rx sample rate range: %s",
                    bladerf_strerror(status));
        }
        else
        {
            min = range->min;
            max = range->max;
            step = range->step;
        }
    }
}

void DeviceBladeRF2::getSampleRateRangeTx(int& min, int& max, int& step)
{
    if (m_dev)
    {
        const struct bladerf_range *range;
        int status;

        status = bladerf_get_sample_rate_range(m_dev, BLADERF_CHANNEL_TX(0), &range);

        if (status < 0)
        {
            qCritical("DeviceBladeRF2::getSampleRateRangeTx: Failed to get Tx sample rate range: %s",
                    bladerf_strerror(status));
        }
        else
        {
            min = range->min;
            max = range->max;
            step = range->step;
        }
    }

}

void DeviceBladeRF2::getBandwidthRangeRx(int& min, int& max, int& step)
{
    if (m_dev)
    {
        const struct bladerf_range *range;
        int status;

        status = bladerf_get_bandwidth_range(m_dev, BLADERF_CHANNEL_RX(0), &range);

        if (status < 0)
        {
            qCritical("DeviceBladeRF2::getBandwidthRangeRx: Failed to get Rx bandwidth range: %s",
                    bladerf_strerror(status));
        }
        else
        {
            min = range->min;
            max = range->max;
            step = range->step;
        }
    }
}

void DeviceBladeRF2::getBandwidthRangeTx(int& min, int& max, int& step)
{
    if (m_dev)
    {
        const struct bladerf_range *range;
        int status;

        status = bladerf_get_bandwidth_range(m_dev, BLADERF_CHANNEL_TX(0), &range);

        if (status < 0)
        {
            qCritical("DeviceBladeRF2::getBandwidthRangeTx: Failed to get Tx bandwidth range: %s",
                    bladerf_strerror(status));
        }
        else
        {
            min = range->min;
            max = range->max;
            step = range->step;
        }
    }
}

void DeviceBladeRF2::getGlobalGainRangeRx(int& min, int& max, int& step)
{
    if (m_dev)
    {
        const struct bladerf_range *range;
        int status;

        status = bladerf_get_gain_range(m_dev, BLADERF_CHANNEL_RX(0), &range);

        if (status < 0)
        {
            qCritical("DeviceBladeRF2::getGlobalGainRangeRx: Failed to get Rx global gain range: %s",
                    bladerf_strerror(status));
        }
        else
        {
            min = range->min;
            max = range->max;
            step = range->step;
        }
    }
}

void DeviceBladeRF2::getGlobalGainRangeTx(int& min, int& max, int& step)
{
    if (m_dev)
    {
        const struct bladerf_range *range;
        int status;

        status = bladerf_get_gain_range(m_dev, BLADERF_CHANNEL_TX(0), &range);

        if (status < 0)
        {
            qCritical("DeviceBladeRF2::getGlobalGainRangeTx: Failed to get Tx global gain range: %s",
                    bladerf_strerror(status));
        }
        else
        {
            min = range->min;
            max = range->max;
            step = range->step;
        }
    }
}
