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
#include "atvdemodwebapiadapter.h"

ATVDemodWebAPIAdapter::ATVDemodWebAPIAdapter()
{}

ATVDemodWebAPIAdapter::~ATVDemodWebAPIAdapter()
{}

int ATVDemodWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAtvDemodSettings(new SWGSDRangel::SWGATVDemodSettings());
    response.getAtvDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int ATVDemodWebAPIAdapter::webapiSettingsPutPatch(
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

void ATVDemodWebAPIAdapter::webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const ATVDemodSettings& settings)
{
    response.getAtvDemodSettings()->setBlnFftFiltering(settings.m_fftFiltering ? 1 : 0);
    response.getAtvDemodSettings()->setBlnHSync(settings.m_hSync ? 1 : 0);
    response.getAtvDemodSettings()->setBlnInvertVideo(settings.m_invertVideo ? 1 : 0);
    response.getAtvDemodSettings()->setBlnVSync(settings.m_vSync ? 1 : 0);
    response.getAtvDemodSettings()->setEnmAtvStandard((int) settings.m_atvStd);
    response.getAtvDemodSettings()->setEnmModulation((int) settings.m_atvModulation);
    response.getAtvDemodSettings()->setFltBfoFrequency(settings.m_bfoFrequency);
    response.getAtvDemodSettings()->setFltFramePerS(settings.m_fps);
    response.getAtvDemodSettings()->setFltRfBandwidth(settings.m_fftBandwidth);
    response.getAtvDemodSettings()->setFltRfOppBandwidth(settings.m_fftOppBandwidth);
    response.getAtvDemodSettings()->setFltVoltLevelSynchroBlack(settings.m_levelBlack);
    response.getAtvDemodSettings()->setFltVoltLevelSynchroTop(settings.m_levelSynchroTop);
    response.getAtvDemodSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getAtvDemodSettings()->setAmScalingFactor(settings.m_amScalingFactor);
    response.getAtvDemodSettings()->setAmOffsetFactor(settings.m_amOffsetFactor);
    response.getAtvDemodSettings()->setFpsIndex(ATVDemodSettings::getFpsIndex(settings.m_fps));
    response.getAtvDemodSettings()->setHalfImage(settings.m_halfFrames ? 1 : 0);
    response.getAtvDemodSettings()->setIntFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getAtvDemodSettings()->setIntNumberOfLines(settings.m_nbLines);
    response.getAtvDemodSettings()->setNbLinesIndex(ATVDemodSettings::getNumberOfLinesIndex(settings.m_nbLines));
    response.getAtvDemodSettings()->setRgbColor(settings.m_rgbColor);
    response.getAtvDemodSettings()->setTitle(new QString(settings.m_title));
    response.getAtvDemodSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getAtvDemodSettings()->setUdpPort(settings.m_udpPort);
}

void ATVDemodWebAPIAdapter::webapiUpdateChannelSettings(
        ATVDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("blnFFTFiltering")) {
        settings.m_fftFiltering = response.getAtvDemodSettings()->getBlnFftFiltering() != 0;
    }
    if (channelSettingsKeys.contains("blnHSync")) {
        settings.m_hSync = response.getAtvDemodSettings()->getBlnHSync() != 0;
    }
    if (channelSettingsKeys.contains("blnInvertVideo")) {
        settings.m_invertVideo = response.getAtvDemodSettings()->getBlnInvertVideo() != 0;
    }
    if (channelSettingsKeys.contains("blnVSync")) {
        settings.m_vSync = response.getAtvDemodSettings()->getBlnVSync() != 0;
    }
    if (channelSettingsKeys.contains("enmATVStandard")) {
        settings.m_atvStd = (ATVDemodSettings::ATVStd) response.getAtvDemodSettings()->getEnmAtvStandard();
    }
    if (channelSettingsKeys.contains("enmModulation")) {
        settings.m_atvModulation = (ATVDemodSettings::ATVModulation) response.getAtvDemodSettings()->getEnmModulation();
    }
    if (channelSettingsKeys.contains("fltBFOFrequency")) {
        settings.m_bfoFrequency = response.getAtvDemodSettings()->getFltBfoFrequency();
    }
    if (channelSettingsKeys.contains("fltFramePerS")) {
        settings.m_fps = response.getAtvDemodSettings()->getFltFramePerS();
    }
    if (channelSettingsKeys.contains("fltRFBandwidth")) {
        settings.m_fftBandwidth = response.getAtvDemodSettings()->getFltRfBandwidth();
    }
    if (channelSettingsKeys.contains("fltRFOppBandwidth")) {
        settings.m_fftOppBandwidth = response.getAtvDemodSettings()->getFltRfOppBandwidth();
    }
    if (channelSettingsKeys.contains("fltVoltLevelSynchroBlack")) {
        settings.m_levelBlack = response.getAtvDemodSettings()->getFltVoltLevelSynchroBlack();
    }
    if (channelSettingsKeys.contains("fltVoltLevelSynchroTop")) {
        settings.m_levelSynchroTop = response.getAtvDemodSettings()->getFltVoltLevelSynchroTop();
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getAtvDemodSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("amOffsetFactor")) {
        settings.m_amOffsetFactor = response.getAtvDemodSettings()->getAmOffsetFactor();
    }
    if (channelSettingsKeys.contains("amScalingFactor")) {
        settings.m_amScalingFactor = response.getAtvDemodSettings()->getAmScalingFactor();
    }
    if (channelSettingsKeys.contains("halfImage")) {
        settings.m_halfFrames = response.getAtvDemodSettings()->getHalfImage() != 0;
    }
    if (channelSettingsKeys.contains("intFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getAtvDemodSettings()->getIntFrequencyOffset();
    }
    if (channelSettingsKeys.contains("intNumberOfLines")) {
        settings.m_nbLines = response.getAtvDemodSettings()->getIntNumberOfLines();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAtvDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getAtvDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getAtvDemodSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getAtvDemodSettings()->getUdpPort();
    }
}