#ifndef INCLUDE_SAMPLESINK_H
#define INCLUDE_SAMPLESINK_H

#include <QObject>
#include "dsptypes.h"
#include "util/export.h"
#include "util/messagequeue.h"

class Message;

class SDRANGELOVE_API SampleSink : public QObject {
public:
	SampleSink();
	virtual ~SampleSink();

	virtual bool init(const Message& cmd) = 0;
	virtual void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly) = 0;
	virtual void start() = 0;
	virtual void stop() = 0;
	virtual bool handleMessage(const Message& cmd) = 0; //!< Processing of a message. Returns true if message has actually been processed

	MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
	MessageQueue *getOutputMessageQueue() { return &m_outputMessageQueue; }

protected:
	MessageQueue m_inputMessageQueue;
	MessageQueue m_outputMessageQueue;
};

#endif // INCLUDE_SAMPLESINK_H
