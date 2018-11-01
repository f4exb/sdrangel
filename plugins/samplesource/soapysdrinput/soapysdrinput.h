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

class SoapySDRInput : public DeviceSampleSource
{
public:
    class MsgConfigureSoapySDR : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SoapySDRInputSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureSoapySDR* create(const SoapySDRInputSettings& settings, bool force)
        {
            return new MsgConfigureSoapySDR(settings, force);
        }

    private:
        SoapySDRInputSettings m_settings;
        bool m_force;

        MsgConfigureSoapySDR(const SoapySDRInputSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
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
    const SoapySDR::RangeList& getRateRanges();

private:
    DeviceSourceAPI *m_deviceAPI;
    DeviceSoapySDRShared m_deviceShared;
    SoapySDRInputThread *m_thread;
    QString m_deviceDescription;
    bool m_running;

    bool openDevice();
    void closeDevice();
    void moveThreadToBuddy();
};



#endif /* PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_SOAPYSDRINPUT_H_ */
