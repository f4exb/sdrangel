#ifndef INCLUDE_NULLSINK_H
#define INCLUDE_NULLSINK_H

#include "dsp/samplesink.h"
#include "util/export.h"

class Message;

class SDRANGELOVE_API NullSink : public SampleSink {
public:

	NullSink();
	virtual ~NullSink();

	virtual bool init(const Message& cmd);
	virtual void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& message);
};

#endif // INCLUDE_NULLSINK_H
