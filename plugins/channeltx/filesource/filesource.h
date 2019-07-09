///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_FILESOURCE_FILESOURCE_H_
#define PLUGINS_CHANNELTX_FILESOURCE_FILESOURCE_H_

#include <ctime>
#include <iostream>
#include <fstream>

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "channel/channelapi.h"
#include "util/message.h"
#include "util/movingaverage.h"
#include "filesourcesettings.h"

class ThreadedBasebandSampleSource;
class UpChannelizer;
class DeviceAPI;
class FileSourceThread;
class QNetworkAccessManager;
class QNetworkReply;

class FileSource : public BasebandSampleSource, public ChannelAPI {
    Q_OBJECT

public:
    class MsgConfigureChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getLog2Interp() const { return m_log2Interp; }
        int getFilterChainHash() const { return m_filterChainHash; }

        static MsgConfigureChannelizer* create(unsigned int m_log2Interp, unsigned int m_filterChainHash) {
            return new MsgConfigureChannelizer(m_log2Interp, m_filterChainHash);
        }

    private:
        unsigned int m_log2Interp;
        unsigned int m_filterChainHash;

        MsgConfigureChannelizer(unsigned int log2Interp, unsigned int filterChainHash) :
            Message(),
            m_log2Interp(log2Interp),
            m_filterChainHash(filterChainHash)
        { }
    };

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

    class MsgSampleRateNotification : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgSampleRateNotification* create(int sampleRate) {
            return new MsgSampleRateNotification(sampleRate);
        }

        int getSampleRate() const { return m_sampleRate; }

    private:

        MsgSampleRateNotification(int sampleRate) :
            Message(),
            m_sampleRate(sampleRate)
        { }

        int m_sampleRate;
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

    FileSource(DeviceAPI *deviceAPI);
    ~FileSource();

    virtual void destroy() { delete this; }

    virtual void pull(Sample& sample);
    virtual void pullAudio(int nbSamples);
    virtual void start();
    virtual void stop();
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return 0; }

    virtual int getNbSinkStreams() const { return 0; }
    virtual int getNbSourceStreams() const { return 1; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return 0;
    }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage);

    /** Set center frequency given in Hz */
    void setCenterFrequency(uint64_t centerFrequency) { m_centerFrequency = centerFrequency; }

    /** Set sample rate given in Hz */
    void setSampleRate(uint32_t sampleRate) { m_sampleRate = sampleRate; }

    quint64 getSamplesCount() const { return m_samplesCount; }
    double getMagSq() const { return m_magsq; }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples)
    {
        if (m_magsqCount > 0)
        {
            m_magsq = m_magsqSum / m_magsqCount;
            m_magSqLevelStore.m_magsq = m_magsq;
            m_magSqLevelStore.m_magsqPeak = m_magsqPeak;
        }

        avg = m_magSqLevelStore.m_magsq;
        peak = m_magSqLevelStore.m_magsqPeak;
        nbSamples = m_magsqCount == 0 ? 1 : m_magsqCount;

        m_magsqSum = 0.0f;
        m_magsqPeak = 0.0f;
        m_magsqCount = 0;
    }

    static const QString m_channelIdURI;
    static const QString m_channelId;

private:
    struct MagSqLevelsStore
    {
        MagSqLevelsStore() :
            m_magsq(1e-12),
            m_magsqPeak(1e-12)
        {}
        double m_magsq;
        double m_magsqPeak;
    };

    DeviceAPI* m_deviceAPI;
    QMutex m_mutex;
    ThreadedBasebandSampleSource* m_threadedChannelizer;
    UpChannelizer* m_channelizer;
    FileSourceSettings m_settings;
	std::ifstream m_ifstream;
	QString m_fileName;
	quint32 m_sampleSize;
	quint64 m_centerFrequency;
    int64_t m_frequencyOffset;
    uint32_t m_fileSampleRate;
    quint64 m_samplesCount;
    uint32_t m_sampleRate;
    uint32_t m_deviceSampleRate;
    quint64 m_recordLength; //!< record length in seconds computed from file size
    quint64 m_startingTimeStamp;
	QTimer m_masterTimer;
    bool m_running;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    double m_linearGain;
	double m_magsq;
	double m_magsqSum;
	double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;
	MovingAverageUtil<Real, double, 16> m_movingAverage;

	void openFileStream();
	void seekFileStream(int seekMillis);
    void handleEOF();
    void applySettings(const FileSourceSettings& settings, bool force = false);
    void validateFilterChainHash(FileSourceSettings& settings);
    void calculateFrequencyOffset();
    void webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const FileSourceSettings& settings);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const FileSourceSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleData();
};

#endif // PLUGINS_CHANNELTX_FILESOURCE_FILESOURCE_H_
