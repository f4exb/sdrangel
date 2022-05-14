///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2019 Edouard Griffiths, F4EXB                              //
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

#ifndef PLUGINS_CHANNELTX_REMOTESRC_REMOTESRC_H_
#define PLUGINS_CHANNELTX_REMOTESRC_REMOTESRC_H_

#include <QObject>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "remotesourcesettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;

class DeviceAPI;
class RemoteSourceBaseband;
class ObjectPipe;

class RemoteSource : public BasebandSampleSource, public ChannelAPI {
public:
    class MsgConfigureRemoteSource : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RemoteSourceSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureRemoteSource* create(const RemoteSourceSettings& settings, bool force)
        {
            return new MsgConfigureRemoteSource(settings, force);
        }

    private:
        RemoteSourceSettings m_settings;
        bool m_force;

        MsgConfigureRemoteSource(const RemoteSourceSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgQueryStreamData : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        static MsgQueryStreamData* create() {
            return new MsgQueryStreamData();
        }
    private:
        MsgQueryStreamData() : Message() {}
    };

    class MsgReportStreamData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        uint32_t get_tv_sec() const { return m_tv_sec; }
        uint32_t get_tv_usec() const { return m_tv_usec; }
        uint32_t get_queueSize() const { return m_queueSize; }
        uint32_t get_queueLength() const { return m_queueLength; }
        uint32_t get_readSamplesCount() const { return m_readSamplesCount; }
        uint32_t get_nbCorrectableErrors() const { return m_nbCorrectableErrors; }
        uint32_t get_nbUncorrectableErrors() const { return m_nbUncorrectableErrors; }
        uint32_t get_nbOriginalBlocks() const { return m_nbOriginalBlocks; }
        uint32_t get_nbFECBlocks() const { return m_nbFECBlocks; }
        uint32_t get_centerFreq() const { return m_centerFreq; }
        uint32_t get_sampleRate() const { return m_sampleRate; }

        static MsgReportStreamData* create(
                uint32_t tv_sec,
                uint32_t tv_usec,
                uint32_t queueSize,
                uint32_t queueLength,
                uint32_t readSamplesCount,
                uint32_t nbCorrectableErrors,
                uint32_t nbUncorrectableErrors,
                uint32_t nbOriginalBlocks,
                uint32_t nbFECBlocks,
                uint32_t centerFreq,
                uint32_t sampleRate)
        {
            return new MsgReportStreamData(
                    tv_sec,
                    tv_usec,
                    queueSize,
                    queueLength,
                    readSamplesCount,
                    nbCorrectableErrors,
                    nbUncorrectableErrors,
                    nbOriginalBlocks,
                    nbFECBlocks,
                    centerFreq,
                    sampleRate);
        }

    protected:
        uint32_t m_tv_sec;
        uint32_t m_tv_usec;
        uint32_t m_queueSize;
        uint32_t m_queueLength;
        uint32_t m_readSamplesCount;
        uint32_t m_nbCorrectableErrors;
        uint32_t m_nbUncorrectableErrors;
        uint32_t m_nbOriginalBlocks;
        uint32_t m_nbFECBlocks;
        uint32_t m_centerFreq;
        uint32_t m_sampleRate;

        MsgReportStreamData(
                uint32_t tv_sec,
                uint32_t tv_usec,
                uint32_t queueSize,
                uint32_t queueLength,
                uint32_t readSamplesCount,
                uint32_t nbCorrectableErrors,
                uint32_t nbUncorrectableErrors,
                uint32_t nbOriginalBlocks,
                uint32_t nbFECBlocks,
                uint32_t centerFreq,
                uint32_t sampleRate) :
            Message(),
            m_tv_sec(tv_sec),
            m_tv_usec(tv_usec),
            m_queueSize(queueSize),
            m_queueLength(queueLength),
            m_readSamplesCount(readSamplesCount),
            m_nbCorrectableErrors(nbCorrectableErrors),
            m_nbUncorrectableErrors(nbUncorrectableErrors),
            m_nbOriginalBlocks(nbOriginalBlocks),
            m_nbFECBlocks(nbFECBlocks),
            m_centerFreq(centerFreq),
            m_sampleRate(sampleRate)
        { }
    };

    RemoteSource(DeviceAPI *deviceAPI);
    virtual ~RemoteSource();
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
    virtual qint64 getCenterFrequency() const { return m_frequencyOffset; }
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

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const RemoteSourceSettings& settings);

    static void webapiUpdateChannelSettings(
            RemoteSourceSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    uint32_t getNumberOfDeviceStreams() const;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    RemoteSourceBaseband *m_basebandSource;
    RemoteSourceSettings m_settings;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    uint64_t m_centerFrequency;
    int64_t m_frequencyOffset;
    uint32_t m_basebandSampleRate;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const RemoteSourceSettings& settings, bool force = false);
    static void validateFilterChainHash(RemoteSourceSettings& settings);
    void calculateFrequencyOffset(uint32_t log2Interp, uint32_t filterChainHash);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const RemoteSourceSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const RemoteSourceSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const RemoteSourceSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // PLUGINS_CHANNELTX_REMOTESRC_REMOTESRC_H_
