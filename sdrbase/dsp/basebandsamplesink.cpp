#include "basebandsamplesink.h"

MESSAGE_CLASS_DEFINITION(BasebandSampleSink::MsgThreadedSink, Message)

BasebandSampleSink::BasebandSampleSink() :
    m_guiMessageQueue(0)
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

