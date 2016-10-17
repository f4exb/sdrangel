///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_DEVICE_DEVICESINKAPI_H_
#define SDRBASE_DEVICE_DEVICESINKAPI_H_

#include <QObject>
#include <QString>

#include "dsp/dspdevicesinkengine.h"
#include "util/export.h"

class MainWindow;
class GLSpectrum;
class ChannelWindow;
class BasebandSampleSource;
class ThreadedBasebandSampleSource;
class DeviceSampleSink;
class MessageQueue;
class ChannelMarker;
class QWidget;
class PluginGUI;
class PluginAPI;
class Preset;

class SDRANGEL_API DeviceSinkAPI : public QObject {
    Q_OBJECT

public:
    // Device engine stuff
    void addSink(BasebandSampleSink* sink);                        //!< Add a sample sink to device engine (spectrum vis)
    void removeSink(BasebandSampleSink* sink);                     //!< Remove a sample sink from device engine (spectrum vis)
    void addSource(BasebandSampleSource* source);                  //!< Add a baseband sample source to device engine
    void removeSource(BasebandSampleSource* sink);                 //!< Remove a baseband sample source from device engine
    void addThreadedSource(ThreadedBasebandSampleSource* sink);    //!< Add a baseband sample source that will run on its own thread to device engine
    void removeThreadedSource(ThreadedBasebandSampleSource* sink); //!< Remove a baseband sample source that runs on its own thread from device engine
    void setSink(DeviceSampleSink* sink);                          //!< Set device engine sample sink type
    bool initGeneration();                                         //!< Initialize device engine generation sequence
    bool startGeneration();                                        //!< Start device engine generation sequence
    void stopGeneration();                                         //!< Stop device engine generation sequence
    DSPDeviceSinkEngine::State state() const;                      //!< device engine state
    QString errorMessage();                                        //!< Return the current device engine error message
    uint getDeviceUID() const;                                     //!< Return the current device engine unique ID
    MessageQueue *getDeviceInputMessageQueue();
    MessageQueue *getDeviceOutputMessageQueue();
    // device related stuff
    GLSpectrum *getSpectrum();                           //!< Direct spectrum getter
    void addChannelMarker(ChannelMarker* channelMarker); //!< Add channel marker to spectrum
    ChannelWindow *getChannelWindow();                   //!< Direct channel window getter
    void addRollupWidget(QWidget *widget);               //!< Add rollup widget to channel window
    void setOutputGUI(QWidget* outputGUI, const QString& sinkDisplayName);

    void setSampleSinkId(const QString& id);
    void setSampleSinkSerial(const QString& serial);
    void setSampleSinkSequence(int sequence);
    void setSampleSinkPluginGUI(PluginGUI *gui);

    void registerChannelInstance(const QString& channelName, PluginGUI* pluginGUI);
    void removeChannelInstance(PluginGUI* pluginGUI);

    void freeAll();

    void loadSinkSettings(const Preset* preset);
    void saveSinkSettings(Preset* preset);
    void loadChannelSettings(const Preset* preset, PluginAPI *pluginAPI);
    void saveChannelSettings(Preset* preset);

    MainWindow *getMainWindow() { return m_mainWindow; }

protected:
    struct ChannelInstanceRegistration
    {
        QString m_channelName;
        PluginGUI* m_gui;

        ChannelInstanceRegistration() :
            m_channelName(),
            m_gui(0)
        { }

        ChannelInstanceRegistration(const QString& channelName, PluginGUI* pluginGUI) :
            m_channelName(channelName),
            m_gui(pluginGUI)
        { }

        bool operator<(const ChannelInstanceRegistration& other) const;
    };

    typedef QList<ChannelInstanceRegistration> ChannelInstanceRegistrations;

    DeviceSinkAPI(MainWindow *mainWindow,
            int deviceTabIndex,
            DSPDeviceSinkEngine *deviceEngine,
            GLSpectrum *glSpectrum,
            ChannelWindow *channelWindow);
    ~DeviceSinkAPI();

    void renameChannelInstances();

    MainWindow *m_mainWindow;
    int m_deviceTabIndex;
    DSPDeviceSinkEngine *m_deviceSinkEngine;
    GLSpectrum *m_spectrum;
    ChannelWindow *m_channelWindow;

    QString m_sampleSinkId;
    QString m_sampleSinkSerial;
    int m_sampleSinkSequence;
    PluginGUI* m_sampleSinkPluginGUI;

    ChannelInstanceRegistrations m_channelInstanceRegistrations;

    friend class MainWindow;
};



#endif /* SDRBASE_DEVICE_DEVICESINKAPI_H_ */
