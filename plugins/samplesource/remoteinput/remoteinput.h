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

#ifndef INCLUDE_REMOTEINPUT_H
#define INCLUDE_REMOTEINPUT_H

#include <ctime>
#include <iostream>
#include <stdint.h>

#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QNetworkRequest>
#include <QDateTime>

#include "dsp/devicesamplesource.h"

#include "remoteinputsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class RemoteInputUDPHandler;

class RemoteInput : public DeviceSampleSource {
    Q_OBJECT
public:
    class MsgConfigureRemoteInput : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RemoteInputSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureRemoteInput* create(const RemoteInputSettings& settings, bool force = false)
        {
            return new MsgConfigureRemoteInput(settings, force);
        }

    private:
        RemoteInputSettings m_settings;
        bool m_force;

        MsgConfigureRemoteInput(const RemoteInputSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

	class MsgConfigureRemoteInputTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:

		static MsgConfigureRemoteInputTiming* create()
		{
			return new MsgConfigureRemoteInputTiming();
		}

	private:

		MsgConfigureRemoteInputTiming() :
			Message()
		{ }
	};

	class MsgReportRemoteInputAcquisition : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool getAcquisition() const { return m_acquisition; }

		static MsgReportRemoteInputAcquisition* create(bool acquisition)
		{
			return new MsgReportRemoteInputAcquisition(acquisition);
		}

	protected:
		bool m_acquisition;

		MsgReportRemoteInputAcquisition(bool acquisition) :
			Message(),
			m_acquisition(acquisition)
		{ }
	};

	class MsgReportRemoteInputStreamData : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getSampleRate() const { return m_sampleRate; }
		quint64 getCenterFrequency() const { return m_centerFrequency; }
		uint32_t get_tv_msec() const { return m_tv_msec; }

		static MsgReportRemoteInputStreamData* create(int sampleRate, quint64 centerFrequency, uint64_t tv_msec)
		{
			return new MsgReportRemoteInputStreamData(sampleRate, centerFrequency, tv_msec);
		}

	protected:
		int m_sampleRate;
		quint64 m_centerFrequency;
		uint64_t m_tv_msec;

		MsgReportRemoteInputStreamData(int sampleRate, quint64 centerFrequency, uint64_t tv_msec) :
			Message(),
			m_sampleRate(sampleRate),
			m_centerFrequency(centerFrequency),
			m_tv_msec(tv_msec)
		{ }
	};

	class MsgReportRemoteInputStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		uint64_t get_tv_msec() const { return m_tv_msec; }
		int getFramesDecodingStatus() const { return m_framesDecodingStatus; }
		bool allBlocksReceived() const { return m_allBlocksReceived; }
		float getBufferLengthInSecs() const { return m_bufferLenSec; }
        int32_t getBufferGauge() const { return m_bufferGauge; }
        int getMinNbBlocks() const { return m_minNbBlocks; }
        int getMinNbOriginalBlocks() const { return m_minNbOriginalBlocks; }
        int getMaxNbRecovery() const { return m_maxNbRecovery; }
        float getAvgNbBlocks() const { return m_avgNbBlocks; }
        float getAvgNbOriginalBlocks() const { return m_avgNbOriginalBlocks; }
        float getAvgNbRecovery() const { return m_avgNbRecovery; }
        int getNbOriginalBlocksPerFrame() const { return m_nbOriginalBlocksPerFrame; }
        int getNbFECBlocksPerFrame() const { return m_nbFECBlocksPerFrame; }
        int getSampleBits() const { return m_sampleBits; }
        int getSampleBytes() const { return m_sampleBytes; }

		static MsgReportRemoteInputStreamTiming* create(uint64_t tv_msec,
				float bufferLenSec,
                int32_t bufferGauge,
                int framesDecodingStatus,
                bool allBlocksReceived,
                int minNbBlocks,
                int minNbOriginalBlocks,
                int maxNbRecovery,
                float avgNbBlocks,
                float avgNbOriginalBlocks,
                float avgNbRecovery,
                int nbOriginalBlocksPerFrame,
                int nbFECBlocksPerFrame,
                int sampleBits,
                int sampleBytes)
		{
			return new MsgReportRemoteInputStreamTiming(tv_msec,
					bufferLenSec,
                    bufferGauge,
                    framesDecodingStatus,
                    allBlocksReceived,
                    minNbBlocks,
                    minNbOriginalBlocks,
                    maxNbRecovery,
                    avgNbBlocks,
                    avgNbOriginalBlocks,
                    avgNbRecovery,
                    nbOriginalBlocksPerFrame,
                    nbFECBlocksPerFrame,
                    sampleBits,
                    sampleBytes);
		}

	protected:
		uint64_t m_tv_msec;
		int      m_framesDecodingStatus;
		bool     m_allBlocksReceived;
		float    m_bufferLenSec;
        int32_t  m_bufferGauge;
        int      m_minNbBlocks;
        int      m_minNbOriginalBlocks;
        int      m_maxNbRecovery;
        float    m_avgNbBlocks;
        float    m_avgNbOriginalBlocks;
        float    m_avgNbRecovery;
        int      m_nbOriginalBlocksPerFrame;
        int      m_nbFECBlocksPerFrame;
        int      m_sampleBits;
        int      m_sampleBytes;

		MsgReportRemoteInputStreamTiming(uint64_t tv_msec,
				float bufferLenSec,
                int32_t bufferGauge,
                int framesDecodingStatus,
                bool allBlocksReceived,
                int minNbBlocks,
                int minNbOriginalBlocks,
                int maxNbRecovery,
                float avgNbBlocks,
                float avgNbOriginalBlocks,
                float avgNbRecovery,
                int nbOriginalBlocksPerFrame,
                int nbFECBlocksPerFrame,
                int sampleBits,
                int sampleBytes) :
			Message(),
			m_tv_msec(tv_msec),
			m_framesDecodingStatus(framesDecodingStatus),
			m_allBlocksReceived(allBlocksReceived),
			m_bufferLenSec(bufferLenSec),
            m_bufferGauge(bufferGauge),
            m_minNbBlocks(minNbBlocks),
            m_minNbOriginalBlocks(minNbOriginalBlocks),
            m_maxNbRecovery(maxNbRecovery),
            m_avgNbBlocks(avgNbBlocks),
            m_avgNbOriginalBlocks(avgNbOriginalBlocks),
            m_avgNbRecovery(avgNbRecovery),
            m_nbOriginalBlocksPerFrame(nbOriginalBlocksPerFrame),
            m_nbFECBlocksPerFrame(nbFECBlocksPerFrame),
            m_sampleBits(sampleBits),
            m_sampleBytes(sampleBytes)
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

	RemoteInput(DeviceAPI *deviceAPI);
	virtual ~RemoteInput();
	virtual void destroy();

    virtual void init();
	virtual bool start();
	virtual void stop();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue);
	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
    virtual void setSampleRate(int sampleRate) { (void) sampleRate; }
	virtual quint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);
    QDateTime getStartingDateTime() const;
    bool isStreaming() const;

    virtual bool handleMessage(const Message &message);

    virtual int webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage);

    virtual int webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGDeviceReport& response,
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
            const RemoteInputSettings& settings);

    static void webapiUpdateDeviceSettings(
            RemoteInputSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

private:
	DeviceAPI *m_deviceAPI;
    int m_sampleRate;
	QMutex m_mutex;
	RemoteInputSettings m_settings;
	RemoteInputUDPHandler* m_remoteInputUDPHandler;
    QString m_remoteAddress;
	QString m_deviceDescription;
    QDateTime m_startingDateTime;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void applySettings(const RemoteInputSettings& settings, bool force = false);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);
    void webapiReverseSendSettings(QList<QString>& deviceSettingsKeys, const RemoteInputSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_REMOTEINPUT_H
