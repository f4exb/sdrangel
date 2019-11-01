///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#ifndef INCLUDE_LOCALSOURCE_H_
#define INCLUDE_LOCALSOURCE_H_

#include <QObject>
#include <QMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "util/message.h"
#include "channel/channelapi.h"
#include "localsourcesettings.h"

class DeviceAPI;
class DeviceSampleSink;
class ThreadedBasebandSampleSource;
class UpChannelizer;
class LocalSourceThread;
class QNetworkAccessManager;
class QNetworkReply;

class LocalSource : public BasebandSampleSource, public ChannelAPI {
    Q_OBJECT
public:
    class MsgConfigureLocalSource : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const LocalSourceSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureLocalSource* create(const LocalSourceSettings& settings, bool force)
        {
            return new MsgConfigureLocalSource(settings, force);
        }

    private:
        LocalSourceSettings m_settings;
        bool m_force;

        MsgConfigureLocalSource(const LocalSourceSettings& settings, bool force) :
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

    LocalSource(DeviceAPI *deviceAPI);
    virtual ~LocalSource();
    virtual void destroy() { delete this; }

    virtual void pull(Sample& sample);
    virtual void pullAudio(int nbSamples);
    virtual void start();
    virtual void stop();
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = "Local Sink"; }
    virtual qint64 getCenterFrequency() const { return m_frequencyOffset; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 0; }
    virtual int getNbSourceStreams() const { return 1; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_frequencyOffset;
    }

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const LocalSourceSettings& settings);

    static void webapiUpdateChannelSettings(
            LocalSourceSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    /** Set center frequency given in Hz */
    void setCenterFrequency(uint64_t centerFrequency) { m_centerFrequency = centerFrequency; }

    /** Set sample rate given in Hz */
    void setSampleRate(uint32_t sampleRate) { m_sampleRate = sampleRate; }

    void setChannelizer(unsigned int log2Interp, unsigned int filterChainHash);
    void getLocalDevices(std::vector<uint32_t>& indexes);

    static const QString m_channelIdURI;
    static const QString m_channelId;

signals:
    void pullSamples(unsigned int count);

private:
    DeviceAPI *m_deviceAPI;
    ThreadedBasebandSampleSource* m_threadedChannelizer;
    UpChannelizer* m_channelizer;
    bool m_running;

    LocalSourceSettings m_settings;
    LocalSourceThread *m_sinkThread;
    SampleSourceFifoDB *m_localSampleSourceFifo;
    int m_chunkSize;
    SampleVector m_localSamples;
    int m_localSamplesIndex;
    int m_localSamplesIndexOffset;

    uint64_t m_centerFrequency;
    int64_t m_frequencyOffset;
    uint32_t m_sampleRate;
    uint32_t m_deviceSampleRate;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    QMutex m_settingsMutex;

    void applySettings(const LocalSourceSettings& settings, bool force = false);
    DeviceSampleSink *getLocalDevice(uint32_t index);
    void propagateSampleRateAndFrequency(uint32_t index);
    static void validateFilterChainHash(LocalSourceSettings& settings);
    void calculateFrequencyOffset();
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const LocalSourceSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void processSamples(int offset);
};

#endif /* INCLUDE_LOCALSOURCE_H_ */
