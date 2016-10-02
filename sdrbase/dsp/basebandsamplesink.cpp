#include <dsp/basebandsamplesink.h>
#include "util/message.h"

BasebandSampleSink::BasebandSampleSink()
{
	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

BasebandSampleSink::~BasebandSampleSink()
{
}

void BasebandSampleSink::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != 0)
	{
		if (handleMessage(*message))
		{
			delete message;
		}
	}
}

