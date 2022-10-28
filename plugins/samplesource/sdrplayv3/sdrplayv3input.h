///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_SAMPLESOURCE_SDRPLAYV3_SDRPLAYV3INPUT_H_
#define PLUGINS_SAMPLESOURCE_SDRPLAYV3_SDRPLAYV3INPUT_H_

#include <stdint.h>

#include <QString>
#include <QByteArray>
#include <QNetworkRequest>

#include <sdrplay_api.h>
#include "dsp/devicesamplesource.h"
#include "sdrplayv3settings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class SDRPlayV3Thread;

class SDRPlayV3Input : public DeviceSampleSource {
    Q_OBJECT
public:

    class MsgConfigureSDRPlayV3 : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SDRPlayV3Settings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureSDRPlayV3* create(const SDRPlayV3Settings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureSDRPlayV3(settings, settingsKeys, force);
        }

    private:
        SDRPlayV3Settings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureSDRPlayV3(const SDRPlayV3Settings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
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

    SDRPlayV3Input(DeviceAPI *deviceAPI);
    virtual ~SDRPlayV3Input();
    virtual void destroy();

    virtual void init();
    virtual bool start();
    virtual void stop();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    virtual const QString& getDeviceDescription() const;
    virtual int getSampleRate() const;
    virtual void setSampleRate(int sampleRate) { (void) sampleRate; }
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

    static void webapiFormatDeviceSettings(
            SWGSDRangel::SWGDeviceSettings& response,
            const SDRPlayV3Settings& settings);

    static void webapiUpdateDeviceSettings(
            SDRPlayV3Settings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

    int getDeviceId();

private:
    DeviceAPI *m_deviceAPI;
    QMutex m_mutex;
    SDRPlayV3Settings m_settings;
    sdrplay_api_DeviceT m_devs[SDRPLAY_MAX_DEVICES];
    sdrplay_api_DeviceT* m_dev;
    sdrplay_api_DeviceParamsT *m_devParams;
    SDRPlayV3Thread* m_sdrPlayThread;
    QString m_deviceDescription;
    int m_devNumber;
    bool m_running;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    bool openDevice();
    void closeDevice();
    bool applySettings(const SDRPlayV3Settings& settings, const QList<QString>& settingsKeys, bool forwardChange, bool force);
    bool setDeviceCenterFrequency(quint64 freq);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const SDRPlayV3Settings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

// ====================================================================

class SDRPlayV3Bandwidths {
public:
    static sdrplay_api_Bw_MHzT getBandwidthEnum(unsigned int bandwidth_index);
    static unsigned int getBandwidth(unsigned int bandwidth_index);
    static unsigned int getBandwidthIndex(unsigned int bandwidth);
    static unsigned int getNbBandwidths();
private:
    static const unsigned int m_nb_bw = 8;
    static unsigned int m_bw[m_nb_bw];
    static sdrplay_api_Bw_MHzT m_bwEnums[m_nb_bw];
};

class SDRPlayV3IF {
public:
    static sdrplay_api_If_kHzT getIFEnum(unsigned int if_index);
    static unsigned int getIF(unsigned int if_index);
    static unsigned int getIFIndex(unsigned int iff);
    static unsigned int getNbIFs();
private:
    static const unsigned int m_nb_if = 4;
    static unsigned int m_if[m_nb_if];
    static sdrplay_api_If_kHzT m_ifEnums[m_nb_if];
};

class SDRPlayV3LNA {
public:
    static const int *getAttenuations(int deviceId, qint64 frequency);
private:
    static const int rsp1Attenuation[3][5];
    static const int rsp1AAttenuation[4][11];
    static const int rsp2Attenuation[3][10];
    static const int rspDuoAttenuation[5][11];
    static const int rspDxAttenuation[6][28];
};

#endif /* PLUGINS_SAMPLESOURCE_SDRPLAYV3_SDRPLAYV3INPUT_H_ */
