///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QGlobalStatic>

#include "util/messagequeue.h"

#include "messagepipesgcworker.h"
#include "messagepipes.h"
#include "pipeendpoint.h"

MessagePipes::MessagePipes()
{
	m_gcWorker = new MessagePipesGCWorker();
	m_gcWorker->setC2FRegistrations(
		m_registrations.getMutex(),
		m_registrations.getElements(),
		m_registrations.getConsumers()
	);
	m_gcWorker->moveToThread(&m_gcThread);
	startGC();
}

MessagePipes::~MessagePipes()
{
	if (m_gcWorker->isRunning()) {
		stopGC();
	}
}

MessageQueue *MessagePipes::registerChannelToFeature(const PipeEndPoint *source, Feature *feature, const QString& type)
{
	return m_registrations.registerProducerToConsumer(source, feature, type);
}

MessageQueue *MessagePipes::unregisterChannelToFeature(const PipeEndPoint *source, Feature *feature, const QString& type)
{
	MessageQueue *messageQueue = m_registrations.unregisterProducerToConsumer(source, feature, type);
	m_gcWorker->addMessageQueueToDelete(messageQueue);
	return messageQueue;
}

QList<MessageQueue*>* MessagePipes::getMessageQueues(const PipeEndPoint *source, const QString& type)
{
	return m_registrations.getElements(source, type);
}

void MessagePipes::startGC()
{
	qDebug("MessagePipes::startGC");
    m_gcWorker->startWork();
    m_gcThread.start();
}

void MessagePipes::stopGC()
{
    qDebug("MessagePipes::stopGC");
	m_gcWorker->stopWork();
	m_gcThread.quit();
	m_gcThread.wait();
}
