///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FILESINKOUTPUT_H
#define INCLUDE_FILESINKOUTPUT_H

#include <QString>
#include <QTimer>
#include <ctime>
#include <iostream>
#include <fstream>

#include "dsp/devicesamplesink.h"
#include "filesinksettings.h"

class FileSinkThread;
class DeviceSinkAPI;

class FileSinkOutput : public DeviceSampleSink {
public:
	class MsgConfigureFileSink : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const FileSinkSettings& getSettings() const { return m_settings; }
		bool getForce() const { return m_force; }

		static MsgConfigureFileSink* create(const FileSinkSettings& settings, bool force)
		{
			return new MsgConfigureFileSink(settings, force);
		}

	private:
		FileSinkSettings m_settings;
		bool m_force;

		MsgConfigureFileSink(const FileSinkSettings& settings, bool force) :
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

	class MsgConfigureFileSinkName : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const QString& getFileName() const { return m_fileName; }

		static MsgConfigureFileSinkName* create(const QString& fileName)
		{
			return new MsgConfigureFileSinkName(fileName);
		}

	private:
		QString m_fileName;

		MsgConfigureFileSinkName(const QString& fileName) :
			Message(),
			m_fileName(fileName)
		{ }
	};

	class MsgConfigureFileSinkWork : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isWorking() const { return m_working; }

		static MsgConfigureFileSinkWork* create(bool working)
		{
			return new MsgConfigureFileSinkWork(working);
		}

	private:
		bool m_working;

		MsgConfigureFileSinkWork(bool working) :
			Message(),
			m_working(working)
		{ }
	};

	class MsgConfigureFileSinkStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:

		static MsgConfigureFileSinkStreamTiming* create()
		{
			return new MsgConfigureFileSinkStreamTiming();
		}

	private:

		MsgConfigureFileSinkStreamTiming() :
			Message()
		{ }
	};

	class MsgReportFileSinkGeneration : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool getAcquisition() const { return m_acquisition; }

		static MsgReportFileSinkGeneration* create(bool acquisition)
		{
			return new MsgReportFileSinkGeneration(acquisition);
		}

	protected:
		bool m_acquisition;

		MsgReportFileSinkGeneration(bool acquisition) :
			Message(),
			m_acquisition(acquisition)
		{ }
	};

	class MsgReportFileSinkStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		std::size_t getSamplesCount() const { return m_samplesCount; }

		static MsgReportFileSinkStreamTiming* create(std::size_t samplesCount)
		{
			return new MsgReportFileSinkStreamTiming(samplesCount);
		}

	protected:
		std::size_t m_samplesCount;

		MsgReportFileSinkStreamTiming(std::size_t samplesCount) :
			Message(),
			m_samplesCount(samplesCount)
		{ }
	};

	FileSinkOutput(DeviceSinkAPI *deviceAPI);
	virtual ~FileSinkOutput();
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
	std::time_t getStartingTimeStamp() const;

	virtual bool handleMessage(const Message& message);

    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

private:
    DeviceSinkAPI *m_deviceAPI;
	QMutex m_mutex;
	FileSinkSettings m_settings;
	std::ofstream m_ofstream;
	FileSinkThread* m_fileSinkThread;
	QString m_deviceDescription;
	QString m_fileName;
	std::time_t m_startingTimeStamp;
	const QTimer& m_masterTimer;

	void openFileStream();
	void applySettings(const FileSinkSettings& settings, bool force = false);
};

#endif // INCLUDE_FILESINKOUTPUT_H
