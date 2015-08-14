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
	return false;
}

void NullSink::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly)
{
}

void NullSink::start()
{
}

void NullSink::stop()
{
}

bool NullSink::handleMessage(const Message& message)
{
	return false;
}
