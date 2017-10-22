///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <QDebug>
#include <QMutexLocker>

#include "scopevismulti.h"
#include "dsp/dspcommands.h"
#include "gui/glscopemulti.h"

MESSAGE_CLASS_DEFINITION(ScopeVisMulti::MsgConfigureScopeVisNG, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisMulti::MsgScopeVisNGAddTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisMulti::MsgScopeVisNGChangeTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisMulti::MsgScopeVisNGRemoveTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisMulti::MsgScopeVisNGMoveTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisMulti::MsgScopeVisNGFocusOnTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisMulti::MsgScopeVisNGAddTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisMulti::MsgScopeVisNGChangeTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisMulti::MsgScopeVisNGRemoveTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisMulti::MsgScopeVisNGMoveTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisMulti::MsgScopeVisNGFocusOnTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisMulti::MsgScopeVisNGOneShot, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisMulti::MsgScopeVisNGMemoryTrace, Message)

ScopeVisMulti::ScopeVisMulti(GLScopeMulti* glScope) :
    m_glScope(glScope),
    m_preTriggerDelay(0),
    m_currentTriggerIndex(0),
    m_focusedTriggerIndex(0),
    m_triggerState(TriggerUntriggered),
    m_focusedTraceIndex(0),
    m_traceSize(m_traceChunkSize),
    m_nbSamples(0),
    m_timeBase(1),
    m_timeOfsProMill(0),
    m_traceStart(true),
    m_postTrigBuffering(false),
    m_traceFill(0),
    m_zTraceIndex(-1),
    m_sampleRate(0),
    m_freeRun(true),
    m_maxTraceDelay(0),
    m_triggerOneShot(false),
    m_triggerWaitForReset(false),
    m_currentTraceMemoryIndex(0),
    m_nbSources(0)
{
    setObjectName("ScopeVisNG");
    m_glScope->setTraces(&m_traces.m_tracesData, &m_traces.m_traces[0]);
}

ScopeVisMulti::~ScopeVisMulti()
{
}

void ScopeVisMulti::setSampleRate(int sampleRate)
{
    if (sampleRate != m_sampleRate)
    {
        m_sampleRate = sampleRate;
        if (m_glScope) m_glScope->setSampleRate(m_sampleRate);
    }
}

void ScopeVisMulti::configure(
        uint32_t nbSources,
        uint32_t traceSize,
        uint32_t timeBase,
        uint32_t timeOfsProMill,
        uint32_t triggerPre,
        bool freeRun)
{
    Message* cmd = MsgConfigureScopeVisNG::create(nbSources, traceSize, timeBase, timeOfsProMill, triggerPre, freeRun);
    m_inputMessageQueue.push(cmd);
}

void ScopeVisMulti::addTrace(const TraceData& traceData)
{
    qDebug() << "ScopeVisMulti::addTrace:"
            << " m_amp: " << traceData.m_amp
            << " m_ofs: " << traceData.m_ofs
            << " m_traceDelay: " << traceData.m_traceDelay;
    Message* cmd = MsgScopeVisNGAddTrace::create(traceData);
    m_inputMessageQueue.push(cmd);
}

void ScopeVisMulti::changeTrace(const TraceData& traceData, uint32_t traceIndex)
{
    qDebug() << "ScopeVisMulti::changeTrace:"
            << " trace: " << traceIndex
            << " m_amp: " << traceData.m_amp
            << " m_ofs: " << traceData.m_ofs
            << " m_traceDelay: " << traceData.m_traceDelay;
    Message* cmd = MsgScopeVisNGChangeTrace::create(traceData, traceIndex);
    m_inputMessageQueue.push(cmd);
}

void ScopeVisMulti::removeTrace(uint32_t traceIndex)
{
    qDebug() << "ScopeVisMulti::removeTrace:"
            << " trace: " << traceIndex;
    Message* cmd = MsgScopeVisNGRemoveTrace::create(traceIndex);
    m_inputMessageQueue.push(cmd);
}

void ScopeVisMulti::moveTrace(uint32_t traceIndex, bool upElseDown)
{
    qDebug() << "ScopeVisMulti::moveTrace:"
            << " trace: " << traceIndex
            << " up: " << upElseDown;
    Message* cmd = MsgScopeVisNGMoveTrace::create(traceIndex, upElseDown);
    m_inputMessageQueue.push(cmd);
}

void ScopeVisMulti::focusOnTrace(uint32_t traceIndex)
{
    Message* cmd = MsgScopeVisNGFocusOnTrace::create(traceIndex);
    m_inputMessageQueue.push(cmd);
}

void ScopeVisMulti::addTrigger(const TriggerData& triggerData)
{
    Message* cmd = MsgScopeVisNGAddTrigger::create(triggerData);
    m_inputMessageQueue.push(cmd);
}

void ScopeVisMulti::changeTrigger(const TriggerData& triggerData, uint32_t triggerIndex)
{
    Message* cmd = MsgScopeVisNGChangeTrigger::create(triggerData, triggerIndex);
    m_inputMessageQueue.push(cmd);
}

void ScopeVisMulti::removeTrigger(uint32_t triggerIndex)
{
    Message* cmd = MsgScopeVisNGRemoveTrigger::create(triggerIndex);
    m_inputMessageQueue.push(cmd);
}

void ScopeVisMulti::moveTrigger(uint32_t triggerIndex, bool upElseDown)
{
    Message* cmd = MsgScopeVisNGMoveTrigger::create(triggerIndex, upElseDown);
    m_inputMessageQueue.push(cmd);
}

void ScopeVisMulti::focusOnTrigger(uint32_t triggerIndex)
{
    Message* cmd = MsgScopeVisNGFocusOnTrigger::create(triggerIndex);
    m_inputMessageQueue.push(cmd);
}

void ScopeVisMulti::setOneShot(bool oneShot)
{
    Message* cmd = MsgScopeVisNGOneShot::create(oneShot);
    m_inputMessageQueue.push(cmd);
}

void ScopeVisMulti::setMemoryIndex(uint32_t memoryIndex)
{
    Message* cmd = MsgScopeVisNGMemoryTrace::create(memoryIndex);
    m_inputMessageQueue.push(cmd);
}

void ScopeVisMulti::feed(const SampleVector::const_iterator& cbegin, const SampleVector::const_iterator& end, uint32_t sourceIndex)
{
    if ((m_triggerWaitForReset) || (m_currentTraceMemoryIndex > 0)) {
        return;
    }

    if(!m_mutex.tryLock(2)) { // prevent conflicts with configuration process
        return;
    }

    m_traceDiscreteMemory.feed(cbegin, end, sourceIndex); // memory storage

    if (m_traceDiscreteMemory.minFill() >= m_traceSize)
    {
        if (m_triggerState == TriggerTriggered)
        {
            processSources();
        }
        else
        {
            lookForTrigger();
        }
    }
}

void ScopeVisMulti::lookForTrigger()
{
    int count = 0;

    if ((m_freeRun) || (m_triggerConditions.size() == 0)) // immediate re-trigger
    {
        if (m_triggerState == TriggerUntriggered)
        {
            m_traceStart = true; // start trace processing
            m_nbSamples = m_traceSize + m_maxTraceDelay;
            m_triggerState = TriggerTriggered;
        }
    }
    else if ((m_triggerState == TriggerUntriggered) || (m_triggerState == TriggerDelay))
    {
        TriggerCondition& triggerCondition = m_triggerConditions[m_currentTriggerIndex]; // current trigger condition
        uint32_t sourceIndex = triggerCondition.m_triggerData.m_inputIndex;

        SampleVector::iterator begin = m_traceDiscreteMemory.getBeginIterator(sourceIndex);
        SampleVector::iterator end = begin + m_traceSize;
        SampleVector::const_iterator cbegin = begin;

        while (begin < end)
        {
            if (m_triggerState == TriggerDelay)
            {
                if (triggerCondition.m_triggerDelayCount > 0) // skip samples during delay period
                {
                    triggerCondition.m_triggerDelayCount--;
                    ++begin;
                    continue;
                }
                else // process trigger
                {
                    if (nextTrigger()) // move to next trigger and keep going
                    {
                        m_triggerComparator.reset();
                        m_triggerState = TriggerUntriggered;
                        ++begin;
                        continue;
                    }
                    else // this was the last trigger then start trace
                    {
                        m_traceStart = true; // start trace processing
                        m_nbSamples = m_traceSize + m_maxTraceDelay;
                        m_triggerComparator.reset();
                        m_triggerState = TriggerTriggered;
                        break;
                    }
                }
            }

            // look for trigger
            if (m_triggerComparator.triggered(*begin, triggerCondition))
            {
                if (triggerCondition.m_triggerData.m_triggerDelay > 0)
                {
                    triggerCondition.m_triggerDelayCount = triggerCondition.m_triggerData.m_triggerDelay; // initialize delayed samples counter
                    m_triggerState = TriggerDelay;
                    ++begin;
                    continue;
                }

                if (nextTrigger()) // move to next trigger and keep going
                {
                    m_triggerComparator.reset();
                    m_triggerState = TriggerUntriggered;
                }
                else // this was the last trigger then start trace
                {
                    m_traceStart = true; // start trace processing
                    m_nbSamples = m_traceSize + m_maxTraceDelay;
                    m_triggerComparator.reset();
                    m_triggerState = TriggerTriggered;
                    break;
                }
            }

            ++begin;
        }

        count = begin - cbegin;
    }

    m_traceDiscreteMemory.consume(count);
}

void ScopeVisMulti::processSources()
{
    if (m_glScope->getDataChanged()) // optimization: process trace only if required by glScope
    {
        m_triggerState = TriggerUntriggered;
        return;
    }

    uint32_t nbSources = m_traceDiscreteMemory.getNbSources();

    for (unsigned int is = 0; is < nbSources; is++)
    {
        SampleVector::iterator begin = m_traceDiscreteMemory.getBeginIterator(is); // trigger position
        SampleVector::iterator end = begin + m_traceSize;

        if (m_traceStart)
        {
            // trace back
            if (m_maxTraceDelay > 0)
            {
                processTraces(begin - m_preTriggerDelay - m_maxTraceDelay, begin - m_preTriggerDelay, true, is);
            }

            // pre-trigger
            if (m_preTriggerDelay > 0)
            {
                processTraces(begin - m_preTriggerDelay, begin, false, is);
            }

            m_traceStart = false;
        }

        processTraces(begin, end, false, is);
        m_traceDiscreteMemory.markEnd(is, end);
    } // sources loop

    m_glScope->newTraces(&m_traces.m_traces[m_traces.currentBufferIndex()]);
    m_traces.switchBuffer();
    m_traceDiscreteMemory.store();
}

void ScopeVisMulti::processMemorySources()
{
    if ((m_currentTraceMemoryIndex > 0) && (m_currentTraceMemoryIndex < m_nbTraceMemories))
    {
        uint32_t nbSources = m_traceDiscreteMemory.getNbSources();

        for (unsigned int is = 0; is < nbSources; is++)
        {
            SampleVector::const_iterator mend = m_traceDiscreteMemory.m_traceBackBuffers[is].at(m_currentTraceMemoryIndex).m_endPoint;
            SampleVector::const_iterator mbegin = mend - m_traceSize;
            SampleVector::const_iterator mbegin_tb = mbegin - m_maxTraceDelay;
            processTraces(mbegin_tb, mbegin, true, is); // traceback
            processTraces(mbegin, mend, false, is);
        }
    }
}


bool ScopeVisMulti::nextTrigger()
{
    TriggerCondition& triggerCondition = m_triggerConditions[m_currentTriggerIndex]; // current trigger condition

    if (triggerCondition.m_triggerData.m_triggerRepeat > 0)
    {
        if (triggerCondition.m_triggerCounter < triggerCondition.m_triggerData.m_triggerRepeat)
        {
            triggerCondition.m_triggerCounter++;
            return true; // not final keep going
        }
        else
        {
            triggerCondition.m_triggerCounter = 0; // reset for next time
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

void ScopeVisMulti::processTraces(const SampleVector::const_iterator& cbegin, const SampleVector::const_iterator& end, bool traceBack, uint32_t sourceIndex)
{
    SampleVector::const_iterator begin(cbegin);
    uint32_t shift = (m_timeOfsProMill / 1000.0) * m_traceSize;
    uint32_t length = m_traceSize / m_timeBase;
    int nbSamples = end - begin;

    while (begin < end)
    {
        std::vector<TraceControl>::iterator itCtl = m_traces.m_tracesControl.begin();
        std::vector<TraceData>::iterator itData = m_traces.m_tracesData.begin();
        std::vector<float *>::iterator itTrace = m_traces.m_traces[m_traces.currentBufferIndex()].begin();

        for (; itCtl != m_traces.m_tracesControl.end(); ++itCtl, ++itData, ++itTrace)
        {
            if (itData->m_inputIndex != sourceIndex) {
                continue;
            }

            if (traceBack && ((end - begin) > itData->m_traceDelay)) { // before start of trace
                continue;
            }

            ProjectionType projectionType = itData->m_projectionType;

            if (itCtl->m_traceCount[m_traces.currentBufferIndex()] < m_traceSize)
            {
                uint32_t& traceCount = itCtl->m_traceCount[m_traces.currentBufferIndex()]; // reference for code clarity
                float v;

                if (projectionType == ProjectionMagLin)
                {
                    v = (itCtl->m_projector.run(*begin) - itData->m_ofs)*itData->m_amp - 1.0f;
                }
                else if (projectionType == ProjectionMagDB)
                {
                   // there is no processing advantage in direct calculation without projector
//                    uint32_t magsq = begin->m_real*begin->m_real + begin->m_imag*begin->m_imag;
//                    v = ((log10f(magsq/1073741824.0f)*0.2f - 2.0f*itData->m_ofs) + 2.0f)*itData->m_amp - 1.0f;
                    float pdB = itCtl->m_projector.run(*begin);
                    float p = pdB - (100.0f * itData->m_ofs);
                    v = ((p/50.0f) + 2.0f)*itData->m_amp - 1.0f;

                    if ((traceCount >= shift) && (traceCount < shift+length)) // power display overlay values construction
                    {
                        if (traceCount == shift)
                        {
                            itCtl->m_maxPow = -200.0f;
                            itCtl->m_sumPow = 0.0f;
                            itCtl->m_nbPow = 1;
                        }

                        if (pdB > -200.0f)
                        {
                            if (pdB > itCtl->m_maxPow)
                            {
                                itCtl->m_maxPow = pdB;
                            }

                            itCtl->m_sumPow += pdB;
                            itCtl->m_nbPow++;
                        }
                    }

                    if ((nbSamples == 1) && (itCtl->m_nbPow > 0)) // on last sample create power display overlay
                    {
                        double avgPow = itCtl->m_sumPow / itCtl->m_nbPow;
                        double peakToAvgPow = itCtl->m_maxPow - avgPow;
                        itData->m_textOverlay = QString("%1  %2  %3").arg(itCtl->m_maxPow, 0, 'f', 1).arg(avgPow, 0, 'f', 1).arg(peakToAvgPow, 4, 'f', 1, ' ');
                        itCtl->m_nbPow = 0;
                    }
                }
                else
                {
                    v = (itCtl->m_projector.run(*begin) - itData->m_ofs) * itData->m_amp;
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
            }
        }

        ++begin;
        nbSamples--;
    }
}

bool ScopeVisMulti::handleMessage(const Message& message)
{
    qDebug() << "ScopeVisNG::handleMessage" << message.getIdentifier();

    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        setSampleRate(notif.getSampleRate());
        qDebug() << "ScopeVisNG::handleMessage: DSPSignalNotification: m_sampleRate: " << m_sampleRate;
        return true;
    }
    else if (MsgConfigureScopeVisNG::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgConfigureScopeVisNG& conf = (MsgConfigureScopeVisNG&) message;

        uint32_t nbSources = conf.getNbSources();
        uint32_t traceSize = conf.getTraceSize();
        uint32_t timeBase = conf.getTimeBase();
        uint32_t timeOfsProMill = conf.getTimeOfsProMill();
        uint32_t triggerPre = conf.getTriggerPre();
        bool freeRun = conf.getFreeRun();

        if (m_nbSources != nbSources)
        {
            if (nbSources == 0) {
                m_nbSources = 1;
            } else if (nbSources > m_maxNbTraceSources) {
                m_nbSources = m_maxNbTraceSources;
            } else {
                m_nbSources = nbSources;
            }
        }

        if (m_traceSize != traceSize)
        {
            m_traceSize = traceSize;
            m_traces.resize(m_traceSize);
            m_traceDiscreteMemory.resizeBuffers(m_traceSize);
            initTraceBuffers();

            if (m_glScope) {
                m_glScope->setTraceSize(m_traceSize);
            }
        }

        if (m_timeBase != timeBase)
        {
            m_timeBase = timeBase;

            if (m_glScope) {
                m_glScope->setTimeBase(m_timeBase);
            }
        }

        if (m_timeOfsProMill != timeOfsProMill)
        {
            m_timeOfsProMill = timeOfsProMill;

            if (m_glScope) {
                m_glScope->setTimeOfsProMill(m_timeOfsProMill);
            }
        }

        if (m_preTriggerDelay != triggerPre)
        {
            m_preTriggerDelay = triggerPre;
            m_glScope->setTriggerPre(m_preTriggerDelay);
        }

        if (freeRun != m_freeRun)
        {
            m_freeRun = freeRun;
        }

        qDebug() << "ScopeVisNG::handleMessage: MsgConfigureScopeVisNG:"
                << " m_traceSize: " << m_traceSize
                << " m_timeOfsProMill: " << m_timeOfsProMill
                << " m_preTriggerDelay: " << m_preTriggerDelay
                << " m_freeRun: " << m_freeRun;

        if ((m_glScope) && (m_currentTraceMemoryIndex > 0)) {
            processMemorySources();
        }

        return true;
    }
    else if (MsgScopeVisNGAddTrigger::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisNGAddTrigger& conf = (MsgScopeVisNGAddTrigger&) message;
        m_triggerConditions.push_back(TriggerCondition(conf.getTriggerData()));
        m_triggerConditions.back().initProjector();
        return true;
    }
    else if (MsgScopeVisNGChangeTrigger::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisNGChangeTrigger& conf = (MsgScopeVisNGChangeTrigger&) message;
        uint32_t triggerIndex = conf.getTriggerIndex();

        if (triggerIndex < m_triggerConditions.size())
        {
            m_triggerConditions[triggerIndex].setData(conf.getTriggerData());

            if (triggerIndex == m_focusedTriggerIndex)
            {
                computeDisplayTriggerLevels();
                m_glScope->setFocusedTriggerData(m_triggerConditions[m_focusedTriggerIndex].m_triggerData);
                updateGLScopeDisplay();
            }
        }

        return true;
    }
    else if (MsgScopeVisNGRemoveTrigger::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisNGRemoveTrigger& conf = (MsgScopeVisNGRemoveTrigger&) message;
        uint32_t triggerIndex = conf.getTriggerIndex();

        if (triggerIndex < m_triggerConditions.size()) {
            m_triggerConditions.erase(m_triggerConditions.begin() + triggerIndex);
        }

        return true;
    }
    else if (MsgScopeVisNGMoveTrigger::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisNGMoveTrigger& conf = (MsgScopeVisNGMoveTrigger&) message;
        int triggerIndex = conf.getTriggerIndex();

        if (!conf.getMoveUp() && (triggerIndex == 0)) {
            return true;
        }

        int nextTriggerIndex = (triggerIndex + (conf.getMoveUp() ? 1 : -1)) % m_triggerConditions.size();

        TriggerCondition nextTrigger = m_triggerConditions[nextTriggerIndex];
        m_triggerConditions[nextTriggerIndex] = m_triggerConditions[triggerIndex];
        m_triggerConditions[triggerIndex] = nextTrigger;

        computeDisplayTriggerLevels();
        m_glScope->setFocusedTriggerData(m_triggerConditions[m_focusedTriggerIndex].m_triggerData);
        updateGLScopeDisplay();

        return true;
    }
    else if (MsgScopeVisNGFocusOnTrigger::match(message))
    {
        MsgScopeVisNGFocusOnTrigger& conf = (MsgScopeVisNGFocusOnTrigger&) message;
        uint32_t triggerIndex = conf.getTriggerIndex();

        if (triggerIndex < m_triggerConditions.size())
        {
            m_focusedTriggerIndex = triggerIndex;
            computeDisplayTriggerLevels();
            m_glScope->setFocusedTriggerData(m_triggerConditions[m_focusedTriggerIndex].m_triggerData);
            updateGLScopeDisplay();
        }

        return true;
    }
    else if (MsgScopeVisNGAddTrace::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisNGAddTrace& conf = (MsgScopeVisNGAddTrace&) message;
        m_traces.addTrace(conf.getTraceData(), m_traceSize);
        initTraceBuffers();
        updateMaxTraceDelay();
        computeDisplayTriggerLevels();
        updateGLScopeDisplay();
        return true;
    }
    else if (MsgScopeVisNGChangeTrace::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisNGChangeTrace& conf = (MsgScopeVisNGChangeTrace&) message;
        bool doComputeTriggerLevelsOnDisplay = m_traces.isVerticalDisplayChange(conf.getTraceData(), conf.getTraceIndex());
        m_traces.changeTrace(conf.getTraceData(), conf.getTraceIndex());
        updateMaxTraceDelay();
        if (doComputeTriggerLevelsOnDisplay) computeDisplayTriggerLevels();
        updateGLScopeDisplay();
        return true;
    }
    else if (MsgScopeVisNGRemoveTrace::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisNGRemoveTrace& conf = (MsgScopeVisNGRemoveTrace&) message;
        m_traces.removeTrace(conf.getTraceIndex());
        updateMaxTraceDelay();
        computeDisplayTriggerLevels();
        updateGLScopeDisplay();
        return true;
    }
    else if (MsgScopeVisNGMoveTrace::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisNGMoveTrace& conf = (MsgScopeVisNGMoveTrace&) message;
        m_traces.moveTrace(conf.getTraceIndex(), conf.getMoveUp());
        //updateMaxTraceDelay();
        computeDisplayTriggerLevels();
        updateGLScopeDisplay();
        return true;
    }
    else if (MsgScopeVisNGFocusOnTrace::match(message))
    {
        MsgScopeVisNGFocusOnTrace& conf = (MsgScopeVisNGFocusOnTrace&) message;
        uint32_t traceIndex = conf.getTraceIndex();

        if (traceIndex < m_traces.m_tracesData.size())
        {
            m_focusedTraceIndex = traceIndex;
            computeDisplayTriggerLevels();
            m_glScope->setFocusedTraceIndex(m_focusedTraceIndex);
            updateGLScopeDisplay();
        }

        return true;
    }
    else if (MsgScopeVisNGOneShot::match(message))
    {
        MsgScopeVisNGOneShot& conf = (MsgScopeVisNGOneShot&) message;
        bool oneShot = conf.getOneShot();
        m_triggerOneShot = oneShot;
        if (m_triggerWaitForReset && !oneShot) m_triggerWaitForReset = false;
        return true;
    }
    else if (MsgScopeVisNGMemoryTrace::match(message))
    {
        MsgScopeVisNGMemoryTrace& conf = (MsgScopeVisNGMemoryTrace&) message;
        uint32_t memoryIndex = conf.getMemoryIndex();

        if (memoryIndex != m_currentTraceMemoryIndex)
        {
            m_currentTraceMemoryIndex = memoryIndex;

            if (m_currentTraceMemoryIndex > 0) {
                processMemorySources();
            }
        }
        return true;
    }
    else
    {
        return false;
    }
}

void ScopeVisMulti::updateMaxTraceDelay()
{
    int maxTraceDelay = 0;
    bool allocateCache = false;
    uint32_t projectorCounts[(int) nbProjectionTypes];
    memset(projectorCounts, 0, ((int) nbProjectionTypes)*sizeof(uint32_t));
    std::vector<TraceData>::iterator itData = m_traces.m_tracesData.begin();
    std::vector<TraceControl>::iterator itCtrl = m_traces.m_tracesControl.begin();

    for (; itData != m_traces.m_tracesData.end(); ++itData, ++itCtrl)
    {
        if (itData->m_traceDelay > maxTraceDelay)
        {
            maxTraceDelay = itData->m_traceDelay;
        }

        if (projectorCounts[(int) itData->m_projectionType] > 0)
        {
            allocateCache = true;
            itCtrl->m_projector.setCacheMaster(false);
        }
        else
        {
            itCtrl->m_projector.setCacheMaster(true);
        }

        projectorCounts[(int) itData->m_projectionType]++;
    }

    itCtrl = m_traces.m_tracesControl.begin();

    for (; itCtrl != m_traces.m_tracesControl.end(); ++itCtrl)
    {
        if (allocateCache) {
            itCtrl->m_projector.setCache(m_projectorCache);
        } else {
            itCtrl->m_projector.setCache(0);
        }
    }

    m_maxTraceDelay = maxTraceDelay;
}

void ScopeVisMulti::initTraceBuffers()
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

void ScopeVisMulti::computeDisplayTriggerLevels()
{
    std::vector<TraceData>::iterator itData = m_traces.m_tracesData.begin();

    for (; itData != m_traces.m_tracesData.end(); ++itData)
    {
        if ((m_focusedTriggerIndex < m_triggerConditions.size()) && (m_triggerConditions[m_focusedTriggerIndex].m_projector.getProjectionType() == itData->m_projectionType))
        {
            float level = m_triggerConditions[m_focusedTriggerIndex].m_triggerData.m_triggerLevel;
            float levelPowerLin = level + 1.0f;
            float levelPowerdB = (100.0f * (level - 1.0f));
            float v;

            if (itData->m_projectionType == ProjectionMagLin)
            {
                v = (levelPowerLin - itData->m_ofs)*itData->m_amp - 1.0f;
            }
            else if (itData->m_projectionType == ProjectionMagDB)
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

void ScopeVisMulti::updateGLScopeDisplay()
{
    if (m_currentTraceMemoryIndex > 0) {
        m_glScope->setConfigChanged();
        processMemorySources();
    } else {
        m_glScope->updateDisplay();
    }
}

void ScopeVisMulti::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}
