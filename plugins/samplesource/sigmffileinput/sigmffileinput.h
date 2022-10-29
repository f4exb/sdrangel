///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_SIGMFFILEINPUT_H
#define INCLUDE_SIGMFFILEINPUT_H

#include <ctime>
#include <iostream>
#include <fstream>

#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QThread>
#include <QNetworkRequest>

#include "dsp/sigmf_forward.h"

#include "dsp/devicesamplesource.h"
#include "sigmffileinputsettings.h"
#include "sigmffiledata.h"

class QNetworkAccessManager;
class QNetworkReply;
class SigMFFileInputWorker;
class DeviceAPI;

class SigMFFileInput : public DeviceSampleSource {
	Q_OBJECT
public:
    /**
     * Communicate settings
     */
	class MsgConfigureSigMFFileInput : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const SigMFFileInputSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigureSigMFFileInput* create(const SigMFFileInputSettings& settings, const QList<QString>& settingsKeys, bool force) {
			return new MsgConfigureSigMFFileInput(settings, settingsKeys, force);
		}

	private:
		SigMFFileInputSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

		MsgConfigureSigMFFileInput(const SigMFFileInputSettings& settings, const QList<QString>& settingsKeys, bool force) :
			Message(),
			m_settings(settings),
            m_settingsKeys(settingsKeys),
			m_force(force)
		{ }
	};

    /**
     * Start/stop track play
     */
	class MsgConfigureTrackWork : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isWorking() const { return m_working; }

		static MsgConfigureTrackWork* create(bool working) {
			return new MsgConfigureTrackWork(working);
		}

	private:
		bool m_working;

		MsgConfigureTrackWork(bool working) :
			Message(),
			m_working(working)
		{ }
	};


    /**
     * Start/stop full file play
     */
	class MsgConfigureFileWork : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isWorking() const { return m_working; }

		static MsgConfigureFileWork* create(bool working) {
			return new MsgConfigureFileWork(working);
		}

	private:
		bool m_working;

		MsgConfigureFileWork(bool working) :
			Message(),
			m_working(working)
		{ }
	};

    /**
     * Move to track
     */
	class MsgConfigureTrackIndex : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
        int getTrackIndex() const { return m_trackIndex; }

		static MsgConfigureTrackIndex* create(int trackIndex)
		{
			return new MsgConfigureTrackIndex(trackIndex);
		}

	private:
        int m_trackIndex;

		MsgConfigureTrackIndex(int trackIndex) :
			Message(),
            m_trackIndex(trackIndex)
		{ }
	};

    /**
     * Seek position in track
     */
	class MsgConfigureTrackSeek : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getMillis() const { return m_seekMillis; }

		static MsgConfigureTrackSeek* create(int seekMillis)
		{
			return new MsgConfigureTrackSeek(seekMillis);
		}

	protected:
		int m_seekMillis; //!< millis of seek position from the beginning 0..1000

		MsgConfigureTrackSeek(int seekMillis) :
			Message(),
			m_seekMillis(seekMillis)
		{ }
	};

    /**
     * Seek position in full file
     */
	class MsgConfigureFileSeek : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getMillis() const { return m_seekMillis; }

		static MsgConfigureFileSeek* create(int seekMillis)
		{
			return new MsgConfigureFileSeek(seekMillis);
		}

	protected:
		int m_seekMillis; //!< millis of seek position from the beginning 0..1000

		MsgConfigureFileSeek(int seekMillis) :
			Message(),
			m_seekMillis(seekMillis)
		{ }
	};

    /**
     * Pull stram timing information
     */
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

    /**
     * Start/stop plugin
     */
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

    /**
     * Push start/stop information
     */
	class MsgReportStartStop : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool getStartStop() const { return m_startStop; }

		static MsgReportStartStop* create(bool startStop)
		{
			return new MsgReportStartStop(startStop);
		}

	protected:
		bool m_startStop;

		MsgReportStartStop(bool startStop) :
			Message(),
			m_startStop(startStop)
		{ }
	};

    /**
     * Push meta data information
     */
    class MsgReportMetaData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SigMFFileMetaInfo& getMetaInfo() const { return m_metaInfo; }
        const QList<SigMFFileCapture>& getCaptures() { return m_captures; }

        static MsgReportMetaData* create(const SigMFFileMetaInfo& metaInfo, const QList<SigMFFileCapture>& captures) {
            return new MsgReportMetaData(metaInfo, captures);
        }

    protected:
        SigMFFileMetaInfo m_metaInfo;
        QList<SigMFFileCapture> m_captures;

        MsgReportMetaData(const SigMFFileMetaInfo& metaInfo, const QList<SigMFFileCapture>& captures) :
            Message(),
            m_metaInfo(metaInfo),
            m_captures(captures)
        {}
    };

    /**
     * Push track change
     */
    class MsgReportTrackChange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getTrackIndex() const { return m_trackIndex; }
        static MsgReportTrackChange* create(int trackIndex) {
            return new MsgReportTrackChange(trackIndex);
        }

    private:
        int m_trackIndex;
        MsgReportTrackChange(int trackIndex) :
            Message(),
            m_trackIndex(trackIndex)
        { }
    };

    /**
     * Push stream timing information
     */
	class MsgReportFileInputStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
        quint64 getSamplesCount() const { return m_samplesCount; }
        quint64 getTrackSamplesCount() const { return m_trackSamplesCount; }
        quint64 getTrackTimeStart() const { return m_trackTimeStart; }
        int getTrackNumber() const { return m_trackNumber; }

        static MsgReportFileInputStreamTiming* create(
            quint64 samplesCount,
            quint64 trackSamplesCount,
            quint64 trackTimeStart,
            int trackNumber
        )
		{
			return new MsgReportFileInputStreamTiming(
                samplesCount,
                trackSamplesCount,
                trackTimeStart,
                trackNumber
            );
		}

	protected:
        quint64 m_samplesCount;
        quint64 m_trackSamplesCount;
        quint64 m_trackTimeStart;
        int m_trackNumber;

        MsgReportFileInputStreamTiming(
            quint64 samplesCount,
            quint64 trackSamplesCount,
            quint64 trackTimeStart,
            int trackNumber
        ) :
			Message(),
			m_samplesCount(samplesCount),
            m_trackSamplesCount(trackSamplesCount),
            m_trackTimeStart(trackTimeStart),
            m_trackNumber(trackNumber)
		{ }
	};

    /**
     * Push CRC (SHA512) information
     */
	class MsgReportCRC : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isOK() const { return m_ok; }

		static MsgReportCRC* create(bool ok) {
			return new MsgReportCRC(ok);
		}

	protected:
		bool m_ok;

		MsgReportCRC(bool ok) :
			Message(),
			m_ok(ok)
		{ }
	};

    /**
     * Push record total check information
     */
	class MsgReportTotalSamplesCheck : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isOK() const { return m_ok; }

		static MsgReportTotalSamplesCheck* create(bool ok) {
			return new MsgReportTotalSamplesCheck(ok);
		}

	protected:
		bool m_ok;

		MsgReportTotalSamplesCheck(bool ok) :
			Message(),
			m_ok(ok)
		{ }
	};

	SigMFFileInput(DeviceAPI *deviceAPI);
	virtual ~SigMFFileInput();
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

    virtual int webapiActionsPost(
            const QStringList& deviceActionsKeys,
            SWGSDRangel::SWGDeviceActions& query,
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
            const SigMFFileInputSettings& settings);

    static void webapiUpdateDeviceSettings(
            SigMFFileInputSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

private:
	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
	SigMFFileInputSettings m_settings;
	std::ifstream m_metaStream;
    std::ifstream m_dataStream;
    SigMFFileMetaInfo m_metaInfo;
    QList<SigMFFileCapture> m_captures;
    std::vector<uint64_t> m_captureStarts;
    bool m_trackMode;
    int m_currentTrackIndex;
    bool m_recordOpen;
    bool m_crcAvailable;
    bool m_crcOK;
    bool m_recordLengthOK;
    QString m_recordSummary;
	SigMFFileInputWorker* m_fileInputWorker;
    QThread m_fileInputWorkerThread;
	QString m_deviceDescription;
	int m_sampleRate;
	unsigned int m_sampleBytes;
	quint64 m_centerFrequency;
    quint64 m_recordLength; //!< record length in seconds computed from file size
    quint64 m_startingTimeStamp;
	QTimer m_masterTimer;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void startWorker();
    void stopWorker();
	bool openFileStreams(const QString& fileName);
    void extractMeta(
        sigmf::SigMF<sigmf::Global<core::DescrT, sdrangel::DescrT>,
        sigmf::Capture<core::DescrT, sdrangel::DescrT>,
        sigmf::Annotation<core::DescrT> >* metaRecord,
        uint64_t dataFileSize
    );
    void extractCaptures(
        sigmf::SigMF<sigmf::Global<core::DescrT, sdrangel::DescrT>,
        sigmf::Capture<core::DescrT, sdrangel::DescrT>,
        sigmf::Annotation<core::DescrT> >* metaRecord
    );
    static void analyzeDataType(const std::string& dataTypeString, SigMFFileDataType& dataType);
    uint64_t getTrackSampleStart(unsigned int trackIndex);
    int getTrackIndex(uint64_t sampleIndex);
	void seekFileStream(uint64_t sampleIndex);
	void seekTrackMillis(int seekMillis);
    void seekFileMillis(int seekMillis);
	bool applySettings(const SigMFFileInputSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const SigMFFileInputSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_SIGMFFILEINPUT_H
