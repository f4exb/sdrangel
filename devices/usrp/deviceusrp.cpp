///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020, 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>          //
// Copyright (C) 2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>         //
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

#include <QDebug>

#include <cstdio>
#include <cstring>
#include <cmath>
#include <regex>
#include <algorithm>
#include <thread>

#include <uhd/types/device_addr.hpp>
#include <uhd/utils/thread.hpp>

#include "util/poweroftwo.h"

#include "deviceusrpparam.h"
#include "deviceusrp.h"

void DeviceUSRP::enumOriginDevices(const QString& hardwareId, PluginInterface::OriginDevices& originDevices)
{
    try
    {
        uhd::device_addr_t hint; // Discover all devices
        uhd::device_addrs_t dev_addrs = uhd::device::find(hint);

        if (dev_addrs.size() <= 0)
        {
            qDebug("DeviceUSRP::enumOriginDevices: Could not find any USRP device");
            return;
        }

        for(unsigned i = 0; i != dev_addrs.size(); i++)
        {
            QString id = QString::fromStdString(dev_addrs[i].to_string());
            QString name = QString::fromStdString(dev_addrs[i].get("name", "N/A"));
            QString serial = QString::fromStdString(dev_addrs[i].get("serial", "N/A"));
            QString product = QString::fromStdString(dev_addrs[i].get("product", "N/A"));
            QString displayedName(QString("%1[%2:$1] %3").arg(name).arg(i).arg(serial));

            qDebug() << "DeviceUSRP::enumOriginDevices: found USRP device " << displayedName;

            // Opening some devices can be a little slow, so use hardcoded number of channels,
            // for known devices
            static const QMap<QString, int> channelMap{{"B200", 1}, {"B205", 1}, {"B200mini", 1}, {"B205mini", 1}, {"B210", 2}};
            if (channelMap.contains(product))
            {
                originDevices.append(PluginInterface::OriginDevice(
                        displayedName,
                        hardwareId,
                        id,
                        (int)i,
                        channelMap[product],
                        channelMap[product]
                    ));
            }
            else
            {
                DeviceUSRPParams usrpParams;
                usrpParams.open(id, true);
                usrpParams.close();

                originDevices.append(PluginInterface::OriginDevice(
                        displayedName,
                        hardwareId,
                        id,
                        (int)i,
                        usrpParams.m_nbRxChannels,
                        usrpParams.m_nbTxChannels
                    ));
            }
        }
    }
    catch (const std::exception& e)
    {
        qDebug() << "DeviceUSRP::enumOriginDevices: exception: " << e.what();
    }
}

void DeviceUSRP::waitForLock(uhd::usrp::multi_usrp::sptr usrp, const QString& clockSource, int channel, bool rx)
{
    int tries;
    const int maxTries = 100;

    try
    {
        // Wait for Ref lock - ref_locked is a motherboard sensor
        if (clockSource != "internal")
        {
            std::vector<std::string> mboardSensorNames = usrp->get_mboard_sensor_names(0);
            if (std::find(mboardSensorNames.begin(), mboardSensorNames.end(), "ref_locked") != mboardSensorNames.end())
            {
                for (tries = 0; !usrp->get_mboard_sensor("ref_locked", 0).to_bool() && (tries < maxTries); tries++)
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                if (tries == maxTries)
                    qCritical("DeviceUSRP::waitForLock: Failed to lock ref");
            }
        }

        // Wait for LO lock
        std::vector<std::string> sensorNames = rx ? usrp->get_rx_sensor_names(channel) : usrp->get_tx_sensor_names(channel);
        if (std::find(sensorNames.begin(), sensorNames.end(), "lo_locked") != sensorNames.end())
        {
            for (tries = 0; !(rx ? usrp->get_rx_sensor("lo_locked", channel) : usrp->get_tx_sensor("lo_locked", channel)).to_bool() && (tries < maxTries); tries++)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (tries == maxTries)
                qCritical("DeviceUSRP::waitForLock: Failed to lock %s LO", rx ? "Rx" : "Tx");
        }
    }
    catch (const std::exception& e)
    {
        qWarning() << "DeviceUSRP::waitForLock: exception: " << e.what();
    }
}

bool DeviceUSRP::channelsShareLO(uhd::usrp::multi_usrp::sptr usrp, bool rx, int channelA, int channelB)
{
    if (!usrp || (channelA < 0) || (channelB < 0)) {
        return false;
    }
    if (channelA == channelB) {
        return true;
    }

    try
    {
        // Assume channels on the same daughterboard share an LO (E.g. B210/E310 "A:A A:B", N310 "A:0 A:1"),
        // whereas channels on different daughterboards do not (E.g. X310 "A:0 B:0")
        if (usrp->get_num_mboards() != 1) {
            return false;
        }

        uhd::usrp::subdev_spec_t spec = rx ? usrp->get_rx_subdev_spec(0) : usrp->get_tx_subdev_spec(0);

        if (((size_t) channelA >= spec.size()) || ((size_t) channelB >= spec.size())) {
            return false;
        }

        return spec[channelA].db_name == spec[channelB].db_name;
    }
    catch (const std::exception& e)
    {
        qWarning() << "DeviceUSRP::channelsShareLO: exception: " << e.what();
        return false;
    }
}

bool DeviceUSRP::getTemperature(uhd::usrp::multi_usrp::sptr usrp, bool rx, int channel, double& temperature)
{
    if (!usrp) {
        return false;
    }

    auto contains = [](const std::vector<std::string>& names, const std::string& name) {
        return std::find(names.begin(), names.end(), name) != names.end();
    };

    try
    {
        // Motherboard sensor (E.g. E3xx, N3xx)
        if (contains(usrp->get_mboard_sensor_names(0), "temp"))
        {
            temperature = usrp->get_mboard_sensor("temp", 0).to_real();
            return true;
        }

        // Frontend sensor (E.g. B2xx, where it is the AD9361 temperature)
        if ((channel >= 0) && contains(rx ? usrp->get_rx_sensor_names(channel) : usrp->get_tx_sensor_names(channel), "temp"))
        {
            temperature = (rx ? usrp->get_rx_sensor("temp", channel) : usrp->get_tx_sensor("temp", channel)).to_real();
            return true;
        }

        // Frontend sensor in other direction (E.g. B2xx only has temperature sensor on Rx frontend)
        if ((rx ? usrp->get_tx_num_channels() : usrp->get_rx_num_channels()) > 0)
        {
            if (contains(rx ? usrp->get_tx_sensor_names(0) : usrp->get_rx_sensor_names(0), "temp"))
            {
                temperature = (rx ? usrp->get_tx_sensor("temp", 0) : usrp->get_rx_sensor("temp", 0)).to_real();
                return true;
            }
        }
    }
    catch (const std::exception& e)
    {
        qDebug() << "DeviceUSRP::getTemperature: exception: " << e.what();
    }

    return false;
}

// Read about 1ms of samples per recv() call, as reading a single packet (E.g. ~1000 samples)
// per call results in a high per call overhead at high sample rates (E.g. FIFO locking and signalling).
// Must be at least the maximum number of samples in a packet and a power of two (for the decimators, see #1161).
// Upper limit avoids too large a latency if the sample rate is reduced while running.
size_t DeviceUSRP::getRecvBufferSamples(size_t maxNumSamps, int sampleRate)
{
    const size_t maxSamples = 16384;
    size_t minSamples = isPowerOfTwo(maxNumSamps) ? maxNumSamps : lowerPowerOfTwo(maxNumSamps);
    size_t samples = sampleRate > 0 ? lowerPowerOfTwo((uint32_t) (sampleRate / 1000)) : minSamples;

    samples = std::min(samples, maxSamples);
    samples = std::max(samples, minSamples);

    return samples;
}

// Set priority of streaming thread higher than normal, so it isn't delayed by other threads (E.g. GUI or DSP),
// which can cause overflows / underflows, as UHD only buffers a few ms of samples by default
void DeviceUSRP::setStreamingThreadPriority()
{
    if (!uhd::set_thread_priority_safe()) {
        qDebug("DeviceUSRP::setStreamingThreadPriority: Failed to set thread priority");
    }
}

// When UHD chooses the master clock rate automatically (B2xx), each time a channel's sample rate is set,
// it chooses one that's a multiple of the LCM of the sample rates of all channels (Rx and Tx), including
// those that haven't been changed yet. So when changing the sample rate of all channels, if no supported
// clock rate is a multiple of both the old and new rates (E.g. 8 and 9 MS/s), the clock rate is left unchanged,
// and the sample rate is coerced to what can be achieved with it (E.g. 8 MS/s).
// To avoid this, set the master clock rate for the new sample rate first, using the same rules as UHD.
bool DeviceUSRP::setMasterClockRateForSampleRate(uhd::usrp::multi_usrp::sptr usrp, double sampleRate, size_t nbChannels)
{
    const std::string autoTickRatePath = "/mboards/0/auto_tick_rate";
    uhd::property_tree::sptr properties = usrp->get_device()->get_tree();

    if (!properties->exists(autoTickRatePath) || !properties->access<bool>(autoTickRatePath).get()) {
        return false; // Master clock rate set manually
    }

    uhd::meta_range_t clockRange = usrp->get_master_clock_rate_range();
    const double minClockRate = clockRange.start();
    const double maxClockRate = clockRange.stop() / std::max(nbChannels, (size_t) 1);

    if ((sampleRate <= 0.0) || (sampleRate > maxClockRate)) {
        return false; // Let UHD report the error when the sample rate is set
    }

    // Highest power of 2 multiple of the sample rate that doesn't exceed the max clock rate (See ad936x_manager::get_auto_tick_rate)
    int multiplier = 1 << (int) std::log2(maxClockRate / sampleRate);
    if ((multiplier == 2) && (sampleRate >= minClockRate)) {
        multiplier = 1;
    }
    const double clockRate = sampleRate * multiplier;

    // Disable automatic selection while sample rates are set, even if the clock rate is already correct,
    // as otherwise it could be changed when the first channel is set
    properties->access<bool>(autoTickRatePath).set(false);

    if (std::abs(usrp->get_master_clock_rate() - clockRate) >= 1.0)
    {
        qDebug("DeviceUSRP::setMasterClockRateForSampleRate: setting master clock rate to %f for sample rate %f", clockRate, sampleRate);
        usrp->set_master_clock_rate(clockRate);
    }

    return true;
}

void DeviceUSRP::enableAutoMasterClockRate(uhd::usrp::multi_usrp::sptr usrp)
{
    usrp->get_device()->get_tree()->access<bool>("/mboards/0/auto_tick_rate").set(true);
}

void DeviceUSRP::setSampleRate(uhd::usrp::multi_usrp::sptr usrp, bool rx, int channel, double sampleRate)
{
    auto set = [&]() {
        if (rx) {
            usrp->set_rx_rate(sampleRate, channel);
        } else {
            usrp->set_tx_rate(sampleRate, channel);
        }
    };

    // Let UHD try first, as it may find a master clock rate that suits both this and other channels' rates
    set();

    double actualRate = rx ? usrp->get_rx_rate(channel) : usrp->get_tx_rate(channel);

    // Single channel streams, so 1 channel for clock rate limit, as UHD uses
    if ((std::abs(actualRate - sampleRate) >= 1.0) && setMasterClockRateForSampleRate(usrp, sampleRate, 1))
    {
        qDebug("DeviceUSRP::setSampleRate: %s channel %d: rate coerced to %f - setting master clock rate for %f",
            rx ? "Rx" : "Tx", channel, actualRate, sampleRate);

        try
        {
            set();
        }
        catch (...)
        {
            enableAutoMasterClockRate(usrp);
            throw;
        }

        enableAutoMasterClockRate(usrp);
    }
}

// When the master clock rate changes, UHD (B2xx) reprograms each channel's DSP for its previously set rate,
// which may be coerced to a different rate, but get_rx/tx_rate() still return the previous rate.
// So find the rate the DSP is actually using, which is the nearest rate achievable with the current clock rate.
double DeviceUSRP::getSampleRate(uhd::usrp::multi_usrp::sptr usrp, bool rx, int channel)
{
    if (rx) {
        return usrp->get_rx_rates(channel).clip(usrp->get_rx_rate(channel), true);
    } else {
        return usrp->get_tx_rates(channel).clip(usrp->get_tx_rate(channel), true);
    }
}
