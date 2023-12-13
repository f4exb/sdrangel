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

#ifndef PLUGINS_CHANNELTX_MODWFM_WFMMOD_H_
#define PLUGINS_CHANNELTX_MODWFM_WFMMOD_H_

#include <vector>
#include <iostream>
#include <fstream>

#include <QRecursiveMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "wfmmodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class CWKeyer;
class WFMModBaseband;
class ObjectPipe;

class WFMMod : public BasebandSampleSource, public ChannelAPI {
public:
    class MsgConfigureWFMMod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const WFMModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureWFMMod* create(const WFMModSettings& settings, bool force)
        {
            return new MsgConfigureWFMMod(settings, force);
        }

    private:
        WFMModSettings m_settings;
        bool m_force;

        MsgConfigureWFMMod(const WFMModSettings& settings, bool force) :
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

        MsgConfigureFileSourceName(const QString& fileName) :
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

    protected:
        int m_seekPercentage; //!< percentage of seek position from the beginning 0..100

        MsgConfigureFileSourceSeek(int seekPercentage) :
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

    protected:
        std::size_t m_samplesCount;

        MsgReportFileSourceStreamTiming(std::size_t samplesCount) :
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

    protected:
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

    WFMMod(DeviceAPI *deviceAPI);
    virtual ~WFMMod();
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
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }
    virtual void setCenterFrequency(qint64 frequency);

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_settings.m_inputFrequencyOffset;
    }

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

    static void webapiFormatChannelSettings(
            SWGSDRangel::SWGChannelSettings& response,
            const WFMModSettings& settings);

    static void webapiUpdateChannelSettings(
            WFMModSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    double getMagSq() const;
    CWKeyer *getCWKeyer();
    void setLevelMeter(QObject *levelMeter);
    uint32_t getNumberOfDeviceStreams() const;
    int getAudioSampleRate() const;
    int getFeedbackAudioSampleRate() const;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

signals:
    /**
     * Level changed
     * \param rmsLevel RMS level in range 0.0 - 1.0
     * \param peakLevel Peak level in range 0.0 - 1.0
     * \param numSamples Number of audio samples analyzed
     */
    void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);


private:
    enum RateState {
        RSInitialFill,
        RSRunning
    };

    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    WFMModBaseband* m_basebandSource;
    WFMModSettings m_settings;

    SampleVector m_sampleBuffer;
    QRecursiveMutex m_settingsMutex;

    std::ifstream m_ifstream;
    QString m_fileName;
    quint64 m_fileSize;     //!< raw file size (bytes)
    quint32 m_recordLength; //!< record length in seconds computed from file size
    int m_sampleRate;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    static const int m_levelNbSamples;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const WFMModSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer();
    void openFileStream();
    void seekFileStream(int seekPercentage);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const WFMModSettings& settings, bool force);
    void webapiReverseSendCWSettings(const CWKeyerSettings& settings);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const WFMModSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const WFMModSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};


#endif /* PLUGINS_CHANNELTX_MODWFM_WFMMOD_H_ */
