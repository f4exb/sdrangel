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

#include <QString>

#include "SWGChannelSettings.h"
#include "chanalyzerwebapiadapter.h"

ChannelAnalyzerWebAPIAdapter::ChannelAnalyzerWebAPIAdapter()
{
    m_settings.setScopeGUI(&m_glScopeSettings);
    m_settings.setSpectrumGUI(&m_SpectrumSettings);
}

ChannelAnalyzerWebAPIAdapter::~ChannelAnalyzerWebAPIAdapter()
{}

int ChannelAnalyzerWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setChannelAnalyzerSettings(new SWGSDRangel::SWGChannelAnalyzerSettings());
    response.getChannelAnalyzerSettings()->init();
    webapiFormatChannelSettings(response, m_settings, m_glScopeSettings, m_SpectrumSettings);
    return 200;
}

void ChannelAnalyzerWebAPIAdapter::webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const ChannelAnalyzerSettings& settings,
        const GLScopeSettings& scopeSettings,
        const SpectrumSettings& spectrumSettings)
{
    response.getChannelAnalyzerSettings()->setFrequency(settings.m_inputFrequencyOffset);
    response.getChannelAnalyzerSettings()->setDownSample(settings.m_rationalDownSample ? 1 : 0);
    response.getChannelAnalyzerSettings()->setDownSampleRate(settings.m_rationalDownSamplerRate);
    response.getChannelAnalyzerSettings()->setBandwidth(settings.m_bandwidth);
    response.getChannelAnalyzerSettings()->setLowCutoff(settings.m_lowCutoff);
    response.getChannelAnalyzerSettings()->setSpanLog2(settings.m_log2Decim);
    response.getChannelAnalyzerSettings()->setSsb(settings.m_ssb ? 1 : 0);
    response.getChannelAnalyzerSettings()->setPll(settings.m_pll ? 1 : 0);
    response.getChannelAnalyzerSettings()->setFll(settings.m_fll ? 1 : 0);
    response.getChannelAnalyzerSettings()->setCostasLoop(settings.m_costasLoop ? 1 : 0);
    response.getChannelAnalyzerSettings()->setRrc(settings.m_rrc ? 1 : 0);
    response.getChannelAnalyzerSettings()->setRrcRolloff(settings.m_rrcRolloff);
    response.getChannelAnalyzerSettings()->setPllPskOrder(settings.m_pllPskOrder);
    response.getChannelAnalyzerSettings()->setPllBandwidth(settings.m_pllBandwidth);
    response.getChannelAnalyzerSettings()->setPllDampingFactor(settings.m_pllBandwidth);
    response.getChannelAnalyzerSettings()->setPllLoopGain(settings.m_pllLoopGain);
    response.getChannelAnalyzerSettings()->setInputType((int) settings.m_inputType);
    response.getChannelAnalyzerSettings()->setRgbColor(settings.m_rgbColor);
    response.getChannelAnalyzerSettings()->setTitle(new QString(settings.m_title));

    // scope
    SWGSDRangel::SWGGLScope *swgScope = new SWGSDRangel::SWGGLScope();
    swgScope->init();
    response.getChannelAnalyzerSettings()->setScopeConfig(swgScope);
    scopeSettings.formatTo(swgScope);

    // spectrum
    SWGSDRangel::SWGGLSpectrum *swgSpectrum = new SWGSDRangel::SWGGLSpectrum();
    swgSpectrum->init();
    response.getChannelAnalyzerSettings()->setSpectrumConfig(swgSpectrum);
    spectrumSettings.formatTo(swgSpectrum);
}

int ChannelAnalyzerWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) force;
    (void) errorMessage;
    webapiUpdateChannelSettings(m_settings, m_glScopeSettings, m_SpectrumSettings, channelSettingsKeys, response);
    return 200;
}

void ChannelAnalyzerWebAPIAdapter::webapiUpdateChannelSettings(
        ChannelAnalyzerSettings& settings,
        GLScopeSettings& scopeSettings,
        SpectrumSettings& spectrumSettings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("bandwidth")) {
        settings.m_bandwidth = response.getChannelAnalyzerSettings()->getBandwidth();
    }
    if (channelSettingsKeys.contains("downSample")) {
        settings.m_rationalDownSample = response.getChannelAnalyzerSettings()->getDownSample() != 0;
    }
    if (channelSettingsKeys.contains("downSampleRate")) {
        settings.m_rationalDownSamplerRate = response.getChannelAnalyzerSettings()->getDownSampleRate();
    }
    if (channelSettingsKeys.contains("fll")) {
        settings.m_fll = response.getChannelAnalyzerSettings()->getFll() != 0;
    }
    if (channelSettingsKeys.contains("frequency")) {
        settings.m_inputFrequencyOffset = response.getChannelAnalyzerSettings()->getFrequency();
    }
    if (channelSettingsKeys.contains("inputType")) {
        settings.m_inputType = (ChannelAnalyzerSettings::InputType) response.getChannelAnalyzerSettings()->getInputType();
    }
    if (channelSettingsKeys.contains("lowCutoff")) {
        settings.m_lowCutoff = response.getChannelAnalyzerSettings()->getLowCutoff();
    }
    if (channelSettingsKeys.contains("pll")) {
        settings.m_pll = response.getChannelAnalyzerSettings()->getPll() != 0;
    }
    if (channelSettingsKeys.contains("costasLoop")) {
        settings.m_costasLoop = response.getChannelAnalyzerSettings()->getCostasLoop() != 0;
    }
    if (channelSettingsKeys.contains("pllPskOrder")) {
        settings.m_pllPskOrder = response.getChannelAnalyzerSettings()->getPllPskOrder();
    }
    if (channelSettingsKeys.contains("pllBandwidth")) {
        settings.m_pllBandwidth = response.getChannelAnalyzerSettings()->getPllBandwidth();
    }
    if (channelSettingsKeys.contains("pllDampingFactor")) {
        settings.m_pllDampingFactor = response.getChannelAnalyzerSettings()->getPllDampingFactor();
    }
    if (channelSettingsKeys.contains("pllLoopGain")) {
        settings.m_pllLoopGain = response.getChannelAnalyzerSettings()->getPllLoopGain();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getChannelAnalyzerSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("rrc")) {
        settings.m_rrc = response.getChannelAnalyzerSettings()->getRrc() != 0;
    }
    if (channelSettingsKeys.contains("rrcRolloff")) {
        settings.m_rrcRolloff = response.getChannelAnalyzerSettings()->getRrcRolloff();
    }
    if (channelSettingsKeys.contains("spanLog2")) {
        settings.m_log2Decim = response.getChannelAnalyzerSettings()->getSpanLog2();
    }
    if (channelSettingsKeys.contains("ssb")) {
        settings.m_ssb = response.getChannelAnalyzerSettings()->getSsb() != 0;
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getChannelAnalyzerSettings()->getTitle();
    }
    // scope
    if (channelSettingsKeys.contains("scopeConfig")) {
        scopeSettings.updateFrom(channelSettingsKeys, response.getChannelAnalyzerSettings()->getScopeConfig());
    }
    // spectrum
    if (channelSettingsKeys.contains("spectrumConfig")) {
        spectrumSettings.updateFrom(channelSettingsKeys, response.getChannelAnalyzerSettings()->getSpectrumConfig());
    }
}
