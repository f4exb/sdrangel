///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLESOURCE_PLUTOSDROUTPUT_PLUTOSDRINPUT_H_
#define PLUGINS_SAMPLESOURCE_PLUTOSDROUTPUT_PLUTOSDRINPUT_H_

#include <QString>

#include "iio.h"
#include <dsp/devicesamplesink.h>
#include "util/message.h"
#include "plutosdr/deviceplutosdrshared.h"
#include "plutosdroutputsettings.h"

class DeviceSinkAPI;
class PlutoSDROutputThread;

class PlutoSDROutput : public DeviceSampleSink {
public:
    class MsgConfigurePlutoSDR : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const PlutoSDROutputSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigurePlutoSDR* create(const PlutoSDROutputSettings& settings, bool force)
        {
            return new MsgConfigurePlutoSDR(settings, force);
        }

    private:
        PlutoSDROutputSettings m_settings;
        bool m_force;

        MsgConfigurePlutoSDR(const PlutoSDROutputSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    PlutoSDROutput(DeviceSinkAPI *deviceAPI);
    ~PlutoSDROutput();
    virtual void destroy();

    virtual bool start();
    virtual void stop();

    virtual const QString& getDeviceDescription() const;
    virtual int getSampleRate() const;
    virtual quint64 getCenterFrequency() const;

    virtual bool handleMessage(const Message& message);

    uint32_t getDACSampleRate() const { return m_deviceSampleRates.m_addaConnvRate; }
    uint32_t getFIRSampleRate() const { return m_deviceSampleRates.m_hb1Rate; }
    void getRSSI(std::string& rssiStr);
    bool fetchTemperature();
    float getTemperature();

 private:
    DeviceSinkAPI *m_deviceAPI;
    QString m_deviceDescription;
    PlutoSDROutputSettings m_settings;
    bool m_running;
    DevicePlutoSDRShared m_deviceShared;
    struct iio_buffer *m_plutoTxBuffer;
    PlutoSDROutputThread *m_plutoSDROutputThread;
    DevicePlutoSDRBox::SampleRates m_deviceSampleRates;
    QMutex m_mutex;

    bool openDevice();
    void closeDevice();
    void suspendBuddies();
    void resumeBuddies();
    bool applySettings(const PlutoSDROutputSettings& settings, bool force = false);
};


#endif /* PLUGINS_SAMPLESOURCE_PLUTOSDROUTPUT_PLUTOSDRINPUT_H_ */
