///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include "devicesoapysdrparams.h"

DeviceSoapySDRParams::DeviceSoapySDRParams(SoapySDR::Device *device) :
    m_device(device)
{
    fillParams();
}

DeviceSoapySDRParams::~DeviceSoapySDRParams()
{}

void DeviceSoapySDRParams::fillParams()
{
    m_deviceSettingsArgs = m_device->getSettingInfo();
    m_nbRx = m_device->getNumChannels(SOAPY_SDR_RX);
    m_nbTx = m_device->getNumChannels(SOAPY_SDR_TX);

    for (unsigned int ichan = 0; ichan < m_nbRx; ichan++) {
        fillChannelParams(m_RxChannelsSettings, SOAPY_SDR_RX, ichan);
    }

    for (unsigned int ichan = 0; ichan < m_nbTx; ichan++) {
        fillChannelParams(m_TxChannelsSettings, SOAPY_SDR_TX, ichan);
    }
}

void DeviceSoapySDRParams::fillChannelParams(std::vector<ChannelSetting>& channelSettings, int direction, unsigned int ichan)
{
    channelSettings.push_back(ChannelSetting());

    channelSettings.back().m_streamSettingsArgs = m_device->getStreamArgsInfo(direction, ichan);
    channelSettings.back().m_antennas = m_device->listAntennas(direction, ichan);
    channelSettings.back().m_hasDCAutoCorrection = m_device->hasDCOffsetMode(direction, ichan);
    channelSettings.back().m_hasDCOffsetValue = m_device->hasDCOffset(direction, ichan);
    channelSettings.back().m_hasIQBalanceValue = m_device->hasIQBalance(direction, ichan);
    channelSettings.back().m_hasFrequencyCorrectionValue = m_device->hasFrequencyCorrection(direction, ichan);

    // gains

    channelSettings.back().m_hasAGC = m_device->hasGainMode(direction, ichan);
    channelSettings.back().m_gainRange = m_device->getGainRange(direction, ichan);
    std::vector<std::string> gainsList = m_device->listGains(direction, ichan);

    for (const auto &it : gainsList)
    {
        channelSettings.back().m_gainSettings.push_back(GainSetting());
        channelSettings.back().m_gainSettings.back().m_name = it;
        channelSettings.back().m_gainSettings.back().m_range = m_device->getGainRange(direction, ichan, it);
    }

    // frequencies

    std::vector<std::string> freqsList = m_device->listFrequencies(direction, ichan);

    for (const auto &it : freqsList)
    {
        channelSettings.back().m_frequencySettings.push_back(FrequencySetting());
        channelSettings.back().m_frequencySettings.back().m_name = it;
        channelSettings.back().m_frequencySettings.back().m_ranges = m_device->getFrequencyRange(direction, ichan, it);
    }

    channelSettings.back().m_frequencySettingsArgs = m_device->getFrequencyArgsInfo(direction, ichan);

    // sample rates

    channelSettings.back().m_ratesRanges = m_device->getSampleRateRange(direction, ichan);

    // bandwidths

    channelSettings.back().m_bandwidthsRanges = m_device->getBandwidthRange(direction, ichan);
}
