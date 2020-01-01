///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
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

#ifndef SDRSRV_MAINCORE_H_
#define SDRSRV_MAINCORE_H_

#include <QObject>
#include <QTimer>

#include "settings/mainsettings.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "export.h"
#include "mainparser.h"

class DSPEngine;
class DSPDeviceSourceEngine;
class DSPDeviceSinkEngine;
class PluginAPI;
class PluginInterface;
class PluginManager;
class ChannelMarker;
class DeviceSet;
class WebAPIRequestMapper;
class WebAPIServer;
class WebAPIAdapterSrv;

namespace qtwebapp {
    class LoggerWithFile;
}

class SDRSRV_API MainCore : public QObject {
    Q_OBJECT

public:
    explicit MainCore(qtwebapp::LoggerWithFile *logger, const MainParser& parser, QObject *parent = 0);
    ~MainCore();
    static MainCore *getInstance() { return m_instance; } // Main Core is de facto a singleton so this just returns its reference

    MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }

    const QTimer& getMasterTimer() const { return m_masterTimer; }
    const MainSettings& getMainSettings() const { return m_settings; }
    const PluginManager *getPluginManager() const { return m_pluginManager; }

    void addSourceDevice();
    void addSinkDevice();
    void removeLastDevice();
    void changeSampleSource(int deviceSetIndex, int selectedDeviceIndex);
    void changeSampleSink(int deviceSetIndex, int selectedDeviceIndex);
    void changeSampleMIMO(int deviceSetIndex, int selectedDeviceIndex);
    void addChannel(int deviceSetIndex, int selectedChannelIndex);
    void deleteChannel(int deviceSetIndex, int channelIndex);

    friend class WebAPIAdapterSrv;

signals:
    void finished();

private:
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

    static MainCore *m_instance;
    MainSettings m_settings;
    int m_masterTabIndex;
    DSPEngine* m_dspEngine;
    int m_lastEngineState;
    qtwebapp::LoggerWithFile *m_logger;

    MessageQueue m_inputMessageQueue;
    QTimer m_masterTimer;
    std::vector<DeviceSet*> m_deviceSets;
    PluginManager* m_pluginManager;

    WebAPIRequestMapper *m_requestMapper;
    WebAPIServer *m_apiServer;
    WebAPIAdapterSrv *m_apiAdapter;

	void loadSettings();
    void applySettings();
	void loadPresetSettings(const Preset* preset, int tabIndex);
	void savePresetSettings(Preset* preset, int tabIndex);
    void setLoggingOptions();

    bool handleMessage(const Message& cmd);

private slots:
    void handleMessages();
};




#endif /* SDRSRV_MAINCORE_H_ */
