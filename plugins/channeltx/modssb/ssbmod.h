///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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

#ifndef PLUGINS_CHANNELTX_MODSSB_SSBMOD_H_
#define PLUGINS_CHANNELTX_MODSSB_SSBMOD_H_

#include <vector>
#include <iostream>
#include <fstream>

#include <QRecursiveMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "dsp/spectrumvis.h"
#include "dsp/cwkeyer.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "ssbmodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class CWKeyer;
class SSBModBaseband;
class ObjectPipe;

class SSBMod : public BasebandSampleSource, public ChannelAPI {
public:
    class MsgConfigureSSBMod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SSBModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureSSBMod* create(const SSBModSettings& settings, bool force)
        {
            return new MsgConfigureSSBMod(settings, force);
        }

    private:
        SSBModSettings m_settings;
        bool m_force;

        MsgConfigureSSBMod(const SSBModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgConfigureFileSourceName : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getFileName() const { return m_fileName; }

        static MsgConfigureFileSourceName* create(const QString& fileName)
        {
            return new MsgConfigureFileSourceName(fileName);
        }

    private:
        QString m_fileName;

        explicit MsgConfigureFileSourceName(const QString& fileName) :
            Message(),
            m_fileName(fileName)
        { }
    };

    class MsgConfigureFileSourceSeek : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getPercentage() const { return m_seekPercentage; }

        static MsgConfigureFileSourceSeek* create(int seekPercentage)
        {
            return new MsgConfigureFileSourceSeek(seekPercentage);
        }

    private:
        int m_seekPercentage; //!< percentage of seek position from the beginning 0..100

        explicit MsgConfigureFileSourceSeek(int seekPercentage) :
            Message(),
            m_seekPercentage(seekPercentage)
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

    class MsgReportFileSourceStreamTiming : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        std::size_t getSamplesCount() const { return m_samplesCount; }

        static MsgReportFileSourceStreamTiming* create(std::size_t samplesCount)
        {
            return new MsgReportFileSourceStreamTiming(samplesCount);
        }

    private:
        std::size_t m_samplesCount;

        explicit MsgReportFileSourceStreamTiming(std::size_t samplesCount) :
            Message(),
            m_samplesCount(samplesCount)
        { }
    };

    class MsgReportFileSourceStreamData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }
        quint32 getRecordLength() const { return m_recordLength; }

        static MsgReportFileSourceStreamData* create(int sampleRate,
                quint32 recordLength)
        {
            return new MsgReportFileSourceStreamData(sampleRate, recordLength);
        }

    private:
        int m_sampleRate;
        quint32 m_recordLength;

        MsgReportFileSourceStreamData(int sampleRate,
                quint32 recordLength) :
            Message(),
            m_sampleRate(sampleRate),
            m_recordLength(recordLength)
        { }
    };

    //=================================================================

    explicit SSBMod(DeviceAPI *deviceAPI);
    ~SSBMod() final;
    void destroy() final { delete this; }
    void setDeviceAPI(DeviceAPI *deviceAPI) final;
    DeviceAPI *getDeviceAPI() final { return m_deviceAPI; }

    void start() final;
    void stop() final;
    void pull(SampleVector::iterator& begin, unsigned int nbSamples) final;
    void pushMessage(Message *msg) final { m_inputMessageQueue.push(msg); }
    QString getSourceName() final { return objectName(); }

    void getIdentifier(QString& id) final { id = objectName(); }
    QString getIdentifier() const final { return objectName(); }
    void getTitle(QString& title) final { title = m_settings.m_title; }
    qint64 getCenterFrequency() const final { return m_settings.m_inputFrequencyOffset; }
    void setCenterFrequency(qint64 frequency) final;

    QByteArray serialize() const final;
    bool deserialize(const QByteArray& data) final;

    int getNbSinkStreams() const final { return 1; }
    int getNbSourceStreams() const final { return 0; }
    int getStreamIndex() const final { return m_settings.m_streamIndex; }

    qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const final
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_settings.m_inputFrequencyOffset;
    }

    int webapiSettingsGet(
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage) final;

    int webapiWorkspaceGet(
            SWGSDRangel::SWGWorkspaceInfo& response,
            QString& errorMessage) final;

    int webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage) final;

    int webapiReportGet(
                SWGSDRangel::SWGChannelReport& response,
                QString& errorMessage) final;

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const SSBModSettings& settings);

    static void webapiUpdateChannelSettings(
            SSBModSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }
    double getMagSq() const;
    CWKeyer *getCWKeyer();
    void setLevelMeter(QObject *levelMeter) { m_levelMeter = levelMeter; }
    int getAudioSampleRate() const;
    int getFeedbackAudioSampleRate() const;
    uint32_t getNumberOfDeviceStreams() const;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    enum RateState {
        RSInitialFill,
        RSRunning
    };

    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    bool m_running = false;
    SSBModBaseband* m_basebandSource;
    SSBModSettings m_settings;
    SpectrumVis m_spectrumVis;

	SampleVector m_sampleBuffer;
    QRecursiveMutex m_settingsMutex;

    std::ifstream m_ifstream;
    QString m_fileName;
    quint64 m_fileSize = 0;     //!< raw file size (bytes)
    quint32 m_recordLength = 0; //!< record length in seconds computed from file size
    int m_sampleRate = 48000;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    CWKeyer m_cwKeyer;
    QObject *m_levelMeter;

    bool handleMessage(const Message& cmd) final;
    void applySettings(const SSBModSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer() const;
    void openFileStream();
    void seekFileStream(int seekPercentage);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response) const;
    void webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const SSBModSettings& settings, bool force);
    void webapiReverseSendCWSettings(const CWKeyerSettings& settings);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        const QList<QString>& channelSettingsKeys,
        const SSBModSettings& settings,
        bool force
    ) const;
    void webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const SSBModSettings& settings,
        bool force
    ) const;

private slots:
    void networkManagerFinished(QNetworkReply *reply) const;
};


#endif /* PLUGINS_CHANNELTX_MODSSB_SSBMOD_H_ */
