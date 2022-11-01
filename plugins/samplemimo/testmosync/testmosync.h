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

#ifndef PLUGINS_SAMPLEMIMO_TESTMOSYNC_TESTMOSYNC_H_
#define PLUGINS_SAMPLEMIMO_TESTMOSYNC_TESTMOSYNC_H_

#include <stdint.h>

#include <QString>
#include <QByteArray>
#include <QTimer>

#include "dsp/devicesamplemimo.h"
#include "dsp/spectrumvis.h"
#include "testmosyncsettings.h"

class DeviceAPI;
class TestMOSyncWorker;
class BasebandSampleSink;
class QThread;

class TestMOSync : public DeviceSampleMIMO {
    Q_OBJECT

public:
	class MsgConfigureTestMOSync : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const TestMOSyncSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigureTestMOSync* create(const TestMOSyncSettings& settings, const QList<QString>& settingsKeys, bool force) {
			return new MsgConfigureTestMOSync(settings, settingsKeys, force);
		}

	private:
		TestMOSyncSettings m_settings;
        QList<QString> m_settingsKeys;
		bool m_force;

		MsgConfigureTestMOSync(const TestMOSyncSettings& settings, const QList<QString>& settingsKeys, bool force) :
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
        bool getRxElseTx() const { return m_rxElseTx; }

        static MsgStartStop* create(bool startStop, bool rxElseTx) {
            return new MsgStartStop(startStop, rxElseTx);
        }

    protected:
        bool m_startStop;
        bool m_rxElseTx;

        MsgStartStop(bool startStop, bool rxElseTx) :
            Message(),
            m_startStop(startStop),
            m_rxElseTx(rxElseTx)
        { }
    };

    TestMOSync(DeviceAPI *deviceAPI);
	virtual ~TestMOSync();
	virtual void destroy();

	virtual void init();
	virtual bool startRx() { return false; }
	virtual void stopRx() {}
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

	virtual int getSinkSampleRate(int index) const;
    virtual void setSinkSampleRate(int sampleRate, int index) { (void) sampleRate; (void) index; }
	virtual quint64 getSinkCenterFrequency(int index) const;
    virtual void setSinkCenterFrequency(qint64 centerFrequency, int index);

    virtual quint64 getMIMOCenterFrequency() const { return getSourceCenterFrequency(0); }
    virtual unsigned int getMIMOSampleRate() const { return getSinkSampleRate(0); }

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
            const TestMOSyncSettings& settings);

    static void webapiUpdateDeviceSettings(
            TestMOSyncSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }
    bool getRxRunning() const { return false; }
    bool getTxRunning() const { return m_runningTx; }
    void setFeedSpectrumIndex(unsigned int  feedSpectrumIndex);

private:
	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
    SpectrumVis m_spectrumVis;
	TestMOSyncSettings m_settings;
    TestMOSyncWorker *m_sinkWorker;
    QThread *m_sinkWorkerThread;
	QString m_deviceDescription;
	bool m_runningTx;
    const QTimer& m_masterTimer;
    unsigned int m_feedSpectrumIndex;

    void startWorker();
    void stopWorker();
	bool applySettings(const TestMOSyncSettings& settings, const QList<QString>& settingsKeys, bool force);
};

#endif // PLUGINS_SAMPLEMIMO_TESTMOSYNC_TESTMOSYNC_H_
