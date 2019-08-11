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
    response.getAtvDemodSettings()->setBlndecimatorEnable(settings.m_blndecimatorEnable ? 1 : 0);
    response.getAtvDemodSettings()->setBlnFftFiltering(settings.m_blnFFTFiltering ? 1 : 0);
    response.getAtvDemodSettings()->setBlnHSync(settings.m_blnHSync ? 1 : 0);
    response.getAtvDemodSettings()->setBlnInvertVideo(settings.m_blnInvertVideo ? 1 : 0);
    response.getAtvDemodSettings()->setBlnVSync(settings.m_blnVSync ? 1 : 0);
    response.getAtvDemodSettings()->setEnmAtvStandard((int) settings.m_enmATVStandard);
    response.getAtvDemodSettings()->setEnmModulation((int) settings.m_enmModulation);
    response.getAtvDemodSettings()->setFltBfoFrequency(settings.m_fltBFOFrequency);
    response.getAtvDemodSettings()->setFltFramePerS(settings.m_fltFramePerS);
    response.getAtvDemodSettings()->setFltLineDuration(settings.m_fltLineDuration);
    response.getAtvDemodSettings()->setFltRatioOfRowsToDisplay(settings.m_fltRatioOfRowsToDisplay);
    response.getAtvDemodSettings()->setFltRfBandwidth(settings.m_fltRFBandwidth);
    response.getAtvDemodSettings()->setFltRfOppBandwidth(settings.m_fltRFOppBandwidth);
    response.getAtvDemodSettings()->setFltTopDuration(settings.m_fltTopDuration);
    response.getAtvDemodSettings()->setFltVoltLevelSynchroBlack(settings.m_fltVoltLevelSynchroBlack);
    response.getAtvDemodSettings()->setFltVoltLevelSynchroTop(settings.m_fltVoltLevelSynchroTop);
    response.getAtvDemodSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getAtvDemodSettings()->setFpsIndex(settings.m_fpsIndex);
    response.getAtvDemodSettings()->setHalfImage(settings.m_halfImage ? 1 : 0);
    response.getAtvDemodSettings()->setIntFrequencyOffset(settings.m_intFrequencyOffset);
    response.getAtvDemodSettings()->setIntNumberOfLines(settings.m_intNumberOfLines);
    response.getAtvDemodSettings()->setIntNumberSamplePerLine(settings.m_intNumberSamplePerLine);
    response.getAtvDemodSettings()->setIntSampleRate(settings.m_intSampleRate);
    response.getAtvDemodSettings()->setIntTvSampleRate(settings.m_intTVSampleRate);
    response.getAtvDemodSettings()->setIntVideoTabIndex(settings.m_intVideoTabIndex);
    response.getAtvDemodSettings()->setLineTimeFactor(settings.m_lineTimeFactor);
    response.getAtvDemodSettings()->setNbLinesIndex(settings.m_nbLinesIndex);
    response.getAtvDemodSettings()->setOppBandwidthFactor(settings.m_OppBandwidthFactor);
    response.getAtvDemodSettings()->setRfBandwidthFactor(settings.m_RFBandwidthFactor);
    response.getAtvDemodSettings()->setRgbColor(settings.m_rgbColor);
    response.getAtvDemodSettings()->setTitle(new QString(settings.m_title));
    response.getAtvDemodSettings()->setTopTimeFactor(settings.m_topTimeFactor);
    response.getAtvDemodSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getAtvDemodSettings()->setUdpPort(settings.m_udpPort);
}

void ATVDemodWebAPIAdapter::webapiUpdateChannelSettings(
        ATVDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("blndecimatorEnable")) {
        settings.m_blndecimatorEnable = response.getAtvDemodSettings()->getBlndecimatorEnable() != 0;
    }
    if (channelSettingsKeys.contains("blnFFTFiltering")) {
        settings.m_blnFFTFiltering = response.getAtvDemodSettings()->getBlnFftFiltering() != 0;
    }
    if (channelSettingsKeys.contains("blnHSync")) {
        settings.m_blnHSync = response.getAtvDemodSettings()->getBlnHSync() != 0;
    }
    if (channelSettingsKeys.contains("blnInvertVideo")) {
        settings.m_blnInvertVideo = response.getAtvDemodSettings()->getBlnInvertVideo() != 0;
    }
    if (channelSettingsKeys.contains("blnVSync")) {
        settings.m_blnVSync = response.getAtvDemodSettings()->getBlnVSync() != 0;
    }
    if (channelSettingsKeys.contains("enmATVStandard")) {
        settings.m_enmATVStandard = (ATVDemodSettings::ATVStd) response.getAtvDemodSettings()->getEnmAtvStandard();
    }
    if (channelSettingsKeys.contains("enmModulation")) {
        settings.m_enmModulation = (ATVDemodSettings::ATVModulation) response.getAtvDemodSettings()->getEnmModulation();
    }
    if (channelSettingsKeys.contains("fltBFOFrequency")) {
        settings.m_fltBFOFrequency = response.getAtvDemodSettings()->getFltBfoFrequency();
    }
    if (channelSettingsKeys.contains("fltFramePerS")) {
        settings.m_fltFramePerS = response.getAtvDemodSettings()->getFltFramePerS();
    }
    if (channelSettingsKeys.contains("fltLineDuration")) {
        settings.m_fltLineDuration = response.getAtvDemodSettings()->getFltLineDuration();
    }
    if (channelSettingsKeys.contains("fltRatioOfRowsToDisplay")) {
        settings.m_fltRatioOfRowsToDisplay = response.getAtvDemodSettings()->getFltRatioOfRowsToDisplay();
    }
    if (channelSettingsKeys.contains("fltRFBandwidth")) {
        settings.m_fltRFBandwidth = response.getAtvDemodSettings()->getFltRfBandwidth();
    }
    if (channelSettingsKeys.contains("fltRFOppBandwidth")) {
        settings.m_fltRFOppBandwidth = response.getAtvDemodSettings()->getFltRfOppBandwidth();
    }
    if (channelSettingsKeys.contains("fltTopDuration")) {
        settings.m_fltTopDuration = response.getAtvDemodSettings()->getFltTopDuration();
    }
    if (channelSettingsKeys.contains("fltVoltLevelSynchroBlack")) {
        settings.m_fltVoltLevelSynchroBlack = response.getAtvDemodSettings()->getFltVoltLevelSynchroBlack();
    }
    if (channelSettingsKeys.contains("fltVoltLevelSynchroTop")) {
        settings.m_fltVoltLevelSynchroTop = response.getAtvDemodSettings()->getFltVoltLevelSynchroTop();
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getAtvDemodSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("fpsIndex")) {
        settings.m_fpsIndex = response.getAtvDemodSettings()->getFpsIndex();
    }
    if (channelSettingsKeys.contains("halfImage")) {
        settings.m_halfImage = response.getAtvDemodSettings()->getHalfImage() != 0;
    }
    if (channelSettingsKeys.contains("intFrequencyOffset")) {
        settings.m_intFrequencyOffset = response.getAtvDemodSettings()->getIntFrequencyOffset();
    }
    if (channelSettingsKeys.contains("intNumberOfLines")) {
        settings.m_intNumberOfLines = response.getAtvDemodSettings()->getIntNumberOfLines();
    }
    if (channelSettingsKeys.contains("intNumberSamplePerLine")) {
        settings.m_intNumberSamplePerLine = response.getAtvDemodSettings()->getIntNumberSamplePerLine();
    }
    if (channelSettingsKeys.contains("intSampleRate")) {
        settings.m_intSampleRate = response.getAtvDemodSettings()->getIntSampleRate();
    }
    if (channelSettingsKeys.contains("intTVSampleRate")) {
        settings.m_intTVSampleRate = response.getAtvDemodSettings()->getIntTvSampleRate();
    }
    if (channelSettingsKeys.contains("intVideoTabIndex")) {
        settings.m_intVideoTabIndex = response.getAtvDemodSettings()->getIntVideoTabIndex();
    }
    if (channelSettingsKeys.contains("lineTimeFactor")) {
        settings.m_lineTimeFactor = response.getAtvDemodSettings()->getLineTimeFactor();
    }
    if (channelSettingsKeys.contains("nbLinesIndex")) {
        settings.m_nbLinesIndex = response.getAtvDemodSettings()->getNbLinesIndex();
    }
    if (channelSettingsKeys.contains("OppBandwidthFactor")) {
        settings.m_OppBandwidthFactor = response.getAtvDemodSettings()->getOppBandwidthFactor();
    }
    if (channelSettingsKeys.contains("RFBandwidthFactor")) {
        settings.m_RFBandwidthFactor = response.getAtvDemodSettings()->getRfBandwidthFactor();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAtvDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getAtvDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("topTimeFactor")) {
        settings.m_topTimeFactor = response.getAtvDemodSettings()->getTopTimeFactor();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getAtvDemodSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getAtvDemodSettings()->getUdpPort();
    }
}