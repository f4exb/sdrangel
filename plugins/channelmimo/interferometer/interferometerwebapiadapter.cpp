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
#include "interferometerwebapiadapter.h"

InterferometerWebAPIAdapter::InterferometerWebAPIAdapter()
{
    m_settings.setScopeGUI(&m_glScopeSettings);
    m_settings.setSpectrumGUI(&m_SpectrumSettings);
}

InterferometerWebAPIAdapter::~InterferometerWebAPIAdapter()
{}

int InterferometerWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setInterferometerSettings(new SWGSDRangel::SWGInterferometerSettings());
    response.getInterferometerSettings()->init();
    webapiFormatChannelSettings(response, m_settings, m_glScopeSettings, m_SpectrumSettings);
    return 200;
}

void InterferometerWebAPIAdapter::webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const InterferometerSettings& settings,
        const GLScopeSettings& scopeSettings,
        const SpectrumSettings& spectrumSettings)
{
    response.getInterferometerSettings()->setCorrelationType((int) settings.m_correlationType);
    response.getInterferometerSettings()->setRgbColor(settings.m_rgbColor);
    response.getInterferometerSettings()->setTitle(new QString(settings.m_title));

    // scope
    SWGSDRangel::SWGGLScope *swgScope = new SWGSDRangel::SWGGLScope();
    swgScope->init();
    response.getInterferometerSettings()->setScopeConfig(swgScope);
    swgScope->setDisplayMode(scopeSettings.m_displayMode);
    swgScope->setGridIntensity(scopeSettings.m_gridIntensity);
    swgScope->setTime(scopeSettings.m_time);
    swgScope->setTimeOfs(scopeSettings.m_timeOfs);
    swgScope->setTraceIntensity(scopeSettings.m_traceIntensity);
    swgScope->setTraceLenMult(scopeSettings.m_traceLenMult);
    swgScope->setTrigPre(scopeSettings.m_trigPre);

    // array of traces
    swgScope->setTracesData(new QList<SWGSDRangel::SWGTraceData *>);
    std::vector<GLScopeSettings::TraceData>::const_iterator traceIt = scopeSettings.m_tracesData.begin();

    for (; traceIt != scopeSettings.m_tracesData.end(); ++traceIt)
    {
        swgScope->getTracesData()->append(new SWGSDRangel::SWGTraceData);
        swgScope->getTracesData()->back()->setAmp(traceIt->m_amp);
        swgScope->getTracesData()->back()->setHasTextOverlay(traceIt->m_hasTextOverlay ? 1 : 0);
        swgScope->getTracesData()->back()->setStreamIndex(traceIt->m_streamIndex);
        swgScope->getTracesData()->back()->setOfs(traceIt->m_ofs);
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
    response.getInterferometerSettings()->setSpectrumConfig(swgSpectrum);
    swgSpectrum->setAveragingMode((int) spectrumSettings.m_averagingMode);
    swgSpectrum->setAveragingValue(SpectrumSettings::getAveragingValue(spectrumSettings.m_averagingIndex, spectrumSettings.m_averagingMode));
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
    swgSpectrum->setFpsPeriodMs(spectrumSettings.m_fpsPeriodMs);
}

int InterferometerWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) force; // no action
    (void) errorMessage;
    webapiUpdateChannelSettings(m_settings, m_glScopeSettings, m_SpectrumSettings, channelSettingsKeys, response);
    return 200;
}

void InterferometerWebAPIAdapter::webapiUpdateChannelSettings(
        InterferometerSettings& settings,
        GLScopeSettings& scopeSettings,
        SpectrumSettings& spectrumSettings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("correlationType")) {
        settings.m_correlationType = (InterferometerSettings::CorrelationType) response.getInterferometerSettings()->getCorrelationType();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getInterferometerSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getInterferometerSettings()->getTitle();
    }
    // scope
    if (channelSettingsKeys.contains("scopeConfig"))
    {
        if (channelSettingsKeys.contains("scopeConfig.displayMode")) {
            scopeSettings.m_displayMode = (GLScopeSettings::DisplayMode) response.getInterferometerSettings()->getScopeConfig()->getDisplayMode();
        }
        if (channelSettingsKeys.contains("scopeConfig.gridIntensity")) {
            scopeSettings.m_gridIntensity = response.getInterferometerSettings()->getScopeConfig()->getGridIntensity();
        }
        if (channelSettingsKeys.contains("scopeConfig.time")) {
            scopeSettings.m_time = response.getInterferometerSettings()->getScopeConfig()->getTime();
        }
        if (channelSettingsKeys.contains("scopeConfig.timeOfs")) {
            scopeSettings.m_timeOfs = response.getInterferometerSettings()->getScopeConfig()->getTimeOfs();
        }
        if (channelSettingsKeys.contains("scopeConfig.traceIntensity")) {
            scopeSettings.m_traceIntensity = response.getInterferometerSettings()->getScopeConfig()->getTraceIntensity();
        }
        if (channelSettingsKeys.contains("scopeConfig.traceLenMult")) {
            scopeSettings.m_traceLenMult = response.getInterferometerSettings()->getScopeConfig()->getTraceLenMult();
        }
        if (channelSettingsKeys.contains("scopeConfig.trigPre")) {
            scopeSettings.m_trigPre = response.getInterferometerSettings()->getScopeConfig()->getTrigPre();
        }
        // traces
        if (channelSettingsKeys.contains("scopeConfig.tracesData"))
        {
            QList<SWGSDRangel::SWGTraceData *> *tracesData = response.getInterferometerSettings()->getScopeConfig()->getTracesData();
            scopeSettings.m_tracesData.clear();

            for (int i = 0; i < 10; i++) // no more than 10 traces anyway
            {
                if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1]").arg(i)))
                {
                    SWGSDRangel::SWGTraceData *traceData = tracesData->at(i);
                    scopeSettings.m_tracesData.push_back(GLScopeSettings::TraceData());

                    if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1].streamIndex").arg(i))) {
                        scopeSettings.m_tracesData.back().m_streamIndex = traceData->getStreamIndex();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1].amp").arg(i))) {
                        scopeSettings.m_tracesData.back().m_amp = traceData->getAmp();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1].hasTextOverlay").arg(i))) {
                        scopeSettings.m_tracesData.back().m_hasTextOverlay = traceData->getHasTextOverlay() != 0;
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1].streamIndex").arg(i))) {
                        scopeSettings.m_tracesData.back().m_streamIndex = traceData->getStreamIndex();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1].ofs").arg(i))) {
                        scopeSettings.m_tracesData.back().m_ofs = traceData->getOfs();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1].projectionType").arg(i))) {
                        scopeSettings.m_tracesData.back().m_projectionType = (Projector::ProjectionType) traceData->getProjectionType();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1].traceColor").arg(i))) {
                        scopeSettings.m_tracesData.back().m_traceColor = intToQColor(traceData->getTraceColor());
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1].traceColorB").arg(i))) {
                        scopeSettings.m_tracesData.back().m_traceColorB = traceData->getTraceColorB();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1].traceColorG").arg(i))) {
                        scopeSettings.m_tracesData.back().m_traceColorG = traceData->getTraceColorG();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1].traceColorR").arg(i))) {
                        scopeSettings.m_tracesData.back().m_traceColorR = traceData->getTraceColorR();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1].traceDelay").arg(i))) {
                        scopeSettings.m_tracesData.back().m_traceDelay = traceData->getTraceDelay();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1].traceDelayCoarse").arg(i))) {
                        scopeSettings.m_tracesData.back().m_traceDelayCoarse = traceData->getTraceDelayCoarse();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1].traceDelayFine").arg(i))) {
                        scopeSettings.m_tracesData.back().m_traceDelayFine = traceData->getTraceDelayFine();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1].triggerDisplayLevel").arg(i))) {
                        scopeSettings.m_tracesData.back().m_triggerDisplayLevel = traceData->getTriggerDisplayLevel();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1].viewTrace").arg(i))) {
                        scopeSettings.m_tracesData.back().m_viewTrace = traceData->getViewTrace() != 0;
                    }
                }
                else
                {
                    break;
                }
            }
        }
        // triggers
        if (channelSettingsKeys.contains("scopeConfig.triggersData"))
        {
            QList<SWGSDRangel::SWGTriggerData *> *triggersData = response.getInterferometerSettings()->getScopeConfig()->getTriggersData();
            scopeSettings.m_triggersData.clear();

            for (int i = 0; i < 10; i++) // no more than 10 triggers anyway
            {
                if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1]").arg(i)))
                {
                    SWGSDRangel::SWGTriggerData *triggerData = triggersData->at(i);
                    scopeSettings.m_triggersData.push_back(GLScopeSettings::TriggerData());

                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].streamIndex").arg(i))) {
                        scopeSettings.m_triggersData.back().m_streamIndex = triggerData->getStreamIndex();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].inputIndex").arg(i))) {
                        scopeSettings.m_triggersData.back().m_inputIndex = triggerData->getInputIndex();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].projectionType").arg(i))) {
                        scopeSettings.m_triggersData.back().m_projectionType = (Projector::ProjectionType) triggerData->getProjectionType();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].triggerBothEdges").arg(i))) {
                        scopeSettings.m_triggersData.back().m_triggerBothEdges = triggerData->getTriggerBothEdges() != 0;
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.tracesData[%1].triggerColor").arg(i))) {
                        scopeSettings.m_tracesData.back().m_traceColor = intToQColor(triggerData->getTriggerColor());
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].triggerColorB").arg(i))) {
                        scopeSettings.m_triggersData.back().m_triggerColorB = triggerData->getTriggerColorB();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].triggerColorG").arg(i))) {
                        scopeSettings.m_triggersData.back().m_triggerColorG = triggerData->getTriggerColorG();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].triggerColorR").arg(i))) {
                        scopeSettings.m_triggersData.back().m_triggerColorR = triggerData->getTriggerColorR();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].triggerDelay").arg(i))) {
                        scopeSettings.m_triggersData.back().m_triggerDelay = triggerData->getTriggerDelay();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].triggerDelayCoarse").arg(i))) {
                        scopeSettings.m_triggersData.back().m_triggerDelayCoarse = triggerData->getTriggerDelayCoarse();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].triggerDelayFine").arg(i))) {
                        scopeSettings.m_triggersData.back().m_triggerDelayFine = triggerData->getTriggerDelayFine();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].triggerDelayMult").arg(i))) {
                        scopeSettings.m_triggersData.back().m_triggerDelayMult = triggerData->getTriggerDelayMult();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].triggerHoldoff").arg(i))) {
                        scopeSettings.m_triggersData.back().m_triggerHoldoff = triggerData->getTriggerHoldoff();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].triggerLevel").arg(i))) {
                        scopeSettings.m_triggersData.back().m_triggerLevel = triggerData->getTriggerLevel();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].triggerLevelCoarse").arg(i))) {
                        scopeSettings.m_triggersData.back().m_triggerLevelCoarse = triggerData->getTriggerLevelCoarse();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].triggerLevelFine").arg(i))) {
                        scopeSettings.m_triggersData.back().m_triggerLevelFine = triggerData->getTriggerLevelFine();
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].triggerPositiveEdge").arg(i))) {
                        scopeSettings.m_triggersData.back().m_triggerPositiveEdge = triggerData->getTriggerPositiveEdge() != 0;
                    }
                    if (channelSettingsKeys.contains(QString("scopeConfig.triggersData[%1].triggerRepeat").arg(i))) {
                        scopeSettings.m_triggersData.back().m_triggerRepeat = triggerData->getTriggerRepeat() != 0;
                    }
                }
            }
        }
    }
    // spectrum
    if (channelSettingsKeys.contains("spectrumConfig"))
    {
        if (channelSettingsKeys.contains("spectrumConfig.averagingMode")) {
            spectrumSettings.m_averagingMode = (SpectrumSettings::AveragingMode) response.getInterferometerSettings()->getSpectrumConfig()->getAveragingMode();
        }
        if (channelSettingsKeys.contains("spectrumConfig.averagingValue"))
        {
            spectrumSettings.m_averagingValue = response.getInterferometerSettings()->getSpectrumConfig()->getAveragingValue();
            spectrumSettings.m_averagingIndex = SpectrumSettings::getAveragingIndex(spectrumSettings.m_averagingValue, spectrumSettings.m_averagingMode);
        }
        if (channelSettingsKeys.contains("spectrumConfig.decay")) {
            spectrumSettings.m_decay = response.getInterferometerSettings()->getSpectrumConfig()->getDecay();
        }
        if (channelSettingsKeys.contains("spectrumConfig.decayDivisor")) {
            spectrumSettings.m_decayDivisor = response.getInterferometerSettings()->getSpectrumConfig()->getDecayDivisor();
        }
        if (channelSettingsKeys.contains("spectrumConfig.displayCurrent")) {
            spectrumSettings.m_displayCurrent = response.getInterferometerSettings()->getSpectrumConfig()->getDisplayCurrent() != 0;
        }
        if (channelSettingsKeys.contains("spectrumConfig.displayGrid")) {
            spectrumSettings.m_displayGrid = response.getInterferometerSettings()->getSpectrumConfig()->getDisplayGrid() != 0;
        }
        if (channelSettingsKeys.contains("spectrumConfig.displayGridIntensity")) {
            spectrumSettings.m_displayGridIntensity = response.getInterferometerSettings()->getSpectrumConfig()->getDisplayGridIntensity();
        }
        if (channelSettingsKeys.contains("spectrumConfig.displayHistogram")) {
            spectrumSettings.m_displayHistogram = response.getInterferometerSettings()->getSpectrumConfig()->getDisplayHistogram() != 0;
        }
        if (channelSettingsKeys.contains("spectrumConfig.displayMaxHold")) {
            spectrumSettings.m_displayMaxHold = response.getInterferometerSettings()->getSpectrumConfig()->getDisplayMaxHold() != 0;
        }
        if (channelSettingsKeys.contains("spectrumConfig.displayTraceIntensity")) {
            spectrumSettings.m_displayTraceIntensity = response.getInterferometerSettings()->getSpectrumConfig()->getDisplayTraceIntensity();
        }
        if (channelSettingsKeys.contains("spectrumConfig.displayWaterfall")) {
            spectrumSettings.m_displayWaterfall = response.getInterferometerSettings()->getSpectrumConfig()->getDisplayWaterfall() != 0;
        }
        if (channelSettingsKeys.contains("spectrumConfig.fftOverlap")) {
            spectrumSettings.m_fftOverlap = response.getInterferometerSettings()->getSpectrumConfig()->getFftOverlap();
        }
        if (channelSettingsKeys.contains("spectrumConfig.fftSize")) {
            spectrumSettings.m_fftSize = response.getInterferometerSettings()->getSpectrumConfig()->getFftSize();
        }
        if (channelSettingsKeys.contains("spectrumConfig.fpsPeriodMs")) {
            spectrumSettings.m_fpsPeriodMs = response.getInterferometerSettings()->getSpectrumConfig()->getFpsPeriodMs();
        }
    }
}

int InterferometerWebAPIAdapter::qColorToInt(const QColor& color)
{
    return 256*256*color.blue() + 256*color.green() + color.red();
}

QColor InterferometerWebAPIAdapter::intToQColor(int intColor)
{
    int r = intColor % 256;
    int bg = intColor / 256;
    int g = bg % 256;
    int b = bg / 256;
    return QColor(r, g, b);
}
