#include "dsp/samplesink.h"
#include "util/message.h"

SampleSink::SampleSink()
{
	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessage()));
}

SampleSink::~SampleSink()
{
}

void SampleSink::handleInputMessages()
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

