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
#include "chanalyzerwebapiadapter.h"

ChannelAnalyzerWebAPIAdapter::ChannelAnalyzerWebAPIAdapter()
{
    m_settings.setScopeGUI(&m_glScopeSettings);
    m_settings.setSpectrumGUI(&m_glSpectrumSettings);
}

ChannelAnalyzerWebAPIAdapter::~ChannelAnalyzerWebAPIAdapter()
{}

void ChannelAnalyzerWebAPIAdapter::webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const ChannelAnalyzerSettings& settings,
        const GLScopeSettings& scopeSettings,
        const GLSpectrumSettings& spectrumSettings)
{
    response.getChannelAnalyzerSettings()->setFrequency(settings.m_frequency);
    response.getChannelAnalyzerSettings()->setDownSample(settings.m_downSample ? 1 : 0);
    response.getChannelAnalyzerSettings()->setDownSampleRate(settings.m_downSampleRate);
    response.getChannelAnalyzerSettings()->setBandwidth(settings.m_bandwidth);
    response.getChannelAnalyzerSettings()->setLowCutoff(settings.m_lowCutoff);
    response.getChannelAnalyzerSettings()->setSpanLog2(settings.m_spanLog2);
    response.getChannelAnalyzerSettings()->setSsb(settings.m_ssb ? 1 : 0);
    response.getChannelAnalyzerSettings()->setPll(settings.m_pll ? 1 : 0);
    response.getChannelAnalyzerSettings()->setFll(settings.m_fll ? 1 : 0);
    response.getChannelAnalyzerSettings()->setRrc(settings.m_rrc ? 1 : 0);
    response.getChannelAnalyzerSettings()->setRrcRolloff(settings.m_rrcRolloff);
    response.getChannelAnalyzerSettings()->setPllPskOrder(settings.m_pllPskOrder);
    response.getChannelAnalyzerSettings()->setInputType((int) settings.m_inputType);
    response.getChannelAnalyzerSettings()->setRgbColor(settings.m_rgbColor);
    response.getChannelAnalyzerSettings()->setTitle(new QString(settings.m_title));
    SWGSDRangel::SWGGLScope *swgScope = new SWGSDRangel::SWGGLScope();
    swgScope->init();
    response.getChannelAnalyzerSettings()->setScopeConfig(swgScope);
    swgScope->setDisplayMode(scopeSettings.m_displayMode);
    swgScope->setGridIntensity(scopeSettings.m_gridIntensity);
    swgScope->setTime(scopeSettings.m_time);
    swgScope->setTimeOfs(scopeSettings.m_timeOfs);
    swgScope->setTraceIntensity(scopeSettings.m_traceIntensity);
    swgScope->setTraceLen(scopeSettings.m_traceLen);
    swgScope->setTrigPre(scopeSettings.m_trigPre);
    // TODO array of traces
    // TODO array of triggers
    SWGSDRangel::SWGGLSpectrum *swgSpectrum = new SWGSDRangel::SWGGLSpectrum();
    swgSpectrum->init();
    response.getChannelAnalyzerSettings()->setSpectrumConfig(swgSpectrum);
    swgSpectrum->setAveragingMode((int) spectrumSettings.m_averagingMode);
    swgSpectrum->setAveragingValue(spectrumSettings.m_averagingNb);
    swgSpectrum->setDecay(spectrumSettings.m_decay);
    swgSpectrum->setDecayDivisor(spectrumSettings.m_decayDivisor);
    swgSpectrum->setDisplayCurrent(spectrumSettings.m_displayCurrent ? 1 : 0);
    swgSpectrum->setDisplayGrid(spectrumSettings.m_displayGrid ? 1 : 0);
    swgSpectrum->setDisplayGridIntensity(spectrumSettings.m_displayGridIntensity);
    swgSpectrum->setDisplayHistogram(spectrumSettings.m_displayHistogram ? 1 : 0);
    swgSpectrum->setDisplayMaxHold(spectrumSettings.m_displayMaxHold ? 1 : 0);
    swgSpectrum->setDisplayTraceIntensity(spectrumSettings.m_displayTraceIntensity);
    swgSpectrum->setDisplayWaterfall(spectrumSettings.m_displayWaterfall ? 1 : 0);
    swgSpectrum->setFftOverlap(spectrumSettings.m_fftOverlap);
    swgSpectrum->setFftSize(spectrumSettings.m_fftSize);

}

void ChannelAnalyzerWebAPIAdapter::webapiUpdateChannelSettings(
        ChannelAnalyzerSettings& settings,
        GLScopeSettings& scopeSettings,
        GLSpectrumSettings& spectrumSettings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{

}

