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

#ifndef _AARONIARTSA_AARONIARTSAINPUT_H_
#define _AARONIARTSA_AARONIARTSAINPUT_H_

#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QNetworkRequest>

#include <dsp/devicesamplesource.h>
#include "aaroniartsainputsettings.h"

class DeviceAPI;
class AaroniaRTSAInputWorker;
class QNetworkAccessManager;
class QNetworkReply;
class QThread;

class AaroniaRTSAInput : public DeviceSampleSource {
    Q_OBJECT
public:
	class MsgConfigureAaroniaRTSA : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const AaroniaRTSAInputSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigureAaroniaRTSA* create(const AaroniaRTSAInputSettings& settings, const QList<QString>& settingsKeys, bool force)
		{
			return new MsgConfigureAaroniaRTSA(settings, settingsKeys, force);
		}

	private:
		AaroniaRTSAInputSettings m_settings;
        QList<QString> m_settingsKeys;
		bool m_force;

		MsgConfigureAaroniaRTSA(const AaroniaRTSAInputSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

	AaroniaRTSAInput(DeviceAPI *deviceAPI);
	virtual ~AaroniaRTSAInput();
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

    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGDeviceReport& response,
            QString& errorMessage);

    static void webapiFormatDeviceSettings(
            SWGSDRangel::SWGDeviceSettings& response,
            const AaroniaRTSAInputSettings& settings);

    static void webapiUpdateDeviceSettings(
            AaroniaRTSAInputSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

private:
	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
    int m_sampleRate;
    quint64 m_centerFrequency;
	AaroniaRTSAInputSettings m_settings;
	AaroniaRTSAInputWorker* m_aaroniaRTSAWorker;
	QThread *m_aaroniaRTSAWorkerThread;
	QString m_deviceDescription;
	bool m_running;
    const QTimer& m_masterTimer;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    int getStatus() const;
	bool applySettings(const AaroniaRTSAInputSettings& settings, const QList<QString>& settingsKeys, bool force);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const AaroniaRTSAInputSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

signals:
	void startWorker();
	void stopWorker();
	void setWorkerCenterFrequency(quint64 centerFrequency);
    void setWorkerSampleRate(int sampleRate);
	void setWorkerServerAddress(QString serverAddress);

private slots:
	void setWorkerStatus(int status);
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // _AARONIARTSA_AARONIARTSAINPUT_H_
