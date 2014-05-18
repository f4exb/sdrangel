#ifndef INCLUDE_MESSAGEQUEUE_H
#define INCLUDE_MESSAGEQUEUE_H

#include <QObject>
#include <QQueue>
#include "spinlock.h"
#include "util/export.h"

class Message;

class SDRANGELOVE_API MessageQueue : public QObject {
	Q_OBJECT

public:
	MessageQueue(QObject* parent = NULL);
	~MessageQueue();

	void submit(Message* message);
	Message* accept();

	int countPending();

signals:
	void messageEnqueued();

private:
	Spinlock m_lock;
	QQueue<Message*> m_queue;
};

#endif // INCLUDE_MESSAGEQUEUE_H
