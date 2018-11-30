///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon sink channel (Rx)                                                   //
//                                                                               //
// SDRdaemon is a detached SDR front end that handles the interface with a       //
// physical device and sends or receives the I/Q samples stream to or from a     //
// SDRangel instance via UDP. It is controlled via a Web REST API.               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_DAEMONSINK_H_
#define INCLUDE_DAEMONSINK_H_

#include <QObject>
#include <QMutex>

#include "dsp/basebandsamplesink.h"
#include "channel/channelsinkapi.h"
#include "channel/sdrdaemondatablock.h"
#include "daemonsinksettings.h"

class DeviceSourceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;
class DaemonSinkThread;

class DaemonSink : public BasebandSampleSink, public ChannelSinkAPI {
    Q_OBJECT
public:
    class MsgConfigureDaemonSink : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DaemonSinkSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureDaemonSink* create(const DaemonSinkSettings& settings, bool force)
        {
            return new MsgConfigureDaemonSink(settings, force);
        }

    private:
        DaemonSinkSettings m_settings;
        bool m_force;

        MsgConfigureDaemonSink(const DaemonSinkSettings& settings, bool force) :
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

    DaemonSink(DeviceSourceAPI *deviceAPI);
    virtual ~DaemonSink();
    virtual void destroy() { delete this; }

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
    virtual void start();
    virtual void stop();
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = "SDRDaemon Sink"; }
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

    static const QString m_channelIdURI;
    static const QString m_channelId;

signals:
    void dataBlockAvailable(SDRDaemonDataBlock *dataBlock);

private:
    DeviceSourceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;
    bool m_running;

    DaemonSinkSettings m_settings;
    DaemonSinkThread *m_sinkThread;

    int m_txBlockIndex;                  //!< Current index in blocks to transmit in the Tx row
    uint16_t m_frameCount;               //!< transmission frame count
    int m_sampleIndex;                   //!< Current sample index in protected block data
    SDRDaemonSuperBlock m_superBlock;
    SDRDaemonMetaDataFEC m_currentMetaFEC;
    SDRDaemonDataBlock *m_dataBlock;
    QMutex m_dataBlockMutex;

    uint64_t m_centerFrequency;
    uint32_t m_sampleRate;
    int m_nbBlocksFEC;
    int m_txDelay;
    QString m_dataAddress;
    uint16_t m_dataPort;

    void applySettings(const DaemonSinkSettings& settings, bool force = false);
    void webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const DaemonSinkSettings& settings);
};

#endif /* INCLUDE_DAEMONSINK_H_ */
