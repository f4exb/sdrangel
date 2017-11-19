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

class BasebandSampleSource;
class ThreadedBasebandSampleSource;
class DeviceSampleSink;
class MessageQueue;
class PluginInstanceGUI;
class PluginInterface;
class Preset;
class DeviceSourceAPI;
class ChannelSourceAPI;

class SDRANGEL_API DeviceSinkAPI : public QObject {
    Q_OBJECT

public:
    DeviceSinkAPI(int deviceTabIndex,
            DSPDeviceSinkEngine *deviceEngine);
    ~DeviceSinkAPI();

    // Device engine stuff
    void addSpectrumSink(BasebandSampleSink* sink);                //!< Add the spectrum sample sink to device engine (spectrum vis)
    void removeSpectrumSink(BasebandSampleSink* sink);             //!< Remove the spectrum sample sink from device engine (spectrum vis)
    void addSource(BasebandSampleSource* source);                  //!< Add a baseband sample source to device engine
    void removeSource(BasebandSampleSource* sink);                 //!< Remove a baseband sample source from device engine
    void addThreadedSource(ThreadedBasebandSampleSource* sink);    //!< Add a baseband sample source that will run on its own thread to device engine
    void removeThreadedSource(ThreadedBasebandSampleSource* sink); //!< Remove a baseband sample source that runs on its own thread from device engine
    void addChannelAPI(ChannelSourceAPI* channelAPI);
    void removeChannelAPI(ChannelSourceAPI* channelAPI);
    uint32_t getNumberOfSources();
    void setSampleSink(DeviceSampleSink* sink);                    //!< Set device engine sample sink type
    DeviceSampleSink *getSampleSink();                             //!< Return pointer to the device sample sink
    bool initGeneration();                                         //!< Initialize device engine generation sequence
    bool startGeneration();                                        //!< Start device engine generation sequence
    void stopGeneration();                                         //!< Stop device engine generation sequence
    DSPDeviceSinkEngine::State state() const;                      //!< device engine state
    QString errorMessage();                                        //!< Return the current device engine error message
    uint getDeviceUID() const;                                     //!< Return the current device engine unique ID
    MessageQueue *getDeviceEngineInputMessageQueue();
    MessageQueue *getSampleSinkInputMessageQueue();
    MessageQueue *getSampleSinkGUIMessageQueue();

    void setHardwareId(const QString& id);
    void setSampleSinkId(const QString& id);
    void resetSampleSinkId();
    void setSampleSinkSerial(const QString& serial);
    void setSampleSinkDisplayName(const QString& serial);
    void setSampleSinkSequence(int sequence);
    void setNbItems(uint32_t nbItems);
    void setItemIndex(uint32_t index);
    void setSampleSinkPluginInterface(PluginInterface *iface);
    void setSampleSinkPluginInstanceUI(PluginInstanceGUI *gui);

    const QString& getHardwareId() const { return m_hardwareId; }
    const QString& getSampleSinkId() const { return m_sampleSinkId; }
    const QString& getSampleSinkSerial() const { return m_sampleSinkSerial; }
    const QString& getSampleSinkDisplayName() const { return m_sampleSinkDisplayName; }
    uint32_t getSampleSinkSequence() const { return m_sampleSinkSequence; }
    uint32_t getNbItems() const { return m_nbItems; }
    uint32_t getItemIndex() const { return m_itemIndex; }
    PluginInterface *getPluginInterface() { return m_pluginInterface; }
    PluginInstanceGUI *getSampleSinkPluginInstanceGUI() { return m_sampleSinkPluginInstanceUI; }
    void getDeviceEngineStateStr(QString& state);
    ChannelSourceAPI *getChanelAPIAt(int index);
    int getNbChannels() const { return m_channelAPIs.size(); }

    void registerChannelInstance(const QString& channelName, PluginInstanceGUI* pluginGUI);
    void removeChannelInstance(PluginInstanceGUI* pluginGUI);


    void loadSinkSettings(const Preset* preset);
    void saveSinkSettings(Preset* preset);

    DSPDeviceSinkEngine *getDeviceSinkEngine() { return m_deviceSinkEngine; }

    const std::vector<DeviceSourceAPI*>& getSourceBuddies() const { return m_sourceBuddies; }
    const std::vector<DeviceSinkAPI*>& getSinkBuddies() const { return m_sinkBuddies; }
    void addSourceBuddy(DeviceSourceAPI* buddy);
    void addSinkBuddy(DeviceSinkAPI* buddy);
    void removeSourceBuddy(DeviceSourceAPI* buddy);
    void removeSinkBuddy(DeviceSinkAPI* buddy);
    void clearBuddiesLists();
    void *getBuddySharedPtr() const { return m_buddySharedPtr; }
    void setBuddySharedPtr(void *ptr) { m_buddySharedPtr = ptr; }
    bool isBuddyLeader() const { return m_isBuddyLeader; }
    void setBuddyLeader(bool isBuddyLeader) { m_isBuddyLeader = isBuddyLeader; }

    const QTimer& getMasterTimer() const { return m_masterTimer; } //!< This is the DSPEngine master timer

protected:
    int m_deviceTabIndex;
    DSPDeviceSinkEngine *m_deviceSinkEngine;

    QString m_hardwareId;            //!< The internal id that identifies the type of hardware (i.e. HackRF, BladeRF, ...)
    QString m_sampleSinkId;          //!< The internal plugin ID corresponding to the device (i.e. for HackRF input, for HackRF output ...)
    QString m_sampleSinkSerial;      //!< The device serial number defined by the vendor
    QString m_sampleSinkDisplayName; //!< The human readable name identifying this instance
    uint32_t m_sampleSinkSequence;   //!< The device sequence. >0 when more than one device of the same type is connected
    uint32_t m_nbItems;              //!< Number of items or streams in the device. Can be >1 for NxM devices (i.e. 2 for LimeSDR)
    uint32_t m_itemIndex;            //!< The Tx stream index. Can be >0 for NxM devices (i.e. 0 or 1 for LimeSDR)
    PluginInterface* m_pluginInterface;
    PluginInstanceGUI* m_sampleSinkPluginInstanceUI;

    std::vector<DeviceSourceAPI*> m_sourceBuddies; //!< Device source APIs referencing the same physical device
    std::vector<DeviceSinkAPI*> m_sinkBuddies;     //!< Device sink APIs referencing the same physical device
    void *m_buddySharedPtr;
    bool m_isBuddyLeader;
    const QTimer& m_masterTimer; //!< This is the DSPEngine master timer

    QList<ChannelSourceAPI*> m_channelAPIs;

    friend class DeviceSourceAPI;

private:
    void renumerateChannels();
};



#endif /* SDRBASE_DEVICE_DEVICESINKAPI_H_ */
