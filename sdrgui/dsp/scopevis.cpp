#include "dsp/scopevis.h"
#include "gui/glscope.h"
#include "dsp/dspcommands.h"
#include "util/messagequeue.h"
#include <algorithm>

#include <QDebug>

#ifndef LINUX
inline double log2f(double n)
{
	return log(n) / log(2.0);
}
#endif

MESSAGE_CLASS_DEFINITION(ScopeVis::MsgConfigureScopeVis, Message)

const uint ScopeVis::m_traceChunkSize = 4800;

ScopeVis::ScopeVis(GLScope* glScope) :
	m_glScope(glScope),
    m_tracebackCount(0),
	m_fill(0),
	m_triggerState(Untriggered),
	m_triggerIndex(0),
	m_prevTrigger(false),
	m_triggerPre(0),
	m_triggerOneShot(false),
	m_armed(false),
    m_triggerDelayCount(0),
	m_triggerCount(0),
	m_sampleRate(0),
	m_prevArg(0.0),
	m_firstArg(true)
{
	setObjectName("ScopeVis");
	m_trace.reserve(100*m_traceChunkSize);
	m_trace.resize(20*m_traceChunkSize);
	m_traceback.resize(20*m_traceChunkSize);

	for (unsigned int i = 0; i < m_nbTriggers; i++)
	{
		m_triggerChannel[i] = TriggerFreeRun;
		m_triggerLevel[i] = 0.0;
		m_triggerPositiveEdge[i] = true;
		m_triggerBothEdges[i] = false;
		m_triggerDelay[i] = 0;
		m_triggerCounts[i] = 0;
	}
}

ScopeVis::~ScopeVis()
{
}

void ScopeVis::configure(MessageQueue* msgQueue,
	uint triggerIndex,
    TriggerChannel triggerChannel,
    Real triggerLevel,
    bool triggerPositiveEdge,
    bool triggerBothEdges,
    uint triggerPre,
    uint triggerDelay,
	uint triggerCounts,
    uint traceSize)
{
	Message* cmd = MsgConfigureScopeVis::create(triggerIndex,
			triggerChannel,
			triggerLevel,
			triggerPositiveEdge,
			triggerBothEdges,
			triggerPre,
			triggerDelay,
			triggerCounts,
			traceSize);
	msgQueue->push(cmd);
}

void ScopeVis::feed(const SampleVector::const_iterator& cbegin, const SampleVector::const_iterator& end, bool positiveOnly __attribute__((unused)))
{
	SampleVector::const_iterator begin(cbegin);

	if (m_triggerChannel[m_triggerIndex] == TriggerFreeRun) {
		m_triggerPoint = begin;
	}
	else if (m_triggerState == Triggered) {
		m_triggerPoint = begin;
	}
	else if (m_triggerState == Untriggered) {
		m_triggerPoint = end;
	}
	else if (m_triggerState == WaitForReset) {
		m_triggerPoint = end;
	}
	else {
		m_triggerPoint = begin;
	}

	while(begin < end)
	{
		if (m_triggerChannel[m_triggerIndex] == TriggerFreeRun)
		{
			int count = end - begin;

			if(count > (int)(m_trace.size() - m_fill))
			{
				count = m_trace.size() - m_fill;
			}

			std::vector<Complex>::iterator it = m_trace.begin() + m_fill;

			for(int i = 0; i < count; ++i)
			{
				*it++ = Complex(begin->real() / 32768.0f, begin->imag() / 32768.0f);
				++begin;
			}

			m_fill += count;

			if(m_fill >= m_trace.size())
			{
				m_glScope->newTrace(m_trace, m_sampleRate);
				m_fill = 0;
			}
		}
		else
		{
			if(m_triggerState == WaitForReset)
			{
				break;
			}
			if(m_triggerState == Config)
			{
                m_glScope->newTrace(m_trace, m_sampleRate); // send a dummy trace
				m_triggerState = Untriggered;
				m_triggerIndex = 0;
			}
            if(m_triggerState == Delay)
            {
                int count = end - begin;
				if (count > (int)(m_trace.size() - m_fill))
                {
					count = m_trace.size() - m_fill;
                }
                begin += count;
                m_fill += count;
				if(m_fill >= m_trace.size())
                {
                    m_fill = 0;
                    m_triggerDelayCount--;
                    if (m_triggerDelayCount == 0)
                    {
                    	if (nextTrigger())
                    	{
                    		m_triggerState = Untriggered;
                    	}
                    	else
                    	{
                    		m_triggerState = Triggered;
                    	}
                    }
                }
            }

			if(m_triggerState == Untriggered)
			{
				m_firstArg = true;

				while(begin < end)
				{
                    bool triggerCdt = triggerCondition(begin);

                    if (m_tracebackCount > m_triggerPre)
                    {
                        bool trigger;

                        if (m_triggerBothEdges[m_triggerIndex]) {
                        	trigger = m_prevTrigger ^ triggerCdt;
                        } else {
                        	trigger = triggerCdt ^ !m_triggerPositiveEdge[m_triggerIndex];
                        }

                        if (trigger)
						{
							if (m_armed)
							{
								m_armed = false;
                                if (m_triggerDelay[m_triggerIndex] > 0)
                                {
                                    m_triggerDelayCount = m_triggerDelay[m_triggerIndex];
                                    m_fill = 0;
                                    m_triggerState = Delay;
                                }
                                else
                                {
                                	if (nextTrigger())
                                	{
                                		m_triggerState = Untriggered;
                                	}
                                	else
                                	{
										m_triggerState = Triggered;
										m_triggerPoint = begin;
										// fill beginning of m_trace with delayed samples from the trace memory FIFO. Increment m_fill accordingly.
										if (m_triggerPre) { // do this process only if there is a pre-trigger delay
											std::copy(m_traceback.end() - m_triggerPre - 1, m_traceback.end() - 1, m_trace.begin());
											m_fill = m_triggerPre; // Increment m_fill accordingly (from 0).
										}
                                	}
                                }
								break;
							}
						}
						else
						{
							m_armed = true;
						}
                    }
                    m_prevTrigger = triggerCdt;
					++begin;
				}
			}

			if(m_triggerState == Triggered)
			{
				int count = end - begin;

				if(count > (int)(m_trace.size() - m_fill))
				{
					count = m_trace.size() - m_fill;
				}

				std::vector<Complex>::iterator it = m_trace.begin() + m_fill;

				for(int i = 0; i < count; ++i)
				{
					*it++ = Complex(begin->real() / 32768.0f, begin->imag() / 32768.0f);
					++begin;
				}

				m_fill += count;

				if(m_fill >= m_trace.size())
				{
					m_glScope->newTrace(m_trace, m_sampleRate);
					m_fill = 0;

					if (m_triggerOneShot) {
						m_triggerState = WaitForReset;
					} else {
                        m_tracebackCount = 0;
						m_triggerState = Untriggered;
						m_triggerIndex = 0;
					}
				}
			}
		}
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
	qDebug() << "ScopeVis::handleMessage";

	if (DSPSignalNotification::match(message))
	{
		DSPSignalNotification& notif = (DSPSignalNotification&) message;
		m_sampleRate = notif.getSampleRate();
		qDebug() << "  - DSPSignalNotification: m_sampleRate: " << m_sampleRate;

		return true;
	}
	else if (MsgConfigureScopeVis::match(message))
	{
		MsgConfigureScopeVis& conf = (MsgConfigureScopeVis&) message;

		m_tracebackCount = 0;
		m_triggerState = Config;
		uint index = conf.getTriggerIndex();
		m_triggerChannel[index] = (TriggerChannel) conf.getTriggerChannel();
		m_triggerLevel[index] = conf.getTriggerLevel();
		m_triggerPositiveEdge[index] = conf.getTriggerPositiveEdge();
		m_triggerBothEdges[index] = conf.getTriggerBothEdges();
		m_triggerPre = conf.getTriggerPre();

		if (m_triggerChannel[index] == TriggerDPhase)
		{
			m_firstArg = true;
		}

        if (m_triggerPre >= m_traceback.size())
        {
        	m_triggerPre = m_traceback.size() - 1; // top sample in FIFO is always the triggering one (pre-trigger delay = 0)
        }

        m_triggerDelay[index] = conf.getTriggerDelay();
        m_triggerCounts[index] = conf.getTriggerCounts();
        uint newSize = conf.getTraceSize();

        if (newSize != m_trace.size())
        {
            m_trace.resize(newSize);
        }

        if (newSize > m_traceback.size()) // fitting the exact required space is not a requirement for the back trace
        {
            m_traceback.resize(newSize);
        }

		qDebug() << "  - MsgConfigureScopeVis:"
				<< " triggerIndex: " << index
				<< " m_triggerChannel: " << m_triggerChannel[index]
				<< " m_triggerLevel: " << m_triggerLevel[index]
				<< " m_triggerPositiveEdge: " << (m_triggerPositiveEdge[index] ? "edge+" : "edge-")
				<< " m_triggerBothEdges: " << (m_triggerBothEdges[index] ? "yes" : "no")
				<< " m_preTrigger: " << m_triggerPre
				<< " m_triggerDelay: " << m_triggerDelay[index]
				<< " m_triggerCounts: " << m_triggerCounts[index]
				<< " m_traceSize: " << m_trace.size();

		return true;
	}
	else
	{
		return false;
	}
}

void ScopeVis::setSampleRate(int sampleRate)
{
	m_sampleRate = sampleRate;
}

bool ScopeVis::triggerCondition(SampleVector::const_iterator& it)
{
	Complex c(it->real()/32768.0f, it->imag()/32768.0f);
    m_traceback.push_back(c); // store into trace memory FIFO

    if (m_tracebackCount < m_traceback.size())
    { // increment count up to trace memory size
        m_tracebackCount++;
    }

	if (m_triggerChannel[m_triggerIndex] == TriggerChannelI)
	{
		return c.real() > m_triggerLevel[m_triggerIndex];
	}
	else if (m_triggerChannel[m_triggerIndex] == TriggerChannelQ)
	{
		return c.imag() > m_triggerLevel[m_triggerIndex];
	}
	else if (m_triggerChannel[m_triggerIndex] == TriggerMagLin)
	{
		return abs(c) > m_triggerLevel[m_triggerIndex];
	}
	else if (m_triggerChannel[m_triggerIndex] == TriggerMagDb)
	{
		Real mult = (10.0f / log2f(10.0f));
		Real v = c.real() * c.real() + c.imag() * c.imag();
		return mult * log2f(v) > m_triggerLevel[m_triggerIndex];
	}
	else if (m_triggerChannel[m_triggerIndex] == TriggerPhase)
	{
		return arg(c) / M_PI > m_triggerLevel[m_triggerIndex];
	}
	else if (m_triggerChannel[m_triggerIndex] == TriggerDPhase)
	{
		Real curArg = arg(c) - m_prevArg;
		m_prevArg = arg(c);

		if (curArg < -M_PI) {
			curArg += 2.0 * M_PI;
		} else if (curArg > M_PI) {
			curArg -= 2.0 * M_PI;
		}

		if (m_firstArg)
		{
			m_firstArg = false;
			return false;
		}
		else
		{
			return curArg / M_PI > m_triggerLevel[m_triggerIndex];
		}
	}
	else
	{
		return false;
	}
}

void ScopeVis::setOneShot(bool oneShot)
{
	m_triggerOneShot = oneShot;

	if ((m_triggerState == WaitForReset) && !oneShot) {
        m_tracebackCount = 0;
		m_triggerState = Untriggered;
		m_triggerIndex = 0;
	}
}

void ScopeVis::blockTrigger(bool blocked)
{
	if (blocked)
	{
		m_triggerState = WaitForReset;
	}
	else
	{
		if (!m_triggerOneShot) {
	        m_tracebackCount = 0;
			m_triggerState = Untriggered;
			m_triggerIndex = 0;
		}
	}
}

bool ScopeVis::nextTrigger()
{
	if (m_triggerCount < m_triggerCounts[m_triggerIndex])
	{
		m_triggerCount++;
		return true;
	}
	else
	{
		m_triggerIndex++;
		m_prevTrigger = false;
		m_triggerDelayCount = 0;
		m_triggerCount = 0;
		m_armed = false;

		if (m_triggerIndex == m_nbTriggers)
		{
			m_triggerIndex = 0;
			return false;
		}
		else if (m_triggerChannel[m_triggerIndex] == TriggerFreeRun)
		{
			m_triggerIndex = 0;
			return false;
		}
		else
		{
			return true;
		}
	}
}
