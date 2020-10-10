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

#include <QTimer>

#include "export.h"
#include "settings/mainsettings.h"
#include "util/message.h"

class DeviceSet;
class FeatureSet;
class PluginManager;
class MessageQueue;

namespace qtwebapp {
    class LoggerWithFile;
}

class SDRBASE_API MainCore
{
public:
	class MsgDeviceSetFocus : public Message {
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

	class MsgDVSerial : public Message {
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

    class MsgLoadPreset : public Message {
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

    class MsgSavePreset : public Message {
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

    class MsgDeletePreset : public Message {
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

    class MsgLoadFeatureSetPreset : public Message {
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

    class MsgSaveFeatureSetPreset : public Message {
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

    class MsgDeleteFeatureSetPreset : public Message {
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

    class MsgDeleteInstance : public Message {
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

    class MsgAddDeviceSet : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getDirection() const { return m_direction; }

        static MsgAddDeviceSet* create(int direction)
        {
            return new MsgAddDeviceSet(direction);
        }

    private:
        int m_direction;

        MsgAddDeviceSet(int direction) :
            Message(),
            m_direction(direction)
        { }
    };

    class MsgRemoveLastDeviceSet : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgRemoveLastDeviceSet* create()
        {
            return new MsgRemoveLastDeviceSet();
        }

    private:
        MsgRemoveLastDeviceSet() :
            Message()
        { }
    };

    class MsgSetDevice : public Message {
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

    class MsgAddChannel : public Message {
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

    class MsgDeleteChannel : public Message {
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

    class MsgApplySettings : public Message {
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

    class MsgAddFeature : public Message {
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

    class MsgDeleteFeature : public Message {
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
    // slave mode
    void appendFeatureSet();
    void removeFeatureSet(int index);
    void removeLastFeatureSet();

    friend class MainServer;
    friend class MainWindow;
    friend class WebAPIAdapterSrv;
    friend class WebAPIAdapterGUI;

private:
    MainSettings m_settings;
    qtwebapp::LoggerWithFile *m_logger;
    MessageQueue *m_mainMessageQueue;
    int m_masterTabIndex;
    QTimer m_masterTimer;
    std::vector<DeviceSet*> m_deviceSets;
    std::vector<FeatureSet*> m_featureSets;
    PluginManager* m_pluginManager;
};

#endif // SDRBASE_MAINCORE_H_
