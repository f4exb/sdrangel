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
#include "gui/glscopeng.h"

MESSAGE_CLASS_DEFINITION(ScopeVisNG::MsgConfigureScopeVisNG, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisNG::MsgScopeVisNGAddTrigger, Message)
MESSAGE_CLASS_DEFINITION(ScopeVisNG::MsgScopeVisNGRemoveTrigger, Message)

const uint ScopeVisNG::m_traceChunkSize = 4800;
const Real ScopeVisNG::ProjectorMagDB::mult = (10.0f / log2f(10.0f));


ScopeVisNG::ScopeVisNG(GLScopeNG* glScope) :
    m_glScope(glScope),
	m_preTriggerDelay(0),
	m_currentTriggerIndex(0),
	m_triggerState(TriggerUntriggered),
	m_traceSize(m_traceChunkSize),
	m_traceStart(true),
	m_traceFill(0),
	m_zTraceIndex(-1),
	m_traceCompleteCount(0)
{
    setObjectName("ScopeVisNG");
    m_tracebackBuffers.resize(1);
    m_tracebackBuffers[0].resize(4*m_traceChunkSize);
}

ScopeVisNG::~ScopeVisNG()
{
	std::vector<TriggerCondition>::iterator it = m_triggerConditions.begin();

	for (; it != m_triggerConditions.end(); ++it)
	{
		delete it->m_projector;
	}
}

void ScopeVisNG::configure(MessageQueue* msgQueue,
        uint traceSize)
{
    Message* cmd = MsgConfigureScopeVisNG::create(traceSize);
    msgQueue->push(cmd);
}


void ScopeVisNG::feed(const SampleVector::const_iterator& cbegin, const SampleVector::const_iterator& end, bool positiveOnly)
{
	uint32_t feedIndex = 0; // TODO: redefine feed interface so it can be passed a feed index

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

	m_tracebackBuffers[feedIndex].write(cbegin, end);
	SampleVector::const_iterator begin(cbegin);
	TriggerCondition& triggerCondition = m_triggerConditions[m_currentTriggerIndex];

	// trigger process
	if ((m_triggerConditions.size() > 0) && (feedIndex == triggerCondition.m_inputIndex))
	{
        while (begin < end)
        {
            if (m_triggerState == TriggerUntriggered)
            {
				bool condition = triggerCondition.m_projector->run(*begin) > triggerCondition.m_triggerLevel;
				bool trigger;

				if (triggerCondition.m_triggerBothEdges) {
					trigger = triggerCondition.m_prevCondition ^ condition;
				} else {
					trigger = condition ^ !triggerCondition.m_triggerPositiveEdge;
				}

				if (trigger)
				{
					if (triggerCondition.m_triggerDelay > 0)
					{
						triggerCondition.m_triggerDelayCount = triggerCondition.m_triggerDelay;
						m_triggerState == TriggerDelay;
					}
					else
					{
					    if (triggerCondition.m_triggerCounts > 0)
					    {
					        triggerCondition.m_triggerCounts--;
					        m_triggerState = TriggerUntriggered;
					    }
					    else
					    {
					        // next trigger
	                        m_currentTriggerIndex++;

	                        if (m_currentTriggerIndex == m_triggerConditions.size())
	                        {
	                            m_currentTriggerIndex = 0;
	                            m_triggerState = TriggerTriggered;
	                            m_triggerPoint = begin;
	                            m_traceStart = true;
	                            break;
	                        }
	                        else
	                        {
	                            m_triggerState = TriggerUntriggered;
	                        }
					    }
					}
				}
			}
            else if (m_triggerState == TriggerDelay)
            {
                if (triggerCondition.m_triggerDelayCount > 0)
                {
                    triggerCondition.m_triggerDelayCount--;
                }
                else
                {
                    triggerCondition.m_triggerDelayCount = 0;

                    // next trigger
                    m_currentTriggerIndex++;

                    if (m_currentTriggerIndex == m_triggerConditions.size())
                    {
                        m_currentTriggerIndex = 0;
                        m_triggerState = TriggerTriggered;
                        m_triggerPoint = begin;
                        m_traceStart = true;
                        break;
                    }
                    else
                    {
                        // initialize a new trace
                        m_triggerState = TriggerUntriggered;
                        m_traceCompleteCount = 0;
                        m_triggerState = TriggerUntriggered;

                        feed(begin, end, positiveOnly); // process the rest of samples
                    }
                }
            }
            else
            {
                break;
            }

            ++begin;
		} // begin < end
	}

	// trace process
	if ((m_triggerConditions.size() == 0) || (m_triggerState == TriggerTriggered))
	{
	    // trace back

	    if (m_traceStart)
	    {
	        int count = begin - cbegin; // number of samples consumed since begin
	        std::vector<Trace>::iterator itTrace = m_traces.begin();

	        for (;itTrace != m_traces.end(); ++itTrace)
	        {
	            if (itTrace->m_inputIndex == feedIndex)
	            {
	                SampleVector::const_iterator startPoint = m_tracebackBuffers[feedIndex].getCurrent() - count;
                    SampleVector::const_iterator prevPoint = m_tracebackBuffers[feedIndex].getCurrent() - count - m_preTriggerDelay - itTrace->m_traceDelay;
                    processPrevTrace(prevPoint, startPoint, itTrace);
	            }
	        }

	        m_traceStart = false;
	    }

	    // live trace

	    int shift = (m_timeOfsProMill / 1000.0) * m_traceSize;

	    while (begin < end)
	    {
	        for (std::vector<Trace>::iterator itTrace = m_traces.begin(); itTrace != m_traces.end(); ++itTrace)
	        {
	            if (itTrace->m_inputIndex == feedIndex)
	            {
	                float posLimit = 1.0 / itTrace->m_amp;
	                float negLimit = -1.0 / itTrace->m_amp;

	                if (itTrace->m_traceCount < m_traceSize)
	                {
	                    float v = itTrace->m_projector->run(*begin) * itTrace->m_amp + itTrace->m_shift;

	                    if(v > posLimit) {
	                        v = posLimit;
	                    } else if (v < negLimit) {
	                        v = negLimit;
	                    }

	                    itTrace->m_trace[2*(itTrace->m_traceCount)] = itTrace->m_traceCount - shift;
	                    itTrace->m_trace[2*(itTrace->m_traceCount)+1] = v;

	                    itTrace->m_traceCount++;
	                }
	                else
	                {
	                    itTrace->m_traceCount = 0;

	                    if (m_traceCompleteCount < m_traces.size())
	                    {
	                        m_traceCompleteCount++;
	                    }
	                    else
	                    {
	                        //m_glScope->newTraces((DisplayTraces&) m_traces);  // TODO: glScopeNG new traces
	                        m_traceCompleteCount = 0;
	                    }
	                }
	            }
	        }

	        begin++;
	    }
	}
}

void ScopeVisNG::processPrevTrace(SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, std::vector<Trace>::iterator& trace)
{
    int shift = (m_timeOfsProMill / 1000.0) * m_traceSize;
    float posLimit = 1.0 / trace->m_amp;
    float negLimit = -1.0 / trace->m_amp;

    while (begin < end)
    {
        float v = trace->m_projector->run(*begin) * trace->m_amp + trace->m_shift;

        if(v > posLimit) {
            v = posLimit;
        } else if (v < negLimit) {
            v = negLimit;
        }

        trace->m_trace[2*(trace->m_traceCount)] = (trace->m_traceCount - shift); // display x
        trace->m_trace[2*(trace->m_traceCount) + 1] = v;                         // display y

        trace->m_traceCount++;
        begin++;
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
}

