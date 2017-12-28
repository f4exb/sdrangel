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

#include <QString>
#include <QByteArray>
#include <stdint.h>

#include <mirisdr.h>
#include <dsp/devicesamplesource.h>
#include "sdrplaysettings.h"

class DeviceSourceAPI;
class SDRPlayThread;
class FileRecord;

class SDRPlayInput : public DeviceSampleSource {
public:
    enum SDRPlayVariant
    {
        SDRPlayUndef,
        SDRPlayRSP1,
        SDRPlayRSP1A,
        SDRPlayRSP2
    };

    class MsgConfigureSDRPlay : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SDRPlaySettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureSDRPlay* create(const SDRPlaySettings& settings, bool force)
        {
            return new MsgConfigureSDRPlay(settings, force);
        }

    private:
        SDRPlaySettings m_settings;
        bool m_force;

        MsgConfigureSDRPlay(const SDRPlaySettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgReportSDRPlayGains : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgReportSDRPlayGains* create(int lnaGain, int mixerGain, int basebandGain, int tunerGain)
        {
            return new MsgReportSDRPlayGains(lnaGain, mixerGain, basebandGain, tunerGain);
        }

        int getLNAGain() const { return m_lnaGain; }
        int getMixerGain() const { return m_mixerGain; }
        int getBasebandGain() const { return m_basebandGain; }
        int getTunerGain() const { return m_tunerGain; }

    protected:
        int m_lnaGain;
        int m_mixerGain;
        int m_basebandGain;
        int m_tunerGain;

        MsgReportSDRPlayGains(int lnaGain, int mixerGain, int basebandGain, int tunerGain) :
            Message(),
            m_lnaGain(lnaGain),
            m_mixerGain(mixerGain),
            m_basebandGain(basebandGain),
            m_tunerGain(tunerGain)
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

    SDRPlayInput(DeviceSourceAPI *deviceAPI);
    virtual ~SDRPlayInput();
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

    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    SDRPlayVariant getVariant() const { return m_variant; }

private:
    bool openDevice();
    void closeDevice();
    bool applySettings(const SDRPlaySettings& settings, bool forwardChange, bool force);
    bool setDeviceCenterFrequency(quint64 freq);

    DeviceSourceAPI *m_deviceAPI;
    QMutex m_mutex;
    SDRPlayVariant m_variant;
    SDRPlaySettings m_settings;
    mirisdr_dev_t* m_dev;
    SDRPlayThread* m_sdrPlayThread;
    QString m_deviceDescription;
    int m_devNumber;
    bool m_running;
    FileRecord *m_fileSink; //!< File sink to record device I/Q output
};

#endif /* PLUGINS_SAMPLESOURCE_SDRPLAY_SDRPLAYINPUT_H_ */
