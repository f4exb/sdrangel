///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2017 Edouard Griffiths, F4EXB                              //
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

#include <cstdio>
#include <cstring>
#include <algorithm>

#include <QtGlobal>

#include "devicebladerf2.h"

DeviceBladeRF2::DeviceBladeRF2() :
    m_dev(0),
    m_nbRxChannels(0),
    m_nbTxChannels(0),
    m_rxOpen(0),
    m_txOpen(0)
{}

DeviceBladeRF2::~DeviceBladeRF2()
{
    if (m_dev)
    {
        bladerf_close(m_dev);
        m_dev = 0;
    }

    if (m_rxOpen) {
        delete[] m_rxOpen;
    }

    if (m_txOpen) {
        delete[] m_txOpen;
    }
}

void DeviceBladeRF2::enumOriginDevices(const QString& hardwareId, PluginInterface::OriginDevices& originDevices)
{
    struct bladerf_devinfo *devinfo = 0;

    int count = bladerf_get_device_list(&devinfo);

    if (devinfo)
    {
        for(int i = 0; i < count; i++)
        {
            struct bladerf *dev;

            int status = bladerf_open_with_devinfo(&dev, &devinfo[i]);

            if (status == BLADERF_ERR_NODEV)
            {
                qCritical("DeviceBladeRF2::enumOriginDevices: No device at index %d", i);
                continue;
            }
            else if (status != 0)
            {
                qCritical("DeviceBladeRF2::enumOriginDevices: Failed to open device at index %d", i);
                continue;
            }

            const char *boardName = bladerf_get_board_name(dev);

            if (strcmp(boardName, "bladerf2") == 0)
            {
                unsigned int nbRxChannels = bladerf_get_channel_count(dev, BLADERF_RX);
                unsigned int nbTxChannels = bladerf_get_channel_count(dev, BLADERF_TX);
                // make the stream index a placeholder for future arg() hence the arg("%1")
                QString displayableName(QString("BladeRF2[%1:$1] %2").arg(devinfo[i].instance).arg(devinfo[i].serial));

                originDevices.append(PluginInterface::OriginDevice(
                    displayableName,
                    hardwareId,
                    QString(devinfo[i].serial),
                    i, // Sequence
                    nbRxChannels,
                    nbTxChannels
                ));
            }

            bladerf_close(dev);
        }

        bladerf_free_device_list(devinfo); // Valgrind memcheck
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

    m_nbRxChannels = bladerf_get_channel_count(m_dev, BLADERF_RX);
    m_nbTxChannels = bladerf_get_channel_count(m_dev, BLADERF_TX);

    m_rxOpen = new bool[m_nbRxChannels];
    m_txOpen = new bool[m_nbTxChannels];
    std::fill(m_rxOpen, m_rxOpen + m_nbRxChannels, false);
    std::fill(m_txOpen, m_txOpen + m_nbTxChannels, false);

    return true;
}

void DeviceBladeRF2::close()
{
    if (m_dev)
    {
        bladerf_close(m_dev);
        m_dev = 0;
    }
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

bool DeviceBladeRF2::openRx(int channel)
{
    if (!m_dev) {
        return false;
    }

    if ((channel < 0) || (channel >= m_nbRxChannels))
    {
        qCritical("DeviceBladeRF2::openRx: invalid Rx channel index %d", channel);
        return false;
    }

    int status;

    if (!m_rxOpen[channel])
    {
        status = bladerf_enable_module(m_dev, BLADERF_CHANNEL_RX(channel), true);

        if (status < 0)
        {
            qCritical("DeviceBladeRF2::openRx: failed to enable Rx channel %d: %s",
                    channel, bladerf_strerror(status));
            return false;
        }
        else
        {
            qDebug("DeviceBladeRF2::openRx: Rx channel %d enabled", channel);
            m_rxOpen[channel] = true;
            return true;
        }
    }
    else
    {
        qDebug("DeviceBladeRF2::openRx: Rx channel %d already opened", channel);
        return true;
    }
}

bool DeviceBladeRF2::openTx(int channel)
{
    if (!m_dev) {
        return false;
    }

    if ((channel < 0) || (channel >= m_nbTxChannels))
    {
        qCritical("DeviceBladeRF2::openTx: invalid Tx channel index %d", channel);
        return false;
    }

    int status;

    if (!m_txOpen[channel])
    {
        status = bladerf_enable_module(m_dev, BLADERF_CHANNEL_TX(channel), true);

        if (status < 0)
        {
            qCritical("DeviceBladeRF2::openTx: Failed to enable Tx channel %d: %s",
                    channel, bladerf_strerror(status));
            return false;
        }
        else
        {
            qDebug("DeviceBladeRF2::openTx: Tx channel %d enabled", channel);
            m_txOpen[channel] = true;
            return true;
        }
    }
    else
    {
        qDebug("DeviceBladeRF2::openTx: Tx channel %d already opened", channel);
        return true;
    }
}

void DeviceBladeRF2::closeRx(int channel)
{
    if (!m_dev) {
        return;
    }

    if ((channel < 0) || (channel >= m_nbRxChannels))
    {
        qCritical("DeviceBladeRF2::closeRx: invalid Rx channel index %d", channel);
        return;
    }

    if (m_rxOpen[channel])
    {
        int status = bladerf_enable_module(m_dev, BLADERF_CHANNEL_RX(channel), false);
        m_rxOpen[channel] = false;

        if (status < 0) {
            qCritical("DeviceBladeRF2::closeRx: failed to disable Rx channel %d: %s", channel, bladerf_strerror(status));
        } else {
            qDebug("DeviceBladeRF2::closeRx: Rx channel %d disabled", channel);
        }
    }
    else
    {
        qDebug("DeviceBladeRF2::closeRx: Rx channel %d already closed", channel);
    }
}

void DeviceBladeRF2::closeTx(int channel)
{
    if (!m_dev) {
        return;
    }

    if ((channel < 0) || (channel >= m_nbTxChannels))
    {
        qCritical("DeviceBladeRF2::closeTx: invalid Tx channel index %d", channel);
        return;
    }

    if (m_txOpen[channel])
    {
        int status = bladerf_enable_module(m_dev, BLADERF_CHANNEL_TX(channel), false);
        m_txOpen[channel] = false;

        if (status < 0) {
            qCritical("DeviceBladeRF2::closeTx: failed to disable Tx channel %d: %s", channel, bladerf_strerror(status));
        } else {
            qDebug("DeviceBladeRF2::closeTx: Tx channel %d disabled", channel);
        }
    }
    else
    {
        qDebug("DeviceBladeRF2::closeTx: Rx channel %d already closed", channel);
    }
}

void DeviceBladeRF2::getFrequencyRangeRx(uint64_t& min, uint64_t& max, int& step, float& scale)
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
            scale = range->scale;
        }
    }
}

void DeviceBladeRF2::getFrequencyRangeTx(uint64_t& min, uint64_t& max, int& step, float& scale)
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
            scale = range->scale;
        }
    }
}

void DeviceBladeRF2::getSampleRateRangeRx(int& min, int& max, int& step, float& scale)
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
            scale = range->scale;
        }
    }
}

void DeviceBladeRF2::getSampleRateRangeTx(int& min, int& max, int& step, float& scale)
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
            scale = range->scale;
        }
    }

}

void DeviceBladeRF2::getBandwidthRangeRx(int& min, int& max, int& step, float& scale)
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
            scale = range->scale;
        }
    }
}

void DeviceBladeRF2::getBandwidthRangeTx(int& min, int& max, int& step, float& scale)
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
            scale = range->scale;
        }
    }
}

void DeviceBladeRF2::getGlobalGainRangeRx(int& min, int& max, int& step, float& scale)
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
            scale = range->scale;
        }
    }
}

void DeviceBladeRF2::getGlobalGainRangeTx(int& min, int& max, int& step, float& scale)
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
            scale = range->scale;
            qDebug("DeviceBladeRF2::getGlobalGainRangeTx: min: %d max: %d step: %d scale: %f",
                min, max, step, scale);
        }
    }
}

int DeviceBladeRF2::getGainModesRx(const bladerf_gain_modes **modes)
{
    if (m_dev)
    {
        // int n = bladerf_get_gain_modes(m_dev, BLADERF_CHANNEL_RX(0), 0); // does not work anymore with libbladerf 2.2.1

        // if (n < 0)
        // {
        //     qCritical("DeviceBladeRF2::getGainModesRx: Failed to get the number of Rx gain modes: %s", bladerf_strerror(n));
        //     return 0;
        // }

        int status = bladerf_get_gain_modes(m_dev, BLADERF_CHANNEL_RX(0), modes);

        if (status < 0)
        {
            qCritical("DeviceBladeRF2::getGainModesRx: Failed to get Rx gain modes: %s", bladerf_strerror(status));
            return 0;
        }
        else
        {
            return status; // This is the number of gain modes (libbladerf 2.2.1)
        }
    }
    else
    {
        return 0;
    }
}

void DeviceBladeRF2::setBiasTeeRx(bool enable)
{
    if (m_dev)
    {
        int status = bladerf_set_bias_tee(m_dev, BLADERF_CHANNEL_RX(0), enable);

        if (status < 0) {
            qCritical("DeviceBladeRF2::setBiasTeeRx: Failed to set Rx bias tee: %s", bladerf_strerror(status));
        }
    }
}

void DeviceBladeRF2::setBiasTeeTx(bool enable)
{
    if (m_dev)
    {
        int status = bladerf_set_bias_tee(m_dev, BLADERF_CHANNEL_TX(0), enable);

        if (status < 0) {
            qCritical("DeviceBladeRF2::setBiasTeeTx: Failed to set Tx bias tee: %s", bladerf_strerror(status));
        }
    }
}
