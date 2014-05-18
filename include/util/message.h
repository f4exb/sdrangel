#ifndef INCLUDE_MESSAGE_H
#define INCLUDE_MESSAGE_H

#include <stdlib.h>
#include <QAtomicInt>
#include "util/export.h"

class MessageQueue;
class QWaitCondition;
class QMutex;

class SDRANGELOVE_API Message {
public:
	Message();
	virtual ~Message();

	virtual const char* getIdentifier() const;
	virtual bool matchIdentifier(const char* identifier) const;
	static bool match(Message* message);

	void* getDestination() const { return m_destination; }

	void submit(MessageQueue* queue, void* destination = NULL);
	int execute(MessageQueue* queue, void* destination = NULL);

	void completed(int result = 0);

protected:
	// addressing
	static const char* m_identifier;
	void* m_destination;

	// stuff for synchronous messages
	bool m_synchronous;
	QWaitCondition* m_waitCondition;
	QMutex* m_mutex;
	QAtomicInt m_complete;
	int m_result;
};

#define MESSAGE_CLASS_DECLARATION \
	public: \
		const char* getIdentifier() const; \
		bool matchIdentifier(const char* identifier) const; \
		static bool match(Message* message); \
	protected: \
		static const char* m_identifier; \
	private:

#define MESSAGE_CLASS_DEFINITION(Name, BaseClass) \
	const char* Name::m_identifier = #Name; \
	const char* Name::getIdentifier() const { return m_identifier; } \
	bool Name::matchIdentifier(const char* identifier) const {\
		return (m_identifier == identifier) ? true : BaseClass::matchIdentifier(identifier); \
	} \
	bool Name::match(Message* message) { return message->matchIdentifier(m_identifier); }

#endif // INCLUDE_MESSAGE_H
