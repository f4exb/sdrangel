///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB                              //
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

#ifndef INCLUDE_FILEINPUT_H
#define INCLUDE_FILEINPUT_H

#include <ctime>
#include <iostream>
#include <fstream>

#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QNetworkRequest>

#include "dsp/devicesamplesource.h"
#include "fileinputsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class FileInputWorker;
class DeviceAPI;

class FileInput : public DeviceSampleSource {
	Q_OBJECT
public:
	class MsgConfigureFileInput : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const FileInputSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigureFileInput* create(const FileInputSettings& settings, const QList<QString>& settingsKeys, bool force) {
			return new MsgConfigureFileInput(settings, settingsKeys, force);
		}

	private:
		FileInputSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

		MsgConfigureFileInput(const FileInputSettings& settings, const QList<QString>& settingsKeys, bool force) :
			Message(),
			m_settings(settings),
            m_settingsKeys(settingsKeys),
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

	class MsgConfigureFileInputWork : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isWorking() const { return m_working; }

		static MsgConfigureFileInputWork* create(bool working)
		{
			return new MsgConfigureFileInputWork(working);
		}

	private:
		bool m_working;

		MsgConfigureFileInputWork(bool working) :
			Message(),
			m_working(working)
		{ }
	};

	class MsgConfigureFileInputStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:

		static MsgConfigureFileInputStreamTiming* create()
		{
			return new MsgConfigureFileInputStreamTiming();
		}

	private:

		MsgConfigureFileInputStreamTiming() :
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

	class MsgReportFileInputStreamData : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getSampleRate() const { return m_sampleRate; }
		quint32 getSampleSize() const { return m_sampleSize; }
		quint64 getCenterFrequency() const { return m_centerFrequency; }
        quint64 getStartingTimeStamp() const { return m_startingTimeStamp; }
        quint64 getRecordLengthMuSec() const { return m_recordLengthMuSec; }

		static MsgReportFileInputStreamData* create(int sampleRate,
		        quint32 sampleSize,
				quint64 centerFrequency,
                quint64 startingTimeStamp,
                quint64 recordLength)
		{
			return new MsgReportFileInputStreamData(sampleRate, sampleSize, centerFrequency, startingTimeStamp, recordLength);
		}

	protected:
		int m_sampleRate;
		quint32 m_sampleSize;
		quint64 m_centerFrequency;
        quint64 m_startingTimeStamp;
        quint64 m_recordLengthMuSec;

		MsgReportFileInputStreamData(int sampleRate,
		        quint32 sampleSize,
				quint64 centerFrequency,
                quint64 startingTimeStamp,
                quint64 recordLengthMuSec) :
			Message(),
			m_sampleRate(sampleRate),
			m_sampleSize(sampleSize),
			m_centerFrequency(centerFrequency),
			m_startingTimeStamp(startingTimeStamp),
			m_recordLengthMuSec(recordLengthMuSec)
		{ }
	};

	class MsgReportFileInputStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
        quint64 getSamplesCount() const { return m_samplesCount; }

        static MsgReportFileInputStreamTiming* create(quint64 samplesCount)
		{
			return new MsgReportFileInputStreamTiming(samplesCount);
		}

	protected:
        quint64 m_samplesCount;

        MsgReportFileInputStreamTiming(quint64 samplesCount) :
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

	FileInput(DeviceAPI *deviceAPI);
	virtual ~FileInput();
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

    static void webapiFormatDeviceSettings(
            SWGSDRangel::SWGDeviceSettings& response,
            const FileInputSettings& settings);

    static void webapiUpdateDeviceSettings(
            FileInputSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

	private:
	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
	FileInputSettings m_settings;
	std::ifstream m_ifstream;
	FileInputWorker* m_fileInputWorker;
	QThread m_fileInputWorkerThread;
	QString m_deviceDescription;
	int m_sampleRate;
	quint32 m_sampleSize;
	quint64 m_centerFrequency;
    quint64 m_recordLengthMuSec; //!< record length in microseconds computed from file size
    quint64 m_startingTimeStamp;
	QTimer m_masterTimer;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

	void startWorker();
	void stopWorker();
	void openFileStream();
	void seekFileStream(int seekMillis);
	bool applySettings(const FileInputSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const FileInputSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);
	bool handleMessage(const Message& message);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_FILESOURCEINPUT_H
