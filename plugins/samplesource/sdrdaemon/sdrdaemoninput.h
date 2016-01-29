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

#ifndef INCLUDE_SDRDAEMONINPUT_H
#define INCLUDE_SDRDAEMONINPUT_H

#include "dsp/samplesource.h"
#include <QString>
#include <QTimer>
#include <ctime>
#include <iostream>

class SDRdaemonThread;

class SDRdaemonInput : public SampleSource {
public:
	struct Settings {
		QString m_fileName;

		Settings();
		void resetToDefaults();
		QByteArray serialize() const;
		bool deserialize(const QByteArray& data);
	};

	class MsgConfigureSDRdaemon : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const Settings& getSettings() const { return m_settings; }

		static MsgConfigureSDRdaemon* create(const Settings& settings)
		{
			return new MsgConfigureSDRdaemon(settings);
		}

	private:
		Settings m_settings;

		MsgConfigureSDRdaemon(const Settings& settings) :
			Message(),
			m_settings(settings)
		{ }
	};

	class MsgConfigureSDRdaemonName : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const QString& getFileName() const { return m_fileName; }

		static MsgConfigureSDRdaemonName* create(const QString& fileName)
		{
			return new MsgConfigureSDRdaemonName(fileName);
		}

	private:
		QString m_fileName;

		MsgConfigureSDRdaemonName(const QString& fileName) :
			Message(),
			m_fileName(fileName)
		{ }
	};

	class MsgConfigureSDRdaemonWork : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isWorking() const { return m_working; }

		static MsgConfigureSDRdaemonWork* create(bool working)
		{
			return new MsgConfigureSDRdaemonWork(working);
		}

	private:
		bool m_working;

		MsgConfigureSDRdaemonWork(bool working) :
			Message(),
			m_working(working)
		{ }
	};

	class MsgConfigureSDRdaemonStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:

		static MsgConfigureSDRdaemonStreamTiming* create()
		{
			return new MsgConfigureSDRdaemonStreamTiming();
		}

	private:

		MsgConfigureSDRdaemonStreamTiming() :
			Message()
		{ }
	};

	class MsgReportSDRdaemonAcquisition : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool getAcquisition() const { return m_acquisition; }

		static MsgReportSDRdaemonAcquisition* create(bool acquisition)
		{
			return new MsgReportSDRdaemonAcquisition(acquisition);
		}

	protected:
		bool m_acquisition;

		MsgReportSDRdaemonAcquisition(bool acquisition) :
			Message(),
			m_acquisition(acquisition)
		{ }
	};

	class MsgReportSDRdaemonStreamData : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getSampleRate() const { return m_sampleRate; }
		quint64 getCenterFrequency() const { return m_centerFrequency; }
		std::time_t getStartingTimeStamp() const { return m_startingTimeStamp; }

		static MsgReportSDRdaemonStreamData* create(int sampleRate, quint64 centerFrequency, std::time_t startingTimeStamp)
		{
			return new MsgReportSDRdaemonStreamData(sampleRate, centerFrequency, startingTimeStamp);
		}

	protected:
		int m_sampleRate;
		quint64 m_centerFrequency;
		std::time_t m_startingTimeStamp;

		MsgReportSDRdaemonStreamData(int sampleRate, quint64 centerFrequency, std::time_t startingTimeStamp) :
			Message(),
			m_sampleRate(sampleRate),
			m_centerFrequency(centerFrequency),
			m_startingTimeStamp(startingTimeStamp)
		{ }
	};

	class MsgReportSDRdaemonStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		std::size_t getSamplesCount() const { return m_samplesCount; }

		static MsgReportSDRdaemonStreamTiming* create(std::size_t samplesCount)
		{
			return new MsgReportSDRdaemonStreamTiming(samplesCount);
		}

	protected:
		std::size_t m_samplesCount;

		MsgReportSDRdaemonStreamTiming(std::size_t samplesCount) :
			Message(),
			m_samplesCount(samplesCount)
		{ }
	};

	SDRdaemonInput(const QTimer& masterTimer);
	virtual ~SDRdaemonInput();

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
	SDRdaemonThread* m_SDRdaemonThread;
	QString m_deviceDescription;
	QString m_fileName;
	int m_sampleRate;
	quint64 m_centerFrequency;
	std::time_t m_startingTimeStamp;
	const QTimer& m_masterTimer;

	bool applySettings(const Settings& settings, bool force);
};

#endif // INCLUDE_SDRDAEMONINPUT_H
