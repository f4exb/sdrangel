///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Remote sink channel (Rx) UDP sender thread                                    //
//                                                                               //
// SDRangel can work as a detached SDR front end. With this plugin it can        //
// sends the I/Q samples stream to another SDRangel instance via UDP.            //
// It is controlled via a Web REST API.                                          //
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

#ifndef INCLUDE_REMOTESINK_H_
#define INCLUDE_REMOTESINK_H_

#include <channel/remotedatablock.h>
#include <QObject>
#include <QMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesink.h"
#include "channel/channelsinkapi.h"
#include "remotesinksettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceSourceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;
class RemoteSinkThread;

class RemoteSink : public BasebandSampleSink, public ChannelSinkAPI {
    Q_OBJECT
public:
    class MsgConfigureRemoteSink : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RemoteSinkSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureRemoteSink* create(const RemoteSinkSettings& settings, bool force)
        {
            return new MsgConfigureRemoteSink(settings, force);
        }

    private:
        RemoteSinkSettings m_settings;
        bool m_force;

        MsgConfigureRemoteSink(const RemoteSinkSettings& settings, bool force) :
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

        static MsgConfigureChannelizer* create(int sampleRate, int centerFrequency)
        {
            return new MsgConfigureChannelizer(sampleRate, centerFrequency);
        }

    private:
        unsigned int m_log2Decim;
        unsigned int m_filterChainHash;

        MsgConfigureChannelizer(unsigned int log2Decim, int filterChainHash) :
            Message(),
            m_log2Decim(log2Decim),
            m_filterChainHash(filterChainHash)
        { }
    };

    RemoteSink(DeviceSourceAPI *deviceAPI);
    virtual ~RemoteSink();
    virtual void destroy() { delete this; }

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
    virtual void start();
    virtual void stop();
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = "Remote Sink"; }
    virtual qint64 getCenterFrequency() const { return 0; }

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

    /** Set center frequency given in Hz */
    void setCenterFrequency(uint64_t centerFrequency) { m_centerFrequency = centerFrequency / 1000; }

    /** Set sample rate given in Hz */
    void setSampleRate(uint32_t sampleRate) { m_sampleRate = sampleRate; }

    void setNbBlocksFEC(int nbBlocksFEC);
    void setTxDelay(int txDelay, int nbBlocksFEC);
    void setDataAddress(const QString& address) { m_dataAddress = address; }
    void setDataPort(uint16_t port) { m_dataPort = port; }
    void setChannelizer(unsigned int log2Decim, unsigned int filterChainHash);

    static const QString m_channelIdURI;
    static const QString m_channelId;

signals:
    void dataBlockAvailable(RemoteDataBlock *dataBlock);

private:
    DeviceSourceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;
    bool m_running;

    RemoteSinkSettings m_settings;
    RemoteSinkThread *m_sinkThread;

    int m_txBlockIndex;                  //!< Current index in blocks to transmit in the Tx row
    uint16_t m_frameCount;               //!< transmission frame count
    int m_sampleIndex;                   //!< Current sample index in protected block data
    RemoteSuperBlock m_superBlock;
    RemoteMetaDataFEC m_currentMetaFEC;
    RemoteDataBlock *m_dataBlock;
    QMutex m_dataBlockMutex;

    uint64_t m_centerFrequency;
    int64_t m_frequencyOffset;
    uint32_t m_sampleRate;
    int m_nbBlocksFEC;
    int m_txDelay;
    QString m_dataAddress;
    uint16_t m_dataPort;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void applySettings(const RemoteSinkSettings& settings, bool force = false);
    void webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const RemoteSinkSettings& settings);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const RemoteSinkSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif /* INCLUDE_REMOTESINK_H_ */
