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

    virtual int webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage);

    virtual int webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGDeviceReport& response,
            QString& errorMessage);

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
    void webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const SDRPlaySettings& settings);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);

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

// ====================================================================

class SDRPlaySampleRates {
public:
    static unsigned int getRate(unsigned int rate_index);
    static unsigned int getRateIndex(unsigned int rate);
    static unsigned int getNbRates();
private:
    static const unsigned int m_nb_rates = 18;
    static unsigned int m_rates[m_nb_rates];
};

class SDRPlayBandwidths {
public:
    static unsigned int getBandwidth(unsigned int bandwidth_index);
    static unsigned int getBandwidthIndex(unsigned int bandwidth);
    static unsigned int getNbBandwidths();
private:
    static const unsigned int m_nb_bw = 8;
    static unsigned int m_bw[m_nb_bw];
};

class SDRPlayIF {
public:
    static unsigned int getIF(unsigned int if_index);
    static unsigned int getIFIndex(unsigned int iff);
    static unsigned int getNbIFs();
private:
    static const unsigned int m_nb_if = 4;
    static unsigned int m_if[m_nb_if];
};

class SDRPlayBands {
public:
    static QString getBandName(unsigned int band_index);
    static unsigned int getBandLow(unsigned int band_index);
    static unsigned int getBandHigh(unsigned int band_index);
    static unsigned int getNbBands();
private:
    static const unsigned int m_nb_bands = 8;
    static unsigned int m_bandLow[m_nb_bands];
    static unsigned int m_bandHigh[m_nb_bands];
    static const char* m_bandName[m_nb_bands];
};

#endif /* PLUGINS_SAMPLESOURCE_SDRPLAY_SDRPLAYINPUT_H_ */
