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

bool DeviceUSRPParams::open(const QString &deviceStr, bool channelNumOnly)
{
    qDebug("DeviceUSRPParams::open: %s", qPrintable(deviceStr));

    try
    {
        std::string device_args(qPrintable(deviceStr));

        // For USB
        // The recv_frame_size must be a multiple of 8 bytes and not a multiple of 1024 bytes.
        // recv_frame_size max is 16360.
        //m_dev = uhd::usrp::multi_usrp::make(device_args + ",recv_frame_size=16392");
        m_dev = uhd::usrp::multi_usrp::make(device_args);

        // Save information about what the radio supports

        m_nbRxChannels = m_dev->get_rx_num_channels();
        m_nbTxChannels = m_dev->get_tx_num_channels();
        qDebug() << "DeviceUSRPParams::open: m_nbRxChannels: " << m_nbRxChannels << " m_nbTxChannels: " << m_nbTxChannels;

        // Speed up program initialisation, by not getting all properties
        // If we could find out number of channles without ::make ing the device
        // that would be even better
        if (!channelNumOnly)
        {
            if (m_nbRxChannels > 0)
            {
                m_lpfRangeRx = m_dev->get_rx_bandwidth_range();
                m_loRangeRx = m_dev->get_fe_rx_freq_range();
            }
            if (m_nbTxChannels > 0)
            {
                m_lpfRangeTx = m_dev->get_tx_bandwidth_range();
                m_loRangeTx = m_dev->get_fe_tx_freq_range();
            }

            // For some devices (B210), rx/tx_rates vary with master_clock_rate
            // which can be set automatically by UHD. For other devices,
            // master_clock_rate must be set manually (currently as a device arg)
            // Note master_clock_rate is rate between FPGA and RFIC
            // tx/rx_rate is rate between PC and FPGA
            uhd::meta_range_t clockRange = m_dev->get_master_clock_rate_range();
            uhd::property_tree::sptr properties = m_dev->get_device()->get_tree();
            if ((clockRange.start() == clockRange.stop()) || !properties->exists("/mboards/0/auto_tick_rate"))
            {
                if (m_nbRxChannels > 0) {
                    m_srRangeRx = m_dev->get_rx_rates();
                }
                if (m_nbTxChannels > 0) {
                    m_srRangeTx = m_dev->get_tx_rates();
                }
            }
            else if (deviceStr.contains("product=B210"))
            {
                // Auto-calculation below can be slow, so use hardcoded values for B210
                m_srRangeRx = uhd::meta_range_t(1e5, 61.444e6);
                m_srRangeTx = uhd::meta_range_t(1e5, 61.444e6);
            }
            else
            {
                // Find max and min sample rate, for max and min master clock rates
                m_dev->set_master_clock_rate(clockRange.start());
                uhd::meta_range_t rxLow;
                uhd::meta_range_t txLow;

                if (m_nbRxChannels > 0) {
                    rxLow = m_dev->get_rx_rates();
                }
                if (m_nbTxChannels > 0) {
                    txLow = m_dev->get_tx_rates();
                }

                m_dev->set_master_clock_rate(clockRange.stop());
                uhd::meta_range_t rxHigh;
                uhd::meta_range_t txHigh;

                if (m_nbRxChannels > 0)
                {
                    rxHigh = m_dev->get_rx_rates();
                    m_srRangeRx = uhd::meta_range_t(std::min(rxLow.start(), rxHigh.start()), std::max(rxLow.stop(), rxHigh.stop()));
                }
                if (m_nbTxChannels > 0)
                {
                    txHigh = m_dev->get_tx_rates();
                    m_srRangeTx = uhd::meta_range_t(std::min(txLow.start(), txHigh.start()), std::max(txLow.stop(), txHigh.stop()));
                }

                // Need to restore automatic clock rate
                if (properties->exists("/mboards/0/auto_tick_rate")) {
                    properties->access<bool>("/mboards/0/auto_tick_rate").set(true);
                }
            }

            if (m_nbRxChannels > 0)
            {
                m_gainRangeRx = m_dev->get_rx_gain_range();

                std::vector<std::string> rxAntennas = m_dev->get_rx_antennas();
                m_rxAntennas.reserve(rxAntennas.size());
                for(size_t i = 0, l = rxAntennas.size(); i < l; ++i) {
                    m_rxAntennas << QString::fromStdString(rxAntennas[i]);
                }

                std::vector<std::string> rxGainNames = m_dev->get_rx_gain_names();
                m_rxGainNames.reserve(rxGainNames.size());
                for(size_t i = 0, l = rxGainNames.size(); i < l; ++i) {
                    m_rxGainNames << QString::fromStdString(rxGainNames[i]);
                }
            }
            if (m_nbTxChannels > 0)
            {
                m_gainRangeTx = m_dev->get_tx_gain_range();

                std::vector<std::string> txAntennas = m_dev->get_tx_antennas();
                m_txAntennas.reserve(txAntennas.size());
                for(size_t i = 0, l = txAntennas.size(); i < l; ++i) {
                    m_txAntennas << QString::fromStdString(txAntennas[i]);
                }
            }

            std::vector<std::string> clockSources = m_dev->get_clock_sources(0);
            m_clockSources.reserve(clockSources.size());
            for(size_t i = 0, l = clockSources.size(); i < l; ++i) {
                m_clockSources << QString::fromStdString(clockSources[i]);
            }
        }

        return true;
    }
    catch (const std::exception& e)
    {
        qDebug() << "DeviceUSRPParams::open: exception: " << e.what();
        return false;
    }
}

void DeviceUSRPParams::close()
{
    m_dev = nullptr;
}
