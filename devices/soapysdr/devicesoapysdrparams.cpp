///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include <sstream>
#include <iostream>

#include <QDebug>

#include "devicesoapysdrparams.h"

DeviceSoapySDRParams::DeviceSoapySDRParams(SoapySDR::Device *device) :
    m_device(device)
{
    fillParams();
    printParams();
}

DeviceSoapySDRParams::~DeviceSoapySDRParams()
{}

std::string DeviceSoapySDRParams::getRxChannelMainTunableElementName(uint32_t index)
{
    if (index < m_nbRx)
    {
        return std::string("RF");
    }
    else
    {
        const ChannelSettings& channelSettings = m_RxChannelsSettings[index];

        if (channelSettings.m_frequencySettings.size() > 0) {
            return channelSettings.m_frequencySettings.front().m_name;
        } else {
            return std::string("RF");
        }
    }
}

std::string DeviceSoapySDRParams::getTxChannelMainTunableElementName(uint32_t index)
{
    if (index < m_nbRx)
    {
        return std::string("RF");
    }
    else
    {
        const ChannelSettings& channelSettings = m_RxChannelsSettings[index];

        if (channelSettings.m_frequencySettings.size() > 0) {
            return channelSettings.m_frequencySettings.front().m_name;
        } else {
            return std::string("RF");
        }
    }
}

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

void DeviceSoapySDRParams::fillChannelParams(std::vector<ChannelSettings>& channelSettings, int direction, unsigned int ichan)
{
    channelSettings.push_back(ChannelSettings());

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

void DeviceSoapySDRParams::printParams()
{
    qDebug() << "DeviceSoapySDRParams::printParams: m_deviceSettingsArgs:\n" << argInfoListToString(m_deviceSettingsArgs).c_str();
    int ichan = 0;

    for (const auto &it : m_RxChannelsSettings)
    {
        qDebug() << "DeviceSoapySDRParams::printParams: Rx channel " << ichan;
        printChannelParams(it);
        ichan++;
    }

    ichan = 0;

    for (const auto &it : m_TxChannelsSettings)
    {
        qDebug() << "DeviceSoapySDRParams::printParams: Tx channel " << ichan;
        printChannelParams(it);
        ichan++;
    }
}

void DeviceSoapySDRParams::printChannelParams(const ChannelSettings& channelSetting)
{
    qDebug() << "DeviceSoapySDRParams::printParams: m_streamSettingsArgs:\n" << argInfoListToString(channelSetting.m_streamSettingsArgs).c_str();
    qDebug() << "DeviceSoapySDRParams::printParams:"
            << " m_hasDCAutoCorrection: " << channelSetting.m_hasDCAutoCorrection
            << " m_hasDCOffsetValue: " << channelSetting.m_hasDCOffsetValue
            << " m_hasIQBalanceValue: " << channelSetting.m_hasIQBalanceValue
            << " m_hasFrequencyCorrectionValue: " << channelSetting.m_hasFrequencyCorrectionValue
            << " m_hasAGC: " << channelSetting.m_hasAGC;
    qDebug() << "DeviceSoapySDRParams::printParams: m_antennas: " << vectorToString(channelSetting.m_antennas).c_str();
    qDebug() << "DeviceSoapySDRParams::printParams: m_gainRange: " << rangeToString(channelSetting.m_gainRange).c_str();

    qDebug() << "DeviceSoapySDRParams::printParams: individual gains...";

    for (const auto &gainIt : channelSetting.m_gainSettings)
    {
        qDebug() << "DeviceSoapySDRParams::printParams: m_name: " << gainIt.m_name.c_str();
        qDebug() << "DeviceSoapySDRParams::printParams: m_range: " << rangeToString(gainIt.m_range).c_str();
    }

    qDebug() << "DeviceSoapySDRParams::printParams: tunable elements...";

    for (const auto &freqIt : channelSetting.m_frequencySettings)
    {
        qDebug() << "DeviceSoapySDRParams::printParams: m_name: " << freqIt.m_name.c_str();
        qDebug() << "DeviceSoapySDRParams::printParams: m_range (kHz): " << rangeListToString(freqIt.m_ranges, 1e3).c_str();
    }

    qDebug() << "DeviceSoapySDRParams::printParams: m_frequencySettingsArgs:\n" << argInfoListToString(channelSetting.m_frequencySettingsArgs).c_str();
    qDebug() << "DeviceSoapySDRParams::printParams: m_ratesRanges (kHz): " << rangeListToString(channelSetting.m_ratesRanges, 1e3).c_str();
    qDebug() << "DeviceSoapySDRParams::printParams: m_bandwidthsRanges (kHz): " << rangeListToString(channelSetting.m_bandwidthsRanges, 1e3).c_str();
}

std::string DeviceSoapySDRParams::argInfoToString(const SoapySDR::ArgInfo &argInfo, const std::string indent)
{
    std::stringstream ss;

    //name, or use key if missing
    std::string name = argInfo.name;
    if (argInfo.name.empty()) name = argInfo.key;
    ss << indent << " * " << name;

    //optional description
    std::string desc = argInfo.description;
    const std::string replace("\n"+indent+"   ");

    for (std::size_t pos = 0; (pos=desc.find("\n", pos)) != std::string::npos; pos+=replace.size()) {
        desc.replace(pos, 1, replace);
    }

    if (not desc.empty()) {
        ss << " - " << desc << std::endl << indent << "  ";
    }

    //other fields
    ss << " [key=" << argInfo.key;

    if (not argInfo.units.empty()) {
        ss << ", units=" << argInfo.units;
    }

    if (not argInfo.value.empty()) {
        ss << ", default=" << argInfo.value;
    }

    //type
    switch (argInfo.type)
    {
    case SoapySDR::ArgInfo::BOOL:
        ss << ", type=bool";
        break;
    case SoapySDR::ArgInfo::INT:
        ss << ", type=int";
        break;
    case SoapySDR::ArgInfo::FLOAT:
        ss << ", type=float";
        break;
    case SoapySDR::ArgInfo::STRING:
        ss << ", type=string";
        break;
    }

    //optional range/enumeration
    if (argInfo.range.minimum() < argInfo.range.maximum()) {
        ss << ", range=" << rangeToString(argInfo.range);
    }

    if (not argInfo.options.empty()) {
        ss << ", options=(" << vectorToString(argInfo.options) << ")";
    }

    ss << "]";

    return ss.str();
}

std::string DeviceSoapySDRParams::argInfoListToString(const SoapySDR::ArgInfoList &argInfos)
{
    std::stringstream ss;

    for (std::size_t i = 0; i < argInfos.size(); i++) {
        ss << argInfoToString(argInfos[i]) << std::endl;
    }

    return ss.str();
}

std::string DeviceSoapySDRParams::rangeToString(const SoapySDR::Range &range)
{
    std::stringstream ss;
    ss << "[" << range.minimum() << ", " << range.maximum();

    if (range.step() != 0.0) {
        ss << ", " << range.step();
    }

    ss << "]";
    return ss.str();
}

std::string DeviceSoapySDRParams::rangeListToString(const SoapySDR::RangeList &range, const double scale)
{
    std::stringstream ss;

    for (std::size_t i = 0; i < range.size(); i++)
    {
        if (not ss.str().empty()) {
            ss << ", ";
        }

        if (range[i].minimum() == range[i].maximum())
        {
            ss << (range[i].minimum()/scale);
        }
        else
        {
            ss << "[" << (range[i].minimum()/scale) << ", " << (range[i].maximum()/scale);

            if (range[i].step() != 0.0) {
                ss << ", " << (range[i].step()/scale);
            }

            ss << "]";
        }
    }

    return ss.str();
}
