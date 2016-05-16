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

#ifndef SDRBASE_DEVICE_DEVICEAPI_H_
#define SDRBASE_DEVICE_DEVICEAPI_H_

#include <QObject>
#include <QString>


#include "util/export.h"
#include "dsp/dspdeviceengine.h"

class MainWindow;
class DSPDeviceEngine;
class GLSpectrum;
class ChannelWindow;
class SampleSink;
class ThreadedSampleSink;
class SampleSource;
class MessageQueue;
class ChannelMarker;
class QWidget;
class PluginGUI;

class SDRANGEL_API DeviceAPI : public QObject {
    Q_OBJECT

public:
    // Device engine stuff
    void addSink(SampleSink* sink);       //!< Add a sample sink to device engine
    void removeSink(SampleSink* sink);    //!< Remove a sample sink from device engine
    void addThreadedSink(ThreadedSampleSink* sink);     //!< Add a sample sink that will run on its own thread to device engine
    void removeThreadedSink(ThreadedSampleSink* sink);  //!< Remove a sample sink that runs on its own thread from device engine
    void setSource(SampleSource* source); //!< Set device engine sample source type
    bool initAcquisition();               //!< Initialize device engine acquisition sequence
    bool startAcquisition();              //!< Start device engine acquisition sequence
    void stopAcquisition();               //!< Stop device engine acquisition sequence
    DSPDeviceEngine::State state() const; //!< device engine state
    QString errorMessage();               //!< Return the current device engine error message
    uint getDeviceUID() const;            //!< Return the current device engine unique ID
    MessageQueue *getDeviceInputMessageQueue();
    MessageQueue *getDeviceOutputMessageQueue();
    void configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection); //!< Configure current device engine DSP corrections

    void setSourceSequence(int sourceSequence);

    // device related stuff
    GLSpectrum *getSpectrum();                           //!< Direct spectrum getter
    void addChannelMarker(ChannelMarker* channelMarker); //!< Add channel marker to spectrum
    ChannelWindow *getChannelWindow();                   //!< Direct channel window getter
    void addRollupWidget(QWidget *widget);               //!< Add rollup widget to channel window
    void setInputGUI(QWidget* inputGUI, const QString& sourceDisplayName);

    void setSampleSourceId(const QString& id);
    void setSampleSourceSerial(const QString& serial);
    void setSampleSourceSequence(int sequence);
    void setSampleSourcePluginGUI(PluginGUI *gui);

    void freeAll();

protected:
    DeviceAPI(MainWindow *mainWindow,
            int deviceTabIndex,
            DSPDeviceEngine *deviceEngine,
            GLSpectrum *glSpectrum,
            ChannelWindow *channelWindow);
    ~DeviceAPI();

    MainWindow *m_mainWindow;
    int m_deviceTabIndex;
    DSPDeviceEngine *m_deviceEngine;
    GLSpectrum *m_spectrum;
    ChannelWindow *m_channelWindow;

    QString m_sampleSourceId;
    QString m_sampleSourceSerial;
    int m_sampleSourceSequence;
    PluginGUI* m_sampleSourcePluginGUI;

    friend class MainWindow;
};


#endif /* SDRBASE_DEVICE_DEVICEAPI_H_ */
