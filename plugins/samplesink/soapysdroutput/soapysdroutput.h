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

#ifndef PLUGINS_SAMPLESINK_SOAPYSDROUTPUT_SOAPYSDROUTPUT_H_
#define PLUGINS_SAMPLESINK_SOAPYSDROUTPUT_SOAPYSDROUTPUT_H_

#include <QString>
#include <QByteArray>
#include <stdint.h>

#include "dsp/devicesamplesink.h"
#include "soapysdr/devicesoapysdrshared.h"

#include "soapysdroutputsettings.h"

class DeviceSinkAPI;
class SoapySDROutputThread;

namespace SoapySDR
{
    class Device;
}

class SoapySDROutput : public DeviceSampleSink {
public:
    class MsgConfigureSoapySDROutput : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SoapySDROutputSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureSoapySDROutput* create(const SoapySDROutputSettings& settings, bool force)
        {
            return new MsgConfigureSoapySDROutput(settings, force);
        }

    private:
        SoapySDROutputSettings m_settings;
        bool m_force;

        MsgConfigureSoapySDROutput(const SoapySDROutputSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
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

    SoapySDROutput(DeviceSinkAPI *deviceAPI);
    virtual ~SoapySDROutput();
    virtual void destroy();

    virtual void init();
    virtual bool start();
    virtual void stop();
    SoapySDROutputThread *getThread() { return m_thread; }
    void setThread(SoapySDROutputThread *thread) { m_thread = thread; }

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
    const SoapySDR::RangeList& getRateRanges();
    const std::vector<std::string>& getAntennas();
    const SoapySDR::RangeList& getBandwidthRanges();
    const std::vector<DeviceSoapySDRParams::FrequencySetting>& getTunableElements();
    const std::vector<DeviceSoapySDRParams::GainSetting>& getIndividualGainsRanges();

private:
    DeviceSinkAPI *m_deviceAPI;
    QMutex m_mutex;
    SoapySDROutputSettings m_settings;
    QString m_deviceDescription;
    bool m_running;
    SoapySDROutputThread *m_thread;
    DeviceSoapySDRShared m_deviceShared;

    bool openDevice();
    void closeDevice();
    SoapySDROutputThread *findThread();
    void moveThreadToBuddy();
    bool applySettings(const SoapySDROutputSettings& settings, bool force = false);
    bool setDeviceCenterFrequency(SoapySDR::Device *dev, int requestedChannel, quint64 freq_hz, int loPpmTenths);
};


#endif /* PLUGINS_SAMPLESINK_SOAPYSDROUTPUT_SOAPYSDROUTPUT_H_ */
