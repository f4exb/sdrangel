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

#ifndef SDRBASE_PIPES_MESSAGEPIPESLEGACYGCWORKER_H_
#define SDRBASE_PIPES_MESSAGEPIPESLEGACYGCWORKER_H_

#include <QObject>
#include <QTimer>

#include "export.h"

#include "messagepipeslegacycommon.h"
#include "elementpipesgc.h"

class QMutex;

class SDRBASE_API MessagePipesLegacyGCWorker : public QObject
{
    Q_OBJECT
public:
    MessagePipesLegacyGCWorker();
    ~MessagePipesLegacyGCWorker();

    void setC2FRegistrations(
        QMutex *c2fMutex,
        QMap<MessagePipesLegacyCommon::ChannelRegistrationKey, QList<MessageQueue*>> *c2fQueues,
        QMap<MessagePipesLegacyCommon::ChannelRegistrationKey, QList<PipeEndPoint*>> *c2fPipeEndPoints
    )
    {
        m_messagePipesGC.setRegistrations(c2fMutex, c2fQueues, c2fPipeEndPoints);
    }

    void startWork();
    void stopWork();
    void addMessageQueueToDelete(MessageQueue *messageQueue);
    bool isRunning() const { return m_running; }

private:
    class MessagePipesGC : public ElementPipesGC<PipeEndPoint, PipeEndPoint, MessageQueue>
    {
    private:
        virtual bool existsProducer(const PipeEndPoint *pipeEndPoint);
        virtual bool existsConsumer(const PipeEndPoint *pipeEndPoint);
        virtual void sendMessageToConsumer(const MessageQueue *messageQueue, MessagePipesLegacyCommon::ChannelRegistrationKey key, PipeEndPoint *pipeEndPoint);
    };

    MessagePipesGC m_messagePipesGC;
    bool m_running;
    QTimer m_gcTimer;

private slots:
    void processGC(); //!< Collect garbage
};

#endif // SDRBASE_PIPES_MESSAGEPIPESLEGACYGCWORKER_H_
