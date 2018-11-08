///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_SOAPYSDRINPUT_H_
#define PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_SOAPYSDRINPUT_H_

#include <QString>
#include <QByteArray>
#include <stdint.h>

#include "soapysdr/devicesoapysdrshared.h"
#include "dsp/devicesamplesource.h"

#include "soapysdrinputsettings.h"

class DeviceSourceAPI;
class SoapySDRInputThread;
class FileRecord;

namespace SoapySDR
{
    class Device;
}

class SoapySDRInput : public DeviceSampleSource
{
public:
    class MsgConfigureSoapySDRInput : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SoapySDRInputSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureSoapySDRInput* create(const SoapySDRInputSettings& settings, bool force)
        {
            return new MsgConfigureSoapySDRInput(settings, force);
        }

    private:
        SoapySDRInputSettings m_settings;
        bool m_force;

        MsgConfigureSoapySDRInput(const SoapySDRInputSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgFileRecord : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgFileRecord* create(bool startStop) {
            return new MsgFileRecord(startStop);
        }

    protected:
        bool m_startStop;

        MsgFileRecord(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    class MsgReportGainChange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SoapySDRInputSettings& getSettings() const { return m_settings; }
        bool getGlobalGain() const { return m_globalGain; }
        bool getIndividualGains() const { return m_individualGains; }

        static MsgReportGainChange* create(const SoapySDRInputSettings& settings, bool globalGain, bool individualGains)
        {
            return new MsgReportGainChange(settings, globalGain, individualGains);
        }

    private:
        SoapySDRInputSettings m_settings;
        bool m_globalGain;
        bool m_individualGains;

        MsgReportGainChange(const SoapySDRInputSettings& settings, bool globalGain, bool individualGains) :
            Message(),
            m_settings(settings),
            m_globalGain(globalGain),
            m_individualGains(individualGains)
        { }
    };

    SoapySDRInput(DeviceSourceAPI *deviceAPI);
    virtual ~SoapySDRInput();
    virtual void destroy();

    virtual void init();
    virtual bool start();
    virtual void stop();
    SoapySDRInputThread *getThread() { return m_thread; }
    void setThread(SoapySDRInputThread *thread) { m_thread = thread; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    virtual const QString& getDeviceDescription() const;
    virtual int getSampleRate() const;
    virtual quint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);

    virtual bool handleMessage(const Message& message);

    void getFrequencyRange(uint64_t& min, uint64_t& max);
    void getGlobalGainRange(int& min, int& max);
    bool isAGCSupported();
    const std::vector<std::string>& getAntennas();
    const SoapySDR::RangeList& getRateRanges();
    const SoapySDR::RangeList& getBandwidthRanges();
    int getAntennaIndex(const std::string& antenna);
    const std::vector<DeviceSoapySDRParams::FrequencySetting>& getTunableElements();
    const std::vector<DeviceSoapySDRParams::GainSetting>& getIndividualGainsRanges();
    void initGainSettings(SoapySDRInputSettings& settings);

private:
    DeviceSourceAPI *m_deviceAPI;
    QMutex m_mutex;
    SoapySDRInputSettings m_settings;
    QString m_deviceDescription;
    bool m_running;
    SoapySDRInputThread *m_thread;
    DeviceSoapySDRShared m_deviceShared;
    FileRecord *m_fileSink; //!< File sink to record device I/Q output

    bool openDevice();
    void closeDevice();
    SoapySDRInputThread *findThread();
    void moveThreadToBuddy();
    bool applySettings(const SoapySDRInputSettings& settings, bool force = false);
    bool setDeviceCenterFrequency(SoapySDR::Device *dev, int requestedChannel, quint64 freq_hz, int loPpmTenths);
    void updateGains(SoapySDR::Device *dev, int requestedChannel, SoapySDRInputSettings& settings);
};



#endif /* PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_SOAPYSDRINPUT_H_ */
