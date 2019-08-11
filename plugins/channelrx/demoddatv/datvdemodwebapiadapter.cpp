///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#include "SWGChannelSettings.h"
#include "datvdemodwebapiadapter.h"

DATVDemodWebAPIAdapter::DATVDemodWebAPIAdapter()
{}

DATVDemodWebAPIAdapter::~DATVDemodWebAPIAdapter()
{}

int DATVDemodWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setDatvDemodSettings(new SWGSDRangel::SWGDATVDemodSettings());
    response.getDatvDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int DATVDemodWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) force;
    (void) errorMessage;
    webapiUpdateChannelSettings(m_settings, channelSettingsKeys, response);
    return 200;
}

void DATVDemodWebAPIAdapter::webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const DATVDemodSettings& settings)
{
    response.getDatvDemodSettings()->setAllowDrift(settings.m_allowDrift ? 1 : 0);
    response.getDatvDemodSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    response.getDatvDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getDatvDemodSettings()->setAudioVolume(settings.m_audioVolume);
    response.getDatvDemodSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getDatvDemodSettings()->setExcursion(settings.m_excursion);
    response.getDatvDemodSettings()->setFastLock(settings.m_fastLock ? 1 : 0);
    response.getDatvDemodSettings()->setFec((int) settings.m_fec);
    response.getDatvDemodSettings()->setFilter((int) settings.m_filter);
    response.getDatvDemodSettings()->setHardMetric(settings.m_hardMetric ? 1 : 0);
    response.getDatvDemodSettings()->setModulation((int) settings.m_modulation);
    response.getDatvDemodSettings()->setNotchFilters(settings.m_notchFilters);
    response.getDatvDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getDatvDemodSettings()->setRgbColor(settings.m_rgbColor);
    response.getDatvDemodSettings()->setRollOff(settings.m_rollOff);
    response.getDatvDemodSettings()->setStandard((int) settings.m_standard);
    response.getDatvDemodSettings()->setSymbolRate(settings.m_symbolRate);
    response.getDatvDemodSettings()->setTitle(new QString(settings.m_title));
    response.getDatvDemodSettings()->setUdpTs(settings.m_udpTS ? 1 : 0);
    response.getDatvDemodSettings()->setUdpTsAddress(new QString(settings.m_udpTSAddress));
    response.getDatvDemodSettings()->setUdpTsPort(settings.m_udpTSPort);
    response.getDatvDemodSettings()->setVideoMute(settings.m_videoMute ? 1 : 0);
    response.getDatvDemodSettings()->setViterbi(settings.m_viterbi ? 1 : 0);
}

void DATVDemodWebAPIAdapter::webapiUpdateChannelSettings(
        DATVDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("allowDrift")) {
        settings.m_allowDrift = response.getDatvDemodSettings()->getAllowDrift() != 0;
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getDatvDemodSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getDatvDemodSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("audioVolume")) {
        settings.m_audioVolume = response.getDatvDemodSettings()->getAudioVolume();
    }
    if (channelSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getDatvDemodSettings()->getCenterFrequency();
    }
    if (channelSettingsKeys.contains("excursion")) {
        settings.m_excursion = response.getDatvDemodSettings()->getExcursion();
    }
    if (channelSettingsKeys.contains("fastLock")) {
        settings.m_fastLock = response.getDatvDemodSettings()->getFastLock() != 0;
    }
    if (channelSettingsKeys.contains("fec")) {
        settings.m_fec = (DATVDemodSettings::DATVCodeRate) response.getDatvDemodSettings()->getFec();
    }
    if (channelSettingsKeys.contains("filter")) {
        settings.m_filter = (DATVDemodSettings::dvb_sampler) response.getDatvDemodSettings()->getFilter();
    }
    if (channelSettingsKeys.contains("hardMetric")) {
        settings.m_hardMetric = response.getDatvDemodSettings()->getHardMetric() != 0;
    }
    if (channelSettingsKeys.contains("modulation")) {
        settings.m_modulation = (DATVDemodSettings::DATVModulation) response.getDatvDemodSettings()->getModulation();
    }
    if (channelSettingsKeys.contains("notchFilters")) {
        settings.m_notchFilters = response.getDatvDemodSettings()->getNotchFilters();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getDatvDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getDatvDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("rollOff")) {
        settings.m_rollOff = response.getDatvDemodSettings()->getRollOff();
    }
    if (channelSettingsKeys.contains("standard")) {
        settings.m_standard = (DATVDemodSettings::dvb_version) response.getDatvDemodSettings()->getStandard();
    }
    if (channelSettingsKeys.contains("symbolRate")) {
        settings.m_symbolRate = response.getDatvDemodSettings()->getSymbolRate();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getDatvDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("udpTS")) {
        settings.m_udpTS = response.getDatvDemodSettings()->getUdpTs() != 0;
    }
    if (channelSettingsKeys.contains("udpTSAddress")) {
        settings.m_udpTSAddress = *response.getDatvDemodSettings()->getUdpTsAddress();
    }
    if (channelSettingsKeys.contains("udpTSPort")) {
        settings.m_udpTSPort = response.getDatvDemodSettings()->getUdpTsPort();
    }
    if (channelSettingsKeys.contains("videoMute")) {
        settings.m_videoMute = response.getDatvDemodSettings()->getVideoMute() != 0;
    }
    if (channelSettingsKeys.contains("viterbi")) {
        settings.m_viterbi = response.getDatvDemodSettings()->getViterbi() != 0;
    }
}
