#ifndef INCLUDE_NULLSINK_H
#define INCLUDE_NULLSINK_H

#include "dsp/samplesink.h"
#include "util/export.h"

class MessageQueue;

class SDRANGELOVE_API NullSink : public SampleSink {
public:

	NullSink();

	void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly);
	void start();
	void stop();
	bool handleMessage(Message* message);
};

#endif // INCLUDE_NULLSINK_H
