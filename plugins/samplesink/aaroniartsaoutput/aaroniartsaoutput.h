///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_AARONIARTSAOUTPUT_H
#define INCLUDE_AARONIARTSAOUTPUT_H

#include <ctime>
#include <iostream>
#include <stdint.h>

#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QNetworkRequest>

#include "dsp/devicesamplesink.h"

#include "aaroniartsaoutputsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class QThread;
class AaroniaRTSAOutputWorker;

class AaroniaRTSAOutput : public DeviceSampleSink {
    Q_OBJECT
public:
    class MsgConfigureAaroniaRTSAOutput : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AaroniaRTSAOutputSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureAaroniaRTSAOutput* create(const AaroniaRTSAOutputSettings& settings, const QList<QString>& settingsKeys, bool force = false) {
            return new MsgConfigureAaroniaRTSAOutput(settings, settingsKeys, force);
        }

    private:
        AaroniaRTSAOutputSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureAaroniaRTSAOutput(const AaroniaRTSAOutputSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

    class MsgReportSampleRateAndFrequency : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }
        int getCenterFrequency() const { return m_centerFrequency; }

        static MsgReportSampleRateAndFrequency* create(int sampleRate, qint64 centerFrequency) {
            return new MsgReportSampleRateAndFrequency(sampleRate, centerFrequency);
        }

    protected:
        int m_sampleRate;
        qint64 m_centerFrequency;

        MsgReportSampleRateAndFrequency(int sampleRate, qint64 centerFrequency) :
            Message(),
            m_sampleRate(sampleRate),
            m_centerFrequency(centerFrequency)
        { }
    };

	class MsgSetStatus : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getStatus() const { return m_status; }

		static MsgSetStatus* create(int status) {
			return new MsgSetStatus(status);
		}

	protected:
		int m_status;

		MsgSetStatus(int status) :
			Message(),
			m_status(status)
		{ }
	};

	AaroniaRTSAOutput(DeviceAPI *deviceAPI);
	virtual ~AaroniaRTSAOutput();
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
            const AaroniaRTSAOutputSettings& settings);

    static void webapiUpdateDeviceSettings(
            AaroniaRTSAOutputSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

private:
	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
	AaroniaRTSAOutputSettings m_settings;
	QString m_deviceDescription;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
	AaroniaRTSAOutputWorker *m_worker;
    QThread *m_workerThread;
    bool m_running;

    int getStatus() const;
    void applySettings(const AaroniaRTSAOutputSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const AaroniaRTSAOutputSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void setWorkerStatus(int status);
};

#endif // INCLUDE_AARONIARTSAOUTPUT_H
