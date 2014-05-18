#ifndef INCLUDE_SCOPEVIS_H
#define INCLUDE_SCOPEVIS_H

#include "dsp/samplesink.h"
#include "util/export.h"

class GLScope;
class MessageQueue;

class SDRANGELOVE_API ScopeVis : public SampleSink {
public:
	enum TriggerChannel {
		TriggerFreeRun,
		TriggerChannelI,
		TriggerChannelQ
	};

	ScopeVis(GLScope* glScope = NULL);

	void configure(MessageQueue* msgQueue, TriggerChannel triggerChannel, Real triggerLevelHigh, Real triggerLevelLow);

	void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool firstOfBurst);
	void start();
	void stop();
	bool handleMessage(Message* message);

private:
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
	FixReal m_triggerLevelHigh;
	FixReal m_triggerLevelLow;
	int m_sampleRate;
};

#endif // INCLUDE_SCOPEVIS_H
