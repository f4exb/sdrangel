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
#include "filesourcereport.h"

class QNetworkAccessManager;
class QNetworkReply;

class DeviceAPI;
class FileSourceBaseband;
class ObjectPipe;

class FileSource : public BasebandSampleSource, public ChannelAPI {
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

    FileSource(DeviceAPI *deviceAPI);
    virtual ~FileSource();
    virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }

    virtual void start();
    virtual void stop();
    virtual void pull(SampleVector::iterator& begin, unsigned int nbSamples);
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSourceName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return 0; }
    virtual void setCenterFrequency(qint64) {}

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

    virtual int webapiWorkspaceGet(
            SWGSDRangel::SWGWorkspaceInfo& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage);

    virtual int webapiActionsPost(
            const QStringList& channelActionsKeys,
            SWGSDRangel::SWGChannelActions& query,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const FileSourceSettings& settings);

    static void webapiUpdateChannelSettings(
            FileSourceSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    /** Set center frequency given in Hz */
    void setCenterFrequency(uint64_t centerFrequency) { m_frequencyOffset = centerFrequency; }

    /** Set sample rate given in Hz */
    void setSampleRate(uint32_t sampleRate) { m_basebandSampleRate = sampleRate; }

    double getMagSq() const;
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) const;
    void setMessageQueueToGUI(MessageQueue* queue) override;
    uint32_t getNumberOfDeviceStreams() const;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    FileSourceBaseband* m_basebandSource;
    FileSourceSettings m_settings;

    SampleVector m_sampleBuffer;
    QMutex m_settingsMutex;
    uint64_t m_frequencyOffset;
    uint32_t m_basebandSampleRate;
    double m_linearGain;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const FileSourceSettings& settings, bool force = false);
    static void validateFilterChainHash(FileSourceSettings& settings);
    void calculateFrequencyOffset();
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const FileSourceSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const FileSourceSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const FileSourceSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // PLUGINS_CHANNELTX_FILESOURCE_FILESOURCE_H_
