///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef SDRBASE_PIPES_MESSAGEPIPESLEGACYCOMON_H_
#define SDRBASE_PIPES_MESSAGEPIPESLEGACYCOMON_H_

#include <QHash>
#include <QMap>
#include <QMutex>

#include "export.h"
#include "util/message.h"
#include "elementpipescommon.h"

class PipeEndPoint;
class MessageQueue;

class SDRBASE_API MessagePipesLegacyCommon
{
public:
    typedef ElementPipesCommon::RegistrationKey<PipeEndPoint> ChannelRegistrationKey;

    /** Send this message to stakeholders when the garbage collector finds that a channel was deleted */
    class SDRBASE_API MsgReportChannelDeleted : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const MessageQueue *getMessageQueue() const { return m_messageQueue; }
        const ChannelRegistrationKey& getChannelRegistrationKey() const { return m_channelRegistrationKey; }

        static MsgReportChannelDeleted* create(const MessageQueue *messageQueue, const ChannelRegistrationKey& channelRegistrationKey) {
            return new MsgReportChannelDeleted(messageQueue, channelRegistrationKey);
        }

    private:
        const MessageQueue *m_messageQueue;
        ChannelRegistrationKey m_channelRegistrationKey;

        MsgReportChannelDeleted(const MessageQueue *messageQueue, const ChannelRegistrationKey& channelRegistrationKey) :
            Message(),
            m_messageQueue(messageQueue),
            m_channelRegistrationKey(channelRegistrationKey)
        { }
    };
};

#endif // SDRBASE_PIPES_MESSAGEPIPESLEGACYCOMON_H_
