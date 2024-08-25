///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef INCLUDE_REMOTEOUTPUT_H
#define INCLUDE_REMOTEOUTPUT_H

#include <ctime>
#include <iostream>
#include <fstream>

#include <QObject>
#include <QString>
#include <QTimer>
#include <QNetworkRequest>
#include <QThread>

#include "dsp/devicesamplesink.h"

#include "remoteoutputsettings.h"

class RemoteOutputWorker;
class DeviceAPI;
class QNetworkAccessManager;
class QNetworkReply;
class QJsonObject;

class RemoteOutput : public DeviceSampleSink {
    Q_OBJECT
public:
	class MsgConfigureRemoteOutput : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const RemoteOutputSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigureRemoteOutput* create(const RemoteOutputSettings& settings, const QList<QString>& settingsKeys, bool force = false) {
			return new MsgConfigureRemoteOutput(settings, settingsKeys, force);
		}

	private:
		RemoteOutputSettings m_settings;
        QList<QString> m_settingsKeys;
		bool m_force;

		MsgConfigureRemoteOutput(const RemoteOutputSettings& settings, const QList<QString>& settingsKeys, bool force) :
			Message(),
			m_settings(settings),
            m_settingsKeys(settingsKeys),
			m_force(force)
		{ }
	};

	class MsgConfigureRemoteOutputWork : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isWorking() const { return m_working; }

		static MsgConfigureRemoteOutputWork* create(bool working)
		{
			return new MsgConfigureRemoteOutputWork(working);
		}

	private:
		bool m_working;

		explicit MsgConfigureRemoteOutputWork(bool working) :
			Message(),
			m_working(working)
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

    class MsgConfigureRemoteOutputChunkCorrection : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getChunkCorrection() const { return m_chunkCorrection; }

        static MsgConfigureRemoteOutputChunkCorrection* create(int chunkCorrection)
        {
            return new MsgConfigureRemoteOutputChunkCorrection(chunkCorrection);
        }

    private:
        int m_chunkCorrection;

        explicit MsgConfigureRemoteOutputChunkCorrection(int chunkCorrection) :
            Message(),
            m_chunkCorrection(chunkCorrection)
        { }
    };

    class MsgReportRemoteData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        struct RemoteData
        {
            quint64 m_centerFrequency; //!< Center frequency where the stream is placed
            int m_sampleRate;          //!< Sample rate requested by remote
            int m_queueSize;           //!< Remote data read queue size
            int m_queueLength;         //!< Remote data queue length
            int m_unrecoverableCount;  //!< Number of unrecoverable errors from FEC
            int m_recoverableCount;    //!< Number of recoverable errors from FEC
            uint64_t m_timestampUs;    //!< Unix time stamp of request in microseconds
            uint32_t m_sampleCount;    //!< Number of samples processed in the remote at request time
        };

        const RemoteData& getData() const { return m_remoteData; }

        static MsgReportRemoteData* create(const RemoteData& remoteData) {
            return new MsgReportRemoteData(remoteData);
        }

    private:
        RemoteData m_remoteData;

        explicit MsgReportRemoteData(const RemoteData& remoteData) :
            Message(),
            m_remoteData(remoteData)
        {}
    };

    class MsgReportRemoteFixedData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        struct RemoteData
        {
            QString m_version;      //!< Remote SDRangel version
            QString m_qtVersion;    //!< Remote Qt version used to build SDRangel
            QString m_architecture; //!< Remote CPU architecture
            QString m_os;           //!< Remote O/S
            int     m_rxBits;       //!< Number of bits used for I or Q sample on Rx side
            int     m_txBits;       //!< Number of bits used for I or Q sample on Tx side
        };

        const RemoteData& getData() const { return m_remoteData; }

        static MsgReportRemoteFixedData* create(const RemoteData& remoteData) {
            return new MsgReportRemoteFixedData(remoteData);
        }

    private:
        RemoteData m_remoteData;

        explicit MsgReportRemoteFixedData(const RemoteData& remoteData) :
            Message(),
            m_remoteData(remoteData)
        {}
    };

    class MsgRequestFixedData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgRequestFixedData* create() {
            return new MsgRequestFixedData();
        }

    private:
        MsgRequestFixedData() :
            Message()
        {}
    };


	explicit RemoteOutput(DeviceAPI *deviceAPI);
	~RemoteOutput() final;
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
    void setCenterFrequency(qint64 centerFrequency) final { (void) centerFrequency; }
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

    int webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
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
        const RemoteOutputSettings& settings);

    static void webapiUpdateDeviceSettings(
        RemoteOutputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response);

private:
    DeviceAPI *m_deviceAPI;
	QRecursiveMutex m_mutex;
    bool m_running = false;
	RemoteOutputSettings m_settings;
	uint64_t m_centerFrequency = 435000000;
    int m_sampleRate = 48000;
	RemoteOutputWorker* m_remoteOutputWorker = nullptr;
    QThread m_remoteOutputWorkerThread;
	QString m_deviceDescription = "RemoteOutput";
	std::time_t m_startingTimeStamp = 0;
	const QTimer& m_masterTimer;
	uint32_t m_tickCount = 0; // for 50 ms timer
    uint32_t m_greaterTickCount = 0; // for 1 s derived timer
    uint32_t m_tickMultiplier = 1; // for greater tick count
    int m_queueLength = 0;
    int m_queueSize = 0;
    int m_recoverableCount = 0;
    int m_unrecoverableCount = 0;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void startWorker();
    void stopWorker();
	void applySettings(const RemoteOutputSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void applyCenterFrequency();
    void applySampleRate();
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response) const;

    void analyzeApiReply(const QJsonObject& jsonObject, const QString& answer);
    void queueLengthCompensation(
        int nominalSR,
        int queueLength,
        int queueSize
    );
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const RemoteOutputSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void tick();
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_REMOTEOUTPUT_H
