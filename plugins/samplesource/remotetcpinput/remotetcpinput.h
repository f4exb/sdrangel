///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2021-2022 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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

#ifndef INCLUDE_REMOTETCPINPUT_H
#define INCLUDE_REMOTETCPINPUT_H

#include <ctime>
#include <iostream>
#include <stdint.h>

#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QThread>
#include <QNetworkRequest>

#include "dsp/devicesamplesource.h"
#include "dsp/replaybuffer.h"

#include "remotetcpinputsettings.h"
#include "remotetcpinputtcphandler.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;

class RemoteTCPInput : public DeviceSampleSource {
    Q_OBJECT
public:

    class MsgConfigureRemoteTCPInput : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RemoteTCPInputSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureRemoteTCPInput* create(const RemoteTCPInputSettings& settings, const QList<QString>& settingsKeys, bool force = false) {
            return new MsgConfigureRemoteTCPInput(settings, settingsKeys, force);
        }

    private:
        RemoteTCPInputSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureRemoteTCPInput(const RemoteTCPInputSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

    private:
        bool m_startStop;

        explicit MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    class MsgReportTCPBuffer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        qint64 getInBytesAvailable() const { return m_inBytesAvailable; }
        qint64 getInSize() const { return m_inSize; }
        float getInSeconds() const { return m_inSeconds; }
        qint64 getOutBytesAvailable() const { return m_outBytesAvailable; }
        qint64 getOutSize() const { return m_outSize; }
        float getOutSeconds() const { return m_outSeconds; }

        static MsgReportTCPBuffer* create(qint64 inBytesAvailable, qint64 inSize, float inSeconds,
                                          qint64 outBytesAvailable, qint64 outSize, float outSeconds) {
            return new MsgReportTCPBuffer(inBytesAvailable, inSize, inSeconds,
                                          outBytesAvailable, outSize, outSeconds);
        }

    private:
        qint64 m_inBytesAvailable;
        qint64 m_inSize;
        float m_inSeconds;
        qint64 m_outBytesAvailable;
        qint64 m_outSize;
        float m_outSeconds;

        MsgReportTCPBuffer(qint64 inBytesAvailable, qint64 inSize, float inSeconds,
                           qint64 outBytesAvailable, qint64 outSize, float outSeconds) :
            Message(),
            m_inBytesAvailable(inBytesAvailable),
            m_inSize(inSize),
            m_inSeconds(inSeconds),
            m_outBytesAvailable(outBytesAvailable),
            m_outSize(outSize),
            m_outSeconds(outSeconds)
        { }
    };

    class MsgSaveReplay : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getFilename() const { return m_filename; }

        static MsgSaveReplay* create(const QString& filename) {
            return new MsgSaveReplay(filename);
        }

    private:
        QString m_filename;

        explicit MsgSaveReplay(const QString& filename) :
            Message(),
            m_filename(filename)
        { }
    };

    class MsgSendMessage : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getCallsign() const { return m_callsign; }
        const QString& getText() const { return m_text; }
        bool getBroadcast() const { return m_broadcast; }

        static MsgSendMessage* create(const QString& callsign, const QString& text, bool broadcast) {
            return new MsgSendMessage(callsign, text, broadcast);
        }

    private:
        QString m_callsign;
        QString m_text;
        bool m_broadcast;

        MsgSendMessage(const QString& callsign, const QString& text, bool broadcast) :
            Message(),
            m_callsign(callsign),
            m_text(text),
            m_broadcast(broadcast)
        { }
    };

    class MsgReportPosition : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		float getLatitude() const { return m_latitude; }
		float getLongitude() const { return m_longitude; }
		float getAltitude() const { return m_altitude; }

		static MsgReportPosition* create(float latitude, float longitude, float altitude) {
			return new MsgReportPosition(latitude, longitude, altitude);
		}

	private:
		float m_latitude;
		float m_longitude;
		float m_altitude;

		MsgReportPosition(float latitude, float longitude, float altitude) :
			Message(),
			m_latitude(latitude),
			m_longitude(longitude),
			m_altitude(altitude)
		{ }
	};

    class MsgReportDirection : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
        bool getIsotropic() const { return m_isotropic; }
		float getAzimuth() const { return m_azimuth; }
		float getElevation() const { return m_elevation; }

		static MsgReportDirection* create(bool isotropic, float azimuth, float elevation) {
			return new MsgReportDirection(isotropic, azimuth, elevation);
		}

	private:
        bool m_isotropic;
		float m_azimuth;
		float m_elevation;

		MsgReportDirection(bool isotropic, float azimuth, float elevation) :
			Message(),
            m_isotropic(isotropic),
			m_azimuth(azimuth),
			m_elevation(elevation)
		{ }
	};

    RemoteTCPInput(DeviceAPI *deviceAPI);
    virtual ~RemoteTCPInput();
    virtual void destroy();

    virtual void init();
    virtual bool start();
    virtual void stop();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue);
    virtual const QString& getDeviceDescription() const;
    virtual int getSampleRate() const;
    virtual void setSampleRate(int sampleRate) { (void) sampleRate; }
    virtual quint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);
    std::time_t getStartingTimeStamp() const;

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
            const RemoteTCPInputSettings& settings);

    static void webapiUpdateDeviceSettings(
            RemoteTCPInputSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

    void getMagSqLevels(double& avg, double& peak, int& nbSamples)
    {
        if (m_remoteInputTCPPHandler) {
            m_remoteInputTCPPHandler->getMagSqLevels(avg, peak, nbSamples);
        } else {
            avg = 0.0; peak = 0.0; nbSamples = 1;
        }
    }

private:
    DeviceAPI *m_deviceAPI;
    QRecursiveMutex m_mutex;
    RemoteTCPInputSettings m_settings;
    RemoteTCPInputTCPHandler* m_remoteInputTCPPHandler;
    QString m_deviceDescription;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    ReplayBuffer<FixReal> m_replayBuffer;
    QThread m_thread;
    bool m_running;
    float m_latitude;   // Position of remote device (antenna)
    float m_longitude;
    float m_altitude;
    bool m_isotropic;   // Direction of remote anntenna
    float m_azimuth;
    float m_elevation;

    void applySettings(const RemoteTCPInputSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const RemoteTCPInputSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_REMOTETCPINPUT_H
