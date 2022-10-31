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

#ifndef INCLUDE_TESTSINKOUTPUT_H
#define INCLUDE_TESTSINKOUTPUT_H

#include <QString>
#include <QTimer>
#include <ctime>
#include <iostream>
#include <fstream>

#include "dsp/devicesamplesink.h"
#include "dsp/spectrumvis.h"
#include "testsinksettings.h"

class QThread;
class TestSinkWorker;
class DeviceAPI;
class BasebandSampleSink;
class Serializable;

class TestSinkOutput : public DeviceSampleSink {
public:
	class MsgConfigureTestSink : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const TestSinkSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigureTestSink* create(const TestSinkSettings& settings, const QList<QString>& settingsKeys, bool force) {
			return new MsgConfigureTestSink(settings, settingsKeys, force);
		}

	private:
		TestSinkSettings m_settings;
        QList<QString> m_settingsKeys;
		bool m_force;

		MsgConfigureTestSink(const TestSinkSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

	TestSinkOutput(DeviceAPI *deviceAPI);
	virtual ~TestSinkOutput();
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

    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_settings.setSpectrumGUI(spectrumGUI); }

private:
    DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
	TestSinkSettings m_settings;
    SpectrumVis m_spectrumVis;
	std::ofstream m_ofstream;
	TestSinkWorker *m_testSinkWorker;
    QThread *m_testSinkWorkerThread;
    bool m_running;
	QString m_deviceDescription;
	const QTimer& m_masterTimer;

	void applySettings(const TestSinkSettings& settings, const QList<QString>& settingsKeys, bool force = false);
};

#endif // INCLUDE_TESTSINKOUTPUT_H
