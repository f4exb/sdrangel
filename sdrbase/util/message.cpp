#include <QWaitCondition>
#include <QMutex>
#include "util/message.h"
#include "util/messagequeue.h"

const char* Message::m_identifier = "Message";

Message::Message() :
	m_destination(NULL),
	m_synchronous(false),
	m_waitCondition(NULL),
	m_mutex(NULL),
	m_complete(0)
{
}

Message::~Message()
{
	if(m_waitCondition != NULL)
		delete m_waitCondition;
	if(m_mutex != NULL)
		delete m_mutex;
}

const char* Message::getIdentifier() const
{
	return m_identifier;
}

bool Message::matchIdentifier(const char* identifier) const
{
	return m_identifier == identifier;
}

bool Message::match(Message* message)
{
	return message->matchIdentifier(m_identifier);
}

void Message::submit(MessageQueue* queue, void* destination)
{
	m_destination = destination;
	m_synchronous = false;
	queue->submit(this);
}

int Message::execute(MessageQueue* queue, void* destination)
{
	m_destination = destination;
	m_synchronous = true;

	if(m_waitCondition == NULL)
		m_waitCondition = new QWaitCondition;
	if(m_mutex == NULL)
		m_mutex = new QMutex;

	m_mutex->lock();
	m_complete.testAndSetAcquire(0, 1);
	queue->submit(this);
	while(!m_complete.testAndSetAcquire(0, 1))
		((QWaitCondition*)m_waitCondition)->wait(m_mutex, 100);
	m_complete = 0;
	int result = m_result;
	m_mutex->unlock();
	return result;
}

void Message::completed(int result)
{
	if(m_synchronous) {
		m_result = result;
		m_complete = 0;
		if(m_waitCondition == NULL)
			qFatal("wait condition is NULL");
		m_waitCondition->wakeAll();
	} else {
		delete this;
	}
}
