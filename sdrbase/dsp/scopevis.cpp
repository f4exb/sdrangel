#include "dsp/scopevis.h"
#include "gui/glscope.h"
#include "dsp/dspcommands.h"
#include "util/messagequeue.h"
#include <algorithm>

#include <cstdio>
#include <iostream>

MESSAGE_CLASS_DEFINITION(ScopeVis::MsgConfigureScopeVis, Message)

const uint ScopeVis::m_traceChunkSize = 4800;

ScopeVis::ScopeVis(GLScope* glScope) :
	m_glScope(glScope),
    m_tracebackCount(0),
	m_fill(0),
	m_triggerState(Untriggered),
	m_triggerChannel(TriggerFreeRun),
	m_triggerLevel(0.0),
	m_triggerPositiveEdge(true),
	m_triggerPre(0),
	m_triggerOneShot(false),
	m_armed(false),
	m_sampleRate(0)
{
	m_trace.reserve(100*m_traceChunkSize);
	m_trace.resize(20*m_traceChunkSize);
	m_traceback.resize(20*m_traceChunkSize);
}

void ScopeVis::configure(MessageQueue* msgQueue, TriggerChannel triggerChannel, Real triggerLevel, bool triggerPositiveEdge, uint triggerPre, uint traceSize)
{
	Message* cmd = MsgConfigureScopeVis::create(triggerChannel, triggerLevel, triggerPositiveEdge, triggerPre, traceSize);
	cmd->submit(msgQueue, this);
}

void ScopeVis::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly)
{
	if (m_triggerChannel == TriggerFreeRun) {
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
		if (m_triggerChannel == TriggerFreeRun)
		{
			int count = end - begin;
			if(count > (int)(m_trace.size() - m_fill))
				count = m_trace.size() - m_fill;
			std::vector<Complex>::iterator it = m_trace.begin() + m_fill;
			for(int i = 0; i < count; ++i) {
				*it++ = Complex(begin->real() / 32768.0, begin->imag() / 32768.0);
				++begin;
			}
			m_fill += count;
			if(m_fill >= m_trace.size()) {
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
			if(m_triggerState == Untriggered)
			{
				while(begin < end)
				{
                    bool trigger = triggerCondition(begin);
					if ((trigger ^ !m_triggerPositiveEdge) && (m_tracebackCount > m_triggerPre))
                    {
						if (m_armed) 
                        {
							m_triggerState = Triggered;
							m_armed = false;
							m_triggerPoint = begin;
                            // fill beginning of m_trace with delayed samples from the trace memory FIFO. Increment m_fill accordingly.
                            if (m_triggerPre) { // do this process only if there is a pre-trigger delay
                                std::copy(m_traceback.end() - m_triggerPre - 1, m_traceback.end() - 1, m_trace.begin());
                                m_fill = m_triggerPre; // Increment m_fill accordingly (from 0).
                            }
							break;
						}
					}
					else 
                    {
						m_armed = true;
					}
					++begin;
				}
			}
			if(m_triggerState == Triggered)
			{
				int count = end - begin;
				if(count > (int)(m_trace.size() - m_fill))
					count = m_trace.size() - m_fill;
				std::vector<Complex>::iterator it = m_trace.begin() + m_fill;
				for(int i = 0; i < count; ++i) {
					*it++ = Complex(begin->real() / 32768.0, begin->imag() / 32768.0);
					++begin;
				}
				m_fill += count;
				if(m_fill >= m_trace.size()) {
					m_glScope->newTrace(m_trace, m_sampleRate);
					m_fill = 0;
					if (m_triggerOneShot) {
						m_triggerState = WaitForReset;
					} else {
                        m_tracebackCount = 0;
						m_triggerState = Untriggered;
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

bool ScopeVis::handleMessageKeep(Message* message)
{
	if(DSPSignalNotification::match(message)) {
		DSPSignalNotification* signal = (DSPSignalNotification*)message;
		m_sampleRate = signal->getSampleRate();
		/*fprintf(stderr, "ScopeVis::handleMessage : %d samples/sec, %lld Hz offset, traceSize: \n",
				m_sampleRate,
				signal->getFrequencyOffset(),
				m_trace.size());*/
		return true;
	} else if(MsgConfigureScopeVis::match(message)) {
		MsgConfigureScopeVis* conf = (MsgConfigureScopeVis*)message;
        m_tracebackCount = 0;
		m_triggerState = Untriggered;
		m_triggerChannel = (TriggerChannel) conf->getTriggerChannel();
		m_triggerLevel = conf->getTriggerLevel();
		m_triggerPositiveEdge = conf->getTriggerPositiveEdge();
		m_triggerPre = conf->getTriggerPre();
        if (m_triggerPre >= m_traceback.size()) {
        	m_triggerPre = m_traceback.size() - 1; // top sample in FIFO is always the triggering one (pre-trigger delay = 0)
        }
        uint newSize = conf->getTraceSize();
        if (newSize != m_trace.size()) {
            m_trace.resize(newSize);
        }
        if (newSize > m_traceback.size()) {  // fitting the exact required space is not a requirement for the back trace
            m_traceback.resize(newSize);
        }
		std::cerr << "ScopeVis::handleMessageKeep:"
				<< " m_triggerChannel: " << m_triggerChannel
				<< " m_triggerLevel: " << m_triggerLevel
				<< " m_triggerPositiveEdge: " << (m_triggerPositiveEdge ? "edge+" : "edge-")
				<< " m_preTrigger: " << m_triggerPre
				<< " m_traceSize: " << m_trace.size() << std::endl;
		return true;
	/*
	} else if(DSPConfigureScopeVis::match(message)) {
		DSPConfigureScopeVis* conf = (DSPConfigureScopeVis*)message;
		m_triggerState = Untriggered;
		m_triggerChannel = (TriggerChannel)conf->getTriggerChannel();
		m_triggerLevelHigh = conf->getTriggerLevelHigh() * 32767;
		m_triggerLevelLow = conf->getTriggerLevelLow() * 32767;
		return true;*/
	} else {
		return false;
	}
}

bool ScopeVis::handleMessage(Message* message)
{
	bool done = handleMessageKeep(message);

	if (done)
	{
		message->completed();
	}

	return done;
}

void ScopeVis::setSampleRate(int sampleRate)
{
	m_sampleRate = sampleRate;
}

bool ScopeVis::triggerCondition(SampleVector::const_iterator& it)
{
	Complex c(it->real()/32768.0, it->imag()/32768.0);
    m_traceback.push_back(c); // store into trace memory FIFO
    
    if (m_tracebackCount < m_traceback.size()) { // increment count up to trace memory size
        m_tracebackCount++;
    }
    
	if (m_triggerChannel == TriggerChannelI) {
		return c.real() > m_triggerLevel;
	}
	else if (m_triggerChannel == TriggerChannelQ) {
		return c.imag() > m_triggerLevel;
	}
	else if (m_triggerChannel == TriggerMagLin) {
		return abs(c) > m_triggerLevel;
	}
	else if (m_triggerChannel == TriggerMagDb) {
		Real mult = (10.0f / log2f(10.0f));
		Real v = c.real() * c.real() + c.imag() * c.imag();
		return mult * log2f(v) > m_triggerLevel;
	}
	else if (m_triggerChannel == TriggerPhase) {
		return arg(c) / M_PI > m_triggerLevel;
	}
	else {
		return false;
	}
}

void ScopeVis::setOneShot(bool oneShot)
{
	m_triggerOneShot = oneShot;

	if ((m_triggerState == WaitForReset) && !oneShot) {
        m_tracebackCount = 0;
		m_triggerState = Untriggered;
	}
}
