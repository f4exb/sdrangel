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

#ifndef PLUGINS_SAMPLESOURCE_SDRPLAY_SDRPLAYINPUT_H_
#define PLUGINS_SAMPLESOURCE_SDRPLAY_SDRPLAYINPUT_H_

#include <dsp/devicesamplesource.h>

#include "sdrplaysettings.h"
#include <mirisdr.h>
#include <QString>
#include <stdint.h>

class DeviceSourceAPI;
class SDRPlayThread;

class SDRPlayInput : public DeviceSampleSource {
public:
    class MsgConfigureSDRPlay : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SDRPlaySettings& getSettings() const { return m_settings; }

        static MsgConfigureSDRPlay* create(const SDRPlaySettings& settings)
        {
            return new MsgConfigureSDRPlay(settings);
        }

    private:
        SDRPlaySettings m_settings;

        MsgConfigureSDRPlay(const SDRPlaySettings& settings) :
            Message(),
            m_settings(settings)
        { }
    };

    class MsgReportSDRPlay : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
		const std::vector<int>& getGains() const { return m_gains; }

        static MsgReportSDRPlay* create(const std::vector<int>& gains)
        {
            return new MsgReportSDRPlay(gains);
        }

    protected:
		std::vector<int> m_gains;

        MsgReportSDRPlay(const std::vector<int>& gains) :
            Message(),
			m_gains(gains)
        { }
    };

    SDRPlayInput(DeviceSourceAPI *deviceAPI);
    virtual ~SDRPlayInput();

    virtual bool init(const Message& message);
    virtual bool start(uint32_t device);
    virtual void stop();

    virtual const QString& getDeviceDescription() const;
    virtual int getSampleRate() const;
    virtual quint64 getCenterFrequency() const;

    virtual bool handleMessage(const Message& message);

private:
    bool applySettings(const SDRPlaySettings& settings, bool force);

    DeviceSourceAPI *m_deviceAPI;
    QMutex m_mutex;
    SDRPlaySettings m_settings;
    mirisdr_dev_t* m_dev;
    SDRPlayThread* m_sdrPlayThread;
    QString m_deviceDescription;
    std::vector<int> m_gains;
};

#endif /* PLUGINS_SAMPLESOURCE_SDRPLAY_SDRPLAYINPUT_H_ */
