///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include "messagepipes.h"

bool MessagePipes::ChannelRegistrationKey::operator<(const ChannelRegistrationKey& other) const
{
	if (m_channel !=  other.m_channel) {
		return m_channel < other.m_channel;
	} else {
		return m_typeId < other.m_typeId;
	}
}

MessagePipes::MessagePipes() :
	m_typeCount(0)
{}

MessagePipes::~MessagePipes()
{
	QMap<ChannelRegistrationKey, QList<MessageQueue*>>::iterator mit = m_messageRegistrations.begin();

	for (; mit != m_messageRegistrations.end(); ++mit)
	{
		QList<MessageQueue*>::iterator lit = mit->begin();

		for (; lit != mit->end(); ++lit) {
			delete *lit;
		}
	}
}

MessageQueue *MessagePipes::registerChannelToFeature(const ChannelAPI *source, const Feature *feature, const QString& type)
{
	int typeId;

	if (m_typeIds.contains(type))
	{
		typeId = m_typeIds.value(type);
	}
	else
	{
		typeId++;
		m_typeIds.insert(type, typeId);
	}

	const ChannelRegistrationKey regKey = ChannelRegistrationKey{source, typeId};

	if (m_messageRegistrations.contains(regKey))
	{
		m_messageRegistrations.insert(regKey, QList<MessageQueue*>());
		m_featureRegistrations.insert(regKey, QList<const Feature*>());
	}

	MessageQueue *messageQueue = new MessageQueue();
	m_messageRegistrations[regKey].append(messageQueue);
	m_featureRegistrations[regKey].append(feature);

	return messageQueue;
}

QList<MessageQueue*>* MessagePipes::getMessageQueues(const ChannelAPI *source, const QString& type)
{
	if (!m_typeIds.contains(type)) {
		return nullptr;
	}

	const ChannelRegistrationKey regKey = ChannelRegistrationKey{source, m_typeIds.value(type)};

	if (m_messageRegistrations.contains(regKey)) {
		return &m_messageRegistrations[regKey];
	} else {
		return nullptr;
	}
}
