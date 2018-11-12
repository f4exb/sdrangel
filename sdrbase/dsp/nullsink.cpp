#include "dsp/nullsink.h"
#include "dsp/dspcommands.h"
#include "util/messagequeue.h"

NullSink::NullSink()
{
	setObjectName("NullSink");
}

NullSink::~NullSink()
{
}

bool NullSink::init(const Message& message)
{
    (void) message;
	return false;
}

void NullSink::feed(const SampleVector::const_iterator& begin __attribute__((unused)), const SampleVector::const_iterator& end __attribute__((unused)), bool positiveOnly __attribute__((unused)))
{
}

void NullSink::start()
{
}

void NullSink::stop()
{
}

bool NullSink::handleMessage(const Message& message __attribute__((unused)))
{
	return false;
}
