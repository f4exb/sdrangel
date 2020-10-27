///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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
#include "deviceusrpparam.h"

bool DeviceUSRPParams::open(const char *deviceStr, bool channelNumOnly)
{
    qDebug("DeviceUSRPParams::open: %s", (const char *) deviceStr);

    std::string device_args(deviceStr);

    m_dev = uhd::usrp::multi_usrp::make(device_args);

    // Save information about what the radio supports

    m_nbRxChannels = m_dev->get_rx_num_channels();
    m_nbTxChannels = m_dev->get_tx_num_channels();

    // Speed up program initialisation, by not getting all properties
    // If we could find out number of channles without ::make ing the device
    // that would be even better
    if (!channelNumOnly)
    {
        m_lpfRangeRx = m_dev->get_rx_bandwidth_range();
        m_lpfRangeTx = m_dev->get_tx_bandwidth_range();

        m_loRangeRx = m_dev->get_fe_rx_freq_range();
        m_loRangeTx = m_dev->get_fe_tx_freq_range();

        // For some devices (B210), rx/tx_rates vary with master_clock_rate
        // Note master_clock_rate is rate between FPGA and RFIC
        // tx/rx_rate is rate between PC and FPGA
        uhd::meta_range_t clockRange = m_dev->get_master_clock_rate_range();
        if (clockRange.start() == clockRange.stop())
        {
            m_srRangeRx = m_dev->get_rx_rates();
            m_srRangeTx = m_dev->get_tx_rates();
        }
        else
        {
            // Find max and min sample rate, for max and min master clock rates
            m_dev->set_master_clock_rate(clockRange.start());
            uhd::meta_range_t rxLow = m_dev->get_rx_rates();
            uhd::meta_range_t txLow = m_dev->get_tx_rates();

            m_dev->set_master_clock_rate(clockRange.stop());
            uhd::meta_range_t rxHigh = m_dev->get_rx_rates();
            uhd::meta_range_t txHigh = m_dev->get_tx_rates();

            m_srRangeRx = uhd::meta_range_t(std::min(rxLow.start(), rxHigh.start()), std::max(rxLow.stop(), rxHigh.stop()));
            m_srRangeTx = uhd::meta_range_t(std::min(txLow.start(), txHigh.start()), std::max(txLow.stop(), txHigh.stop()));

            // Need to restore automatic clock rate
            uhd::property_tree::sptr properties = m_dev->get_device()->get_tree();
            if (properties->exists("/mboards/0/auto_tick_rate"))
            {
                properties->access<bool>("/mboards/0/auto_tick_rate").set(true);
            }
        }

        m_gainRangeRx = m_dev->get_rx_gain_range();
        m_gainRangeTx = m_dev->get_tx_gain_range();

        std::vector<std::string> txAntennas = m_dev->get_tx_antennas();
        m_txAntennas.reserve(txAntennas.size());
        for(size_t i = 0, l = txAntennas.size(); i < l; ++i)
            m_txAntennas << QString::fromStdString(txAntennas[i]);

        std::vector<std::string> rxAntennas = m_dev->get_rx_antennas();
        m_rxAntennas.reserve(rxAntennas.size());
        for(size_t i = 0, l = rxAntennas.size(); i < l; ++i)
            m_rxAntennas << QString::fromStdString(rxAntennas[i]);

        std::vector<std::string> rxGainNames = m_dev->get_rx_gain_names();
        m_rxGainNames.reserve(rxGainNames.size());
        for(size_t i = 0, l = rxGainNames.size(); i < l; ++i)
            m_rxGainNames << QString::fromStdString(rxGainNames[i]);

        std::vector<std::string> clockSources = m_dev->get_clock_sources(0);
        m_clockSources.reserve(clockSources.size());
        for(size_t i = 0, l = clockSources.size(); i < l; ++i)
            m_clockSources << QString::fromStdString(clockSources[i]);
    }

    return true;
}

void DeviceUSRPParams::close()
{
    m_dev = nullptr;
}
