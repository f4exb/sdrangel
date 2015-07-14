#include "dsp/scopevis.h"
#include "gui/glscope.h"
#include "dsp/dspcommands.h"
#include "util/messagequeue.h"

#include <cstdio>
#include <iostream>

MESSAGE_CLASS_DEFINITION(ScopeVis::MsgConfigureScopeVis, Message)

ScopeVis::ScopeVis(GLScope* glScope) :
	m_glScope(glScope),
	m_trace(96000),
	m_fill(0),
	m_triggerState(Untriggered),
	m_triggerChannel(TriggerFreeRun),
	m_triggerLevel(0.0),
	m_triggerPositiveEdge(true),
	m_triggerOneShot(false),
	m_armed(false),
	m_sampleRate(0)
{
}

void ScopeVis::configure(MessageQueue* msgQueue, TriggerChannel triggerChannel, Real triggerLevel, bool triggerPositiveEdge)
{
	Message* cmd = MsgConfigureScopeVis::create(triggerChannel, triggerLevel, triggerPositiveEdge);
	cmd->submit(msgQueue, this);
}

void ScopeVis::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly)
{
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
				if (!m_triggerOneShot) {
					m_triggerState = Untriggered;
				}
			}
			if(m_triggerState == Untriggered)
			{
				while(begin < end)
				{
					if (triggerCondition(begin) ^ !m_triggerPositiveEdge) {
						if (m_armed) {
							m_triggerState = Triggered;
							m_armed = false;
							break;
						}
					}
					else {
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
		//fprintf(stderr, "ScopeVis::handleMessage @%x : %d samples/sec, %lld Hz offset\n", this, signal->getSampleRate(), signal->getFrequencyOffset());
		m_sampleRate = signal->getSampleRate();
		return true;
	} else if(MsgConfigureScopeVis::match(message)) {
		MsgConfigureScopeVis* conf = (MsgConfigureScopeVis*)message;
		m_triggerState = Untriggered;
		m_triggerChannel = (TriggerChannel) conf->getTriggerChannel();
		m_triggerLevel = conf->getTriggerLevel();
		m_triggerPositiveEdge = conf->getTriggerPositiveEdge();
		std::cerr << "ScopeVis::handleMessageKeep:"
				<< " m_triggerChannel: " << m_triggerChannel
				<< " m_triggerLevel: " << m_triggerLevel
				<< " m_triggerPositiveEdge: " << (m_triggerPositiveEdge ? "edge+" : "edge-") << std::endl;
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
}
