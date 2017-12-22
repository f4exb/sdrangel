///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QWaitCondition>
#include <QMutex>
#include "util/message.h"
#include "util/messagequeue.h"

const char* Message::m_identifier = 0;

Message::Message() :
	m_destination(0)
{
}

Message::~Message()
{
}

const char* Message::getIdentifier() const
{
	return m_identifier;
}

bool Message::matchIdentifier(const char* identifier) const
{
	return m_identifier == identifier;
}

bool Message::match(const Message* message)
{
	return message->matchIdentifier(m_identifier);
}
