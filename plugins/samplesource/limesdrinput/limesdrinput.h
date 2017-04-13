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

#ifndef PLUGINS_SAMPLESOURCE_LIMESDRINPUT_LIMESDRINPUT_H_
#define PLUGINS_SAMPLESOURCE_LIMESDRINPUT_LIMESDRINPUT_H_

#include <QString>
#include "dsp/devicesamplesource.h"
#include "limesdr/devicelimesdrparam.h"
#include "limesdrinputsettings.h"

class DeviceSourceAPI;
class LimeSDRInputThread;
struct DeviceLimeSDRParams;

class LimeSDRInput : public DeviceSampleSource
{
public:
    LimeSDRInput(DeviceSourceAPI *deviceAPI);
    virtual ~LimeSDRInput();

    virtual bool start(int device);
    virtual void stop();

    virtual const QString& getDeviceDescription() const;
    virtual int getSampleRate() const;
    virtual quint64 getCenterFrequency() const;

    virtual bool handleMessage(const Message& message);

private:
    DeviceSourceAPI *m_deviceAPI;
    QMutex m_mutex;
    LimeSDRInputSettings m_settings;
    LimeSDRInputThread* m_limeSDRInputThread;
    QString m_deviceDescription;
    bool m_running;
    DeviceLimeSDRParams m_deviceParams;

    bool openDevice();
    void closeDevice();
    bool applySettings(const LimeSDRInputSettings& settings, bool force);
};

#endif /* PLUGINS_SAMPLESOURCE_LIMESDRINPUT_LIMESDRINPUT_H_ */
