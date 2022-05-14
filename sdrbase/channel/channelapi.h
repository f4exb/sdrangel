///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// API for channels                                                              //
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

#ifndef SDRBASE_CHANNEL_CHANNELAPI_H_
#define SDRBASE_CHANNEL_CHANNELAPI_H_

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QList>

#include <stdint.h>

#include "export.h"
#include "util/messagequeue.h"

class DeviceAPI;

namespace SWGSDRangel
{
    class SWGChannelSettings;
    class SWGChannelReport;
    class SWGChannelActions;
    class SWGWorkspaceInfo;
}

class SDRBASE_API ChannelAPI : public QObject {
    Q_OBJECT
public:
    enum StreamType //!< This is the same enum as in PluginInterface
    {
        StreamSingleSink,   //!< Exposes a single sink stream (input, Rx)
        StreamSingleSource, //!< Exposes a single source stream (output, Tx)
        StreamMIMO          //!< May expose any number of sink and/or source streams
    };

    ChannelAPI(const QString& name, StreamType streamType);
    virtual ~ChannelAPI() {}
    virtual void destroy() = 0;
    virtual void setDeviceAPI(DeviceAPI*) = 0;
    virtual DeviceAPI *getDeviceAPI() = 0;

    virtual void getIdentifier(QString& id) = 0;
    virtual QString getIdentifier() const = 0;
    const QString& getURI() const { return m_uri; }
    virtual void getTitle(QString& title) = 0;
    virtual void setName(const QString& name) { m_name = name; }
    virtual const QString& getName() const { return m_name; }
    virtual qint64 getCenterFrequency() const = 0; //!< Applies to a default stream
    virtual void setCenterFrequency(qint64 frequency) = 0;

    virtual QByteArray serialize() const = 0;
    virtual bool deserialize(const QByteArray& data) = 0;

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    MessageQueue *getMessageQueueToGUI() { return m_guiMessageQueue; }
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

    /**
     * API adapter for the channel settings GET requests
     */
    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage)
    {
        (void) response;
        errorMessage = "Not implemented"; return 501;
    }

    /**
     * API adapter for the channel settings PUT and PATCH requests
     */
    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage)
    {
        (void) force;
        (void) channelSettingsKeys;
        (void) response;
        errorMessage = "Not implemented"; return 501;
    }

    /**
     * API adapter for the channel report GET requests
     */
    virtual int webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage)
    {
        (void) response;
        errorMessage = "Not implemented"; return 501;
    }

    /**
     * API adapter for the channel actions POST requests
     */
    virtual int webapiActionsPost(
            const QStringList& channelActionsKeys,
            SWGSDRangel::SWGChannelActions& query,
            QString& errorMessage)
    {
        (void) query;
        (void) channelActionsKeys;
        errorMessage = "Not implemented"; return 501;
    }

    /**
     * API adapter for the channel UI workspace GET requests
     */
    virtual int webapiWorkspaceGet(
            SWGSDRangel::SWGWorkspaceInfo& query,
            QString& errorMessage)
    {
        (void) query;
        errorMessage = "Not implemented"; return 501;
    }

    int getIndexInDeviceSet() const { return m_indexInDeviceSet; }

    void setIndexInDeviceSet(int indexInDeviceSet)
    {
        m_indexInDeviceSet = indexInDeviceSet;
        emit indexInDeviceSetChanged(indexInDeviceSet);
    }

    int getDeviceSetIndex() const { return m_deviceSetIndex; }
    void setDeviceSetIndex(int deviceSetIndex) { m_deviceSetIndex = deviceSetIndex; }
    uint64_t getUID() const { return m_uid; }

    // MIMO support
    StreamType getStreamType() const { return m_streamType; }
    virtual int getNbSinkStreams() const = 0;
    virtual int getNbSourceStreams() const = 0;
    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const = 0;

    void handlePipeMessageQueue(MessageQueue* messageQueue)
    {
        Message* message;

        while ((message = messageQueue->pop()) != nullptr) {
            m_inputMessageQueue.push(message);
        }
    }

protected:
	virtual bool handleMessage(const Message& cmd) = 0; //!< Processing of a message. Returns true if message has actually been processed
    MessageQueue *m_guiMessageQueue;    //!< Input message queue to the GUI
	MessageQueue m_inputMessageQueue;     //!< Queue for asynchronous inbound communication

protected slots:
	void handleInputMessages();

private:
    StreamType m_streamType;
    /** Unique identifier in a device set used for sorting. Used when there is no GUI.
     * In GUI version it is supported by GUI object name accessed through ChannelGUI.
     */
    QString m_name;
    /** Unique identifier attached to channel type */
    QString m_uri;

    int m_indexInDeviceSet;
    int m_deviceSetIndex;
    uint64_t m_uid;

signals:
    void indexInDeviceSetChanged(int index);
};


#endif // SDRBASE_CHANNEL_CHANNELAPI_H_
