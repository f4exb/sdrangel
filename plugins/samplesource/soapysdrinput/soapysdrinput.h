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

class DeviceSourceAPI;

class SoapySDRInput : public DeviceSampleSource
{
public:
    SoapySDRInput(DeviceSourceAPI *deviceAPI);
    virtual ~SoapySDRInput();
    virtual void destroy();

    virtual void init();
    virtual bool start();
    virtual void stop();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    virtual const QString& getDeviceDescription() const;
    virtual int getSampleRate() const;
    virtual quint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);

    virtual bool handleMessage(const Message& message);

private:
    DeviceSourceAPI *m_deviceAPI;
    DeviceSoapySDRShared m_deviceShared;
    QString m_deviceDescription;
    bool m_running;

    bool openDevice();
    void closeDevice();
};



#endif /* PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_SOAPYSDRINPUT_H_ */
