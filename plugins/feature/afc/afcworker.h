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

#ifndef INCLUDE_FEATURE_AFCWORKER_H_
#define INCLUDE_FEATURE_AFCWORKER_H_

#include <QObject>
#include <QMap>
#include <QTimer>

#include "util/message.h"
#include "util/messagequeue.h"

#include "afcsettings.h"

class WebAPIAdapterInterface;
class DeviceSet;
class ChannelAPI;

class AFCWorker : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureAFCWorker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AFCSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureAFCWorker* create(const AFCSettings& settings, const QList<QString>& settingsKeys, bool force)
        {
            return new MsgConfigureAFCWorker(settings, settingsKeys, force);
        }

    private:
        AFCSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureAFCWorker(const AFCSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgTrackedDeviceChange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getDeviceIndex() const { return m_deviceIndex; }

        static MsgTrackedDeviceChange* create(int deviceIndex)
        {
            return new MsgTrackedDeviceChange(deviceIndex);
        }

    private:
        int m_deviceIndex;

        MsgTrackedDeviceChange(int deviceIndex) :
            Message(),
            m_deviceIndex(deviceIndex)
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

    AFCWorker(WebAPIAdapterInterface *webAPIAdapterInterface);
    ~AFCWorker();
    void reset();
    void startWork();
    void stopWork();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_msgQueueToGUI = messageQueue; }
    uint64_t getTrackerDeviceFrequency() const { return m_trackerDeviceFrequency; }
    int getTrackerChannelOffset() const { return m_trackerChannelOffset; }

private:
    struct ChannelTracking
    {
        int m_channelOffset;
        int m_trackerOffset;
        int m_channelDirection;

        ChannelTracking() :
            m_channelOffset(0),
            m_trackerOffset(0),
            m_channelDirection(0)
        {}

        ChannelTracking(int channelOffset, int trackerOffset, int channelDirection) :
            m_channelOffset(channelOffset),
            m_trackerOffset(trackerOffset),
            m_channelDirection(channelDirection)
        {}

        ChannelTracking(const ChannelTracking& other) :
            m_channelOffset(other.m_channelOffset),
            m_trackerOffset(other.m_trackerOffset),
            m_channelDirection(other.m_channelDirection)
        {}
    };

    WebAPIAdapterInterface *m_webAPIAdapterInterface;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    MessageQueue *m_msgQueueToGUI; //!< Queue to report state to GUI
    AFCSettings m_settings;
    DeviceSet *m_trackerDeviceSet;
    DeviceSet *m_trackedDeviceSet;
    ChannelAPI *m_freqTracker;
    uint64_t m_trackerDeviceFrequency;
    int m_trackerChannelOffset;
    QMap<ChannelAPI*, ChannelTracking> m_channelsMap;
	QTimer m_updateTimer;
    QRecursiveMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const AFCSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void initTrackerDeviceSet(int deviceSetIndex);
    void initTrackedDeviceSet(int deviceSetIndex);
    void processChannelSettings(
        const ChannelAPI *channelAPI,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings
    );
    bool updateChannelOffset(ChannelAPI *channelAPI, int direction, int offset);
    bool updateDeviceFrequency(DeviceSet *deviceSet, const QString& key, int64_t frequency);
    int getDeviceDirection(DeviceAPI *deviceAPI);
    void getDeviceSettingsKey(DeviceAPI *deviceAPI, QString& settingsKey);
    void reportUpdateTarget(int correction, bool done);

private slots:
    void updateTarget();
    void handleInputMessages();
};

#endif // INCLUDE_FEATURE_AFCWORKER_H_
