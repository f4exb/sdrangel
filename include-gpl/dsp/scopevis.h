#ifndef INCLUDE_SCOPEVIS_H
#define INCLUDE_SCOPEVIS_H

#include "dsp/samplesink.h"
#include "util/export.h"
#include "util/message.h"

class GLScope;
class MessageQueue;

class SDRANGELOVE_API ScopeVis : public SampleSink {
public:
	enum TriggerChannel {
		TriggerFreeRun,
		TriggerChannelI,
		TriggerChannelQ,
		TriggerMagLin,
		TriggerMagDb,
		TriggerPhase
	};

	ScopeVis(GLScope* glScope = NULL);

	void configure(MessageQueue* msgQueue, TriggerChannel triggerChannel, Real triggerLevel, bool triggerPositiveEdge);
	void setOneShot(bool oneShot);

	void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly);
	void start();
	void stop();
	bool handleMessageKeep(Message* message);
	bool handleMessage(Message* message);

	void setSampleRate(int sampleRate);
	int getSampleRate() const { return m_sampleRate; }

private:
	class MsgConfigureScopeVis : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getTriggerChannel() const { return m_triggerChannel; }
		Real getTriggerLevel() const { return m_triggerLevel; }
		Real getTriggerPositiveEdge() const { return m_triggerPositiveEdge; }

		static MsgConfigureScopeVis* create(int triggerChannel, Real triggerLevel, bool triggerPositiveEdge)
		{
			return new MsgConfigureScopeVis(triggerChannel, triggerLevel, triggerPositiveEdge);
		}

	private:
		int m_triggerChannel;
		Real m_triggerLevel;
		bool m_triggerPositiveEdge;

		MsgConfigureScopeVis(int triggerChannel, Real triggerLevel, bool triggerPositiveEdge) :
			Message(),
			m_triggerChannel(triggerChannel),
			m_triggerLevel(triggerLevel),
			m_triggerPositiveEdge(triggerPositiveEdge)
		{ }
	};

	enum TriggerState {
		Untriggered,
		Triggered,
		WaitForReset
	};

	GLScope* m_glScope;
	std::vector<Complex> m_trace;
	uint m_fill;
	TriggerState m_triggerState;
	TriggerChannel m_triggerChannel;
	Real m_triggerLevel;
	bool m_triggerPositiveEdge;
	bool m_triggerOneShot;
	bool m_armed;
	int m_sampleRate;

	bool triggerCondition(SampleVector::const_iterator& it);
};

#endif // INCLUDE_SCOPEVIS_H
