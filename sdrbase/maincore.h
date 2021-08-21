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

#ifndef SDRBASE_MAINCORE_H_
#define SDRBASE_MAINCORE_H_

#include <vector>

#include <QMap>
#include <QTimer>
#include <QDateTime>

#include "export.h"
#include "settings/mainsettings.h"
#include "util/message.h"
#include "pipes/messagepipes.h"
#include "pipes/datapipes.h"
#include "channel/channelapi.h"

class DeviceSet;
class FeatureSet;
class Feature;
class PluginManager;
class MessageQueue;

namespace qtwebapp {
    class LoggerWithFile;
}

namespace SWGSDRangel
{
    class SWGChannelReport;
    class SWGChannelSettings;
    class SWGMapItem;
    class SWGTargetAzimuthElevation;
}

class SDRBASE_API MainCore
{
public:
	class SDRBASE_API MsgDeviceSetFocus : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getDeviceSetIndex() const { return m_deviceSetIndex; }

        static MsgDeviceSetFocus* create(int deviceSetIndex)
        {
            return new MsgDeviceSetFocus(deviceSetIndex);
        }

    private:
        int m_deviceSetIndex;

        MsgDeviceSetFocus(int deviceSetIndex) :
            Message(),
            m_deviceSetIndex(deviceSetIndex)
        { }
    };

	class SDRBASE_API MsgDVSerial : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getActive() const { return m_active; }

        static MsgDVSerial* create(bool active)
        {
            return new MsgDVSerial(active);
        }

    private:
        bool m_active;

        MsgDVSerial(bool active) :
            Message(),
            m_active(active)
        { }
    };

    class SDRBASE_API MsgLoadPreset : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const Preset *getPreset() const { return m_preset; }
        int getDeviceSetIndex() const { return m_deviceSetIndex; }

        static MsgLoadPreset* create(const Preset *preset, int deviceSetIndex)
        {
            return new MsgLoadPreset(preset, deviceSetIndex);
        }

    private:
        const Preset *m_preset;
        int m_deviceSetIndex;

        MsgLoadPreset(const Preset *preset, int deviceSetIndex) :
            Message(),
            m_preset(preset),
            m_deviceSetIndex(deviceSetIndex)
        { }
    };

    class SDRBASE_API MsgSavePreset : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        Preset *getPreset() const { return m_preset; }
        int getDeviceSetIndex() const { return m_deviceSetIndex; }
        bool isNewPreset() const { return m_newPreset; }

        static MsgSavePreset* create(Preset *preset, int deviceSetIndex, bool newPreset)
        {
            return new MsgSavePreset(preset, deviceSetIndex, newPreset);
        }

    private:
        Preset *m_preset;
        int m_deviceSetIndex;
        bool m_newPreset;

        MsgSavePreset(Preset *preset, int deviceSetIndex, bool newPreset) :
            Message(),
            m_preset(preset),
            m_deviceSetIndex(deviceSetIndex),
            m_newPreset(newPreset)
        { }
    };

    class SDRBASE_API MsgDeletePreset : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const Preset *getPreset() const { return m_preset; }

        static MsgDeletePreset* create(const Preset *preset)
        {
            return new MsgDeletePreset(preset);
        }

    private:
        const Preset *m_preset;

        MsgDeletePreset(const Preset *preset) :
            Message(),
            m_preset(preset)
        { }
    };

    class SDRBASE_API MsgLoadFeatureSetPreset : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const FeatureSetPreset *getPreset() const { return m_preset; }
        int getFeatureSetIndex() const { return m_featureSetIndex; }

        static MsgLoadFeatureSetPreset* create(const FeatureSetPreset *preset, int featureSetIndex) {
            return new MsgLoadFeatureSetPreset(preset, featureSetIndex);
        }

    private:
        const FeatureSetPreset *m_preset;
        int m_featureSetIndex;

        MsgLoadFeatureSetPreset(const FeatureSetPreset *preset, int featureSetIndex) :
            Message(),
            m_preset(preset),
            m_featureSetIndex(featureSetIndex)
        { }
    };

    class SDRBASE_API MsgSaveFeatureSetPreset : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        FeatureSetPreset *getPreset() const { return m_preset; }
        int getFeatureSetIndex() const { return m_featureSetIndex; }
        bool isNewPreset() const { return m_newPreset; }

        static MsgSaveFeatureSetPreset* create(FeatureSetPreset *preset, int featureSetIndex, bool newPreset)
        {
            return new MsgSaveFeatureSetPreset(preset, featureSetIndex, newPreset);
        }

    private:
        FeatureSetPreset *m_preset;
        int m_featureSetIndex;
        bool m_newPreset;

        MsgSaveFeatureSetPreset(FeatureSetPreset *preset, int featureSetIndex, bool newPreset) :
            Message(),
            m_preset(preset),
            m_featureSetIndex(featureSetIndex),
            m_newPreset(newPreset)
        { }
    };

    class SDRBASE_API MsgDeleteFeatureSetPreset : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const FeatureSetPreset *getPreset() const { return m_preset; }

        static MsgDeleteFeatureSetPreset* create(const FeatureSetPreset *preset)
        {
            return new MsgDeleteFeatureSetPreset(preset);
        }

    private:
        const FeatureSetPreset *m_preset;

        MsgDeleteFeatureSetPreset(const FeatureSetPreset *preset) :
            Message(),
            m_preset(preset)
        { }
    };

    class SDRBASE_API MsgDeleteInstance : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgDeleteInstance* create()
        {
            return new MsgDeleteInstance();
        }

    private:
        MsgDeleteInstance() :
            Message()
        { }
    };

    class SDRBASE_API MsgAddDeviceSet : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getDirection() const { return m_direction; }

        static MsgAddDeviceSet* create(int direction) {
            return new MsgAddDeviceSet(direction);
        }

    private:
        int m_direction;

        MsgAddDeviceSet(int direction) :
            Message(),
            m_direction(direction)
        { }
    };

    class SDRBASE_API MsgRemoveLastDeviceSet : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgRemoveLastDeviceSet* create() {
            return new MsgRemoveLastDeviceSet();
        }

    private:
        MsgRemoveLastDeviceSet() :
            Message()
        { }
    };

    class SDRBASE_API MsgAddFeatureSet : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgAddFeatureSet* create() {
            return new MsgAddFeatureSet();
        }

    private:
        MsgAddFeatureSet() :
            Message()
        { }
    };

    class SDRBASE_API MsgRemoveLastFeatureSet : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgRemoveLastFeatureSet* create() {
            return new MsgRemoveLastFeatureSet();
        }

    private:
        MsgRemoveLastFeatureSet() :
            Message()
        { }
    };

    class SDRBASE_API MsgSetDevice : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getDeviceSetIndex() const { return m_deviceSetIndex; }
        int getDeviceIndex() const { return m_deviceIndex; }
        int getDeviceType() const { return m_deviceType; }

        static MsgSetDevice* create(int deviceSetIndex, int deviceIndex, int deviceType)
        {
            return new MsgSetDevice(deviceSetIndex, deviceIndex, deviceType);
        }

    private:
        int m_deviceSetIndex;
        int m_deviceIndex;
        int m_deviceType;

        MsgSetDevice(int deviceSetIndex, int deviceIndex, int deviceType) :
            Message(),
            m_deviceSetIndex(deviceSetIndex),
            m_deviceIndex(deviceIndex),
            m_deviceType(deviceType)
        { }
    };

    class SDRBASE_API MsgAddChannel : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getDeviceSetIndex() const { return m_deviceSetIndex; }
        int getChannelRegistrationIndex() const { return m_channelRegistrationIndex; }
        bool isTx() const { return m_tx; }

        static MsgAddChannel* create(int deviceSetIndex, int channelRegistrationIndex, bool tx)
        {
            return new MsgAddChannel(deviceSetIndex, channelRegistrationIndex, tx);
        }

    private:
        int m_deviceSetIndex;
        int m_channelRegistrationIndex;
        bool m_tx;

        MsgAddChannel(int deviceSetIndex, int channelRegistrationIndex, bool tx) :
            Message(),
            m_deviceSetIndex(deviceSetIndex),
            m_channelRegistrationIndex(channelRegistrationIndex),
            m_tx(tx)
        { }
    };

    class SDRBASE_API MsgDeleteChannel : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getDeviceSetIndex() const { return m_deviceSetIndex; }
        int getChannelIndex() const { return m_channelIndex; }

        static MsgDeleteChannel* create(int deviceSetIndex, int channelIndex)
        {
            return new MsgDeleteChannel(deviceSetIndex, channelIndex);
        }

    private:
        int m_deviceSetIndex;
        int m_channelIndex;

        MsgDeleteChannel(int deviceSetIndex, int channelIndex) :
            Message(),
            m_deviceSetIndex(deviceSetIndex),
            m_channelIndex(channelIndex)
        { }
    };

    class SDRBASE_API MsgApplySettings : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgApplySettings* create() {
            return new MsgApplySettings();
        }

    private:
        MsgApplySettings() :
            Message()
        { }
    };

    class SDRBASE_API MsgAddFeature : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getFeatureSetIndex() const { return m_featureSetIndex; }
        int getFeatureRegistrationIndex() const { return m_featureRegistrationIndex; }

        static MsgAddFeature* create(int featureSetIndex, int featureRegistrationIndex)
        {
            return new MsgAddFeature(featureSetIndex, featureRegistrationIndex);
        }

    private:
        int m_featureSetIndex;
        int m_featureRegistrationIndex;

        MsgAddFeature(int featureSetIndex, int featureRegistrationIndex) :
            Message(),
            m_featureSetIndex(featureSetIndex),
            m_featureRegistrationIndex(featureRegistrationIndex)
        { }
    };

    class SDRBASE_API MsgDeleteFeature : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getFeatureSetIndex() const { return m_featureSetIndex; }
        int getFeatureIndex() const { return m_featureIndex; }

        static MsgDeleteFeature* create(int m_featureSetIndex, int m_featureIndex) {
            return new MsgDeleteFeature(m_featureSetIndex, m_featureIndex);
        }

    private:
        int m_featureSetIndex;
        int m_featureIndex;

        MsgDeleteFeature(int m_featureSetIndex, int m_featureIndex) :
            Message(),
            m_featureSetIndex(m_featureSetIndex),
            m_featureIndex(m_featureIndex)
        { }
    };

    class SDRBASE_API MsgChannelReport : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ChannelAPI *getChannelAPI() const { return m_channelAPI; }
        SWGSDRangel::SWGChannelReport *getSWGReport() const { return m_swgReport; }

        static MsgChannelReport* create(
            const ChannelAPI *channelAPI,
            SWGSDRangel::SWGChannelReport *swgReport)
        {
            return new MsgChannelReport(channelAPI, swgReport);
        }

    private:
        const ChannelAPI *m_channelAPI;
        SWGSDRangel::SWGChannelReport *m_swgReport;

        MsgChannelReport(
            const ChannelAPI *channelAPI,
            SWGSDRangel::SWGChannelReport *swgReport
        ) :
            Message(),
            m_channelAPI(channelAPI),
            m_swgReport(swgReport)
        { }
    };

    class SDRBASE_API MsgChannelDemodQuery : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgChannelDemodQuery* create() {
            return new MsgChannelDemodQuery();
        }

    private:
        MsgChannelDemodQuery() :
            Message()
        { }
    };

    class SDRBASE_API MsgChannelDemodReport : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ChannelAPI *getChannelAPI() const { return m_channelAPI; }
        int getSampleRate() const { return m_sampleRate; }

        static MsgChannelDemodReport* create(const ChannelAPI *channelAPI, int sampleRate) {
            return new MsgChannelDemodReport(channelAPI, sampleRate);
        }

    private:
        const ChannelAPI *m_channelAPI;
        int m_sampleRate;

        MsgChannelDemodReport(const ChannelAPI *channelAPI, int sampleRate) :
            Message(),
            m_channelAPI(channelAPI),
            m_sampleRate(sampleRate)
        { }
    };

    class SDRBASE_API MsgChannelSettings : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ChannelAPI *getChannelAPI() const { return m_channelAPI; }
        const QList<QString>& getChannelSettingsKeys() const { return m_channelSettingsKeys; }
        SWGSDRangel::SWGChannelSettings *getSWGSettings() const { return m_swgSettings; }
        bool getForce() const { return m_force; }

        static MsgChannelSettings* create(
            const ChannelAPI *channelAPI,
            const QList<QString>& channelSettingsKey,
            SWGSDRangel::SWGChannelSettings *swgSettings,
            bool force)
        {
            return new MsgChannelSettings(channelAPI, channelSettingsKey, swgSettings, force);
        }

    private:
        const ChannelAPI *m_channelAPI;
        QList<QString> m_channelSettingsKeys;
        SWGSDRangel::SWGChannelSettings *m_swgSettings;
        bool m_force;

        MsgChannelSettings(
            const ChannelAPI *channelAPI,
            const QList<QString>& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings *swgSettings,
            bool force
        ) :
            Message(),
            m_channelAPI(channelAPI),
            m_channelSettingsKeys(channelSettingsKeys),
            m_swgSettings(swgSettings),
            m_force(force)
        { }
    };

    // Message to Map feature to display an item on the map
    class SDRBASE_API MsgMapItem : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const PipeEndPoint *getPipeSource() const { return m_pipeSource; }
        SWGSDRangel::SWGMapItem *getSWGMapItem() const { return m_swgMapItem; }

        static MsgMapItem* create(const PipeEndPoint *pipeSource, SWGSDRangel::SWGMapItem *swgMapItem)
        {
            return new MsgMapItem(pipeSource, swgMapItem);
        }

    private:
        const PipeEndPoint *m_pipeSource;
        SWGSDRangel::SWGMapItem *m_swgMapItem;

        MsgMapItem(const PipeEndPoint *pipeSource, SWGSDRangel::SWGMapItem *swgMapItem) :
            Message(),
            m_pipeSource(pipeSource),
            m_swgMapItem(swgMapItem)
        { }
    };

    // Message to pass received packets between channels and features
    class SDRBASE_API MsgPacket : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const PipeEndPoint *getPipeSource() const { return m_pipeSource; }
        QByteArray getPacket() const { return m_packet; }
        QDateTime getDateTime() const { return m_dateTime; }

        static MsgPacket* create(const PipeEndPoint *pipeSource, QByteArray packet)
        {
            return new MsgPacket(pipeSource, packet, QDateTime::currentDateTime());
        }

        static MsgPacket* create(const PipeEndPoint *pipeSource, QByteArray packet, QDateTime dateTime)
        {
            return new MsgPacket(pipeSource, packet, dateTime);
        }

    private:
        const PipeEndPoint *m_pipeSource;
        QByteArray m_packet;
        QDateTime m_dateTime;

        MsgPacket(const PipeEndPoint *pipeSource, QByteArray packet, QDateTime dateTime) :
            Message(),
            m_pipeSource(pipeSource),
            m_packet(packet),
            m_dateTime(dateTime)
        {
        }
    };

    // Message to pass target azimuth and elevation between channels & features
    class SDRBASE_API MsgTargetAzimuthElevation : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const PipeEndPoint *getPipeSource() const { return m_pipeSource; }
        SWGSDRangel::SWGTargetAzimuthElevation *getSWGTargetAzimuthElevation() const { return m_swgTargetAzimuthElevation; }

        static MsgTargetAzimuthElevation* create(const PipeEndPoint *pipeSource, SWGSDRangel::SWGTargetAzimuthElevation *swgTargetAzimuthElevation)
        {
            return new MsgTargetAzimuthElevation(pipeSource, swgTargetAzimuthElevation);
        }

    private:
        const PipeEndPoint *m_pipeSource;
        SWGSDRangel::SWGTargetAzimuthElevation *m_swgTargetAzimuthElevation;

        MsgTargetAzimuthElevation(const PipeEndPoint *pipeSource, SWGSDRangel::SWGTargetAzimuthElevation *swgTargetAzimuthElevation) :
            Message(),
            m_pipeSource(pipeSource),
            m_swgTargetAzimuthElevation(swgTargetAzimuthElevation)
        { }
    };



	MainCore();
	~MainCore();
	static MainCore *instance();

    const QTimer& getMasterTimer() const { return m_masterTimer; }
    const MainSettings& getSettings() const { return m_settings; }
    MessageQueue *getMainMessageQueue() { return m_mainMessageQueue; }
    const PluginManager *getPluginManager() const { return m_pluginManager; }
    std::vector<DeviceSet*>& getDeviceSets() { return m_deviceSets; }
    std::vector<FeatureSet*>& getFeatureeSets() { return m_featureSets; }
    void setLoggingOptions();
    ChannelAPI *getChannel(unsigned int deviceSetIndex, int channelIndex);
    Feature *getFeature(unsigned int featureSetIndex, int featureIndex);
    bool existsChannel(const ChannelAPI *channel) const { return m_channelsMap.contains(const_cast<ChannelAPI*>(channel)); }
    bool existsFeature(const Feature *feature) const { return m_featuresMap.contains(const_cast<Feature*>(feature)); }
    // slave mode
    void appendFeatureSet();
    void removeFeatureSet(unsigned int index);
    void removeLastFeatureSet();
    void appendDeviceSet(int deviceType);
    void removeLastDeviceSet();
    // slave mode - channels
    void addChannelInstance(DeviceSet *deviceSet, ChannelAPI *channelAPI);
    void removeChannelInstanceAt(DeviceSet *deviceSet, int channelIndex);
    void removeChannelInstance(ChannelAPI *channelAPI);
    void clearChannels(DeviceSet *deviceSet);
    // slave mode - features
    void addFeatureInstance(FeatureSet *featureSet, Feature *feature);
    void removeFeatureInstanceAt(FeatureSet *featureSet, int featureIndex);
    void removeFeatureInstance(Feature *feature);
    void clearFeatures(FeatureSet *featureSet);
    // pipes
    MessagePipes& getMessagePipes() { return m_messagePipes; }
    DataPipes& getDataPipes() { return m_dataPipes; }

    friend class MainServer;
    friend class MainWindow;
    friend class WebAPIAdapter;

private:
    MainSettings m_settings;
    qtwebapp::LoggerWithFile *m_logger;
    MessageQueue *m_mainMessageQueue;
    int m_masterTabIndex;
    QTimer m_masterTimer;
    std::vector<DeviceSet*> m_deviceSets;
    std::vector<FeatureSet*> m_featureSets;
    QMap<DeviceSet*, int> m_deviceSetsMap;       //!< Device set instance to device set index map
    QMap<FeatureSet*, int> m_featureSetsMap;     //!< Feature set instance to feature set index map
    QMap<ChannelAPI*, DeviceSet*> m_channelsMap; //!< Channel to device set map
    QMap<Feature*, FeatureSet*> m_featuresMap;   //!< Feature to feature set map
    PluginManager* m_pluginManager;
    MessagePipes m_messagePipes;
    DataPipes m_dataPipes;

    void debugMaps();
};

#endif // SDRBASE_MAINCORE_H_
