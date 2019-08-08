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
    m_settings.setSpectrumGUI(&m_glSpectrumSettings);
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
    webapiFormatChannelSettings(response, m_settings, m_glScopeSettings, m_glSpectrumSettings);
    return 200;
}

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

    // scope
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

    // array of traces
    swgScope->setTracesData(new QList<SWGSDRangel::SWGTraceData *>);
    std::vector<GLScopeSettings::TraceData>::const_iterator traceIt = scopeSettings.m_tracesData.begin();

    for (; traceIt != scopeSettings.m_tracesData.end(); ++traceIt)
    {
        swgScope->getTracesData()->append(new SWGSDRangel::SWGTraceData);
        swgScope->getTracesData()->back()->setAmp(traceIt->m_amp);
        swgScope->getTracesData()->back()->setAmpIndex(traceIt->m_ampIndex);
        swgScope->getTracesData()->back()->setHasTextOverlay(traceIt->m_hasTextOverlay ? 1 : 0);
        swgScope->getTracesData()->back()->setInputIndex(traceIt->m_inputIndex);
        swgScope->getTracesData()->back()->setOfs(traceIt->m_ofs);
        swgScope->getTracesData()->back()->setOfsCoarse(traceIt->m_ofsCoarse);
        swgScope->getTracesData()->back()->setOfsFine(traceIt->m_ofsFine);
        swgScope->getTracesData()->back()->setProjectionType((int) traceIt->m_projectionType);
        swgScope->getTracesData()->back()->setTextOverlay(new QString(traceIt->m_textOverlay));
        swgScope->getTracesData()->back()->setTraceColor(qColorToInt(traceIt->m_traceColor));
        swgScope->getTracesData()->back()->setTraceColorB(traceIt->m_traceColorB);
        swgScope->getTracesData()->back()->setTraceColorG(traceIt->m_traceColorG);
        swgScope->getTracesData()->back()->setTraceColorR(traceIt->m_traceColorR);
        swgScope->getTracesData()->back()->setTraceDelay(traceIt->m_traceDelay);
        swgScope->getTracesData()->back()->setTraceDelayCoarse(traceIt->m_traceDelayCoarse);
        swgScope->getTracesData()->back()->setTraceDelayFine(traceIt->m_traceDelayFine);
        swgScope->getTracesData()->back()->setTriggerDisplayLevel(traceIt->m_triggerDisplayLevel);
        swgScope->getTracesData()->back()->setViewTrace(traceIt->m_viewTrace ? 1 : 0);
    }

    // array of triggers
    swgScope->setTriggersData(new QList<SWGSDRangel::SWGTriggerData *>);
    std::vector<GLScopeSettings::TriggerData>::const_iterator triggerIt = scopeSettings.m_triggersData.begin();

    for (; triggerIt != scopeSettings.m_triggersData.end(); ++triggerIt)
    {
        swgScope->getTriggersData()->append(new SWGSDRangel::SWGTriggerData);
        swgScope->getTriggersData()->back()->setInputIndex(triggerIt->m_inputIndex);
        swgScope->getTriggersData()->back()->setProjectionType((int) triggerIt->m_projectionType);
        swgScope->getTriggersData()->back()->setTriggerBothEdges(triggerIt->m_triggerBothEdges ? 1 : 0);
        swgScope->getTriggersData()->back()->setTriggerColor(qColorToInt(triggerIt->m_triggerColor));
        swgScope->getTriggersData()->back()->setTriggerColorB(triggerIt->m_triggerColorB);
        swgScope->getTriggersData()->back()->setTriggerColorG(triggerIt->m_triggerColorG);
        swgScope->getTriggersData()->back()->setTriggerColorR(triggerIt->m_triggerColorR);
        swgScope->getTriggersData()->back()->setTriggerDelay(triggerIt->m_triggerDelay);
        swgScope->getTriggersData()->back()->setTriggerDelayCoarse(triggerIt->m_triggerDelayCoarse);
        swgScope->getTriggersData()->back()->setTriggerDelayFine(triggerIt->m_triggerDelayFine);
        swgScope->getTriggersData()->back()->setTriggerDelayMult(triggerIt->m_triggerDelayMult);
        swgScope->getTriggersData()->back()->setTriggerHoldoff(triggerIt->m_triggerHoldoff ? 1 : 0);
        swgScope->getTriggersData()->back()->setTriggerLevel(triggerIt->m_triggerLevel);
        swgScope->getTriggersData()->back()->setTriggerLevelCoarse(triggerIt->m_triggerLevelCoarse);
        swgScope->getTriggersData()->back()->setTriggerLevelFine(triggerIt->m_triggerLevelFine);
        swgScope->getTriggersData()->back()->setTriggerPositiveEdge(triggerIt->m_triggerPositiveEdge ? 1 : 0);
        swgScope->getTriggersData()->back()->setTriggerRepeat(triggerIt->m_triggerRepeat);
    }

    // spectrum
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

int ChannelAnalyzerWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) force;
    (void) errorMessage;
    webapiUpdateChannelSettings(m_settings, m_glScopeSettings, m_glSpectrumSettings, channelSettingsKeys, response);
}

void ChannelAnalyzerWebAPIAdapter::webapiUpdateChannelSettings(
        ChannelAnalyzerSettings& settings,
        GLScopeSettings& scopeSettings,
        GLSpectrumSettings& spectrumSettings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("frequency")) {
        settings.m_frequency = response.getChannelAnalyzerSettings()->getFrequency();
    }
    if (channelSettingsKeys.contains("downSample")) {
        settings.m_downSample = response.getChannelAnalyzerSettings()->getDownSample() != 0;
    }
    if (channelSettingsKeys.contains("downSampleRate")) {
        settings.m_downSampleRate = response.getChannelAnalyzerSettings()->getDownSampleRate();
    }
    if (channelSettingsKeys.contains("bandwidth")) {
        settings.m_bandwidth = response.getChannelAnalyzerSettings()->getBandwidth();
    }
    if (channelSettingsKeys.contains("lowCutoff")) {
        settings.m_lowCutoff = response.getChannelAnalyzerSettings()->getLowCutoff();
    }
    if (channelSettingsKeys.contains("spanLog2")) {
        settings.m_spanLog2 = response.getChannelAnalyzerSettings()->getSpanLog2();
    }
    if (channelSettingsKeys.contains("ssb")) {
        settings.m_ssb = response.getChannelAnalyzerSettings()->getSsb() != 0;
    }
    if (channelSettingsKeys.contains("pll")) {
        settings.m_pll = response.getChannelAnalyzerSettings()->getPll() != 0;
    }
    if (channelSettingsKeys.contains("fll")) {
        settings.m_fll = response.getChannelAnalyzerSettings()->getFll() != 0;
    }
    if (channelSettingsKeys.contains("rrc")) {
        settings.m_rrc = response.getChannelAnalyzerSettings()->getRrc() != 0;
    }
    if (channelSettingsKeys.contains("rrcRolloff")) {
        settings.m_rrcRolloff = response.getChannelAnalyzerSettings()->getRrcRolloff();
    }
    if (channelSettingsKeys.contains("pllPskOrder")) {
        settings.m_pllPskOrder = response.getChannelAnalyzerSettings()->getPllPskOrder();
    }
    if (channelSettingsKeys.contains("inputType")) {
        settings.m_inputType = (ChannelAnalyzerSettings::InputType) response.getChannelAnalyzerSettings()->getInputType();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getChannelAnalyzerSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getChannelAnalyzerSettings()->getTitle();
    }
    // scope
    if (channelSettingsKeys.contains("scopeConfig.displayMode")) {
        scopeSettings.m_displayMode = (GLScopeSettings::DisplayMode) response.getChannelAnalyzerSettings()->getScopeConfig()->getDisplayMode();
    }
    // TODO ...
    for (int i = 0; i < 10; i++) // no more than 10 traces anyway
    {
        // TODO ...
    }
}

int ChannelAnalyzerWebAPIAdapter::qColorToInt(const QColor& color)
{
    return 256*256*color.blue() + 256*color.green() + color.red();
}

QColor ChannelAnalyzerWebAPIAdapter::intToQColor(int intColor)
{
    int r = intColor % 256;
    int bg = intColor / 256;
    int g = bg % 256;
    int b = bg / 256;
    return QColor(r, g, b);
}