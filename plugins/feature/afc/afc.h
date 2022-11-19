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

#ifndef INCLUDE_FEATURE_AFC_H_
#define INCLUDE_FEATURE_AFC_H_

#include <QNetworkRequest>
#include <QRecursiveMutex>

#include "feature/feature.h"
#include "util/message.h"

#include "afcsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class WebAPIAdapterInterface;
class DeviceSet;
class AFCWorker;
class QThread;

namespace SWGSDRangel {
    class SWGDeviceState;
}

class AFC : public Feature
{
	Q_OBJECT
public:
    class MsgConfigureAFC : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AFCSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureAFC* create(const AFCSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureAFC(settings, settingsKeys, force);
        }

    private:
        AFCSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureAFC(const AFCSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    class MsgDeviceTrack : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgDeviceTrack* create() {
            return new MsgDeviceTrack();
        }
    protected:
        MsgDeviceTrack() :
            Message()
        { }
    };

    class MsgDevicesApply : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgDevicesApply* create() {
            return new MsgDevicesApply();
        }
    protected:
        MsgDevicesApply() :
            Message()
        { }
    };

    class MsgDeviceSetListsQuery : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgDeviceSetListsQuery* create() {
            return new MsgDeviceSetListsQuery();
        }
    protected:
        MsgDeviceSetListsQuery() :
            Message()
        { }
    };

    class MsgDeviceSetListsReport : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        struct DeviceSetReference
        {
            unsigned int m_deviceIndex;
            bool m_rx;
        };

        void addTrackerDevice(unsigned int index, bool rx) {
            m_trackerDevices.push_back(DeviceSetReference{index, rx});
        }
        void addTrackedDevice(unsigned int index, bool rx) {
            m_trackedDevices.push_back(DeviceSetReference{index, rx});
        }
        const QList<DeviceSetReference>& getTrackerDevices() const {
            return m_trackerDevices;
        }
        const QList<DeviceSetReference>& getTrackedDevices() const {
            return m_trackedDevices;
        }
        static MsgDeviceSetListsReport* create() {
            return new MsgDeviceSetListsReport();
        }
    private:
        MsgDeviceSetListsReport() :
            Message()
        { }
        QList<DeviceSetReference> m_trackerDevices;
        QList<DeviceSetReference> m_trackedDevices;
    };

    AFC(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~AFC();
    virtual void destroy() { delete this; }
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) const { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) const { title = m_settings.m_title; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int webapiRun(bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGFeatureReport& response,
            QString& errorMessage);

    virtual int webapiActionsPost(
            const QStringList& featureActionsKeys,
            SWGSDRangel::SWGFeatureActions& query,
            QString& errorMessage);

    static void webapiFormatFeatureSettings(
        SWGSDRangel::SWGFeatureSettings& response,
        const AFCSettings& settings);

    static void webapiUpdateFeatureSettings(
            AFCSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    QThread *m_thread;
    QRecursiveMutex m_mutex;
    bool m_running;
    AFCWorker *m_worker;
    AFCSettings m_settings;
    DeviceSet *m_trackerDeviceSet;
    DeviceSet *m_trackedDeviceSet;
    int m_trackerIndexInDeviceSet;
    ChannelAPI *m_trackerChannelAPI;
    QList<ChannelAPI*> m_trackedChannelAPIs;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void start();
    void stop();
    void applySettings(const AFCSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response);
    void webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const AFCSettings& settings, bool force);
    void trackerDeviceChange(int deviceIndex);
    void trackedDeviceChange(int deviceIndex);
    void removeTrackerFeatureReference();
    void removeTrackedFeatureReferences();
    void updateDeviceSetLists();

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleChannelMessageQueue(MessageQueue* messageQueue);
    void handleTrackerMessagePipeToBeDeleted(int reason, QObject* object);
    void handleTrackedMessagePipeToBeDeleted(int reason, QObject* object);
};

#endif // INCLUDE_FEATURE_AFC_H_
