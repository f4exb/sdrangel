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

#include "messagepipesgcworker.h"
#include "messagepipes.h"

MessagePipes::MessagePipes() :
	m_typeCount(0),
	m_c2fMutex(QMutex::Recursive)
{
	m_gcWorker = new MessagePipesGCWorker();
	m_gcWorker->setC2FRegistrations(&m_c2fMutex, &m_c2fQueues, &m_c2fFEatures);
	m_gcWorker->moveToThread(&m_gcThread);
	startGC();
}

MessagePipes::~MessagePipes()
{
	if (m_gcWorker->isRunning()) {
		stopGC();
	}

	QMap<MessagePipesCommon::ChannelRegistrationKey, QList<MessageQueue*>>::iterator mit = m_c2fQueues.begin();

	for (; mit != m_c2fQueues.end(); ++mit)
	{
		QList<MessageQueue*>::iterator lit = mit->begin();

		for (; lit != mit->end(); ++lit) {
			delete *lit;
		}
	}
}

MessageQueue *MessagePipes::registerChannelToFeature(const ChannelAPI *source, Feature *feature, const QString& type)
{
	int typeId;
	QMutexLocker mlock(&m_c2fMutex);

	if (m_typeIds.contains(type))
	{
		typeId = m_typeIds.value(type);
	}
	else
	{
		typeId++;
		m_typeIds.insert(type, typeId);
	}

	const MessagePipesCommon::ChannelRegistrationKey regKey = MessagePipesCommon::ChannelRegistrationKey{source, typeId};
	MessageQueue *messageQueue;

	if (m_c2fFEatures[regKey].contains(feature))
	{
		int i = m_c2fFEatures[regKey].indexOf(feature);
		messageQueue = m_c2fQueues[regKey][i];
	}
	else
	{
		messageQueue = new MessageQueue();
		m_c2fQueues[regKey].append(messageQueue);
		m_c2fFEatures[regKey].append(feature);
	}

	return messageQueue;
}

void MessagePipes::unregisterChannelToFeature(const ChannelAPI *source, Feature *feature, const QString& type)
{
	if (m_typeIds.contains(type))
	{
		int typeId = m_typeIds.value(type);
		const MessagePipesCommon::ChannelRegistrationKey regKey = MessagePipesCommon::ChannelRegistrationKey{source, typeId};

		if (m_c2fFEatures.contains(regKey) && m_c2fFEatures[regKey].contains(feature))
		{
			QMutexLocker mlock(&m_c2fMutex);
			int i = m_c2fFEatures[regKey].indexOf(feature);
			m_c2fFEatures[regKey].removeAt(i);
			MessageQueue *messageQueue = m_c2fQueues[regKey][i];
			delete messageQueue;
			m_c2fQueues[regKey].removeAt(i);
		}
	}
}

QList<MessageQueue*>* MessagePipes::getMessageQueues(const ChannelAPI *source, const QString& type)
{
	if (!m_typeIds.contains(type)) {
		return nullptr;
	}

	QMutexLocker mlock(&m_c2fMutex);
	const MessagePipesCommon::ChannelRegistrationKey regKey = MessagePipesCommon::ChannelRegistrationKey{source, m_typeIds.value(type)};

	if (m_c2fQueues.contains(regKey)) {
		return &m_c2fQueues[regKey];
	} else {
		return nullptr;
	}
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
