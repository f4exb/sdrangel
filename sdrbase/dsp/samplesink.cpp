#include "dsp/samplesink.h"
#include "util/message.h"

SampleSink::SampleSink()
{
	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

SampleSink::~SampleSink()
{
}

void SampleSink::handleInputMessages()
{
	Message* message;
	int queueSize = m_inputMessageQueue.size();

	for (int i = 0; i < queueSize; i++)
	{
		message = m_inputMessageQueue.pop();

		if (handleMessage(*message))
		{
			delete message;
		}
		else
		{
			m_inputMessageQueue.push(message);
		}
	}
}

