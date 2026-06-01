///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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

#ifndef _FOBOS_FOBOSINPUT_H_
#define _FOBOS_FOBOSINPUT_H_

#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QNetworkRequest>

#include <dsp/devicesamplesource.h>
#include "fobossettings.h"

class DeviceAPI;
class FOBOSWorker;
class QNetworkAccessManager;
class QNetworkReply;
class QThread;

class FOBOSInput : public DeviceSampleSource {
    Q_OBJECT
public:
	class MsgConfigureFOBOS : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const FOBOSSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigureFOBOS* create(const FOBOSSettings& settings, const QList<QString>& settingsKeys, bool force) {
			return new MsgConfigureFOBOS(settings, settingsKeys, force);
		}

	private:
		FOBOSSettings m_settings;
        QList<QString> m_settingsKeys;
		bool m_force;

		MsgConfigureFOBOS(const FOBOSSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

    class MsgReportFOBOSBackend : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getBackend() const { return m_backend; }
        const QString& getDetails() const { return m_details; }

        static MsgReportFOBOSBackend* create(const QString& backend, const QString& details) {
            return new MsgReportFOBOSBackend(backend, details);
        }

    protected:
        QString m_backend;
        QString m_details;

        MsgReportFOBOSBackend(const QString& backend, const QString& details) :
            Message(),
            m_backend(backend),
            m_details(details)
        { }
    };

    FOBOSInput(DeviceAPI *deviceAPI);
	virtual ~FOBOSInput();
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

    static void webapiFormatDeviceSettings(
            SWGSDRangel::SWGDeviceSettings& response,
            const FOBOSSettings& settings);

    static void webapiUpdateDeviceSettings(
            FOBOSSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

private:
	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
	FOBOSSettings m_settings;
	FOBOSWorker *m_worker;
    QThread *m_workerThread;
	QString m_deviceDescription;
	bool m_running;
    const QTimer& m_masterTimer;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

	bool applySettings(const FOBOSSettings& settings, const QList<QString>& settingsKeys, bool force);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const FOBOSSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleWorkerBackendStatus(const QString& backend, const QString& details);
};

#endif // _FOBOS_FOBOSINPUT_H_
