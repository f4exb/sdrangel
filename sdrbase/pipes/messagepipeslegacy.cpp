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

#include "messagepipeslegacygcworker.h"
#include "messagepipeslegacy.h"
#include "pipeendpoint.h"

MessagePipesLegacy::MessagePipesLegacy()
{
	m_gcWorker = new MessagePipesLegacyGCWorker();
	m_gcWorker->setC2FRegistrations(
		m_registrations.getMutex(),
		m_registrations.getElements(),
		m_registrations.getConsumers()
	);
	m_gcWorker->moveToThread(&m_gcThread);
	startGC();
}

MessagePipesLegacy::~MessagePipesLegacy()
{
	if (m_gcWorker->isRunning()) {
		stopGC();
	}
}

MessageQueue *MessagePipesLegacy::registerChannelToFeature(const PipeEndPoint *source, PipeEndPoint *dest, const QString& type)
{
	qDebug("MessagePipesLegacy::registerChannelToFeature: %p %p %s", source, dest, qPrintable(type));
	return m_registrations.registerProducerToConsumer(source, dest, type);
}

MessageQueue *MessagePipesLegacy::unregisterChannelToFeature(const PipeEndPoint *source, PipeEndPoint *dest, const QString& type)
{
	qDebug("MessagePipesLegacy::unregisterChannelToFeature: %p %p %s", source, dest, qPrintable(type));
	MessageQueue *messageQueue = m_registrations.unregisterProducerToConsumer(source, dest, type);
	m_gcWorker->addMessageQueueToDelete(messageQueue);
	return messageQueue;
}

QList<MessageQueue*>* MessagePipesLegacy::getMessageQueues(const PipeEndPoint *source, const QString& type)
{
	//qDebug("MessagePipesLegacy::getMessageQueues: %p %s", source, qPrintable(type));
	return m_registrations.getElements(source, type);
}

void MessagePipesLegacy::startGC()
{
	qDebug("MessagePipesLegacy::startGC");
    m_gcWorker->startWork();
    m_gcThread.start();
}

void MessagePipesLegacy::stopGC()
{
    qDebug("MessagePipesLegacy::stopGC");
	m_gcWorker->stopWork();
	m_gcThread.quit();
	m_gcThread.wait();
}
