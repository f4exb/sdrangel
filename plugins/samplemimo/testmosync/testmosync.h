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

#include "dsp/devicesamplemimo.h"
#include "testmosyncsettings.h"

class DeviceAPI;
class TestMOSyncThread;

class TestMOSync : public DeviceSampleMIMO {
    Q_OBJECT

public:
	class MsgConfigureTestMOSync : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const TestMOSyncSettings& getSettings() const { return m_settings; }
		bool getForce() const { return m_force; }

		static MsgConfigureTestMOSync* create(const TestMOSyncSettings& settings, bool force)
		{
			return new MsgConfigureTestMOSync(settings, force);
		}

	private:
		TestMOSyncSettings m_settings;
		bool m_force;

		MsgConfigureTestMOSync(const TestMOSyncSettings& settings, bool force) :
			Message(),
			m_settings(settings),
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
	virtual bool startRx() {}
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

	virtual bool handleMessage(const Message& message);

    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    bool getRxRunning() const { return false; }
    bool getTxRunning() const { return m_runningTx; }

private:
	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
	TestMOSyncSettings m_settings;
    TestMOSyncThread* m_sinkThread;
	QString m_deviceDescription;
	bool m_runningTx;

	bool applySettings(const TestMOSyncSettings& settings, bool force);
};

#endif // PLUGINS_SAMPLEMIMO_TESTMOSYNC_TESTMOSYNC_H_
