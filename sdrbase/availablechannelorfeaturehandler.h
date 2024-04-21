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

#ifndef SDRBASE_AVAILABLECHANNELORFEATUREHANDLER_H_
#define SDRBASE_AVAILABLECHANNELORFEATUREHANDLER_H_

#include "availablechannelorfeature.h"
#include "export.h"
#include "util/messagequeue.h"

class ChannelAPI;
class Feature;

// Utility class to help keeping track of list of available channels or features and optionally register pipes to them
class SDRBASE_API AvailableChannelOrFeatureHandler : public QObject
{
    Q_OBJECT

public:

    // Use this constructor to just keep track of available channels and features with specified URIs and kinds
    AvailableChannelOrFeatureHandler(QStringList uris, const QString& kinds = "RTMF") :
        m_uris(uris),
        m_kinds(kinds)
    {
        init();
    }

    // Use this constructor to keep track of available channels and features with specified URIs and kinds and register pipes with the given names to them
    AvailableChannelOrFeatureHandler(QStringList uris, QStringList pipeNames, const QString& kinds = "RTMF") :
        m_uris(uris),
        m_pipeNames(pipeNames),
        m_kinds(kinds)
    {
        init();
    }

    void scanAvailableChannelsAndFeatures();

    const AvailableChannelOrFeatureList& getAvailableChannelOrFeatureList() const {
        return m_availableChannelOrFeatureList;
    }

    QObject* registerPipes(const QString& longIdFrom, const QStringList& pipeNames);
    void deregisterPipes(QObject* from, const QStringList& pipeNames);

private:

    AvailableChannelOrFeatureList m_availableChannelOrFeatureList;

    QStringList m_uris;             //!< URIs of channels/features we want to create a list for
    QStringList m_pipeNames;        //!< List of pipe names to register
    QString m_kinds;

    void init();
    void registerPipe(const QString& pipeName, QObject *channelOrFeature);

private slots:

    void handleChannelAdded(int deviceSetIndex, ChannelAPI *channel);
    void handleChannelRemoved(int deviceSetIndex, ChannelAPI *channel);
    void handleStreamIndexChanged(int streamIndex);
    void handleFeatureAdded(int featureSetIndex, Feature *feature);
    void handleFeatureRemoved(int featureSetIndex, Feature *feature);

signals:
    void channelsOrFeaturesChanged(const QStringList& renameFrom, const QStringList& renameTo, const QStringList& removed, const QStringList& added);  //!< Emitted when list of channels or features has changed
    void messageEnqueued(MessageQueue *messageQueue);   //!< Emitted when message enqueued to a pipe

};

#endif // SDRBASE_AVAILABLECHANNELORFEATUREHANDLER_H_
