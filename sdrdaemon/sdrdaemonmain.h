///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon instance                                                            //
//                                                                               //
// SDRdaemon is a detached SDR front end that handles the interface with a       //
// physical device and sends or receives the I/Q samples stream to or from a     //
// SDRangel instance via UDP. It is controlled via a Web REST API.               //
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

#ifndef SDRDAEMON_SDRDAEMONMAIN_H_
#define SDRDAEMON_SDRDAEMONMAIN_H_

#include <QObject>
#include <QTimer>

#include "sdrdaemonsettings.h"
#include "util/messagequeue.h"

namespace SDRDaemon {
    class WebAPIRequestMapper;
    class WebAPIServer;
}

namespace qtwebapp {
    class LoggerWithFile;
}

class SDRDaemonParser;
class DSPEngine;
class PluginManager;
class Message;
class WebAPIAdapterDaemon;
class DSPDeviceSourceEngine;
class DeviceSourceAPI;
class DSPDeviceSinkEngine;
class DeviceSinkAPI;
class SDRDaemonChannelSink;

class SDRDaemonMain : public QObject {
    Q_OBJECT
public:
    explicit SDRDaemonMain(qtwebapp::LoggerWithFile *logger, const SDRDaemonParser& parser, QObject *parent = 0);
    ~SDRDaemonMain();
    static SDRDaemonMain *getInstance() { return m_instance; } // Main Core is de facto a singleton so this just returns its reference

    MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }

    const QTimer& getMasterTimer() const { return m_masterTimer; }
    const SDRDaemonSettings& getSettings() const { return m_settings; }

    bool addSourceDevice();
    bool addSinkDevice();
    void removeDevice();

    friend class WebAPIAdapterDaemon;

signals:
    void finished();

private:
    static SDRDaemonMain *m_instance;
    qtwebapp::LoggerWithFile *m_logger;
    SDRDaemonSettings m_settings;
    DSPEngine* m_dspEngine;
    int m_lastEngineState;
    PluginManager* m_pluginManager;
    MessageQueue m_inputMessageQueue;
    QTimer m_masterTimer;

    SDRDaemon::WebAPIRequestMapper *m_requestMapper;
    SDRDaemon::WebAPIServer *m_apiServer;
    WebAPIAdapterDaemon *m_apiAdapter;

    bool m_tx;
    QString m_deviceType;
    QString m_deviceSerial;
    int m_deviceSequence;

    DSPDeviceSourceEngine *m_deviceSourceEngine;
    DeviceSourceAPI *m_deviceSourceAPI;
    DSPDeviceSinkEngine *m_deviceSinkEngine;
    DeviceSinkAPI *m_deviceSinkAPI;
    SDRDaemonChannelSink *m_channelSink;

    void loadSettings();
    void setLoggingOptions();
    int getDeviceIndex();
    bool handleMessage(const Message& cmd);
    void addChannelSink();

private slots:
    void handleMessages();
};

#endif /* SDRDAEMON_SDRDAEMONMAIN_H_ */
