///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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
#include "util/export.h"
#include "mainparser.h"

class AudioDeviceInfo;
class DSPEngine;
class DSPDeviceSourceEngine;
class DSPDeviceSinkEngine;
class DeviceSourceAPI;
class DeviceSinkAPI;
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

class SDRANGEL_API MainCore : public QObject {
    Q_OBJECT

public:
    explicit MainCore(qtwebapp::LoggerWithFile *logger, const MainParser& parser, QObject *parent = 0);
    ~MainCore();
    static MainCore *getInstance() { return m_instance; } // Main Core is de facto a singleton so this just returns its reference

    MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }

    const QTimer& getMasterTimer() const { return m_masterTimer; }
    const MainSettings& getMainSettings() const { return m_settings; }

    friend class WebAPIAdapterSrv;

signals:
    void finished();

private:
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
    AudioDeviceInfo m_audioDeviceInfo;

    WebAPIRequestMapper *m_requestMapper;
    WebAPIServer *m_apiServer;
    WebAPIAdapterSrv *m_apiAdapter;

    bool handleMessage(const Message& cmd);

private slots:
    void handleMessages();
};




#endif /* SDRSRV_MAINCORE_H_ */
