///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// API for features                                                              //
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

#ifndef SDRBASE_FETURE_FEATUREAPI_H_
#define SDRBASE_FETURE_FEATUREAPI_H_

#include <QObject>
#include <QString>
#include <QByteArray>

#include "export.h"
#include "util/messagequeue.h"

class WebAPIAdapterInterface;
class ChannelAPI;
class Message;

namespace SWGSDRangel
{
    class SWGFeatureSettings;
    class SWGFeatureReport;
    class SWGFeatureActions;
    class SWGDeviceState;
    class SWGChannelSettings;
}

class SDRBASE_API Feature : public QObject {
    Q_OBJECT
public:
    enum FeatureState {
		StNotStarted,  //!< feature is before initialization
		StIdle,        //!< feature is idle
		StRunning,     //!< feature is running
		StError        //!< feature is in error
	};

    Feature(const QString& name, WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~Feature() {}
    virtual void destroy() = 0;
	virtual bool handleMessage(const Message& cmd) = 0; //!< Processing of a message. Returns true if message has actually been processed

    const QString& getURI() const { return m_uri; }
    virtual void getIdentifier(QString& id) const = 0;
    virtual QString getIdentifier() const = 0;
    virtual void getTitle(QString& title) const = 0;
    virtual void setName(const QString& name) { m_name = name; }
    virtual const QString& getName() const { return m_name; }

    virtual QByteArray serialize() const = 0;
    virtual bool deserialize(const QByteArray& data) = 0;

    /**
     * API adapter for the feature run GET requests
     */
    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage) const;

    /**
     * API adapter for the feature run POST and DELETE requests
     */
    virtual int webapiRun(bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage)
    {
        (void) run;
        (void) response;
        errorMessage = "Not implemented";
        return 501;
    }

    /**
     * API adapter for the feature settings GET requests
     */
    virtual int webapiSettingsGet(
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage)
    {
        (void) response;
        errorMessage = "Not implemented"; return 501;
    }

    /**
     * API adapter for the feature settings PUT and PATCH requests
     */
    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage)
    {
        (void) force;
        (void) featureSettingsKeys;
        (void) response;
        errorMessage = "Not implemented"; return 501;
    }

    /**
     * API adapter for the feature report GET requests
     */
    virtual int webapiReportGet(
            SWGSDRangel::SWGFeatureReport& response,
            QString& errorMessage)
    {
        (void) response;
        errorMessage = "Not implemented"; return 501;
    }

    /**
     * API adapter for the feature actions POST requests
     */
    virtual int webapiActionsPost(
            const QStringList& featureActionsKeys,
            SWGSDRangel::SWGFeatureActions& query,
            QString& errorMessage)
    {
        (void) query;
        (void) featureActionsKeys;
        errorMessage = "Not implemented"; return 501;
    }

    int getIndexInFeatureSet() const { return m_indexInFeatureSet; }

    void setIndexInFeatureSet(int indexInFeatureSet)
    {
        m_indexInFeatureSet = indexInFeatureSet;
        emit indexInFeatureSetChanged(m_indexInFeatureSet);
    }

    uint64_t getUID() const { return m_uid; }
    FeatureState getState() const { return m_state; }
    const QString& getErrorMessage() const { return m_errorMessage; }

	MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    MessageQueue *getMessageQueueToGUI() { return m_guiMessageQueue; }
    void setWorkspaceIndex(int index) { m_workspaceIndex = index; }
    int getWorkspaceIndex() const { return m_workspaceIndex; }

protected:
    MessageQueue m_inputMessageQueue;
    MessageQueue *m_guiMessageQueue;  //!< Input message queue to the GUI
    FeatureState m_state;
    QString m_errorMessage;
    WebAPIAdapterInterface *m_webAPIAdapterInterface;

    void getFeatureStateStr(QString& stateStr) const;

protected slots:
	void handleInputMessages();
    void handlePipeMessageQueue(MessageQueue* messageQueue);

private:
    QString m_name; //!< Unique identifier in a device set used for sorting may change depending on relative position in device set
    QString m_uri;  //!< Unique non modifiable identifier attached to channel type
    uint64_t m_uid;
    int m_indexInFeatureSet;
    int m_workspaceIndex;

signals:
    void indexInFeatureSetChanged(int index);
};

#endif // SDRBASE_FETURE_FEATUREAPI_H_
