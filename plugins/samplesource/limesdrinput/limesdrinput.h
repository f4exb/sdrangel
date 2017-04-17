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
#include <stdint.h>

#include "dsp/devicesamplesource.h"
#include "limesdr/devicelimesdrshared.h"
#include "limesdrinputsettings.h"

class DeviceSourceAPI;
class LimeSDRInputThread;
struct DeviceLimeSDRParams;

class LimeSDRInput : public DeviceSampleSource
{
public:
    class MsgConfigureLimeSDR : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const LimeSDRInputSettings& getSettings() const { return m_settings; }

        static MsgConfigureLimeSDR* create(const LimeSDRInputSettings& settings)
        {
            return new MsgConfigureLimeSDR(settings);
        }

    private:
        LimeSDRInputSettings m_settings;

        MsgConfigureLimeSDR(const LimeSDRInputSettings& settings) :
            Message(),
            m_settings(settings)
        { }
    };

    class MsgSetReferenceConfig : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const LimeSDRInputSettings& getSettings() const { return m_settings; }

        static MsgSetReferenceConfig* create(const LimeSDRInputSettings& settings)
        {
            return new MsgSetReferenceConfig(settings);
        }

    private:
        LimeSDRInputSettings m_settings;

        MsgSetReferenceConfig(const LimeSDRInputSettings& settings) :
            Message(),
            m_settings(settings)
        { }
    };

    class MsgReportLimeSDRToGUI : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float    getCenterFrequency() const { return m_centerFrequency; }
        int      getSampleRate() const { return m_sampleRate; }
        uint32_t getLog2HardDecim() const { return m_log2HardDecim; }

        static MsgReportLimeSDRToGUI* create(float centerFrequency, int sampleRate, uint32_t log2HardDecim)
        {
            return new MsgReportLimeSDRToGUI(centerFrequency, sampleRate, log2HardDecim);
        }

    private:
        float    m_centerFrequency;
        int      m_sampleRate;
        uint32_t m_log2HardDecim;

        MsgReportLimeSDRToGUI(float centerFrequency, int sampleRate, uint32_t log2HardDecim) :
            Message(),
            m_centerFrequency(centerFrequency),
            m_sampleRate(sampleRate),
            m_log2HardDecim(log2HardDecim)
        { }
    };

    LimeSDRInput(DeviceSourceAPI *deviceAPI);
    virtual ~LimeSDRInput();

    virtual bool start();
    virtual void stop();

    virtual const QString& getDeviceDescription() const;
    virtual int getSampleRate() const;
    virtual quint64 getCenterFrequency() const;

    virtual bool handleMessage(const Message& message);

    std::size_t getChannelIndex();
    void getLORange(float& minF, float& maxF, float& stepF) const;
    void getSRRange(float& minF, float& maxF, float& stepF) const;
    void getLPRange(float& minF, float& maxF, float& stepF) const;
    int getLPIndex(float lpfBW) const;
    float getLPValue(int index) const;
    uint32_t getHWLog2Decim() const;

private:
    DeviceSourceAPI *m_deviceAPI;
    QMutex m_mutex;
    LimeSDRInputSettings m_settings;
    LimeSDRInputThread* m_limeSDRInputThread;
    QString m_deviceDescription;
    bool m_running;
    DeviceLimeSDRShared m_deviceShared;

    lms_stream_t m_streamId;

    bool openDevice();
    void closeDevice();
    bool applySettings(const LimeSDRInputSettings& settings, bool force);
};

#endif /* PLUGINS_SAMPLESOURCE_LIMESDRINPUT_LIMESDRINPUT_H_ */
