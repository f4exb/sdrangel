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

#include <QDebug>
#include <QMutexLocker>

#include "scopevis.h"
#include "dsp/dspcommands.h"
#include "gui/glscope.h"

MESSAGE_CLASS_DEFINITION(ScopeVis::MsgConfigureScopeVisNG, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisNGAddTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisNGChangeTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisNGRemoveTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisNGMoveTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisNGFocusOnTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisNGAddTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisNGChangeTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisNGRemoveTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisNGMoveTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisNGFocusOnTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisNGOneShot, Message)
MESSAGE_CLASS_DEFINITION(ScopeVis::MsgScopeVisNGMemoryTrace, Message)

const uint ScopeVis::m_traceChunkDefaultSize = 4800;


ScopeVis::ScopeVis(GLScope* glScope) :
    m_glScope(glScope),
    m_preTriggerDelay(0),
    m_livePreTriggerDelay(0),
    m_currentTriggerIndex(0),
    m_focusedTriggerIndex(0),
    m_triggerState(TriggerUntriggered),
    m_focusedTraceIndex(0),
    m_traceChunkSize(m_traceChunkDefaultSize),
    m_traceSize(m_traceChunkDefaultSize),
    m_liveTraceSize(m_traceChunkDefaultSize),
    m_nbSamples(0),
    m_timeBase(1),
    m_timeOfsProMill(0),
    m_traceStart(true),
    m_triggerLocation(0),
    m_sampleRate(0),
    m_liveSampleRate(0),
    m_traceDiscreteMemory(m_nbTraceMemories),
    m_freeRun(true),
    m_maxTraceDelay(0),
    m_triggerOneShot(false),
    m_triggerWaitForReset(false),
    m_currentTraceMemoryIndex(0)
{
    setObjectName("ScopeVis");
    m_traceDiscreteMemory.resize(m_traceChunkDefaultSize); // arbitrary
    m_glScope->setTraces(&m_traces.m_tracesData, &m_traces.m_traces[0]);
    for (int i = 0; i < (int) Projector::nbProjectionTypes; i++) {
        m_projectorCache[i] = 0.0;
    }
}

ScopeVis::~ScopeVis()
{
    for (std::vector<TriggerCondition*>::iterator it = m_triggerConditions.begin(); it != m_triggerConditions.end(); ++ it) {
        delete *it;
    }
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

void ScopeVis::configure(uint32_t traceSize, uint32_t timeBase, uint32_t timeOfsProMill, uint32_t triggerPre, bool freeRun)
{
    Message* cmd = MsgConfigureScopeVisNG::create(traceSize, timeBase, timeOfsProMill, triggerPre, freeRun);
    getInputMessageQueue()->push(cmd);
}

void ScopeVis::addTrace(const TraceData& traceData)
{
    qDebug() << "ScopeVis::addTrace:"
            << " m_amp: " << traceData.m_amp
            << " m_ofs: " << traceData.m_ofs
            << " m_traceDelay: " << traceData.m_traceDelay;
    Message* cmd = MsgScopeVisNGAddTrace::create(traceData);
    getInputMessageQueue()->push(cmd);
}

void ScopeVis::changeTrace(const TraceData& traceData, uint32_t traceIndex)
{
    qDebug() << "ScopeVis::changeTrace:"
            << " trace: " << traceIndex
            << " m_amp: " << traceData.m_amp
            << " m_ofs: " << traceData.m_ofs
            << " m_traceDelay: " << traceData.m_traceDelay;
    Message* cmd = MsgScopeVisNGChangeTrace::create(traceData, traceIndex);
    getInputMessageQueue()->push(cmd);
}

void ScopeVis::removeTrace(uint32_t traceIndex)
{
    qDebug() << "ScopeVis::removeTrace:"
            << " trace: " << traceIndex;
    Message* cmd = MsgScopeVisNGRemoveTrace::create(traceIndex);
    getInputMessageQueue()->push(cmd);
}

void ScopeVis::moveTrace(uint32_t traceIndex, bool upElseDown)
{
    qDebug() << "ScopeVis::moveTrace:"
            << " trace: " << traceIndex
            << " up: " << upElseDown;
    Message* cmd = MsgScopeVisNGMoveTrace::create(traceIndex, upElseDown);
    getInputMessageQueue()->push(cmd);
}

void ScopeVis::focusOnTrace(uint32_t traceIndex)
{
    Message* cmd = MsgScopeVisNGFocusOnTrace::create(traceIndex);
    getInputMessageQueue()->push(cmd);
}

void ScopeVis::addTrigger(const TriggerData& triggerData)
{
    Message* cmd = MsgScopeVisNGAddTrigger::create(triggerData);
    getInputMessageQueue()->push(cmd);
}

void ScopeVis::changeTrigger(const TriggerData& triggerData, uint32_t triggerIndex)
{
    Message* cmd = MsgScopeVisNGChangeTrigger::create(triggerData, triggerIndex);
    getInputMessageQueue()->push(cmd);
}

void ScopeVis::removeTrigger(uint32_t triggerIndex)
{
    Message* cmd = MsgScopeVisNGRemoveTrigger::create(triggerIndex);
    getInputMessageQueue()->push(cmd);
}

void ScopeVis::moveTrigger(uint32_t triggerIndex, bool upElseDown)
{
    Message* cmd = MsgScopeVisNGMoveTrigger::create(triggerIndex, upElseDown);
    getInputMessageQueue()->push(cmd);
}

void ScopeVis::focusOnTrigger(uint32_t triggerIndex)
{
    Message* cmd = MsgScopeVisNGFocusOnTrigger::create(triggerIndex);
    getInputMessageQueue()->push(cmd);
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

void ScopeVis::feed(const SampleVector::const_iterator& cbegin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    (void) positiveOnly;

    if (m_currentTraceMemoryIndex > 0) { // in memory mode live trace is suspended
        return;
    }

    if (!m_mutex.tryLock(0)) { // prevent conflicts with configuration process
        return;
    }

    if (m_freeRun) {
        m_triggerLocation = end - cbegin;
    }
    else if (m_triggerState == TriggerTriggered) {
        m_triggerLocation = end - cbegin;
    }
    else if (m_triggerState == TriggerUntriggered) {
        m_triggerLocation = 0;
    }
    else if (m_triggerWaitForReset) {
        m_triggerLocation = 0;
    }
    else {
        m_triggerLocation = end - cbegin;
    }

    SampleVector::const_iterator begin(cbegin);
    int triggerPointToEnd;

    while (begin < end)
    {
        if (begin + m_traceSize > end) // buffer smaller than trace size (end - bagin) < m_traceSize
        {
            triggerPointToEnd = -1;
            processTrace(begin, end, triggerPointToEnd); // use all buffer
            m_triggerLocation = triggerPointToEnd < 0 ? 0 : triggerPointToEnd; // trim negative values
            m_triggerLocation = m_triggerLocation > end - begin ? end - begin : m_triggerLocation; // trim past begin values

            begin = end; // effectively breaks out the loop
        }
        else // trace size fits in buffer
        {
            triggerPointToEnd = -1;
            processTrace(begin, begin + m_traceSize, triggerPointToEnd); // use part of buffer to fit trace size
            //m_triggerPoint = begin + m_traceSize - triggerPointToEnd;
            m_triggerLocation = end - begin + m_traceSize - triggerPointToEnd; // should always refer to end iterator
            m_triggerLocation = m_triggerLocation < 0 ? 0 : m_triggerLocation; // trim negative values
            m_triggerLocation = m_triggerLocation > end - begin ? end - begin : m_triggerLocation; // trim past begin values

            begin += m_traceSize;
        }
    }

    m_mutex.unlock();
}

void ScopeVis::processMemoryTrace()
{
    if ((m_currentTraceMemoryIndex > 0) && (m_currentTraceMemoryIndex < m_nbTraceMemories))
    {
        int traceMemoryIndex = m_traceDiscreteMemory.currentIndex() - m_currentTraceMemoryIndex; // actual index in memory bank

        if (traceMemoryIndex < 0) {
            traceMemoryIndex += m_nbTraceMemories;
        }

        SampleVector::const_iterator mend = m_traceDiscreteMemory.at(traceMemoryIndex).m_endPoint;
        SampleVector::const_iterator mbegin = mend - m_traceSize;
        SampleVector::const_iterator mbegin_tb = mbegin - m_maxTraceDelay;
        m_nbSamples = m_traceSize + m_maxTraceDelay;

        processTraces(mbegin_tb, mbegin, true); // traceback
        processTraces(mbegin, mend, false);
    }
}

void ScopeVis::processTrace(const SampleVector::const_iterator& cbegin, const SampleVector::const_iterator& end, int& triggerPointToEnd)
{
    SampleVector::const_iterator begin(cbegin);

    // memory storage

    m_traceDiscreteMemory.current().write(cbegin, end);

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

        while (begin < end)
        {
            if (m_triggerState == TriggerDelay) // delayed trigger
            {
                if (triggerCondition->m_triggerDelayCount > 0) // skip samples during delay period
                {
                    begin += triggerCondition->m_triggerDelayCount;
                    triggerCondition->m_triggerDelayCount = 0;
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
                        triggerPointToEnd = end - begin;
                        break;
                    }
                }
            }

            if (m_triggerComparator.triggered(*begin, *triggerCondition)) // matched the current trigger
            {
                if (triggerCondition->m_triggerData.m_triggerDelay > 0)
                {
                    triggerCondition->m_triggerDelayCount = triggerCondition->m_triggerData.m_triggerDelay; // initialize delayed samples counter
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
                    m_traceStart = true; // start of trace processing
                    m_nbSamples = m_traceSize + m_maxTraceDelay;
                    m_triggerComparator.reset();
                    m_triggerState = TriggerTriggered;
                    triggerPointToEnd = end - begin;
                    break;
                }
            }

            ++begin;
        } // look for trigger
    } // untriggered or delayed

    // trace process

    if (m_triggerState == TriggerTriggered)
    {
        int remainder;
        int count = end - begin; // number of samples in traceback buffer past the current point
        SampleVector::iterator mend = m_traceDiscreteMemory.current().current();
        SampleVector::iterator mbegin = mend - count;

        if (m_traceStart) // start of trace processing
        {
            // if trace time is 1s or more the display is progressive so we have to clear it first

            float traceTime = ((float) m_traceSize) / m_sampleRate;

            if (traceTime >= 1.0f) {
                initTraceBuffers();
            }

            // process until begin point

            if (m_maxTraceDelay > 0) { // trace back
                processTraces(mbegin - m_preTriggerDelay - m_maxTraceDelay, mbegin - m_preTriggerDelay, true);
            }

            if (m_preTriggerDelay > 0) { // pre-trigger
                processTraces(mbegin - m_preTriggerDelay, mbegin);
            }

            // process the rest of the trace

            remainder = processTraces(mbegin, mend);
            m_traceStart = false;
        }
        else // process the current trace
        {
            remainder = processTraces(mbegin, mend);
        }

        if (remainder >= 0) // finished
        {
            mbegin = mend - remainder;
            m_traceDiscreteMemory.current().m_endPoint = mbegin;
            m_traceDiscreteMemory.store(m_preTriggerDelay+remainder); // next memory trace.
            m_triggerState = TriggerUntriggered;
            m_triggerWaitForReset = m_triggerOneShot;

            //if (m_glScope) m_glScope->incrementTraceCounter();

            // process remainder recursively
            if (remainder != 0)
            {
                int mTriggerPointToEnd = -1;

                processTrace(mbegin, mend, mTriggerPointToEnd);

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

int ScopeVis::processTraces(const SampleVector::const_iterator& cbegin, const SampleVector::const_iterator& end, bool traceBack)
{
    SampleVector::const_iterator begin(cbegin);
    uint32_t shift = (m_timeOfsProMill / 1000.0) * m_traceSize;
    uint32_t length = m_traceSize / m_timeBase;

    while ((begin < end) && (m_nbSamples > 0))
    {
        std::vector<TraceControl*>::iterator itCtl = m_traces.m_tracesControl.begin();
        std::vector<TraceData>::iterator itData = m_traces.m_tracesData.begin();
        std::vector<float *>::iterator itTrace = m_traces.m_traces[m_traces.currentBufferIndex()].begin();

        for (; itCtl != m_traces.m_tracesControl.end(); ++itCtl, ++itData, ++itTrace)
        {
            if (traceBack && ((end - begin) > itData->m_traceDelay)) { // before start of trace
                continue;
            }

            Projector::ProjectionType projectionType = itData->m_projectionType;

            if ((*itCtl)->m_traceCount[m_traces.currentBufferIndex()] < m_traceSize)
            {
                uint32_t& traceCount = (*itCtl)->m_traceCount[m_traces.currentBufferIndex()]; // reference for code clarity
                float v;

                if (projectionType == Projector::ProjectionMagLin)
                {
                    v = ((*itCtl)->m_projector.run(*begin) - itData->m_ofs)*itData->m_amp - 1.0f;
                }
                else if (projectionType == Projector::ProjectionMagSq)
                {
                    Real magsq = (*itCtl)->m_projector.run(*begin);
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
                    Real re = begin->m_real / SDR_RX_SCALEF;
                    Real im = begin->m_imag / SDR_RX_SCALEF;
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
                    v = ((*itCtl)->m_projector.run(*begin) - itData->m_ofs) * itData->m_amp;
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
        m_nbSamples--;
    }

    float traceTime = ((float) m_traceSize) / m_sampleRate;

    if (traceTime >= 1.0f) { // display continuously if trace time is 1 second or more
        m_glScope->newTraces(m_traces.m_traces, m_traces.currentBufferIndex(), &m_traces.m_projectionTypes);
    }

    if (m_nbSamples == 0) // finished
    {
        // display only at trace end if trace time is less than 1 second
        if (traceTime < 1.0f)
        {
            if (m_glScope->getProcessingTraceIndex().load() < 0) {
                m_glScope->newTraces(m_traces.m_traces, m_traces.currentBufferIndex(), &m_traces.m_projectionTypes);
            }
        }

        // switch to next buffer only if it is not being processed by the scope
        if (m_glScope->getProcessingTraceIndex().load() != (((int) m_traces.currentBufferIndex() + 1) % 2)) {
            m_traces.switchBuffer();
        }

        return end - begin; // return remainder count
    }
    else
    {
        return -1; // mark not finished
    }
}

void ScopeVis::start()
{
}

void ScopeVis::stop()
{
}

bool ScopeVis::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        setLiveRate(notif.getSampleRate());
        qDebug() << "ScopeVis::handleMessage: DSPSignalNotification: m_sampleRate: " << m_sampleRate;
        return true;
    }
    else if (MsgConfigureScopeVisNG::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgConfigureScopeVisNG& conf = (MsgConfigureScopeVisNG&) message;

        uint32_t traceSize = conf.getTraceSize();
        uint32_t timeBase = conf.getTimeBase();
        uint32_t timeOfsProMill = conf.getTimeOfsProMill();
        uint32_t triggerPre = conf.getTriggerPre();
        bool freeRun = conf.getFreeRun();

        if (m_traceSize != traceSize)
        {
            setTraceSize(traceSize);
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
            setPreTriggerDelay(triggerPre);
        }

        if (freeRun != m_freeRun)
        {
            m_freeRun = freeRun;
        }

        qDebug() << "ScopeVis::handleMessage: MsgConfigureScopeVisNG:"
                << " m_traceSize: " << m_traceSize
                << " m_timeOfsProMill: " << m_timeOfsProMill
                << " m_preTriggerDelay: " << m_preTriggerDelay
                << " m_freeRun: " << m_freeRun;

        if ((m_glScope) && (m_currentTraceMemoryIndex > 0)) {
            processMemoryTrace();
        }

        return true;
    }
    else if (MsgScopeVisNGAddTrigger::match(message))
    {
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisNGAddTrigger";
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisNGAddTrigger& conf = (MsgScopeVisNGAddTrigger&) message;
        m_triggerConditions.push_back(new TriggerCondition(conf.getTriggerData()));
        m_triggerConditions.back()->initProjector();
        return true;
    }
    else if (MsgScopeVisNGChangeTrigger::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisNGChangeTrigger& conf = (MsgScopeVisNGChangeTrigger&) message;
        uint32_t triggerIndex = conf.getTriggerIndex();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisNGChangeTrigger: " << triggerIndex;

        if (triggerIndex < m_triggerConditions.size())
        {
            m_triggerConditions[triggerIndex]->setData(conf.getTriggerData());

            if (triggerIndex == m_focusedTriggerIndex)
            {
                computeDisplayTriggerLevels();
                m_glScope->setFocusedTriggerData(m_triggerConditions[m_focusedTriggerIndex]->m_triggerData);
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
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisNGRemoveTrigger: " << triggerIndex;

        if (triggerIndex < m_triggerConditions.size())
        {
            TriggerCondition *triggerCondition = m_triggerConditions[triggerIndex];
            m_triggerConditions.erase(m_triggerConditions.begin() + triggerIndex);
            delete triggerCondition;
        }

        return true;
    }
    else if (MsgScopeVisNGMoveTrigger::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisNGMoveTrigger& conf = (MsgScopeVisNGMoveTrigger&) message;
        int triggerIndex = conf.getTriggerIndex();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisNGMoveTrigger: " << triggerIndex;

        if (!conf.getMoveUp() && (triggerIndex == 0)) {
            return true;
        }

        int nextTriggerIndex = (triggerIndex + (conf.getMoveUp() ? 1 : -1)) % m_triggerConditions.size();

        TriggerCondition *nextTrigger = m_triggerConditions[nextTriggerIndex];
        m_triggerConditions[nextTriggerIndex] = m_triggerConditions[triggerIndex];
        m_triggerConditions[triggerIndex] = nextTrigger;

        computeDisplayTriggerLevels();
        m_glScope->setFocusedTriggerData(m_triggerConditions[m_focusedTriggerIndex]->m_triggerData);
        updateGLScopeDisplay();

        return true;
    }
    else if (MsgScopeVisNGFocusOnTrigger::match(message))
    {
        MsgScopeVisNGFocusOnTrigger& conf = (MsgScopeVisNGFocusOnTrigger&) message;
        uint32_t triggerIndex = conf.getTriggerIndex();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisNGFocusOnTrigger: " << triggerIndex;

        if (triggerIndex < m_triggerConditions.size())
        {
            m_focusedTriggerIndex = triggerIndex;
            computeDisplayTriggerLevels();
            m_glScope->setFocusedTriggerData(m_triggerConditions[m_focusedTriggerIndex]->m_triggerData);
            updateGLScopeDisplay();
        }

        return true;
    }
    else if (MsgScopeVisNGAddTrace::match(message))
    {
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisNGAddTrace";
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
        uint32_t traceIndex = conf.getTraceIndex();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisNGChangeTrace: " << traceIndex;
        m_traces.changeTrace(conf.getTraceData(), traceIndex);
        updateMaxTraceDelay();
        if (doComputeTriggerLevelsOnDisplay) computeDisplayTriggerLevels();
        updateGLScopeDisplay();
        return true;
    }
    else if (MsgScopeVisNGRemoveTrace::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisNGRemoveTrace& conf = (MsgScopeVisNGRemoveTrace&) message;
        uint32_t traceIndex = conf.getTraceIndex();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisNGRemoveTrace: " << traceIndex;
        m_traces.removeTrace(traceIndex);
        updateMaxTraceDelay();
        computeDisplayTriggerLevels();
        updateGLScopeDisplay();
        return true;
    }
    else if (MsgScopeVisNGMoveTrace::match(message))
    {
        QMutexLocker configLocker(&m_mutex);
        MsgScopeVisNGMoveTrace& conf = (MsgScopeVisNGMoveTrace&) message;
        uint32_t traceIndex = conf.getTraceIndex();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisNGMoveTrace: " << traceIndex;
        m_traces.moveTrace(traceIndex, conf.getMoveUp());
        //updateMaxTraceDelay();
        computeDisplayTriggerLevels();
        updateGLScopeDisplay();
        return true;
    }
    else if (MsgScopeVisNGFocusOnTrace::match(message))
    {
        MsgScopeVisNGFocusOnTrace& conf = (MsgScopeVisNGFocusOnTrace&) message;
        uint32_t traceIndex = conf.getTraceIndex();
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisNGFocusOnTrace: " << traceIndex;

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
        qDebug() << "ScopeVis::handleMessage: MsgScopeVisNGOneShot";
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

void ScopeVis::updateMaxTraceDelay()
{
    int maxTraceDelay = 0;
    bool allocateCache = false;
    uint32_t projectorCounts[(int) Projector::nbProjectionTypes];
    memset(projectorCounts, 0, ((int) Projector::nbProjectionTypes)*sizeof(uint32_t));
    std::vector<TraceData>::iterator itData = m_traces.m_tracesData.begin();
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

        if (projectorCounts[(int) itData->m_projectionType] > 0)
        {
            allocateCache = true;
            (*itCtrl)->m_projector.setCacheMaster(false);
        }
        else
        {
            (*itCtrl)->m_projector.setCacheMaster(true);
        }

        projectorCounts[(int) itData->m_projectionType]++;
    }

    itCtrl = m_traces.m_tracesControl.begin();

    for (; itCtrl != m_traces.m_tracesControl.end(); ++itCtrl)
    {
        if (allocateCache) {
            (*itCtrl)->m_projector.setCache(m_projectorCache);
        } else {
            (*itCtrl)->m_projector.setCache(0);
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
    std::vector<TraceData>::iterator itData = m_traces.m_tracesData.begin();

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
    if (m_currentTraceMemoryIndex > 0) {
        m_glScope->setConfigChanged();
        processMemoryTrace();
    } else {
        m_glScope->updateDisplay();
    }
}
