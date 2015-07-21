#ifndef INCLUDE_SCOPEVIS_H
#define INCLUDE_SCOPEVIS_H

#include <boost/circular_buffer.hpp>
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

	static const uint m_traceChunkSize;

	ScopeVis(GLScope* glScope = NULL);

	void configure(MessageQueue* msgQueue, TriggerChannel triggerChannel, Real triggerLevel, bool triggerPositiveEdge, uint triggerDelay, uint traceSize);
	void setOneShot(bool oneShot);

	void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly);
	void start();
	void stop();
	bool handleMessageKeep(Message* message);
	bool handleMessage(Message* message);

	void setSampleRate(int sampleRate);
	int getSampleRate() const { return m_sampleRate; }
	SampleVector::const_iterator getTriggerPoint() const { return m_triggerPoint; }

private:
	class MsgConfigureScopeVis : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getTriggerChannel() const { return m_triggerChannel; }
		Real getTriggerLevel() const { return m_triggerLevel; }
		Real getTriggerPositiveEdge() const { return m_triggerPositiveEdge; }
        uint getTriggerDelay() const { return m_triggerDelay; }
        uint getTraceSize() const { return m_traceSize; }

		static MsgConfigureScopeVis* create(int triggerChannel, Real triggerLevel, bool triggerPositiveEdge, uint triggerDelay, uint traceSize)
		{
			return new MsgConfigureScopeVis(triggerChannel, triggerLevel, triggerPositiveEdge, triggerDelay, traceSize);
		}

	private:
		int m_triggerChannel;
		Real m_triggerLevel;
		bool m_triggerPositiveEdge;
        uint m_triggerDelay;
        uint m_traceSize;

		MsgConfigureScopeVis(int triggerChannel, Real triggerLevel, bool triggerPositiveEdge, uint triggerDelay, uint traceSize) :
			Message(),
			m_triggerChannel(triggerChannel),
			m_triggerLevel(triggerLevel),
			m_triggerPositiveEdge(triggerPositiveEdge),
            m_triggerDelay(triggerDelay),
            m_traceSize(traceSize)
		{ }
	};

	enum TriggerState {
		Untriggered,
		Triggered,
		WaitForReset
	};

	GLScope* m_glScope;
	std::vector<Complex> m_trace;                //!< Raw trace to be used by GLScope
	boost::circular_buffer<Complex> m_traceback; //!< FIFO for samples prior to triggering point to support pre-trigger (when in triggered mode)
    uint m_tracebackCount;                       //!< Count of samples stored into trace memory since triggering is active up to trace memory size
	uint m_fill;
	TriggerState m_triggerState;
	TriggerChannel m_triggerChannel;
	Real m_triggerLevel;
	bool m_triggerPositiveEdge;
    uint  m_triggerDelay; //!< Trigger delay in number of samples
	bool m_triggerOneShot;
	bool m_armed;
	int m_sampleRate;
	SampleVector::const_iterator m_triggerPoint;

	bool triggerCondition(SampleVector::const_iterator& it);
};

#endif // INCLUDE_SCOPEVIS_H
