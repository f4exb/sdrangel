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

#ifndef SDRBASE_PIPES_MESSAGEPIPES_H_
#define SDRBASE_PIPES_MESSAGEPIPES_H_

#include <QObject>
#include <QHash>
#include <QMap>

#include "export.h"
#include "util/messagequeue.h"

class ChannelAPI;
class Feature;

class SDRBASE_API MessagePipes : public QObject
{
    Q_OBJECT
public:
    struct ChannelRegistrationKey
    {
        const ChannelAPI *m_channel;
        int m_typeId;

        bool operator<(const ChannelRegistrationKey& other) const;
    };

    MessagePipes();
    MessagePipes(const MessagePipes&) = delete;
    MessagePipes& operator=(const MessagePipes&) = delete;
    ~MessagePipes();

    MessageQueue *registerChannelToFeature(const ChannelAPI *source, const Feature *feature, const QString& type);
    QList<MessageQueue*>* getMessageQueues(const ChannelAPI *source, const QString& type);

private:
    QHash<QString, int> m_typeIds;
    int m_typeCount;
    QMap<ChannelRegistrationKey, QList<MessageQueue*>> m_messageRegistrations;
    QMap<ChannelRegistrationKey, QList<const Feature*>> m_featureRegistrations;
};

#endif // SDRBASE_PIPES_MESSAGEPIPES_H_
