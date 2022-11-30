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

#ifndef INCLUDE_FEATURE_VORLOCALIZERWORKER_H_
#define INCLUDE_FEATURE_VORLOCALIZERWORKER_H_

#include <QObject>
#include <QTimer>

#include "util/message.h"
#include "util/messagequeue.h"

#include "vorlocalizersettings.h"

class WebAPIAdapterInterface;
class ChannelAPI;
class Feature;

class VorLocalizerWorker : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureVORLocalizerWorker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const VORLocalizerSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureVORLocalizerWorker* create(const VORLocalizerSettings& settings, const QList<QString>& settingsKeys, bool force)
        {
            return new MsgConfigureVORLocalizerWorker(settings, settingsKeys, force);
        }

    private:
        VORLocalizerSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureVORLocalizerWorker(const VORLocalizerSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgRefreshChannels : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgRefreshChannels* create() {
            return new MsgRefreshChannels();
        }

    protected:
        MsgRefreshChannels() :
            Message()
        { }
    };

    VorLocalizerWorker(WebAPIAdapterInterface *webAPIAdapterInterface);
    ~VorLocalizerWorker();
    void reset();
    void startWork();
    void stopWork();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToFeature(MessageQueue *messageQueue) { m_msgQueueToFeature = messageQueue; }
    void setAvailableChannels(QHash<ChannelAPI*, VORLocalizerSettings::AvailableChannel> *avaialbleChannels) {
        m_availableChannels = avaialbleChannels;
    }

private:
    struct VORRange
    {
        std::vector<int> m_vorIndices;
        int m_frequencyRange;

        VORRange() = default;
        VORRange(const VORRange&) = default;
        VORRange& operator=(const VORRange&) = default;
    };

    struct RRDevice
    {
        int m_deviceIndex;
        int m_frequency;

        RRDevice() = default;
        RRDevice(const RRDevice&) = default;
        RRDevice& operator=(const RRDevice&) = default;
    };

    struct RRChannel
    {
        ChannelAPI *m_channelAPI;
        int m_channelIndex;
        int m_frequencyShift;
        int m_navId;

        RRChannel() = default;
        RRChannel(const RRChannel&) = default;
        RRChannel& operator=(const RRChannel&) = default;
    };

    struct RRTurnPlan
    {
        RRDevice m_device;
        int m_bandwidth;
        std::vector<RRChannel> m_channels;
        bool m_fixedCenterFrequency;    // Devices such as FileInput that can't have center freq changed

        RRTurnPlan() = default;
        RRTurnPlan(const RRTurnPlan&) = default;
        RRTurnPlan& operator=(const RRTurnPlan&) = default;
    };

    struct ChannelAllocation
    {
        int m_navId;
        int m_deviceIndex;
        int m_channelIndex;

        ChannelAllocation() = default;
        ChannelAllocation(const ChannelAllocation&) = default;
        ChannelAllocation& operator=(const ChannelAllocation&) = default;
    };

    WebAPIAdapterInterface *m_webAPIAdapterInterface;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    MessageQueue *m_msgQueueToFeature; //!< Queue to report state to GUI
    VORLocalizerSettings m_settings;
    QList<VORLocalizerSettings::VORChannel> m_vorChannels;
    QHash<int, ChannelAllocation> m_channelAllocations;
    QHash<ChannelAPI*, VORLocalizerSettings::AvailableChannel> *m_availableChannels;
	QTimer m_updateTimer;
    QRecursiveMutex m_mutex;
    QTimer m_rrTimer;
    std::vector<std::vector<RRTurnPlan>> m_rrPlans; //!< Round robin plans for each device
    std::vector<int> m_rrTurnCounters; //!< Round robin turn count for each device

    bool handleMessage(const Message& cmd);
    void applySettings(const VORLocalizerSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void removeVORChannel(int navId);
    void addVORChannel(const VORLocalizerSubChannelSettings& subChannelSettings);
    void updateChannels(); //!< (re)allocate channels to service VORs
    void setChannelShift(int deviceIndex, int channelIndex, double targetOffset, int vorNavId);
    void setAudioMute(int vorNavId, bool audioMute);
    static quint64 getDeviceCenterFrequency(int deviceIndex);
    static int getDeviceSampleRate(int deviceIndex);
    static bool hasCenterFrequencySetting(int deviceIndex);
    static void generateIndexCombinations(int length, int subLength, std::vector<std::vector<int>>& indexCombinations);
    static void getVORRanges(const QList<VORLocalizerSettings::VORChannel>& vors, int subLength, std::vector<VORRange>& vorRanges);
    static void filterVORRanges(std::vector<VORRange>& vorRanges, int thresholdBW);
    static void getChannelsByDevice(const QHash<ChannelAPI*, VORLocalizerSettings::AvailableChannel> *availableChannels, std::vector<RRTurnPlan>& m_deviceChannels);

private slots:
    void started();
    void finished();
    void handleInputMessages();
	void updateHardware();
    void rrNextTurn();
};

#endif // INCLUDE_FEATURE_VORLOCALIZERWORKER_H_
