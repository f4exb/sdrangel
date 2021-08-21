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

#ifndef SDRSRV_MAINSERVER_H_
#define SDRSRV_MAINSERVER_H_

#include <QObject>
#include <QTimer>

#include "maincore.h"
#include "settings/mainsettings.h"
#include "util/messagequeue.h"
#include "export.h"

class MainParser;
class DSPEngine;
class DSPDeviceSourceEngine;
class DSPDeviceSinkEngine;
class PluginAPI;
class PluginInterface;
class ChannelMarker;
class DeviceSet;
class FeatureSet;
class WebAPIRequestMapper;
class WebAPIServer;
class WebAPIAdapter;

namespace qtwebapp {
    class LoggerWithFile;
}

class SDRSRV_API MainServer : public QObject {
    Q_OBJECT

public:
    explicit MainServer(qtwebapp::LoggerWithFile *logger, const MainParser& parser, QObject *parent = 0);
    ~MainServer();
    static MainServer *getInstance() { return m_instance; } // Main Core is de facto a singleton so this just returns its reference

    MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }

    const QTimer& getMasterTimer() const { return m_mainCore->m_masterTimer; }

    void addSourceDevice();
    void addSinkDevice();
    void addMIMODevice();
    void removeLastDevice();
    void changeSampleSource(int deviceSetIndex, int selectedDeviceIndex);
    void changeSampleSink(int deviceSetIndex, int selectedDeviceIndex);
    void changeSampleMIMO(int deviceSetIndex, int selectedDeviceIndex);
    void addChannel(int deviceSetIndex, int selectedChannelIndex);
    void deleteChannel(int deviceSetIndex, int channelIndex);
    void addFeatureSet();
    void removeFeatureSet(unsigned int featureSetIndex);
    void addFeature(int featureSetIndex, int selectedFeatureIndex);
    void deleteFeature(int featureSetIndex, int featureIndex);

    friend class WebAPIAdapter;

signals:
    void finished();

private:
    static MainServer *m_instance;
    MainCore *m_mainCore;
    DSPEngine* m_dspEngine;

    MessageQueue m_inputMessageQueue;

    WebAPIRequestMapper *m_requestMapper;
    WebAPIServer *m_apiServer;
    WebAPIAdapter *m_apiAdapter;

	void loadSettings();
    void applySettings();
	void loadPresetSettings(const Preset* preset, int tabIndex);
	void savePresetSettings(Preset* preset, int tabIndex);
	void loadFeatureSetPresetSettings(const FeatureSetPreset* preset, int featureSetIndex);
	void saveFeatureSetPresetSettings(FeatureSetPreset* preset, int featureSetIndex);

    bool handleMessage(const Message& cmd);

private slots:
    void handleMessages();
};




#endif /* SDRSRV_MAINSERVER_H_ */
