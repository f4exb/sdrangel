#include "dsp/scopevis.h"
#include "gui/glscope.h"
#include "dsp/dspcommands.h"
#include "util/messagequeue.h"

ScopeVis::ScopeVis(GLScope* glScope) :
	m_glScope(glScope),
	m_trace(100000),
	m_fill(0),
	m_triggerState(Untriggered),
	m_triggerChannel(TriggerFreeRun),
	m_triggerLevelHigh(0.01 * 32768),
	m_triggerLevelLow(0.01 * 32768 - 1024),
	m_sampleRate(0)
{
}

void ScopeVis::configure(MessageQueue* msgQueue, TriggerChannel triggerChannel, Real triggerLevelHigh, Real triggerLevelLow)
{
	Message* cmd = DSPConfigureScopeVis::create(triggerChannel, triggerLevelHigh, triggerLevelLow);
	cmd->submit(msgQueue, this);
}

void ScopeVis::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool firstOfBurst)
{
	while(begin < end) {
		if(m_triggerChannel == TriggerChannelI) {
			if(m_triggerState == Untriggered) {
				while(begin < end) {
					if(begin->real() >= m_triggerLevelHigh) {
						m_triggerState = Triggered;
						break;
					}
					++begin;
				}
			}
			if(m_triggerState == Triggered) {
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
					m_triggerState = WaitForReset;
				}
			}
			if(m_triggerState == WaitForReset) {
				while(begin < end) {
					if(begin->real() < m_triggerLevelLow) {
						m_triggerState = Untriggered;
						break;
					}
					++begin;
				}
			}
		} else if(m_triggerChannel == TriggerChannelQ) {
			if(m_triggerState == Untriggered) {
				while(begin < end) {
					if(begin->imag() >= m_triggerLevelHigh) {
						m_triggerState = Triggered;
						break;
					}
					++begin;
				}
			}
			if(m_triggerState == Triggered) {
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
					m_triggerState = WaitForReset;
				}
			}
			if(m_triggerState == WaitForReset) {
				while(begin < end) {
					if(begin->imag() < m_triggerLevelLow) {
						m_triggerState = Untriggered;
						break;
					}
					++begin;
				}
			}
		} else {
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
	}
}

void ScopeVis::start()
{
}

void ScopeVis::stop()
{
}

bool ScopeVis::handleMessage(Message* message)
{
	if(DSPSignalNotification::match(message)) {
		DSPSignalNotification* signal = (DSPSignalNotification*)message;
		m_sampleRate = signal->getSampleRate();
		message->completed();
		return true;
	} else if(DSPConfigureScopeVis::match(message)) {
		DSPConfigureScopeVis* conf = (DSPConfigureScopeVis*)message;
		m_triggerState = Untriggered;
		m_triggerChannel = (TriggerChannel)conf->getTriggerChannel();
		m_triggerLevelHigh = conf->getTriggerLevelHigh() * 32767;
		m_triggerLevelLow = conf->getTriggerLevelLow() * 32767;
		message->completed();
		return true;
	} else {
		return false;
	}
}
