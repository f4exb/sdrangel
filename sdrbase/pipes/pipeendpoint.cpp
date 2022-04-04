///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <vector>

#include <QRegExp>
#include <QDebug>

#include "dsp/dspengine.h"
#include "device/deviceset.h"
#include "channel/channelapi.h"
#include "feature/featureset.h"
#include "feature/feature.h"
#include "maincore.h"

#include "pipeendpoint.h"

MESSAGE_CLASS_DEFINITION(PipeEndPoint::MsgReportPipes, Message)

QList<PipeEndPoint::AvailablePipeSource> PipeEndPoint::updateAvailablePipeSources(QString pipeName, QStringList pipeTypes, QStringList pipeURIs, PipeEndPoint *destination)
{
    MainCore *mainCore = MainCore::instance();
    MessagePipesLegacy& messagePipes = mainCore->getMessagePipesLegacy();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();
    QHash<PipeEndPoint *, AvailablePipeSource> availablePipes;

    // Source is a channel
    int deviceIndex = 0;
    for (std::vector<DeviceSet*>::const_iterator it = deviceSets.begin(); it != deviceSets.end(); ++it, deviceIndex++)
    {
        DSPDeviceSourceEngine *deviceSourceEngine =  (*it)->m_deviceSourceEngine;
        DSPDeviceSinkEngine *deviceSinkEngine =  (*it)->m_deviceSinkEngine;

        if (deviceSourceEngine || deviceSinkEngine)
        {
            for (int chi = 0; chi < (*it)->getNumberOfChannels(); chi++)
            {
                ChannelAPI *channel = (*it)->getChannelAt(chi);
                int i = pipeURIs.indexOf(channel->getURI());

                if (i >= 0)
                {
                    if (!availablePipes.contains(channel))
                    {
                        MessageQueue *messageQueue = messagePipes.registerChannelToFeature(channel, destination, pipeName);
                        if (MainCore::instance()->existsFeature((const Feature *)destination))
                        {
                            // Destination is feature
                            Feature *featureDest = (Feature *)destination;
                            QObject::connect(
                                messageQueue,
                                &MessageQueue::messageEnqueued,
                                featureDest,
                                [=](){ featureDest->handlePipeMessageQueue(messageQueue); },
                                Qt::QueuedConnection
                            );
                        }
                        else
                        {
                            // Destination is a channel
                            // Can't use Qt::QueuedConnection because ChannelAPI isn't a QObject
                            ChannelAPI *channelDest = (ChannelAPI *)destination;
                            QObject::connect(
                                messageQueue,
                                &MessageQueue::messageEnqueued,
                                [=](){ channelDest->handlePipeMessageQueue(messageQueue); }
                            );
                        }
                    }

                    AvailablePipeSource availablePipe =
                        AvailablePipeSource{
                            deviceSinkEngine != nullptr ? AvailablePipeSource::TX : AvailablePipeSource::RX,
                            deviceIndex,
                            chi,
                            channel,
                            pipeTypes.at(i)
                        };
                    availablePipes[channel] = availablePipe;
                }
            }
        }
    }

    // Source is a feature
    std::vector<FeatureSet*>& featureSets = mainCore->getFeatureeSets();
    int featureIndex = 0;
    for (std::vector<FeatureSet*>::const_iterator it = featureSets.begin(); it != featureSets.end(); ++it, featureIndex++)
    {
        for (int fi = 0; fi < (*it)->getNumberOfFeatures(); fi++)
        {
            Feature *feature = (*it)->getFeatureAt(fi);
            int i = pipeURIs.indexOf(feature->getURI());

            if (i >= 0)
            {
                if (!availablePipes.contains(feature))
                {
                    MessageQueue *messageQueue = messagePipes.registerChannelToFeature(feature, destination, pipeName);
                    if (MainCore::instance()->existsFeature((const Feature *)destination))
                    {
                        // Destination is feature
                        Feature *featureDest = (Feature *)destination;
                        QObject::connect(
                            messageQueue,
                            &MessageQueue::messageEnqueued,
                            featureDest,
                            [=](){ featureDest->handlePipeMessageQueue(messageQueue); },
                            Qt::QueuedConnection
                        );
                    }
                    else
                    {
                        // Destination is a channel
                        // Can't use Qt::QueuedConnection because ChannelAPI isn't a QObject
                        ChannelAPI *channelDest = (ChannelAPI *)destination;
                        QObject::connect(
                            messageQueue,
                            &MessageQueue::messageEnqueued,
                            [=](){ channelDest->handlePipeMessageQueue(messageQueue); }
                        );
                    }
                }

                AvailablePipeSource availablePipe =
                    AvailablePipeSource{
                        AvailablePipeSource::Feature,
                        featureIndex,
                        fi,
                        feature,
                        pipeTypes.at(i)
                    };
                availablePipes[feature] = availablePipe;
            }
        }
    }

    QList<AvailablePipeSource> availablePipeList;
    QHash<PipeEndPoint*, AvailablePipeSource>::iterator it = availablePipes.begin();

    for (; it != availablePipes.end(); ++it) {
        availablePipeList.push_back(*it);
    }
    return availablePipeList;
}

PipeEndPoint *PipeEndPoint::getPipeEndPoint(const QString name, const QList<AvailablePipeSource> &availablePipeSources)
{
    QRegExp re("([TRF])([0-9]+):([0-9]+) ([a-zA-Z0-9]+)");
    if (re.exactMatch(name))
    {
        QString type = re.capturedTexts()[1];
        int setIndex = re.capturedTexts()[2].toInt();
        int index = re.capturedTexts()[3].toInt();
        QString id = re.capturedTexts()[4];

        QListIterator<AvailablePipeSource> itr(availablePipeSources);
        while (itr.hasNext())
        {
            AvailablePipeSource p = itr.next();
            if ((p.m_setIndex == setIndex) && (p.m_index == index) && (id == p.m_id)) {
                return p.m_source;
            }
        }
    }
    else
    {
        qDebug() << "PipeEndPoint::getPipeEndPoint: " << name << " is malformed";
    }
    return nullptr;
}
