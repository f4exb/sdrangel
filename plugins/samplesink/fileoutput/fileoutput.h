///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FILEOUTPUT_H
#define INCLUDE_FILEOUTPUT_H

#include <QString>
#include <QTimer>
#include <QThread>
#include <QNetworkRequest>

#include <ctime>
#include <iostream>
#include <fstream>

#include "dsp/devicesamplesink.h"
#include "fileoutputsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class FileOutputWorker;
class DeviceAPI;

class FileOutput : public DeviceSampleSink {
public:
	class MsgConfigureFileOutput : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const FileOutputSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigureFileOutput* create(const FileOutputSettings& settings, const QList<QString>& settingsKeys, bool force) {
			return new MsgConfigureFileOutput(settings, settingsKeys, force);
		}

	private:
		FileOutputSettings m_settings;
        QList<QString> m_settingsKeys;
		bool m_force;

		MsgConfigureFileOutput(const FileOutputSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

	class MsgConfigureFileOutputName : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const QString& getFileName() const { return m_fileName; }

		static MsgConfigureFileOutputName* create(const QString& fileName)
		{
			return new MsgConfigureFileOutputName(fileName);
		}

	private:
		QString m_fileName;

		MsgConfigureFileOutputName(const QString& fileName) :
			Message(),
			m_fileName(fileName)
		{ }
	};

	class MsgConfigureFileOutputWork : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isWorking() const { return m_working; }

		static MsgConfigureFileOutputWork* create(bool working)
		{
			return new MsgConfigureFileOutputWork(working);
		}

	private:
		bool m_working;

		MsgConfigureFileOutputWork(bool working) :
			Message(),
			m_working(working)
		{ }
	};

	class MsgConfigureFileOutputStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:

		static MsgConfigureFileOutputStreamTiming* create()
		{
			return new MsgConfigureFileOutputStreamTiming();
		}

	private:

		MsgConfigureFileOutputStreamTiming() :
			Message()
		{ }
	};

	class MsgReportFileOutputGeneration : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool getAcquisition() const { return m_acquisition; }

		static MsgReportFileOutputGeneration* create(bool acquisition)
		{
			return new MsgReportFileOutputGeneration(acquisition);
		}

	protected:
		bool m_acquisition;

		MsgReportFileOutputGeneration(bool acquisition) :
			Message(),
			m_acquisition(acquisition)
		{ }
	};

	class MsgReportFileOutputStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		std::size_t getSamplesCount() const { return m_samplesCount; }

		static MsgReportFileOutputStreamTiming* create(std::size_t samplesCount)
		{
			return new MsgReportFileOutputStreamTiming(samplesCount);
		}

	protected:
		std::size_t m_samplesCount;

		MsgReportFileOutputStreamTiming(std::size_t samplesCount) :
			Message(),
			m_samplesCount(samplesCount)
		{ }
	};

	FileOutput(DeviceAPI *deviceAPI);
	virtual ~FileOutput();
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
	std::time_t getStartingTimeStamp() const;

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

    static void webapiFormatDeviceSettings(
            SWGSDRangel::SWGDeviceSettings& response,
            const FileOutputSettings& settings);

    static void webapiUpdateDeviceSettings(
            FileOutputSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

private:
    DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
	FileOutputSettings m_settings;
	std::ofstream m_ofstream;
	FileOutputWorker* m_fileOutputWorker;
    QThread m_fileOutputWorkerThread;
	QString m_deviceDescription;
	qint64 m_startingTimeStamp;
	const QTimer& m_masterTimer;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void startWorker();
    void stopWorker();
	void openFileStream();
	void applySettings(const FileOutputSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const FileOutputSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_FILEOUTPUT_H
