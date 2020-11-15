#ifndef INCLUDE_NULLSINK_H
#define INCLUDE_NULLSINK_H

#include <dsp/basebandsamplesink.h>
#include "export.h"

class Message;

class SDRBASE_API NullSink : public BasebandSampleSink {
public:

	NullSink();
	virtual ~NullSink();

	virtual bool init(const Message& cmd);
	using BasebandSampleSink::feed;
	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& message);
};

#endif // INCLUDE_NULLSINK_H
