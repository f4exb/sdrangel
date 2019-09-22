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

#ifndef INCLUDE_LOCALSINK_H_
#define INCLUDE_LOCALSINK_H_

#include <QObject>
#include <QMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "localsinksettings.h"

class DeviceAPI;
class DeviceSampleSource;
class ThreadedBasebandSampleSink;
class DownChannelizer;
class LocalSinkThread;
class QNetworkAccessManager;
class QNetworkReply;

class LocalSink : public BasebandSampleSink, public ChannelAPI {
    Q_OBJECT
public:
    class MsgConfigureLocalSink : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const LocalSinkSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureLocalSink* create(const LocalSinkSettings& settings, bool force)
        {
            return new MsgConfigureLocalSink(settings, force);
        }

    private:
        LocalSinkSettings m_settings;
        bool m_force;

        MsgConfigureLocalSink(const LocalSinkSettings& settings, bool force) :
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
        int getLog2Decim() const { return m_log2Decim; }
        int getFilterChainHash() const { return m_filterChainHash; }

        static MsgConfigureChannelizer* create(unsigned int log2Decim, unsigned int filterChainHash) {
            return new MsgConfigureChannelizer(log2Decim, filterChainHash);
        }

    private:
        unsigned int m_log2Decim;
        unsigned int m_filterChainHash;

        MsgConfigureChannelizer(unsigned int log2Decim, unsigned int filterChainHash) :
            Message(),
            m_log2Decim(log2Decim),
            m_filterChainHash(filterChainHash)
        { }
    };

    LocalSink(DeviceAPI *deviceAPI);
    virtual ~LocalSink();
    virtual void destroy() { delete this; }

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
    virtual void start();
    virtual void stop();
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = "Local Sink"; }
    virtual qint64 getCenterFrequency() const { return m_frequencyOffset; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

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
        const LocalSinkSettings& settings);

    static void webapiUpdateChannelSettings(
            LocalSinkSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    /** Set center frequency given in Hz */
    void setCenterFrequency(uint64_t centerFrequency) { m_centerFrequency = centerFrequency; }

    /** Set sample rate given in Hz */
    void setSampleRate(uint32_t sampleRate) { m_sampleRate = sampleRate; }

    void setChannelizer(unsigned int log2Decim, unsigned int filterChainHash);
    void getLocalDevices(std::vector<uint32_t>& indexes);
    uint32_t getNumberOfDeviceStreams() const;

    static const QString m_channelIdURI;
    static const QString m_channelId;

signals:
    void samplesAvailable(const quint8* data, uint count);

private:
    DeviceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;
    bool m_running;

    LocalSinkSettings m_settings;
    LocalSinkThread *m_sinkThread;

    uint64_t m_centerFrequency;
    int64_t m_frequencyOffset;
    uint32_t m_sampleRate;
    uint32_t m_deviceSampleRate;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void applySettings(const LocalSinkSettings& settings, bool force = false);
    DeviceSampleSource *getLocalDevice(uint32_t index);
    void propagateSampleRateAndFrequency(uint32_t index);
    static void validateFilterChainHash(LocalSinkSettings& settings);
    void calculateFrequencyOffset();
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const LocalSinkSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif /* INCLUDE_LOCALSINK_H_ */
