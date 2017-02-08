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
#include "scopevisng.h"
#include "dsp/dspcommands.h"
#include "gui/glscopeng.h"

MESSAGE_CLASS_DEFINITION(ScopeVisNG::MsgConfigureScopeVisNG, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisNG::MsgScopeVisNGAddTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisNG::MsgScopeVisNGChangeTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisNG::MsgScopeVisNGRemoveTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisNG::MsgScopeVisNGAddTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisNG::MsgScopeVisNGChangeTrace, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisNG::MsgScopeVisNGRemoveTrace, Message)

const uint ScopeVisNG::m_traceChunkSize = 4800;
const Real ScopeVisNG::ProjectorMagDB::mult = (10.0f / log2f(10.0f));


ScopeVisNG::ScopeVisNG(GLScopeNG* glScope) :
    m_glScope(glScope),
	m_preTriggerDelay(0),
	m_currentTriggerIndex(0),
	m_triggerState(TriggerUntriggered),
	m_traceSize(m_traceChunkSize),
	m_memTraceSize(0),
	m_traceStart(true),
	m_traceFill(0),
	m_zTraceIndex(-1),
	m_traceCompleteCount(0),
	m_timeOfsProMill(0),
	m_sampleRate(0),
	m_traceDiscreteMemory(10),
	m_freeRun(true),
	m_maxTraceDelay(0)
{
    setObjectName("ScopeVisNG");
    m_traceDiscreteMemory.resize(m_traceChunkSize); // arbitrary
    m_glScope->setTraces(&m_traces.m_tracesData, &m_traces.m_traces[0]);
}

ScopeVisNG::~ScopeVisNG()
{
}

void ScopeVisNG::setSampleRate(int sampleRate)
{
    if (sampleRate != m_sampleRate)
    {
        m_sampleRate = sampleRate;
        if (m_glScope) m_glScope->setSampleRate(m_sampleRate);
    }
}

void ScopeVisNG::configure(uint32_t traceSize, uint32_t timeOfsProMill, uint32_t triggerPre, bool freeRun)
{
    Message* cmd = MsgConfigureScopeVisNG::create(traceSize, timeOfsProMill, triggerPre, freeRun);
    getInputMessageQueue()->push(cmd);
}

void ScopeVisNG::addTrace(const TraceData& traceData)
{
    Message* cmd = MsgScopeVisNGAddTrace::create(traceData);
    getInputMessageQueue()->push(cmd);
}

void ScopeVisNG::changeTrace(const TraceData& traceData, uint32_t traceIndex)
{
    qDebug() << "ScopeVisNG::changeTrace:"
            << " trace: " << traceIndex
            << " m_amp: " << traceData.m_amp
            << " m_ofs: " << traceData.m_ofs;
    Message* cmd = MsgScopeVisNGChangeTrace::create(traceData, traceIndex);
    getInputMessageQueue()->push(cmd);
}

void ScopeVisNG::removeTrace(uint32_t traceIndex)
{
    Message* cmd = MsgScopeVisNGRemoveTrace::create(traceIndex);
    getInputMessageQueue()->push(cmd);
}

void ScopeVisNG::addTrigger(const TriggerData& triggerData)
{
    Message* cmd = MsgScopeVisNGAddTrigger::create(triggerData);
    getInputMessageQueue()->push(cmd);
}

void ScopeVisNG::changeTrigger(const TriggerData& triggerData, uint32_t triggerIndex)
{
    Message* cmd = MsgScopeVisNGChangeTrigger::create(triggerData, triggerIndex);
    getInputMessageQueue()->push(cmd);
}

void ScopeVisNG::removeTrigger(uint32_t triggerIndex)
{
    Message* cmd = MsgScopeVisNGRemoveTrigger::create(triggerIndex);
    getInputMessageQueue()->push(cmd);
}


void ScopeVisNG::feed(const SampleVector::const_iterator& cbegin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    if (m_freeRun) {
        m_triggerPoint = cbegin;
    }
    else if (m_triggerState == TriggerTriggered) {
        m_triggerPoint = cbegin;
    }
    else if (m_triggerState == TriggerUntriggered) {
        m_triggerPoint = end;
    }
    else if (m_triggerState == TriggerWait) {
        m_triggerPoint = end;
    }
    else {
        m_triggerPoint = cbegin;
    }

	if (m_triggerState == TriggerNewConfig)
	{
		m_triggerState = TriggerUntriggered;
		return;
	}

	if ((m_triggerConditions.size() > 0) && (m_triggerState == TriggerWait)) {
		return;
	}

	SampleVector::const_iterator begin(cbegin);

	// memory storage

    m_traceDiscreteMemory.current().write(cbegin, end);

    if (m_traceDiscreteMemory.current().absoluteFill() < m_traceSize)
    {
        return; // not enough samples in memory
    }

    // trigger process

	if (!m_freeRun && (m_triggerConditions.size() > 0) && ((m_triggerState == TriggerUntriggered) || (m_triggerState == TriggerDelay)))
	{
		TriggerCondition& triggerCondition = m_triggerConditions[m_currentTriggerIndex]; // current trigger condition

        while (begin < end)
        {
            // look for trigger
            if (m_triggerComparator.triggered(*begin, triggerCondition))
            {
                m_traceStart = true; // start trace processing
                m_triggerPoint = begin;
                m_triggerComparator.reset();
                m_triggerState = TriggerTriggered;
                break;
            }

            ++begin;
        }
	}
	else
	{
	    m_traceStart = true;
	}

	int remainder = -1;
    int count = end - begin; // number of samples in traceback buffer past the current point
    SampleVector::iterator nend = m_traceDiscreteMemory.current().current();
    SampleVector::iterator nbegin = nend - count;

	// trace process
	if ((m_freeRun) || (m_triggerConditions.size() == 0) || (m_triggerState == TriggerTriggered))
	{
	    // trace back

	    if ((m_traceStart) && (m_preTriggerDelay + m_maxTraceDelay > 0))
	    {
	        remainder = processTraces(count + m_preTriggerDelay + m_maxTraceDelay, count, m_traceDiscreteMemory.current(), true);
	        m_traceStart = false;
	    }

	    if (remainder < 0)
        {
	        // live trace
            remainder = processTraces(count, 0, m_traceDiscreteMemory.current());
        }

	    if (remainder >= 0) // finished
	    {
	        nbegin = nend - remainder;
	        m_traceDiscreteMemory.current().m_endPoint = nbegin;
	        m_traceDiscreteMemory.store(); // next memory trace
            m_traceCompleteCount = 0;
            m_triggerState = TriggerUntriggered;
	    }
	}

	// process remainder recursively

	if (remainder > 0)
	{
	    feed(nbegin, nend, positiveOnly);
	}
}

bool ScopeVisNG::nextTrigger()
{
	TriggerCondition& triggerCondition = m_triggerConditions[m_currentTriggerIndex]; // current trigger condition

	if (triggerCondition.m_triggerData.m_triggerRepeat > 0)
	{
		if (triggerCondition.m_triggerCounter < triggerCondition.m_triggerData.m_triggerRepeat)
		{
			triggerCondition.m_triggerCounter++;
			m_triggerState = TriggerUntriggered; // repeat operations for next occurence
			return true;
		}
		else
		{
			triggerCondition.m_triggerCounter = 0; // reset for next time
		}
	}

	if (m_currentTriggerIndex < m_triggerConditions.size() - 1)
	{
		m_currentTriggerIndex++;
		m_triggerState = TriggerUntriggered; // repeat operations for next trigger
		return true; // not final keep going
	}
	else
	{
	    // now this is really finished
	    m_triggerState = TriggerTriggered;
	    m_currentTriggerIndex = 0;
	    return false; // final
	}
}

int ScopeVisNG::processTraces(int beginPointDelta, int endPointDelta, TraceBackBuffer& traceBuffer, bool traceBack)
{
    SampleVector::iterator begin = traceBuffer.current() - beginPointDelta;
    SampleVector::const_iterator end = traceBuffer.current() - endPointDelta;
    int shift = (m_timeOfsProMill / 1000.0) * m_traceSize;

    while (begin < end)
    {
        std::vector<TraceControl>::iterator itCtl = m_traces.m_tracesControl.begin();
        std::vector<TraceData>::iterator itData = m_traces.m_tracesData.begin();
        std::vector<float *>::iterator itTrace = m_traces.m_traces[m_traces.currentBufferIndex()].begin();

        for (; itCtl != m_traces.m_tracesControl.end(); ++itCtl, ++itData, ++itTrace)
        {
            if (traceBack && ((end - begin) > m_preTriggerDelay + itData->m_traceDelay)) {
                continue;
            }

            if (itCtl->m_traceCount[m_traces.currentBufferIndex()] < m_traceSize)
            {
                ProjectionType projectionType = itData->m_projectionType;
                float v;

                if (projectionType == ProjectionMagLin) {
                    v = (itCtl->m_projector->run(*begin) - itData->m_ofs)*itData->m_amp - 1.0f;
                } else if (projectionType == ProjectionMagDB) {
                    float p = itCtl->m_projector->run(*begin) - (100.0f * itData->m_ofs);
                    v = ((p/50.0f) + 2.0f)*itData->m_amp - 1.0f;
                } else {
                    v = (itCtl->m_projector->run(*begin) - itData->m_ofs) * itData->m_amp;
                }

                if(v > 1.0f) {
                    v = 1.0f;
                } else if (v < -1.0f) {
                    v = -1.0f;
                }

                (*itTrace)[2*(itCtl->m_traceCount[m_traces.currentBufferIndex()])]
                           = (itCtl->m_traceCount[m_traces.currentBufferIndex()] - shift);   // display x
                (*itTrace)[2*(itCtl->m_traceCount[m_traces.currentBufferIndex()]) + 1] = v;  // display y
                itCtl->m_traceCount[m_traces.currentBufferIndex()]++;
            }
            else
            {
                itCtl->m_traceCount[m_traces.currentBufferIndex()]++;

                if (m_traceCompleteCount < m_traces.size())
                {
                    m_traceCompleteCount++;
                }
                else // finished
                {
                    break;
                }
            }
        }

        ++begin;
    }

    if (m_traceCompleteCount == m_traces.size()) // finished
    {
        m_glScope->newTraces(&m_traces.m_traces[m_traces.currentBufferIndex()]);
        m_traces.switchBuffer();
        traceBuffer.m_endPoint = begin;
        return end - begin; // return remainder count
    }
    else
    {
        return -1; // mark not finished
    }
}

void ScopeVisNG::start()
{
}

void ScopeVisNG::stop()
{
}

bool ScopeVisNG::handleMessage(const Message& message)
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
        MsgConfigureScopeVisNG& conf = (MsgConfigureScopeVisNG&) message;

        uint32_t traceSize = conf.getTraceSize();
        uint32_t timeOfsProMill = conf.getTimeOfsProMill();
        uint32_t triggerPre = conf.getTriggerPre();
        bool freeRun = conf.getFreeRun();

        if (m_traceSize != traceSize)
        {
            m_traceSize = traceSize;
            m_traces.resize(m_traceSize);
            m_traceDiscreteMemory.resize(m_traceSize);
            initTraceBuffers();

            if (m_glScope) {
                m_glScope->setTraceSize(m_traceSize);
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

        return true;
    }
    else if (MsgScopeVisNGAddTrigger::match(message))
    {
        MsgScopeVisNGAddTrigger& conf = (MsgScopeVisNGAddTrigger&) message;
        m_triggerConditions.push_back(TriggerCondition(conf.getTriggerData()));
        m_triggerConditions.back().init();
        return true;
    }
    else if (MsgScopeVisNGChangeTrigger::match(message))
    {
        MsgScopeVisNGChangeTrigger& conf = (MsgScopeVisNGChangeTrigger&) message;
        int triggerIndex = conf.getTriggerIndex();

        if (triggerIndex < m_triggerConditions.size()) {
            m_triggerConditions[triggerIndex].setData(conf.getTriggerData());
        }

        return true;
    }
    else if (MsgScopeVisNGRemoveTrigger::match(message))
    {
        MsgScopeVisNGRemoveTrigger& conf = (MsgScopeVisNGRemoveTrigger&) message;
        int triggerIndex = conf.getTriggerIndex();

        if (triggerIndex < m_triggerConditions.size()) {
            m_triggerConditions.erase(m_triggerConditions.begin() + triggerIndex);
        }

        return true;
    }
    else if (MsgScopeVisNGAddTrace::match(message))
    {
        MsgScopeVisNGAddTrace& conf = (MsgScopeVisNGAddTrace&) message;
        m_traces.addTrace(conf.getTraceData(), m_traceSize);
        initTraceBuffers();
        updateMaxTraceDelay();
        return true;
    }
    else if (MsgScopeVisNGChangeTrace::match(message))
    {
        MsgScopeVisNGChangeTrace& conf = (MsgScopeVisNGChangeTrace&) message;
        m_traces.changeTrace(conf.getTraceData(), conf.getTraceIndex());
        updateMaxTraceDelay();
        m_glScope->updateDisplay();

        return true;
    }
    else if (MsgScopeVisNGRemoveTrace::match(message))
    {
        MsgScopeVisNGRemoveTrace& conf = (MsgScopeVisNGRemoveTrace&) message;
        m_traces.removeTrace(conf.getTraceIndex());
        updateMaxTraceDelay();
        return true;
    }
    else
    {
        return false;
    }
}


void ScopeVisNG::updateMaxTraceDelay()
{
    int maxTraceDelay = 0;

    for (std::vector<TraceData>::iterator itData = m_traces.m_tracesData.begin(); itData != m_traces.m_tracesData.end(); ++itData)
    {
        if (itData->m_traceDelay > maxTraceDelay)
        {
            maxTraceDelay = itData->m_traceDelay;
        }
    }

    m_maxTraceDelay = maxTraceDelay;
}

void ScopeVisNG::initTraceBuffers()
{
    int shift = (m_timeOfsProMill / 1000.0) * m_traceSize;

    std::vector<float *>::iterator it0 = m_traces.m_traces[0].begin();
    std::vector<float *>::iterator it1 = m_traces.m_traces[1].begin();

    for (; it0 != m_traces.m_traces[0].end(); ++it0, ++it1)
    {
        for (int i = 0; i < m_traceSize; i++)
        {
            (*it0)[2*i] = (i - shift); // display x
            (*it0)[2*i + 1] = 0.0f;    // display y
            (*it1)[2*i] = (i - shift); // display x
            (*it1)[2*i + 1] = 0.0f;    // display y
        }
    }
}

