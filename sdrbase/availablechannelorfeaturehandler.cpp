///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston <jon@beniston.com>                            //
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

#include "availablechannelorfeaturehandler.h"
#include "feature/feature.h"
#include "channel/channelapi.h"
#include "maincore.h"

void AvailableChannelOrFeatureHandler::init()
{
    QObject::connect(MainCore::instance(), &MainCore::channelAdded, this, &AvailableChannelOrFeatureHandler::handleChannelAdded);
    QObject::connect(MainCore::instance(), &MainCore::channelRemoved, this, &AvailableChannelOrFeatureHandler::handleChannelRemoved);
    QObject::connect(MainCore::instance(), &MainCore::featureAdded, this, &AvailableChannelOrFeatureHandler::handleFeatureAdded);
    QObject::connect(MainCore::instance(), &MainCore::featureRemoved, this, &AvailableChannelOrFeatureHandler::handleFeatureRemoved);
    // Don't call scanAvailableChannelsAndFeatures() here, as channelsOrFeaturesChanged slot will not yet be connected
    // Owner should call scanAvailableChannelsAndFeatures after connection
}

void AvailableChannelOrFeatureHandler::scanAvailableChannelsAndFeatures()
{
    // Get current list of channels and features with specified URIs
    AvailableChannelOrFeatureList availableChannelOrFeatureList = MainCore::instance()->getAvailableChannelsAndFeatures(m_uris, m_kinds);

    // Look for new channels or features
    for (const auto& channelOrFeature : availableChannelOrFeatureList)
    {
        if (!m_availableChannelOrFeatureList.contains(channelOrFeature))
        {
            // For MIMO channels, get notified when stream index changes
            if (channelOrFeature.m_kind == 'M')
            {
                ChannelAPI *channel = qobject_cast<ChannelAPI *>(channelOrFeature.m_object);
                if (channel) {
                    QObject::connect(channel, &ChannelAPI::streamIndexChanged, this, &AvailableChannelOrFeatureHandler::handleStreamIndexChanged);
                }
            }
            // Register pipes for any new channels or features
            for (const auto& pipeName: m_pipeNames) {
                registerPipe(pipeName, channelOrFeature.m_object);
            }
        }
    }

    // Check to see if list has changed
    bool changes = m_availableChannelOrFeatureList != availableChannelOrFeatureList;

    // Check to see if anything has been renamed, due to indexes changing after device/channel/feature removal
    // or if stream index was changed
    QStringList renameFrom;
    QStringList renameTo;
    for (const auto& channelOrFeature : availableChannelOrFeatureList)
    {
        int index = m_availableChannelOrFeatureList.indexOfObject(channelOrFeature.m_object);
        if (index >= 0)
        {
            const AvailableChannelOrFeature& oldEntry = m_availableChannelOrFeatureList.at(index);
            if ((oldEntry.m_superIndex != channelOrFeature.m_superIndex)
                || (oldEntry.m_index != channelOrFeature.m_index)
                || ((channelOrFeature.m_kind == 'M') && (oldEntry.m_streamIndex != channelOrFeature.m_streamIndex))
                )
            {
                renameFrom.append(oldEntry.getId());
                renameTo.append(channelOrFeature.getId());
                renameFrom.append(oldEntry.getLongId());
                renameTo.append(channelOrFeature.getLongId());
            }
        }
    }

    // Create lists of which channels and features have been added or removed
    QStringList added;
    QStringList removed;

    for (const auto& channelOrFeature : availableChannelOrFeatureList)
    {
        if (m_availableChannelOrFeatureList.indexOfObject(channelOrFeature.m_object) < 0) {
            added.append(channelOrFeature.getId());
        }
    }
    for (const auto& channelOrFeature : m_availableChannelOrFeatureList)
    {
        if (availableChannelOrFeatureList.indexOfObject(channelOrFeature.m_object) < 0) {
            removed.append(channelOrFeature.getId());
        }
    }

    m_availableChannelOrFeatureList = availableChannelOrFeatureList;

    // Signal if list has changed
    if (changes) {
        emit channelsOrFeaturesChanged(renameFrom, renameTo, removed, added);
    }
}

QObject* AvailableChannelOrFeatureHandler::registerPipes(const QString& longIdFrom, const QStringList& pipeNames)
{
    int index = m_availableChannelOrFeatureList.indexOfLongId(longIdFrom);
    if (index >= 0)
    {
        QObject *object = m_availableChannelOrFeatureList[index].m_object;
        for (const auto& pipeName : pipeNames) {
            registerPipe(pipeName, object);
        }
        return object;
    }
    else
    {
        return nullptr;
    }
}

void AvailableChannelOrFeatureHandler::deregisterPipes(QObject* from, const QStringList& pipeNames)
{
    // Don't dereference 'from' here, as it may have been deleted
    if (from)
    {
        qDebug("AvailableChannelOrFeatureHandler::deregisterPipes: unregister (%p)", from);
        MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();
        for (const auto& pipeName : pipeNames) {
            messagePipes.unregisterProducerToConsumer(from, this, pipeName);
        }
    }
}

void AvailableChannelOrFeatureHandler::registerPipe(const QString& pipeName, QObject *channelOrFeature)
{
    qDebug("MessagePipeHandler::registerPipe: register %s (%p)", qPrintable(channelOrFeature->objectName()), channelOrFeature);
    MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();

    ObjectPipe *pipe = messagePipes.registerProducerToConsumer(channelOrFeature, this, pipeName);
    MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
    QObject::connect(
        messageQueue,
        &MessageQueue::messageEnqueued,
        this,
        [=](){ emit messageEnqueued(messageQueue); },
        Qt::QueuedConnection
    );
}

void AvailableChannelOrFeatureHandler::handleChannelAdded(int deviceSetIndex, ChannelAPI *channel)
{
    (void) deviceSetIndex;
    (void) channel;

    scanAvailableChannelsAndFeatures();
}

void AvailableChannelOrFeatureHandler::handleChannelRemoved(int deviceSetIndex, ChannelAPI *channel)
{
    (void) deviceSetIndex;
    (void) channel;

    scanAvailableChannelsAndFeatures();
}

void AvailableChannelOrFeatureHandler::handleStreamIndexChanged(int streamIndex)
{
    (void) streamIndex;

    scanAvailableChannelsAndFeatures();
}

void AvailableChannelOrFeatureHandler::handleFeatureAdded(int featureSetIndex, Feature *feature)
{
    (void) featureSetIndex;
    (void) feature;

    scanAvailableChannelsAndFeatures();
}

void AvailableChannelOrFeatureHandler::handleFeatureRemoved(int featureSetIndex, Feature *feature)
{
    (void) featureSetIndex;
    (void) feature;

    scanAvailableChannelsAndFeatures();
}
