#include "dsp/nullsink.h"
#include "dsp/dspcommands.h"
#include "util/messagequeue.h"

NullSink::NullSink()
{
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

bool NullSink::handleMessage(Message* message)
{
	message->completed();
	return true;
}
