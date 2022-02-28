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

#include "channel/channelapi.h"
#include "feature/feature.h"
#include "util/messagequeue.h"
#include "maincore.h"
#include "messagepipeslegacycommon.h"
#include "messagepipeslegacygcworker.h"

bool MessagePipesLegacyGCWorker::MessagePipesGC::existsProducer(const PipeEndPoint *pipeEndPoint)
{
    return MainCore::instance()->existsChannel((const ChannelAPI *)pipeEndPoint)
        || MainCore::instance()->existsFeature((const Feature *)pipeEndPoint);
}

bool MessagePipesLegacyGCWorker::MessagePipesGC::existsConsumer(const PipeEndPoint *pipeEndPoint)
{
    return MainCore::instance()->existsChannel((const ChannelAPI *)pipeEndPoint)
        || MainCore::instance()->existsFeature((const Feature *)pipeEndPoint);
}

void MessagePipesLegacyGCWorker::MessagePipesGC::sendMessageToConsumer(
    const MessageQueue *,
    MessagePipesLegacyCommon::ChannelRegistrationKey,
    PipeEndPoint *)
{
}

MessagePipesLegacyGCWorker::MessagePipesLegacyGCWorker() :
    m_running(false)
{}

MessagePipesLegacyGCWorker::~MessagePipesLegacyGCWorker()
{}

void MessagePipesLegacyGCWorker::startWork()
{
    connect(&m_gcTimer, SIGNAL(timeout()), this, SLOT(processGC()));
    m_gcTimer.start(10000); // collect garbage every 10s
    m_running = true;
}

void MessagePipesLegacyGCWorker::stopWork()
{
    m_running = false;
    m_gcTimer.stop();
    disconnect(&m_gcTimer, SIGNAL(timeout()), this, SLOT(processGC()));
}

void MessagePipesLegacyGCWorker::addMessageQueueToDelete(MessageQueue *messageQueue)
{
    if (messageQueue)
    {
        m_gcTimer.start(10000); // restart GC to make sure deletion is postponed
        m_messagePipesGC.addElementToDelete(messageQueue);
    }
}

void MessagePipesLegacyGCWorker::processGC()
{
    // qDebug("MessagePipesLegacyGCWorker::processGC");
    m_messagePipesGC.processGC();
}
