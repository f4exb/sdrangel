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

#include "feature/feature.h"
#include "maincore.h"
#include "messagepipescommon.h"
#include "messagepipesgcworker.h"

MessagePipesGCWorker::MessagePipesGCWorker() :
    m_running(false),
    m_c2fMutex(nullptr),
    m_c2fQueues(nullptr),
    m_c2fFeatures(nullptr)
{}

MessagePipesGCWorker::~MessagePipesGCWorker()
{}

void MessagePipesGCWorker::startWork()
{
    connect(&m_gcTimer, SIGNAL(timeout()), this, SLOT(processGC()));
    m_gcTimer.start(10000); // collect garbage every 10s
    m_running = true;
}

void MessagePipesGCWorker::stopWork()
{
    m_running = false;
    m_gcTimer.stop();
    disconnect(&m_gcTimer, SIGNAL(timeout()), this, SLOT(processGC()));
}

void MessagePipesGCWorker::processGC()
{
    // qDebug("MessagePipesGCWorker::processGC");
    if (m_c2fMutex)
    {
        QMutexLocker mlock(m_c2fMutex);
        QMap<MessagePipesCommon::ChannelRegistrationKey, QList<Feature*>>::iterator fIt = m_c2fFeatures->begin();

        // check deleted channels and features
        for (;fIt != m_c2fFeatures->end(); ++fIt)
        {
            MessagePipesCommon::ChannelRegistrationKey channelKey = fIt.key();
            const ChannelAPI *channel = channelKey.m_channel;

            if (MainCore::instance()->existsChannel(channel)) // look for deleted features
            {
                QList<Feature*>& features = fIt.value();
                int i = 0;

                while (i < features.size())
                {
                    if (MainCore::instance()->existsFeature(features[i])) {
                        i++;
                    }
                    else
                    {
                        features.removeAt(i);
                        m_c2fQueues->operator[](channelKey).removeAt(i);
                    }
                }
            }
            else // channel was destroyed
            {
                QList<Feature*>& features = fIt.value();

                for (int i = 0; i < features.size(); i++)
                {
                    MessagePipesCommon::MsgReportChannelDeleted *msg = MessagePipesCommon::MsgReportChannelDeleted::create(
                        m_c2fQueues->operator[](channelKey)[i], channelKey);
                    features[i]->getInputMessageQueue()->push(msg);
                }
            }
        }

        // remove keys with empty features
        fIt = m_c2fFeatures->begin();

        while (fIt != m_c2fFeatures->end())
        {
            if (fIt.value().size() == 0) {
                fIt = m_c2fFeatures->erase(fIt);
            } else {
                ++fIt;
            }
        }

        // remove keys with empty message queues
        QMap<MessagePipesCommon::ChannelRegistrationKey, QList<MessageQueue*>>::iterator qIt = m_c2fQueues->begin();

        while (qIt != m_c2fQueues->end())
        {
            if (qIt.value().size() == 0) {
                qIt = m_c2fQueues->erase(qIt);
            } else {
                ++qIt;
            }
        }
    }
}
