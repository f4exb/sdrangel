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

#ifndef SDRBASE_PIPES_MESSAGEPIPESGCWORKER_H_
#define SDRBASE_PIPES_MESSAGEPIPESGCWORKER_H_

#include <QObject>
#include <QTimer>

#include "export.h"

#include "messagepipescommon.h"
#include "elementpipesgc.h"

class QMutex;

class SDRBASE_API MessagePipesGCWorker : public QObject
{
    Q_OBJECT
public:
    MessagePipesGCWorker();
    ~MessagePipesGCWorker();

    void setC2FRegistrations(
        QMutex *c2fMutex,
        QMap<MessagePipesCommon::ChannelRegistrationKey, QList<MessageQueue*>> *c2fQueues,
        QMap<MessagePipesCommon::ChannelRegistrationKey, QList<Feature*>> *c2fFeatures
    )
    {
        m_messagePipesGC.setRegistrations(c2fMutex, c2fQueues, c2fFeatures);
    }

    void startWork();
    void stopWork();
    void addMessageQueueToDelete(MessageQueue *messageQueue);
    bool isRunning() const { return m_running; }

private:
    class MessagePipesGC : public ElementPipesGC<PipeEndPoint, Feature, MessageQueue>
    {
    private:
        virtual bool existsProducer(const PipeEndPoint *pipeEndPoint);
        virtual bool existsConsumer(const Feature *feature);
        virtual void sendMessageToConsumer(const MessageQueue *messageQueue,  MessagePipesCommon::ChannelRegistrationKey key, Feature *feature);
    };

    MessagePipesGC m_messagePipesGC;
    bool m_running;
    QTimer m_gcTimer;

private slots:
    void processGC(); //!< Collect garbage
};

#endif
