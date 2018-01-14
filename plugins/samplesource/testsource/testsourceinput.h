///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef _TESTSOURCE_TESTSOURCEINPUT_H_
#define _TESTSOURCE_TESTSOURCEINPUT_H_

#include <QString>
#include <QByteArray>
#include <QTimer>

#include <dsp/devicesamplesource.h>
#include "testsourcesettings.h"

class DeviceSourceAPI;
class TestSourceThread;
class FileRecord;

class TestSourceInput : public DeviceSampleSource {
public:
	class MsgConfigureTestSource : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const TestSourceSettings& getSettings() const { return m_settings; }
		bool getForce() const { return m_force; }

		static MsgConfigureTestSource* create(const TestSourceSettings& settings, bool force)
		{
			return new MsgConfigureTestSource(settings, force);
		}

	private:
		TestSourceSettings m_settings;
		bool m_force;

		MsgConfigureTestSource(const TestSourceSettings& settings, bool force) :
			Message(),
			m_settings(settings),
			m_force(force)
		{ }
	};

    class MsgFileRecord : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgFileRecord* create(bool startStop) {
            return new MsgFileRecord(startStop);
        }

    protected:
        bool m_startStop;

        MsgFileRecord(bool startStop) :
            Message(),
            m_startStop(startStop)
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

    TestSourceInput(DeviceSourceAPI *deviceAPI);
	virtual ~TestSourceInput();
	virtual void destroy();

	virtual void init();
	virtual bool start();
	virtual void stop();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
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

private:
	DeviceSourceAPI *m_deviceAPI;
    FileRecord *m_fileSink; //!< File sink to record device I/Q output
	QMutex m_mutex;
	TestSourceSettings m_settings;
	TestSourceThread* m_testSourceThread;
	QString m_deviceDescription;
	bool m_running;
    const QTimer& m_masterTimer;

	bool applySettings(const TestSourceSettings& settings, bool force);
};

#endif // _TESTSOURCE_TESTSOURCEINPUT_H_
