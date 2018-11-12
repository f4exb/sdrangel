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

void NullSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    (void) begin;
    (void) end;
    (void) positiveOnly;
}

void NullSink::start()
{
}

void NullSink::stop()
{
}

bool NullSink::handleMessage(const Message& message)
{
    (void) message;
	return false;
}
