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

#ifndef SDRDAEMON_CHANNEL_SDRDAEMONCHANNELSINK_H_
#define SDRDAEMON_CHANNEL_SDRDAEMONCHANNELSINK_H_

#include <QMutex>

#include "cm256.h"

#include "dsp/basebandsamplesink.h"
#include "channel/channelsinkapi.h"
#include "channel/sdrdaemondataqueue.h"
#include "channel/sdrdaemondatablock.h"

class DeviceSourceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;
class SDRDaemonChannelSinkThread;

class SDRDaemonChannelSink : public BasebandSampleSink, public ChannelSinkAPI {
    Q_OBJECT
public:
    SDRDaemonChannelSink(DeviceSourceAPI *deviceAPI);
    virtual ~SDRDaemonChannelSink();
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

    /** Set center frequency given in Hz */
    void setCenterFrequency(uint64_t centerFrequency) { m_centerFrequency = centerFrequency / 1000; }

    /** Set sample rate given in Hz */
    void setSampleRate(uint32_t sampleRate) { m_sampleRate = sampleRate; }

    void setSampleBytes(uint8_t sampleBytes) { m_sampleBytes = sampleBytes; }
    void setSampleBits(uint8_t sampleBits) { m_sampleBits = sampleBits; }
    void setNbBlocksFEC(int nbBlocksFEC);
    void setTxDelay(int txDelay);

    static const QString m_channelIdURI;
    static const QString m_channelId;

private:
    DeviceSourceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;
    bool m_running;

    SDRDaemonDataQueue m_dataQueue;
    SDRDaemonChannelSinkThread *m_sinkThread;
    CM256 m_cm256;
    CM256 *m_cm256p;

    int m_txBlockIndex;                  //!< Current index in blocks to transmit in the Tx row
    uint16_t m_frameCount;               //!< transmission frame count
    int m_sampleIndex;                   //!< Current sample index in protected block data
    SDRDaemonSuperBlock m_superBlock;
    SDRDaemonMetaDataFEC m_currentMetaFEC;
    SDRDaemonDataBlock *m_dataBlock;
    QMutex m_dataBlockMutex;

    uint64_t m_centerFrequency;
    uint32_t m_sampleRate;
    uint8_t m_sampleBytes;
    uint8_t m_sampleBits;
    int m_nbBlocksFEC;
    int m_txDelay;
};

#endif /* SDRDAEMON_CHANNEL_SDRDAEMONCHANNELSINK_H_ */
