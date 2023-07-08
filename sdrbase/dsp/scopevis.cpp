///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <QtGlobal>
#include <QDebug>
#include <QMutexLocker>

#include "scopevis.h"
#include "spectrumvis.h"
#include "dsp/dspcommands.h"
#include "dsp/glscopeinterface.h"

MESSAGE_CLASS_DEFINITION(ScopeVis::MsgConfigureScopeVis, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisAddTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisChangeTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisRemoveTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisMoveTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisFocusOnTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisAddTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisChangeTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisRemoveTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisMoveTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisFocusOnTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisNGOneShot, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisNGMemoryTrace, Message)


ScopeVis::ScopeVis() :
    m_glScope(nullptr),
    m_spectrumVis(nullptr),
    m_ssbSpectrum(false),
    m_preTriggerDelay(0),
    m_livePreTriggerDelay(0),
    m_currentTriggerIndex(0),
    m_focusedTriggerIndex(0),
    m_triggerState(TriggerUntriggered),
    m_focusedTraceIndex(0),
    m_nbStreams(1),
    m_traceChunkSize(GLScopeSettings::m_traceChunkDefaultSize),
    m_traceSize(GLScopeSettings::m_traceChunkDefaultSize),
    m_liveTraceSize(GLScopeSettings::m_traceChunkDefaultSize),
    m_nbSamples(0),
    m_timeBase(1),
    m_timeOfsProMill(0),
    m_traceStart(true),
    m_triggerLocation(0),
    m_sampleRate(0),
    m_liveSampleRate(0),
    m_traceDiscreteMemory(GLScopeSettings::m_nbTraceMemories),
    m_freeRun(true),
    m_maxTraceDelay(0),
    m_triggerOneShot(false),
    m_triggerWaitForReset(false),
    m_currentTraceMemoryIndex(0)
{
    setObjectName("ScopeVis");
    m_traceDiscreteMemory.resize(GLScopeSettings::m_traceChunkDefaultSize); // arbitrary
    m_convertBuffers.resize(GLScopeSettings::m_traceChunkDefaultSize);

    for (int i = 0; i < (int) Projector::nbProjectionTypes; i++) {
        m_projectorCache[i] = 0.0;
    }

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

ScopeVis::~ScopeVis()
{
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    for (std::vector<TriggerCondition*>::iterator it = m_triggerConditions.begin(); it != m_triggerConditions.end(); ++ it) {
        delete *it;
    }
}

void ScopeVis::setGLScope(GLScopeInterface* glScope)
{
    m_glScope = glScope;
    m_glScope->setTraces(&m_traces.m_tracesData, &m_traces.m_traces[0]);
}

void ScopeVis::setLiveRate(int sampleRate)
{
    m_liveSampleRate = sampleRate;

    if (m_currentTraceMemoryIndex == 0) { // update only in live mode
        setSampleRate(m_liveSampleRate);
    }
}

void ScopeVis::setSampleRate(int sampleRate)
{
    qDebug("ScopeVis::setSampleRate: %d S/s", sampleRate);
    m_sampleRate = sampleRate;

    if (m_glScope) {
        m_glScope->setSampleRate(m_sampleRate);
    }
}

void ScopeVis::setTraceSize(uint32_t traceSize, bool emitSignal)
{
    m_traceSize = traceSize;
    m_traces.resize(m_traceSize);
    m_traceDiscreteMemory.resize(m_traceSize);
    initTraceBuffers();

    if (m_glScope) {
        m_glScope->setTraceSize(m_traceSize, emitSignal);
    }
}

void ScopeVis::setPreTriggerDelay(uint32_t preTriggerDelay, bool emitSignal)
{
    m_preTriggerDelay = preTriggerDelay;

    if (m_glScope) {
        m_glScope->setTriggerPre(m_preTriggerDelay, emitSignal);
    }
}

void ScopeVis::setNbStreams(uint32_t nbStreams)
{
    QMutexLocker configLocker(&m_mutex);

    if (m_nbStreams != nbStreams)
    {
        m_traceDiscreteMemory.setNbStreams(nbStreams);
        m_convertBuffers.setNbStreams(nbStreams);
        m_nbStreams = nbStreams;
    }
}

void ScopeVis::configure(
    uint32_t traceSize,
    uint32_t timeBase,
    uint32_t timeOfsProMill,
    uint32_t triggerPre,
    bool freeRun
)
{
    QMutexLocker configLocker(&m_mutex);

    if (m_traceSize != traceSize)
    {
        setTraceSize(traceSize);
        m_settings.m_traceLenMult = traceSize / getTraceChunkSize();
        m_triggerState = TriggerUntriggered;
        m_traces.resetControls();
    }

    if (m_timeBase != timeBase)
    {
        m_timeBase = timeBase;
        m_settings.m_time = timeBase;

        if (m_glScope) {
            m_glScope->setTimeBase(m_timeBase);
        }
    }

    if (m_timeOfsProMill != timeOfsProMill)
    {
        m_timeOfsProMill = timeOfsProMill;
        m_settings.m_timeOfs = timeOfsProMill;

        if (m_glScope) {
            m_glScope->setTimeOfsProMill(m_timeOfsProMill);
        }
    }

    if (m_preTriggerDelay != triggerPre)
    {
        setPreTriggerDelay(triggerPre);
        m_settings.m_trigPre = triggerPre;
    }

    if (freeRun != m_freeRun) {
        m_freeRun = freeRun;
    }

    qDebug() << "ScopeVis::configure:"
        << " m_nbStreams: " << m_nbStreams
        << " m_traceSize: " << m_traceSize
        << " m_timeOfsProMill: " << m_timeOfsProMill
        << " m_preTriggerDelay: " << m_preTriggerDelay
        << " m_freeRun: " << m_freeRun;

    if ((m_glScope) && (m_currentTraceMemoryIndex > 0)) {
        processMemoryTrace();
    }
}

void ScopeVis::configure(
    GLScopeSettings::DisplayMode displayMode,
    uint32_t traceIntensity,
    uint32_t gridIntensity
)
{
    QMutexLocker configLocker(&m_mutex);

    m_settings.m_displayMode = displayMode;
    m_settings.m_traceIntensity = traceIntensity;
    m_settings.m_gridIntensity = gridIntensity;

    qDebug() << "ScopeVis::configure:"
        << " displayMode: " << displayMode
        << " traceIntensity: " << traceIntensity
        << " gridIntensity: " << gridIntensity;
}

void ScopeVis::addTrace(const GLScopeSettings::TraceData& traceData)
{
    qDebug() << "ScopeVis::addTrace:"
            << " trace: " << m_traces.size()
            << " m_streamIndex: " << traceData.m_streamIndex
            << " m_amp: " << traceData.m_amp
            << " m_ofs: " << traceData.m_ofs
            << " m_traceDelay: " << traceData.m_traceDelay;
    m_traces.addTrace(traceData, m_traceSize);
    initTraceBuffers();
    updateMaxTraceDelay();
    computeDisplayTriggerLevels();
    updateGLScopeDisplay();
    m_settings.m_tracesData.push_back(traceData);
}

void ScopeVis::changeTrace(const GLScopeSettings::TraceData& traceData, uint32_t traceIndex)
{
    qDebug() << "ScopeVis::changeTrace:"
            << " trace: " << traceIndex
            << " m_streamIndex: " << traceData.m_streamIndex
            << " m_amp: " << traceData.m_amp
            << " m_ofs: " << traceData.m_ofs
            << " m_traceDelay: " << traceData.m_traceDelay;
    bool doComputeTriggerLevelsOnDisplay = m_traces.isVerticalDisplayChange(traceData, traceIndex);
    m_traces.changeTrace(traceData, traceIndex);
    updateMaxTraceDelay();

    if (doComputeTriggerLevelsOnDisplay) {
        computeDisplayTriggerLevels();
    }

    updateGLScopeDisplay();

    if (traceIndex < m_settings.m_tracesData.size()) {
        m_settings.m_tracesData[traceIndex] = traceData;
    }
}

void ScopeVis::removeTrace(uint32_t traceIndex)
{
    qDebug() << "ScopeVis::removeTrace:"
            << " trace: " << traceIndex;
    m_traces.removeTrace(traceIndex);
    updateMaxTraceDelay();
    computeDisplayTriggerLevels();
    updateGLScopeDisplay();

    unsigned int iDest = 0;

    for (unsigned int iSource = 0; iSource < m_settings.m_tracesData.size(); iSource++)
    {
        if (iSource != traceIndex) {
            m_settings.m_tracesData[iDest++] = m_settings.m_tracesData[iSource];
        }
    }

    if (m_settings.m_tracesData.size() != 0) {
        m_settings.m_tracesData.pop_back();
    }
}

void ScopeVis::moveTrace(uint32_t traceIndex, bool upElseDown)
{
    qDebug() << "ScopeVis::moveTrace:"
            << " trace: " << traceIndex
            << " up: " << upElseDown;
    m_traces.moveTrace(traceIndex, upElseDown);
    computeDisplayTriggerLevels();
    updateGLScopeDisplay();

    int nextTraceIndex = (traceIndex + (upElseDown ? 1 : -1)) % m_settings.m_tracesData.size();
    GLScopeSettings::TraceData nextTraceData = m_settings.m_tracesData[nextTraceIndex];
    m_settings.m_tracesData[nextTraceIndex] = m_settings.m_tracesData[traceIndex];
    m_settings.m_tracesData[traceIndex] = nextTraceData;
}

void ScopeVis::focusOnTrace(uint32_t traceIndex)
{
    if (traceIndex < m_traces.m_tracesData.size())
    {
        m_focusedTraceIndex = traceIndex;
        computeDisplayTriggerLevels();

        if (m_glScope) {
            m_glScope->setFocusedTraceIndex(m_focusedTraceIndex);
        }

        updateGLScopeDisplay();
    }
}

void ScopeVis::addTrigger(const GLScopeSettings::TriggerData& triggerData)
{
    m_triggerConditions.push_back(new TriggerCondition(triggerData));
    m_triggerConditions.back()->initProjector();
    m_settings.m_triggersData.push_back(triggerData);
}

void ScopeVis::changeTrigger(const GLScopeSettings::TriggerData& triggerData, uint32_t triggerIndex)
{
    if (triggerIndex < m_triggerConditions.size())
    {
        m_triggerConditions[triggerIndex]->setData(triggerData);

        if (triggerIndex == m_focusedTriggerIndex)
        {
            computeDisplayTriggerLevels();

            if (m_glScope) {
                m_glScope->setFocusedTriggerData(m_triggerConditions[m_focusedTriggerIndex]->m_triggerData);
            }

            updateGLScopeDisplay();
        }
    }

    if (triggerIndex < m_settings.m_triggersData.size()) {
        m_settings.m_triggersData[triggerIndex] = triggerData;
    }
}

void ScopeVis::removeTrigger(uint32_t triggerIndex)
{
    if (triggerIndex < m_triggerConditions.size())
    {
        TriggerCondition *triggerCondition = m_triggerConditions[triggerIndex];
        m_triggerConditions.erase(m_triggerConditions.begin() + triggerIndex);
        delete triggerCondition;
    }

    unsigned int iDest = 0;

    for (unsigned int iSource = 0; iSource < m_settings.m_triggersData.size(); iSource++)
    {
        if (iSource != triggerIndex) {
            m_settings.m_triggersData[iDest++] = m_settings.m_triggersData[iSource];
        }
    }

    if (m_settings.m_triggersData.size() != 0) {
        m_settings.m_triggersData.pop_back();
    }
}

void ScopeVis::moveTrigger(uint32_t triggerIndex, bool upElseDown)
{
    int nextTriggerIndex = (triggerIndex + (upElseDown ? 1 : -1)) % m_triggerConditions.size();

    TriggerCondition *nextTrigger = m_triggerConditions[nextTriggerIndex];
    m_triggerConditions[nextTriggerIndex] = m_triggerConditions[triggerIndex];
    m_triggerConditions[triggerIndex] = nextTrigger;

    computeDisplayTriggerLevels();

    if (m_glScope) {
        m_glScope->setFocusedTriggerData(m_triggerConditions[m_focusedTriggerIndex]->m_triggerData);
    }

    updateGLScopeDisplay();

    int nextTriggerIndexSettings = (triggerIndex + (upElseDown ? 1 : -1)) % m_settings.m_triggersData.size();
    GLScopeSettings::TriggerData nextTriggerData = m_settings.m_triggersData[nextTriggerIndexSettings];
    m_settings.m_triggersData[nextTriggerIndexSettings] = m_settings.m_triggersData[triggerIndex];
    m_settings.m_triggersData[triggerIndex] = nextTriggerData;
}

void ScopeVis::focusOnTrigger(uint32_t triggerIndex)
{
    if (triggerIndex < m_triggerConditions.size())
    {
        m_focusedTriggerIndex = triggerIndex;
        computeDisplayTriggerLevels();

        if (m_glScope) {
            m_glScope->setFocusedTriggerData(m_triggerConditions[m_focusedTriggerIndex]->m_triggerData);
        }

        updateGLScopeDisplay();
    }
}

void ScopeVis::setOneShot(bool oneShot)
{
    Message* cmd = MsgScopeVisNGOneShot::create(oneShot);
    getInputMessageQueue()->push(cmd);
}

void ScopeVis::setMemoryIndex(uint32_t memoryIndex)
{
    Message* cmd = MsgScopeVisNGMemoryTrace::create(memoryIndex);
    getInputMessageQueue()->push(cmd);
}

void ScopeVis::feed(const std::vector<SampleVector::const_iterator>& vbegin, int nbSamples)
{
    std::vector<ComplexVector::const_iterator> vcbegin;
    std::vector<ComplexVector>& convertBuffers = m_convertBuffers.getBuffers();

    if (nbSamples > (int) m_convertBuffers.size()) {
        m_convertBuffers.resize(nbSamples);
    }

    for (unsigned int s = 0; s < vbegin.size(); s++)
    {
        std::transform(
            vbegin[s],
            vbegin[s] + nbSamples,
            convertBuffers[s].begin(),
            [](const Sample& s) -> Complex {
                return Complex{s.m_real / SDR_RX_SCALEF, s.m_imag / SDR_RX_SCALEF};
            }
        );
        vcbegin.push_back(convertBuffers[s].begin());
    }

    feed(vcbegin, nbSamples);
}

void ScopeVis::feed(const std::vector<ComplexVector::const_iterator>& vbegin, int nbSamples)
{
    if (vbegin.size() == 0) {
        return;
    }

    if (m_currentTraceMemoryIndex > 0) { // in memory mode live trace is suspended
        return;
    }

    if (!m_mutex.tryLock(0)) { // prevent conflicts with configuration process
        return;
    }

    if (m_triggerWaitForReset)
    {
        m_triggerLocation = 0;
        m_mutex.unlock();
        return;
    }

    if (m_freeRun) {
        m_triggerLocation = nbSamples;
    }
    else if (m_triggerState == TriggerTriggered) {
        m_triggerLocation = nbSamples;
    }
    else if (m_triggerState == TriggerUntriggered) {
        m_triggerLocation = 0;
    }
    else {
        m_triggerLocation = nbSamples;
    }

    // ComplexVector::const_iterator begin(vbegin[0]);
    //const SampleVector::const_iterator end = vbegin[0] + nbSamples;
    int triggerPointToEnd;
    int remainder = nbSamples;
    std::vector<ComplexVector::const_iterator> nvbegin(vbegin);

    //while (begin < end)
    while (remainder > 0)
    {
        if (remainder < (int) m_traceSize) // buffer smaller than trace size (end - bagin) < m_traceSize
        {
            triggerPointToEnd = -1;
            processTrace(nvbegin, remainder, triggerPointToEnd); // use all buffer
            m_triggerLocation = triggerPointToEnd < 0 ? 0 : triggerPointToEnd; // trim negative values
            m_triggerLocation = m_triggerLocation > remainder ? remainder : m_triggerLocation; // trim past begin values

            remainder = 0; // effectively breaks out the loop
        }
        else // trace size fits in buffer
        {
            triggerPointToEnd = -1;
            processTrace(nvbegin, m_traceSize, triggerPointToEnd); // use part of buffer to fit trace size
            //m_triggerPoint = begin + m_traceSize - triggerPointToEnd;
            m_triggerLocation = remainder + m_traceSize - triggerPointToEnd; // should always refer to end iterator
            m_triggerLocation = m_triggerLocation < 0 ? 0 : m_triggerLocation; // trim negative values
            m_triggerLocation = m_triggerLocation > remainder ? remainder : m_triggerLocation; // trim past begin values

            for (auto begin : nvbegin) {
                begin += m_traceSize;
            }

            remainder -= m_traceSize;
        }
    }

    m_mutex.unlock();
}

void ScopeVis::processMemoryTrace()
{
    if ((m_currentTraceMemoryIndex > 0) && (m_currentTraceMemoryIndex <= m_traceDiscreteMemory.maxIndex()))
    {
        int traceMemoryIndex = m_traceDiscreteMemory.currentIndex() - m_currentTraceMemoryIndex; // actual index in memory bank

        if (traceMemoryIndex < 0) {
            traceMemoryIndex += GLScopeSettings::m_nbTraceMemories;
        }

        std::vector<ComplexVector::const_iterator> mend;
        m_traceDiscreteMemory.getEndPointAt(traceMemoryIndex, mend);
        std::vector<ComplexVector::const_iterator> mbegin(mend.size());
        TraceBackDiscreteMemory::moveIt(mend, mbegin, -m_traceSize);
        std::vector<ComplexVector::const_iterator> mbegin_tb(mbegin.size());
        TraceBackDiscreteMemory::moveIt(mbegin, mbegin_tb, -m_maxTraceDelay);
        m_nbSamples = m_traceSize + m_maxTraceDelay;

        processTraces(mbegin_tb, m_maxTraceDelay, true); // traceback
        processTraces(mbegin, m_traceSize, false);
    }
}

void ScopeVis::processTrace(const std::vector<ComplexVector::const_iterator>& vcbegin, int length, int& triggerPointToEnd)
{
    std::vector<ComplexVector::const_iterator> vbegin(vcbegin);
    int firstRemainder = length;

    // memory storage

    m_traceDiscreteMemory.writeCurrent(vbegin, length);

    // Removed in 4.2.4 may cause trigger bug
    // if (m_traceDiscreteMemory.current().absoluteFill() < m_traceSize)
    // {
    //     return; // not enough samples in memory
    // }

    // trigger process

    if ((m_freeRun) || (m_triggerConditions.size() == 0)) // immediate re-trigger
    {
        if (m_triggerState == TriggerUntriggered)
        {
            m_traceStart = true; // start trace processing
            m_nbSamples = m_traceSize + m_maxTraceDelay;
            m_triggerState = TriggerTriggered;
        }
    }
    else if ((m_triggerState == TriggerUntriggered) || (m_triggerState == TriggerDelay)) // look for trigger or past trigger in delay mode
    {
        TriggerCondition* triggerCondition = m_triggerConditions[m_currentTriggerIndex]; // current trigger condition
        int processed = 0;

        while (firstRemainder > 0)
        {
            if (m_triggerState == TriggerDelay) // delayed trigger
            {
                if (triggerCondition->m_triggerDelayCount > 0) // skip samples during delay period
                {
                    for (auto begin : vbegin) {
                        begin += triggerCondition->m_triggerDelayCount;
                    }
                    processed += triggerCondition->m_triggerDelayCount;
                    firstRemainder -= triggerCondition->m_triggerDelayCount;
                    triggerCondition->m_triggerDelayCount = 0;
                    continue;
                }
                else // process trigger
                {
                    if (nextTrigger()) // move to next trigger and keep going
                    {
                        m_triggerComparator.reset();
                        m_triggerState = TriggerUntriggered;
                        for (auto begin : vbegin) {
                            ++begin;
                        }
                        ++processed;
                        --firstRemainder;
                        continue;
                    }
                    else // this was the last trigger then start trace
                    {
                        m_traceStart = true; // start trace processing
                        m_nbSamples = m_traceSize + m_maxTraceDelay;
                        m_triggerComparator.reset();
                        m_triggerState = TriggerTriggered;
                        triggerPointToEnd = firstRemainder;
                        break;
                    }
                }
            }

            uint32_t triggerStreamIndex = triggerCondition->m_triggerData.m_streamIndex;
            const Complex& s = *vbegin[triggerStreamIndex];

            if (m_triggerComparator.triggered(s, *triggerCondition)) // matched the current trigger
            {
                if (triggerCondition->m_triggerData.m_triggerDelay > 0)
                {
                    triggerCondition->m_triggerDelayCount = triggerCondition->m_triggerData.m_triggerDelay; // initialize delayed samples counter
                    m_triggerState = TriggerDelay;
                    for (auto begin : vbegin) {
                        ++begin;
                    }
                    ++processed;
                    --firstRemainder;
                    continue;
                }

                if (nextTrigger()) // move to next trigger and keep going
                {
                    m_triggerComparator.reset();
                    m_triggerState = TriggerUntriggered;
                }
                else // this was the last trigger then start trace
                {
                    m_traceStart = true; // start of trace processing
                    m_nbSamples = m_traceSize + m_maxTraceDelay;
                    m_triggerComparator.reset();
                    m_triggerState = TriggerTriggered;
                    triggerPointToEnd = firstRemainder;
                    break;
                }
            }

            for (auto begin : vbegin) {
                ++begin;
            }

            ++processed;
            --firstRemainder;
        } // look for trigger
    } // untriggered or delayed

    // trace process

    if (m_triggerState == TriggerTriggered)
    {
        int remainder;
        int count = firstRemainder; // number of samples in traceback buffer past the current point
        std::vector<ComplexVector::const_iterator> mend;
        m_traceDiscreteMemory.getCurrent(mend);
        std::vector<ComplexVector::const_iterator> mbegin(mend.size());
        TraceBackDiscreteMemory::moveIt(mend, mbegin, -count);

        if (m_traceStart) // start of trace processing
        {
            // if trace time is 1s or more the display is progressive so we have to clear it first

            float traceTime = ((float) m_traceSize) / m_sampleRate;

            if (traceTime >= 1.0f) {
                initTraceBuffers();
            }

            // process until begin point

            if (m_maxTraceDelay > 0)
            { // trace back
                std::vector<ComplexVector::const_iterator> tbegin(mbegin.size());
                TraceBackDiscreteMemory::moveIt(mbegin, tbegin, - m_preTriggerDelay - m_maxTraceDelay);
                processTraces(tbegin, m_maxTraceDelay, true);
            }

            if (m_preTriggerDelay > 0)
            { // pre-trigger
                std::vector<ComplexVector::const_iterator> tbegin(mbegin.size());
                TraceBackDiscreteMemory::moveIt(mbegin, tbegin, -m_preTriggerDelay);
                processTraces(tbegin, m_preTriggerDelay);
            }

            // process the rest of the trace

            remainder = processTraces(mbegin, count);
            m_traceStart = false;
        }
        else // process the current trace
        {
            remainder = processTraces(mbegin, count);
        }

        if (remainder >= 0) // finished
        {
            TraceBackDiscreteMemory::moveIt(mend, mbegin, -remainder);
            m_traceDiscreteMemory.setCurrentEndPoint(mbegin);
            m_traceDiscreteMemory.store(m_preTriggerDelay+remainder); // next memory trace.
            m_triggerState = TriggerUntriggered;
            m_triggerWaitForReset = m_triggerOneShot;

            //if (m_glScope) m_glScope->incrementTraceCounter();

            // process remainder recursively
            if (remainder != 0)
            {
                int mTriggerPointToEnd = -1;
                processTrace(mbegin, remainder, mTriggerPointToEnd);

                if (mTriggerPointToEnd >= 0) {
                    triggerPointToEnd = mTriggerPointToEnd;
                }

                //qDebug("ScopeVis::processTrace: process remainder recursively (%d %d)", mpoint, mTriggerPoint);
            }
        }
    }
}

bool ScopeVis::nextTrigger()
{
    TriggerCondition *triggerCondition = m_triggerConditions[m_currentTriggerIndex]; // current trigger condition

    if (triggerCondition->m_triggerData.m_triggerRepeat > 0)
    {
        if (triggerCondition->m_triggerCounter < triggerCondition->m_triggerData.m_triggerRepeat)
        {
            triggerCondition->m_triggerCounter++;
            return true; // not final keep going
        }
        else
        {
            triggerCondition->m_triggerCounter = 0; // reset for next time
        }
    }

    if (m_triggerConditions.size() == 0)
    {
        m_currentTriggerIndex = 0;
        return false; // final
    }
    else if (m_currentTriggerIndex < m_triggerConditions.size() - 1) // check if next trigger is available
    {
        m_currentTriggerIndex++;
        return true; // not final keep going
    }
    else
    {
        // now this is really finished
        m_currentTriggerIndex = 0;
        return false; // final
    }
}

int ScopeVis::processTraces(const std::vector<ComplexVector::const_iterator>& vcbegin, int ilength, bool traceBack)
{
    std::vector<ComplexVector::const_iterator> vbegin(vcbegin);
    uint32_t shift = (m_timeOfsProMill / 1000.0) * m_traceSize;
    uint32_t length = m_traceSize / m_timeBase;
    int remainder = ilength;

    if (m_spectrumVis) {
        m_spectrumVis->feed(vcbegin[0], vcbegin[0] + ilength, m_ssbSpectrum);
    }

    while ((remainder > 0) && (m_nbSamples > 0))
    {
        std::vector<TraceControl*>::iterator itCtl = m_traces.m_tracesControl.begin();
        std::vector<GLScopeSettings::TraceData>::iterator itData = m_traces.m_tracesData.begin();
        std::vector<float *>::iterator itTrace = m_traces.m_traces[m_traces.currentBufferIndex()].begin();

        for (unsigned int ti = 0; itCtl != m_traces.m_tracesControl.end(); ++itCtl, ++itData, ++itTrace, ti++)
        {
            if (traceBack && ((remainder) > itData->m_traceDelay)) { // before start of trace
                continue;
            }

            Projector::ProjectionType projectionType = itData->m_projectionType;

            if ((*itCtl)->m_traceCount[m_traces.currentBufferIndex()] < m_traceSize)
            {
                uint32_t& traceCount = (*itCtl)->m_traceCount[m_traces.currentBufferIndex()]; // reference for code clarity
                float v;
                uint32_t streamIndex = itData->m_streamIndex;

                if (projectionType == Projector::ProjectionMagLin)
                {
                    v = ((*itCtl)->m_projector.run(*vbegin[streamIndex]) - itData->m_ofs)*itData->m_amp - 1.0f;
                }
                else if (projectionType == Projector::ProjectionMagSq)
                {
                    Real magsq = (*itCtl)->m_projector.run(*vbegin[streamIndex]);
                    v = (magsq - itData->m_ofs)*itData->m_amp - 1.0f;

                    if ((traceCount >= shift) && (traceCount < shift+length)) // power display overlay values construction
                    {
                        if (traceCount == shift)
                        {
                            (*itCtl)->m_maxPow = 0.0f;
                            (*itCtl)->m_sumPow = 0.0f;
                            (*itCtl)->m_nbPow = 1;
                        }

                        if (magsq > 0.0f)
                        {
                            if (magsq > (*itCtl)->m_maxPow)
                            {
                                (*itCtl)->m_maxPow = magsq;
                            }

                            (*itCtl)->m_sumPow += magsq;
                            (*itCtl)->m_nbPow++;
                        }
                    }

                    if ((m_nbSamples == 1) && ((*itCtl)->m_nbPow > 0)) // on last sample create power display overlay
                    {
                        double avgPow = (*itCtl)->m_sumPow / (*itCtl)->m_nbPow;
                        itData->m_textOverlay = QString("%1  %2").arg((*itCtl)->m_maxPow, 0, 'e', 2).arg(avgPow, 0, 'e', 2);
                        (*itCtl)->m_nbPow = 0;
                    }
                }
                else if (projectionType == Projector::ProjectionMagDB)
                {
                    Real re = vbegin[streamIndex]->real();
                    Real im = vbegin[streamIndex]->imag();
                    double magsq = re*re + im*im;
                    float pdB = log10f(magsq) * 10.0f;
                    float p = pdB - (100.0f * itData->m_ofs);
                    v = ((p/50.0f) + 2.0f)*itData->m_amp - 1.0f;

                    if ((traceCount >= shift) && (traceCount < shift+length)) // power display overlay values construction
                    {
                        if (traceCount == shift)
                        {
                            (*itCtl)->m_maxPow = 0.0f;
                            (*itCtl)->m_sumPow = 0.0f;
                            (*itCtl)->m_nbPow = 1;
                        }

                        if (magsq > 0.0f)
                        {
                            if (magsq > (*itCtl)->m_maxPow)
                            {
                                (*itCtl)->m_maxPow = magsq;
                            }

                            (*itCtl)->m_sumPow += magsq;
                            (*itCtl)->m_nbPow++;
                        }
                    }

                    if ((m_nbSamples == 1) && ((*itCtl)->m_nbPow > 0)) // on last sample create power display overlay
                    {
                        double avgPow = log10f((*itCtl)->m_sumPow / (*itCtl)->m_nbPow)*10.0;
                        double peakPow = log10f((*itCtl)->m_maxPow)*10.0;
                        double peakToAvgPow = peakPow - avgPow;
                        itData->m_textOverlay = QString("%1  %2  %3").arg(peakPow, 0, 'f', 1).arg(avgPow, 0, 'f', 1).arg(peakToAvgPow, 4, 'f', 1, ' ');
                        (*itCtl)->m_nbPow = 0;
                    }
                }
                else
                {
                    v = ((*itCtl)->m_projector.run(*vbegin[streamIndex]) - itData->m_ofs) * itData->m_amp;
                }

                if(v > 1.0f) {
                    v = 1.0f;
                } else if (v < -1.0f) {
                    v = -1.0f;
                }

                (*itTrace)[2*traceCount]
                           = traceCount - shift;   // display x
                (*itTrace)[2*traceCount + 1] = v;  // display y
                traceCount++;
            } // process one sample
        } // loop on traces

        for (unsigned int i = 0; i < vbegin.size(); i++) {
            ++vbegin[i];
        }
        remainder--;
        m_nbSamples--;
    } // loop on samples

    float traceTime = ((float) m_traceSize) / m_sampleRate;

    if (m_glScope && (traceTime >= 1.0f)) { // display continuously if trace time is 1 second or more
        m_glScope->newTraces(m_traces.m_traces, m_traces.currentBufferIndex(), &m_traces.m_projectionTypes);
    }

    if (m_glScope && (m_nbSamples == 0)) // finished
    {
        // display only at trace end if trace time is less than 1 second
        if (traceTime < 1.0f)
        {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            if (m_glScope->getProcessingTraceIndex().loadRelaxed() < 0) {
                m_glScope->newTraces(m_traces.m_traces, m_traces.currentBufferIndex(), &m_traces.m_projectionTypes);
            }
#else
            if (m_glScope->getProcessingTraceIndex().load() < 0) {
                m_glScope->newTraces(m_traces.m_traces, m_traces.currentBufferIndex(), &m_traces.m_projectionTypes);
            }
#endif
        }

        // switch to next buffer only if it is not being processed by the scope
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        if (m_glScope->getProcessingTraceIndex().loadRelaxed() != (((int) m_traces.currentBufferIndex() + 1) % 2)) {
            m_traces.switchBuffer();
        }
#else
        if (m_glScope->getProcessingTraceIndex().load() != (((int) m_traces.currentBufferIndex() + 1) % 2)) {
            m_traces.switchBuffer();
        }
#endif

        return remainder; // return remainder count
    }
    else
    {
        return -1; // mark not finished
    }
}

void ScopeVis::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool ScopeVis::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        setLiveRate(notif.getSampleRate());
        qDebug() << "ScopeVis::handleMessage: DSPSignalNotification: m_sampleRate: " << m_sampleRate;
        return true;
    }
    else if (MsgConfigureScopeVis::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        const MsgConfigureScopeVis& cmd = (const MsgConfigureScopeVis&) message;
        applySettings(cmd.getSettings(), cmd.getForce());
        return true;
    }
    else if (MsgScopeVisAddTrigger::match(message))
    {
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisAddTrigger";
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisAddTrigger& conf = (MsgScopeVisAddTrigger&) message;
        addTrigger(conf.getTriggerData());
        return true;
    }
    else if (MsgScopeVisChangeTrigger::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisChangeTrigger& conf = (MsgScopeVisChangeTrigger&) message;
        uint32_t triggerIndex = conf.getTriggerIndex();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisChangeTrigger: " << triggerIndex;
        changeTrigger(conf.getTriggerData(), triggerIndex);
        return true;
    }
    else if (MsgScopeVisRemoveTrigger::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisRemoveTrigger& conf = (MsgScopeVisRemoveTrigger&) message;
        uint32_t triggerIndex = conf.getTriggerIndex();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisRemoveTrigger: " << triggerIndex;
        removeTrigger(triggerIndex);
        return true;
    }
    else if (MsgScopeVisMoveTrigger::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisMoveTrigger& conf = (MsgScopeVisMoveTrigger&) message;
        int triggerIndex = conf.getTriggerIndex();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisMoveTrigger: " << triggerIndex;

        if (!conf.getMoveUp() && (triggerIndex == 0)) {
            return true;
        }

        moveTrigger(triggerIndex, conf.getMoveUp());
        return true;
    }
    else if (MsgScopeVisFocusOnTrigger::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisFocusOnTrigger& conf = (MsgScopeVisFocusOnTrigger&) message;
        uint32_t triggerIndex = conf.getTriggerIndex();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisFocusOnTrigger: " << triggerIndex;
        focusOnTrigger(triggerIndex);
        return true;
    }
    else if (MsgScopeVisAddTrace::match(message))
    {
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisAddTrace";
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisAddTrace& conf = (MsgScopeVisAddTrace&) message;
        addTrace(conf.getTraceData());
        return true;
    }
    else if (MsgScopeVisChangeTrace::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisChangeTrace& conf = (MsgScopeVisChangeTrace&) message;
        uint32_t traceIndex = conf.getTraceIndex();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisChangeTrace: " << traceIndex;
        changeTrace(conf.getTraceData(), traceIndex);
        return true;
    }
    else if (MsgScopeVisRemoveTrace::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisRemoveTrace& conf = (MsgScopeVisRemoveTrace&) message;
        uint32_t traceIndex = conf.getTraceIndex();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisRemoveTrace: " << traceIndex;
        removeTrace(traceIndex);
        return true;
    }
    else if (MsgScopeVisMoveTrace::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisMoveTrace& conf = (MsgScopeVisMoveTrace&) message;
        uint32_t traceIndex = conf.getTraceIndex();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisMoveTrace: " << traceIndex;
        moveTrace(traceIndex, conf.getMoveUp());
        return true;
    }
    else if (MsgScopeVisFocusOnTrace::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisFocusOnTrace& conf = (MsgScopeVisFocusOnTrace&) message;
        uint32_t traceIndex = conf.getTraceIndex();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisFocusOnTrace: " << traceIndex;
        focusOnTrace(traceIndex);
        return true;
    }
    else if (MsgScopeVisNGOneShot::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisNGOneShot& conf = (MsgScopeVisNGOneShot&) message;
        bool oneShot = conf.getOneShot();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisNGOneShot: oneShot:" << oneShot;
        m_triggerOneShot = oneShot;

        if (m_triggerWaitForReset && !oneShot) {
            m_triggerWaitForReset = false;
        }

        return true;
    }
    else if (MsgScopeVisNGMemoryTrace::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisNGMemoryTrace& conf = (MsgScopeVisNGMemoryTrace&) message;
        uint32_t memoryIndex = conf.getMemoryIndex();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisNGMemoryTrace: " << memoryIndex;

        if (memoryIndex != m_currentTraceMemoryIndex)
        {
            // transition from live mode
            if (m_currentTraceMemoryIndex == 0)
            {
                m_liveTraceSize = m_traceSize;
                m_livePreTriggerDelay = m_preTriggerDelay;
            }

            m_currentTraceMemoryIndex = memoryIndex;

            // transition to live mode
            if (m_currentTraceMemoryIndex == 0)
            {
                setLiveRate(m_liveSampleRate); // reset to live rate
                setTraceSize(m_liveTraceSize, true); // reset to live trace size
                setPreTriggerDelay(m_livePreTriggerDelay, true); // reset to live pre-trigger delay
            }
            else
            {
                processMemoryTrace();
            }
        }
        return true;
    }
    else
    {
        qDebug() << "ScopeVis::handleMessage" << message.getIdentifier() << " not handled";
        return false;
    }
}

void ScopeVis::applySettings(const GLScopeSettings& settings, bool force)
{
    (void) force;

    if (m_traces.size() > m_settings.m_tracesData.size())
    {
        for (unsigned int i = m_traces.size(); i > m_settings.m_tracesData.size(); i--) {
            removeTrace(i-1);
        }
    }

    for (unsigned int i = 0; i < m_settings.m_tracesData.size(); i++)
    {
        if (i < m_traces.size()) { // change trace
            changeTrace(settings.m_tracesData[i], i);
        } else {  // add trace
            addTrace(settings.m_tracesData[i]);
        }
    }

    m_settings = settings;
}

void ScopeVis::updateMaxTraceDelay()
{
    int maxTraceDelay = 0;
    bool allocateCache = false;
    uint32_t projectorCounts[(int) Projector::nbProjectionTypes];
    memset(projectorCounts, 0, ((int) Projector::nbProjectionTypes)*sizeof(uint32_t));
    std::vector<GLScopeSettings::TraceData>::iterator itData = m_traces.m_tracesData.begin();
    std::vector<TraceControl*>::iterator itCtrl = m_traces.m_tracesControl.begin();

    for (; itData != m_traces.m_tracesData.end(); ++itData, ++itCtrl)
    {
        if (itData->m_traceDelay > maxTraceDelay)
        {
            maxTraceDelay = itData->m_traceDelay;
        }

        if (itData->m_projectionType < 0) {
            itData->m_projectionType = Projector::ProjectionReal;
        }

        if (m_nbStreams <= 1) // Works only for single stream mode. Fixes #872
        {
            if (projectorCounts[(int) itData->m_projectionType] > 0)
            {
                allocateCache = true;
                (*itCtrl)->m_projector.setCacheMaster(false);
            }
            else
            {
                (*itCtrl)->m_projector.setCacheMaster(true);
            }
        }

        projectorCounts[(int) itData->m_projectionType]++;
    }

    itCtrl = m_traces.m_tracesControl.begin();

    for (; itCtrl != m_traces.m_tracesControl.end(); ++itCtrl)
    {
        if (allocateCache) {
            (*itCtrl)->m_projector.setCache(m_projectorCache);
        } else {
            (*itCtrl)->m_projector.setCache(nullptr);
        }
    }

    m_maxTraceDelay = maxTraceDelay;
}

void ScopeVis::initTraceBuffers()
{
    int shift = (m_timeOfsProMill / 1000.0) * m_traceSize;

    std::vector<float *>::iterator it0 = m_traces.m_traces[0].begin();
    std::vector<float *>::iterator it1 = m_traces.m_traces[1].begin();

    for (; it0 != m_traces.m_traces[0].end(); ++it0, ++it1)
    {
        for (unsigned int i = 0; i < m_traceSize; i++)
        {
            (*it0)[2*i] = (i - shift); // display x
            (*it0)[2*i + 1] = 0.0f;    // display y
            (*it1)[2*i] = (i - shift); // display x
            (*it1)[2*i + 1] = 0.0f;    // display y
        }
    }
}

void ScopeVis::computeDisplayTriggerLevels()
{
    std::vector<GLScopeSettings::TraceData>::iterator itData = m_traces.m_tracesData.begin();

    for (; itData != m_traces.m_tracesData.end(); ++itData)
    {
        if ((m_focusedTriggerIndex < m_triggerConditions.size()) && (m_triggerConditions[m_focusedTriggerIndex]->m_projector.getProjectionType() == itData->m_projectionType))
        {
            float level = m_triggerConditions[m_focusedTriggerIndex]->m_triggerData.m_triggerLevel;
            float levelPowerLin = level + 1.0f;
            float levelPowerdB = (100.0f * (level - 1.0f));
            float v;

            if ((itData->m_projectionType == Projector::ProjectionMagLin) || (itData->m_projectionType == Projector::ProjectionMagSq))
            {
                v = (levelPowerLin - itData->m_ofs)*itData->m_amp - 1.0f;
            }
            else if (itData->m_projectionType == Projector::ProjectionMagDB)
            {
                float ofsdB = itData->m_ofs * 100.0f;
                v = ((levelPowerdB + 100.0f - ofsdB)*itData->m_amp)/50.0f - 1.0f;
            }
            else
            {
                v = (level - itData->m_ofs) * itData->m_amp;
            }

            if(v > 1.0f) {
                v = 1.0f;
            } else if (v < -1.0f) {
                v = -1.0f;
            }

            itData->m_triggerDisplayLevel = v;
        }
        else
        {
            itData->m_triggerDisplayLevel = 2.0f;
        }
    }
}

void ScopeVis::updateGLScopeDisplay()
{
    if (!m_glScope) {
        return;
    }

    if (m_currentTraceMemoryIndex > 0)
    {
        m_glScope->setConfigChanged();
        processMemoryTrace();
    }
    else
    {
        m_glScope->updateDisplay();
    }
}
