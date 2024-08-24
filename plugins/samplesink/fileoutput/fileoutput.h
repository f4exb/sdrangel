///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2017, 2019-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2021 Andreas Baulig <free.geronimo@hotmail.de>                  //
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

    private:
        bool m_startStop;

        explicit MsgStartStop(bool startStop) :
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

		explicit MsgConfigureFileOutputName(const QString& fileName) :
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

		explicit MsgConfigureFileOutputWork(bool working) :
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

	private:
		bool m_acquisition;

		explicit MsgReportFileOutputGeneration(bool acquisition) :
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

	private:
		std::size_t m_samplesCount;

		explicit MsgReportFileOutputStreamTiming(std::size_t samplesCount) :
			Message(),
			m_samplesCount(samplesCount)
		{ }
	};

	explicit FileOutput(DeviceAPI *deviceAPI);
	~FileOutput() final;
	void destroy() final;

    void init() final;
	bool start() final;
	void stop() final;

    QByteArray serialize() const final;
    bool deserialize(const QByteArray& data) final;

    void setMessageQueueToGUI(MessageQueue *queue) final { m_guiMessageQueue = queue; }
	const QString& getDeviceDescription() const final;
	int getSampleRate() const final;
    void setSampleRate(int sampleRate) final { (void) sampleRate; }
	quint64 getCenterFrequency() const final;
    void setCenterFrequency(qint64 centerFrequency) final;
	std::time_t getStartingTimeStamp() const;

	bool handleMessage(const Message& message) final;

	int webapiSettingsGet(
        SWGSDRangel::SWGDeviceSettings& response,
        QString& errorMessage) final;

	int webapiSettingsPutPatch(
        bool force,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response, // query + response
        QString& errorMessage) final;

    int webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage) final;

    int webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage) final;

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
    bool m_running = false;
	FileOutputSettings m_settings;
	std::ofstream m_ofstream;
	FileOutputWorker* m_fileOutputWorker = nullptr;
    QThread m_fileOutputWorkerThread;
	QString m_deviceDescription;
	qint64 m_startingTimeStamp = 0;
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
    void networkManagerFinished(QNetworkReply *reply) const;
};

#endif // INCLUDE_FILEOUTPUT_H
