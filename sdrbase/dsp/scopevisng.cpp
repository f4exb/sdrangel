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
	m_traceDiscreteMemory(10)
{
    setObjectName("ScopeVisNG");
    m_traceDiscreteMemory.resize(m_traceChunkSize); // arbitrary
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

void ScopeVisNG::configure(uint32_t traceSize, uint32_t timeOfsProMill)
{
    Message* cmd = MsgConfigureScopeVisNG::create(traceSize, timeOfsProMill);
    getInputMessageQueue()->push(cmd);
}

void ScopeVisNG::addTrace(const TraceData& traceData)
{
    Message* cmd = MsgScopeVisNGAddTrace::create(traceData);
    getInputMessageQueue()->push(cmd);
}

void ScopeVisNG::changeTrace(const TraceData& traceData, uint32_t traceIndex)
{
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
    if (m_triggerState == TriggerFreeRun) {
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

	if ((m_triggerConditions.size() > 0) && ((m_triggerState == TriggerUntriggered) || (m_triggerState == TriggerDelay)))
	{
		TriggerCondition& triggerCondition = m_triggerConditions[m_currentTriggerIndex]; // current trigger condition

        while (begin < end)
        {
        	if (m_triggerState == TriggerDelay)
        	{
                if (triggerCondition.m_triggerDelayCount > 0)
                {
                    triggerCondition.m_triggerDelayCount--; // pass
                }
                else // delay expired => fire this trigger
                {
                	if (!nextTrigger()) // finished
                	{
                		m_traceStart = true; // start trace processing
                		m_triggerPoint = begin;
                		break;
                	}
                }
        	}
        	else // look for trigger
        	{
        		bool condition = triggerCondition.m_projector->run(*begin) > triggerCondition.m_triggerData.m_triggerLevel;
        		bool trigger;

				if (triggerCondition.m_triggerData.m_triggerBothEdges) {
					trigger = triggerCondition.m_prevCondition ^ condition;
				} else {
					trigger = condition ^ !triggerCondition.m_triggerData.m_triggerPositiveEdge;
				}

				if (trigger) // trigger condition
				{
					if (triggerCondition.m_triggerData.m_triggerDelay > 0) // there is a delay => initialize the delay
					{
						triggerCondition.m_triggerDelayCount = triggerCondition.m_triggerData.m_triggerDelay;
						m_triggerState = TriggerDelay;
					}
					else
					{
	                	if (!nextTrigger()) // finished
	                	{
	                		m_traceStart = true; // start trace processing
	                		m_triggerPoint = begin;
	                		break;
	                	}
					}
				}
        	}

            ++begin;
		} // begin < end
	}

	int remainder = -1;
    int count = end - begin; // number of samples in traceback buffer past the current point
    SampleVector::iterator nend = m_traceDiscreteMemory.current().current();
    SampleVector::iterator nbegin = nend - count;

	// trace process
	if ((m_triggerState == TriggerFreeRun) || (m_triggerConditions.size() == 0) || (m_triggerState == TriggerTriggered))
	{
	    // trace back

	    if (m_traceStart)
	    {
        	int maxTraceDelay = 0;

	        for (std::vector<Trace>::iterator itTrace = m_traces.begin(); itTrace != m_traces.end(); ++itTrace)
	        {
	        	if (itTrace->m_traceData.m_traceDelay > maxTraceDelay)
	        	{
	        		maxTraceDelay = itTrace->m_traceData.m_traceDelay;
	        	}
	        }

	        remainder = processTraces(count + m_preTriggerDelay + maxTraceDelay, count, m_traceDiscreteMemory.current(), true);
	        m_traceStart = false;
	    }

	    if (remainder < 0)
        {
	        // live trace
            remainder = processTraces(count, 0, m_traceDiscreteMemory.current());
        }

	    if (remainder >= 0) // finished
	    {
	        m_glScope->newTraces();

	        nbegin = nend - remainder;
	        m_traceDiscreteMemory.current().m_endPoint = nbegin;
	        m_traceDiscreteMemory.store(); // next memory trace

	        for (std::vector<Trace>::iterator itTrace = m_traces.begin(); itTrace != m_traces.end(); ++itTrace) {
                itTrace->reset();
            }

            m_traceCompleteCount = 0;
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

	if (m_currentTriggerIndex < m_triggerConditions.size())
	{
		m_currentTriggerIndex++;
		m_triggerState = TriggerUntriggered; // repeat operations for next trigger
		return true; // not final keep going
	}

	// now this is really finished
	m_triggerState == TriggerTriggered;
	m_currentTriggerIndex = 0;
	return false; // final
}

int ScopeVisNG::processTraces(int beginPointDelta, int endPointDelta, TraceBackBuffer& traceBuffer, bool traceStart)
{
    SampleVector::iterator begin = traceBuffer.current() - beginPointDelta;
    SampleVector::const_iterator end = traceBuffer.current() - endPointDelta;
    int shift = (m_timeOfsProMill / 1000.0) * m_traceSize;

    while (begin < end)
    {
        for (std::vector<Trace>::iterator itTrace = m_traces.begin(); itTrace != m_traces.end(); ++itTrace)
        {
            if (traceStart && ((end - begin) > m_preTriggerDelay + itTrace->m_traceData.m_traceDelay)) {
                continue;
            }

            if (itTrace->m_traceCount < m_traceSize)
            {
                float posLimit = 1.0 / itTrace->m_traceData.m_amp;
                float negLimit = -1.0 / itTrace->m_traceData.m_amp;

                float v = itTrace->m_projector->run(*begin) * itTrace->m_traceData.m_amp + itTrace->m_traceData.m_ofs;

                if(v > posLimit) {
                    v = posLimit;
                } else if (v < negLimit) {
                    v = negLimit;
                }

                itTrace->m_trace[2*(itTrace->m_traceCount)] = (itTrace->m_traceCount - shift); // display x
                itTrace->m_trace[2*(itTrace->m_traceCount) + 1] = v;                           // display y
                itTrace->m_traceCount++;
            }
            else if (itTrace->m_traceCount < m_traceSize)
            {
                itTrace->m_traceCount++;

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

        begin++;
    }

    if (m_traceCompleteCount == m_traces.size()) // finished
    {
        m_glScope->newTraces();
        traceBuffer.m_endPoint = begin;
        return end - begin; // return remainder count
    }
    else
    {
        return -1; // mark not finished
    }

}

void ScopeVisNG::initTrace(Trace& trace)
{
    int shift = (m_timeOfsProMill / 1000.0) * m_traceSize;

    for (int i = 0; i < m_traceSize; i++)
    {
        trace.m_trace[2*(trace.m_traceCount)] = (trace.m_traceCount - shift); // display x
        trace.m_trace[2*(trace.m_traceCount) + 1] = 0.0f;                     // display y
        trace.m_traceCount++;
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

        if (m_traceSize != traceSize)
        {
            m_traceSize = traceSize;

            std::vector<Trace>::iterator it = m_traces.begin();

            for (; it != m_traces.end(); ++it)
            {
                it->resize(m_traceSize);
                initTrace(*it);
            }

            m_traceDiscreteMemory.resize(m_traceSize);

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

        qDebug() << "ScopeVisNG::handleMessage: MsgConfigureScopeVisNG:"
                << " m_traceSize: " << m_traceSize
                << " m_timeOfsProMill: " << m_timeOfsProMill;

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
        m_traces.push_back(Trace(conf.getTraceData(), m_traceSize));
        m_traces.back().init();
        initTrace(m_traces.back());
        m_glScope->addTrace(&m_traces.back());
        return true;
    }
    else if (MsgScopeVisNGChangeTrace::match(message))
    {
        MsgScopeVisNGChangeTrace& conf = (MsgScopeVisNGChangeTrace&) message;
        int traceIndex = conf.getTraceIndex();

        if (traceIndex < m_traces.size()) {
            m_traces[traceIndex].setData(conf.getTraceData());
        }

        return true;
    }
    else if (MsgScopeVisNGRemoveTrace::match(message))
    {
        MsgScopeVisNGRemoveTrace& conf = (MsgScopeVisNGRemoveTrace&) message;
        int traceIndex = conf.getTraceIndex();

        if (traceIndex < m_traces.size()) {
            m_traces.erase(m_traces.begin() + traceIndex);
            m_glScope->removeTrace(traceIndex);
        }

        return true;
    }
    else
    {
        return false;
    }
}

