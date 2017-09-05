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

#ifndef PLUGINS_SAMPLESOURCE_PLUTOSDRINPUT_PLUTOSDRINPUT_H_
#define PLUGINS_SAMPLESOURCE_PLUTOSDRINPUT_PLUTOSDRINPUT_H_

#include <QString>

#include "iio.h"
#include <dsp/devicesamplesource.h>
#include "util/message.h"
#include "plutosdr/deviceplutosdrshared.h"
#include "plutosdrinputsettings.h"

class DeviceSourceAPI;
class FileRecord;

class PlutoSDRInput : public DeviceSampleSource {
public:
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


    PlutoSDRInput(DeviceSourceAPI *deviceAPI);
    ~PlutoSDRInput();

    virtual bool start();
    virtual void stop();

    virtual const QString& getDeviceDescription() const;
    virtual int getSampleRate() const;
    virtual quint64 getCenterFrequency() const;

    virtual bool handleMessage(const Message& message);

 private:
    DeviceSourceAPI *m_deviceAPI;
    FileRecord *m_fileSink;
    QString m_deviceDescription;
    PlutoSDRInputSettings m_settings;
    bool m_running;
    DevicePlutoSDRShared m_deviceShared;
    struct iio_buffer *m_plutoRxBuffer;
    QMutex m_mutex;

    bool openDevice();
    void closeDevice();
    void suspendBuddies();
    void resumeBuddies();
};


#endif /* PLUGINS_SAMPLESOURCE_PLUTOSDRINPUT_PLUTOSDRINPUT_H_ */
