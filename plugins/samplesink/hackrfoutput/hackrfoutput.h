///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_HACKRFOUTPUT_H
#define INCLUDE_HACKRFOUTPUT_H

#include <QString>
#include <QNetworkRequest>

#include "dsp/devicesamplesink.h"
#include "libhackrf/hackrf.h"
#include "hackrf/devicehackrf.h"
#include "hackrf/devicehackrfparam.h"
#include "hackrfoutputsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class HackRFOutputThread;

class HackRFOutput : public DeviceSampleSink {
	Q_OBJECT
public:

	class MsgConfigureHackRF : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const HackRFOutputSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigureHackRF* create(const HackRFOutputSettings& settings, const QList<QString>& settingsKeys, bool force) {
			return new MsgConfigureHackRF(settings, settingsKeys, force);
		}

	private:
		HackRFOutputSettings m_settings;
        QList<QString> m_settingsKeys;
		bool m_force;

		MsgConfigureHackRF(const HackRFOutputSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

	class MsgReportHackRF : public Message {
		MESSAGE_CLASS_DECLARATION

	public:

		static MsgReportHackRF* create()
		{
			return new MsgReportHackRF();
		}

	protected:

		MsgReportHackRF() :
			Message()
		{ }
	};

	HackRFOutput(DeviceAPI *deviceAPI);
	virtual ~HackRFOutput();
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
            const HackRFOutputSettings& settings);

    static void webapiUpdateDeviceSettings(
            HackRFOutputSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

private:
	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
	HackRFOutputSettings m_settings;
	struct hackrf_device* m_dev;
	HackRFOutputThread* m_hackRFThread;
	QString m_deviceDescription;
	DeviceHackRFParams m_sharedParams;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
	bool m_running;

    bool openDevice();
    void closeDevice();
	bool applySettings(const HackRFOutputSettings& settings, const QList<QString>& settingsKeys, bool force);
//	hackrf_device *open_hackrf_from_sequence(int sequence);
	void setDeviceCenterFrequency(quint64 freq_hz, int loPpmTenths);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const HackRFOutputSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_HACKRFINPUT_H
