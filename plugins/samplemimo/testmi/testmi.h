///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef _TESTMI_TESTMI_H_
#define _TESTMI_TESTMI_H_

#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QNetworkRequest>
#include <QThread>

#include "dsp/devicesamplemimo.h"
#include "testmisettings.h"

class DeviceAPI;
class TestMIWorker;
class QNetworkAccessManager;
class QNetworkReply;

class TestMI : public DeviceSampleMIMO {
    Q_OBJECT
public:
	class MsgConfigureTestSource : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const TestMISettings& getSettings() const { return m_settings; }
		bool getForce() const { return m_force; }

		static MsgConfigureTestSource* create(const TestMISettings& settings, bool force)
		{
			return new MsgConfigureTestSource(settings, force);
		}

	private:
		TestMISettings m_settings;
		bool m_force;

		MsgConfigureTestSource(const TestMISettings& settings, bool force) :
			Message(),
			m_settings(settings),
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

    TestMI(DeviceAPI *deviceAPI);
	virtual ~TestMI();
	virtual void destroy();

	virtual void init();
	virtual bool startRx();
	virtual void stopRx();
	virtual bool startTx();
	virtual void stopTx();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
	virtual const QString& getDeviceDescription() const;

	virtual int getSourceSampleRate(int index) const;
    virtual void setSourceSampleRate(int sampleRate, int index) { (void) sampleRate; (void) index; }
	virtual quint64 getSourceCenterFrequency(int index) const;
    virtual void setSourceCenterFrequency(qint64 centerFrequency, int index);

	virtual int getSinkSampleRate(int index) const { return 0; (void) index; }
    virtual void setSinkSampleRate(int sampleRate, int index) { (void) sampleRate; (void) index; }
	virtual quint64 getSinkCenterFrequency(int index) const { return 0; (void) index; }
    virtual void setSinkCenterFrequency(qint64 centerFrequency, int index) { (void) centerFrequency; (void) index; }

    virtual quint64 getMIMOCenterFrequency() const { return getSourceCenterFrequency(0); }
    virtual unsigned int getMIMOSampleRate() const { return getSourceSampleRate(0); }

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
            int subsystemIndex,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            int subsystemIndex,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    static void webapiFormatDeviceSettings(
            SWGSDRangel::SWGDeviceSettings& response,
            const TestMISettings& settings);

    static void webapiUpdateDeviceSettings(
            TestMISettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

private:
    struct DeviceSettingsKeys
    {
        QList<QString> m_commonSettingsKeys;
        QList<QList<QString>> m_streamsSettingsKeys;
    };

	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
	TestMISettings m_settings;
	std::vector<TestMIWorker*> m_testSourceWorkers;
    std::vector<QThread*> m_testSourceWorkerThreads;
	QString m_deviceDescription;
	bool m_running;
    const QTimer& m_masterTimer;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void startWorkers();
    void stopWorkers();
	bool applySettings(const TestMISettings& settings, bool force);
    void webapiReverseSendSettings(const DeviceSettingsKeys& deviceSettingsKeys, const TestMISettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // _TESTMI_TESTMI_H_
