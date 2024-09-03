///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2021-2022 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2023 Daniele Forsi <iu5hkx@gmail.com>                           //
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

#include "util/messagequeue.h"
#include "messagequeuestore.h"

MessageQueueStore::MessageQueueStore()
{}

MessageQueueStore::~MessageQueueStore()
{
    deleteAllElements();
}

QObject *MessageQueueStore::createElement()
{
    MessageQueue *messageQueue = new MessageQueue();
    m_messageQueues.push_back(messageQueue);
    qDebug("MessageQueueStore::createElement: %d added", (int)m_messageQueues.size() - 1);
    return messageQueue;
}

void MessageQueueStore::deleteElement(QObject *element)
{
    int i = m_messageQueues.indexOf((MessageQueue*) element);

    if (i >= 0)
    {
        qDebug("MessageQueueStore::deleteElement: delete element at %d", i);
        delete m_messageQueues[i];
        m_messageQueues.removeAt(i);
    }
}

void MessageQueueStore::deleteAllElements()
{
    for (auto& messageQueue : m_messageQueues) {
        delete messageQueue;
    }

    m_messageQueues.clear();
}
