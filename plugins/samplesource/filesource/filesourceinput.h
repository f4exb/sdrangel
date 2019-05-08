///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FILESOURCEINPUT_H
#define INCLUDE_FILESOURCEINPUT_H

#include <ctime>
#include <iostream>
#include <fstream>

#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QNetworkRequest>

#include "dsp/devicesamplesource.h"
#include "filesourcesettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class FileSourceThread;
class DeviceAPI;

class FileSourceInput : public DeviceSampleSource {
	Q_OBJECT
public:
	class MsgConfigureFileSource : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const FileSourceSettings& getSettings() const { return m_settings; }
		bool getForce() const { return m_force; }

		static MsgConfigureFileSource* create(const FileSourceSettings& settings, bool force)
		{
			return new MsgConfigureFileSource(settings, force);
		}

	private:
		FileSourceSettings m_settings;
        bool m_force;

		MsgConfigureFileSource(const FileSourceSettings& settings, bool force) :
			Message(),
			m_settings(settings),
			m_force(force)
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
		int getMillis() const { return m_seekMillis; }

		static MsgConfigureFileSourceSeek* create(int seekMillis)
		{
			return new MsgConfigureFileSourceSeek(seekMillis);
		}

	protected:
		int m_seekMillis; //!< millis of seek position from the beginning 0..1000

		MsgConfigureFileSourceSeek(int seekMillis) :
			Message(),
			m_seekMillis(seekMillis)
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

    class MsgPlayPause : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getPlayPause() const { return m_playPause; }

        static MsgPlayPause* create(bool playPause) {
            return new MsgPlayPause(playPause);
        }

    protected:
        bool m_playPause;

        MsgPlayPause(bool playPause) :
            Message(),
            m_playPause(playPause)
        { }
    };

	class MsgReportFileSourceStreamData : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getSampleRate() const { return m_sampleRate; }
		quint32 getSampleSize() const { return m_sampleSize; }
		quint64 getCenterFrequency() const { return m_centerFrequency; }
        quint64 getStartingTimeStamp() const { return m_startingTimeStamp; }
        quint64 getRecordLength() const { return m_recordLength; }

		static MsgReportFileSourceStreamData* create(int sampleRate,
		        quint32 sampleSize,
				quint64 centerFrequency,
                quint64 startingTimeStamp,
                quint64 recordLength)
		{
			return new MsgReportFileSourceStreamData(sampleRate, sampleSize, centerFrequency, startingTimeStamp, recordLength);
		}

	protected:
		int m_sampleRate;
		quint32 m_sampleSize;
		quint64 m_centerFrequency;
        quint64 m_startingTimeStamp;
        quint64 m_recordLength;

		MsgReportFileSourceStreamData(int sampleRate,
		        quint32 sampleSize,
				quint64 centerFrequency,
                quint64 startingTimeStamp,
                quint64 recordLength) :
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
        quint64 getSamplesCount() const { return m_samplesCount; }

        static MsgReportFileSourceStreamTiming* create(quint64 samplesCount)
		{
			return new MsgReportFileSourceStreamTiming(samplesCount);
		}

	protected:
        quint64 m_samplesCount;

        MsgReportFileSourceStreamTiming(quint64 samplesCount) :
			Message(),
			m_samplesCount(samplesCount)
		{ }
	};

	class MsgReportHeaderCRC : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isOK() const { return m_ok; }

		static MsgReportHeaderCRC* create(bool ok) {
			return new MsgReportHeaderCRC(ok);
		}

	protected:
		bool m_ok;

		MsgReportHeaderCRC(bool ok) :
			Message(),
			m_ok(ok)
		{ }
	};

	FileSourceInput(DeviceAPI *deviceAPI);
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
    virtual void setSampleRate(int sampleRate) { (void) sampleRate; }
	virtual quint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);
    quint64 getStartingTimeStamp() const;

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

    virtual int webapiReportGet(
            SWGSDRangel::SWGDeviceReport& response,
            QString& errorMessage);

	private:
	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
	FileSourceSettings m_settings;
	std::ifstream m_ifstream;
	FileSourceThread* m_fileSourceThread;
	QString m_deviceDescription;
	QString m_fileName;
	int m_sampleRate;
	quint32 m_sampleSize;
	quint64 m_centerFrequency;
    quint64 m_recordLength; //!< record length in seconds computed from file size
    quint64 m_startingTimeStamp;
	const QTimer& m_masterTimer;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

	void openFileStream();
	void seekFileStream(int seekMillis);
	bool applySettings(const FileSourceSettings& settings, bool force = false);
    void webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const FileSourceSettings& settings);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);
    void webapiReverseSendSettings(QList<QString>& deviceSettingsKeys, const FileSourceSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_FILESOURCEINPUT_H
