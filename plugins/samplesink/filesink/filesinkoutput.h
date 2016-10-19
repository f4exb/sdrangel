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

#include <dsp/devicesamplesink.h>
#include <QString>
#include <QTimer>
#include <ctime>
#include <iostream>
#include <fstream>

class FileSinkThread;

class FileSinkOutput : public DeviceSampleSink {
public:
	struct Settings {
		QString m_fileName;

		Settings();
		void resetToDefaults();
		QByteArray serialize() const;
		bool deserialize(const QByteArray& data);
	};

	class MsgConfigureFileSink : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const Settings& getSettings() const { return m_settings; }

		static MsgConfigureFileSink* create(const Settings& settings)
		{
			return new MsgConfigureFileSink(settings);
		}

	private:
		Settings m_settings;

		MsgConfigureFileSink(const Settings& settings) :
			Message(),
			m_settings(settings)
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

	class MsgReportFileSinkStreamData : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getSampleRate() const { return m_sampleRate; }
		quint64 getCenterFrequency() const { return m_centerFrequency; }
		std::time_t getStartingTimeStamp() const { return m_startingTimeStamp; }

		static MsgReportFileSinkStreamData* create(int sampleRate,
				quint64 centerFrequency,
				std::time_t startingTimeStamp)
		{
			return new MsgReportFileSinkStreamData(sampleRate, centerFrequency, startingTimeStamp);
		}

	protected:
		int m_sampleRate;
		quint64 m_centerFrequency;
		std::time_t m_startingTimeStamp;

		MsgReportFileSinkStreamData(int sampleRate,
				quint64 centerFrequency,
				std::time_t startingTimeStamp) :
			Message(),
			m_sampleRate(sampleRate),
			m_centerFrequency(centerFrequency),
			m_startingTimeStamp(startingTimeStamp)
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

	FileSinkOutput(const QTimer& masterTimer);
	virtual ~FileSinkOutput();

	virtual bool init(const Message& message);
	virtual bool start(int device);
	virtual void stop();

	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
	virtual quint64 getCenterFrequency() const;
	std::time_t getStartingTimeStamp() const;

	virtual bool handleMessage(const Message& message);

private:
	QMutex m_mutex;
	Settings m_settings;
	std::ofstream m_ofstream;
	FileSinkThread* m_fileSinkThread;
	QString m_deviceDescription;
	QString m_fileName;
	int m_sampleRate;
	quint64 m_centerFrequency;
	std::time_t m_startingTimeStamp;
	const QTimer& m_masterTimer;

	void openFileStream();
};

#endif // INCLUDE_FILESINKOUTPUT_H
