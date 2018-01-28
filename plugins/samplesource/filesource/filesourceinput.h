///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FILESOURCEINPUT_H
#define INCLUDE_FILESOURCEINPUT_H

#include <QString>
#include <QByteArray>
#include <QTimer>
#include <ctime>
#include <iostream>
#include <fstream>

#include <dsp/devicesamplesource.h>
#include "filesourcesettings.h"

class FileSourceThread;
class DeviceSourceAPI;

class FileSourceInput : public DeviceSampleSource {
public:
	class MsgConfigureFileSource : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const FileSourceSettings& getSettings() const { return m_settings; }

		static MsgConfigureFileSource* create(const FileSourceSettings& settings)
		{
			return new MsgConfigureFileSource(settings);
		}

	private:
		FileSourceSettings m_settings;

		MsgConfigureFileSource(const FileSourceSettings& settings) :
			Message(),
			m_settings(settings)
		{ }
	};

	class MsgConfigureFileSourceName : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const QString& getFileName() const { return m_fileName; }

		static MsgConfigureFileSourceName* create(const QString& fileName)
		{
			return new MsgConfigureFileSourceName(fileName);
		}

	private:
		QString m_fileName;

		MsgConfigureFileSourceName(const QString& fileName) :
			Message(),
			m_fileName(fileName)
		{ }
	};

	class MsgConfigureFileSourceWork : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isWorking() const { return m_working; }

		static MsgConfigureFileSourceWork* create(bool working)
		{
			return new MsgConfigureFileSourceWork(working);
		}

	private:
		bool m_working;

		MsgConfigureFileSourceWork(bool working) :
			Message(),
			m_working(working)
		{ }
	};

	class MsgConfigureFileSourceStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:

		static MsgConfigureFileSourceStreamTiming* create()
		{
			return new MsgConfigureFileSourceStreamTiming();
		}

	private:

		MsgConfigureFileSourceStreamTiming() :
			Message()
		{ }
	};

	class MsgConfigureFileSourceSeek : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getPercentage() const { return m_seekPercentage; }

		static MsgConfigureFileSourceSeek* create(int seekPercentage)
		{
			return new MsgConfigureFileSourceSeek(seekPercentage);
		}

	protected:
		int m_seekPercentage; //!< percentage of seek position from the beginning 0..100

		MsgConfigureFileSourceSeek(int seekPercentage) :
			Message(),
			m_seekPercentage(seekPercentage)
		{ }
	};

	class MsgReportFileSourceAcquisition : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool getAcquisition() const { return m_acquisition; }

		static MsgReportFileSourceAcquisition* create(bool acquisition)
		{
			return new MsgReportFileSourceAcquisition(acquisition);
		}

	protected:
		bool m_acquisition;

		MsgReportFileSourceAcquisition(bool acquisition) :
			Message(),
			m_acquisition(acquisition)
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

	class MsgReportFileSourceStreamData : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getSampleRate() const { return m_sampleRate; }
		quint32 getSampleSize() const { return m_sampleSize; }
		quint64 getCenterFrequency() const { return m_centerFrequency; }
		std::time_t getStartingTimeStamp() const { return m_startingTimeStamp; }
		quint32 getRecordLength() const { return m_recordLength; }

		static MsgReportFileSourceStreamData* create(int sampleRate,
		        quint32 sampleSize,
				quint64 centerFrequency,
				std::time_t startingTimeStamp,
				quint32 recordLength)
		{
			return new MsgReportFileSourceStreamData(sampleRate, sampleSize, centerFrequency, startingTimeStamp, recordLength);
		}

	protected:
		int m_sampleRate;
		quint32 m_sampleSize;
		quint64 m_centerFrequency;
		std::time_t m_startingTimeStamp;
		quint32 m_recordLength;

		MsgReportFileSourceStreamData(int sampleRate,
		        quint32 sampleSize,
				quint64 centerFrequency,
				std::time_t startingTimeStamp,
				quint32 recordLength) :
			Message(),
			m_sampleRate(sampleRate),
			m_sampleSize(sampleSize),
			m_centerFrequency(centerFrequency),
			m_startingTimeStamp(startingTimeStamp),
			m_recordLength(recordLength)
		{ }
	};

	class MsgReportFileSourceStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		std::size_t getSamplesCount() const { return m_samplesCount; }

		static MsgReportFileSourceStreamTiming* create(std::size_t samplesCount)
		{
			return new MsgReportFileSourceStreamTiming(samplesCount);
		}

	protected:
		std::size_t m_samplesCount;

		MsgReportFileSourceStreamTiming(std::size_t samplesCount) :
			Message(),
			m_samplesCount(samplesCount)
		{ }
	};

	FileSourceInput(DeviceSourceAPI *deviceAPI);
	virtual ~FileSourceInput();
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

	virtual int webapiSettingsGet(
	            SWGSDRangel::SWGDeviceSettings& response,
	            QString& errorMessage);

    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

	private:
	DeviceSourceAPI *m_deviceAPI;
	QMutex m_mutex;
	FileSourceSettings m_settings;
	std::ifstream m_ifstream;
	FileSourceThread* m_fileSourceThread;
	QString m_deviceDescription;
	QString m_fileName;
	int m_sampleRate;
	quint32 m_sampleSize;
	quint64 m_centerFrequency;
	quint32 m_recordLength; //!< record length in seconds computed from file size
	std::time_t m_startingTimeStamp;
	const QTimer& m_masterTimer;

	void openFileStream();
	void seekFileStream(int seekPercentage);
};

#endif // INCLUDE_FILESOURCEINPUT_H
