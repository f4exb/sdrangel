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

#include <sys/time.h>
#include <unistd.h>
#include <boost/crc.hpp>
#include <boost/cstdint.hpp>

#include "util/simpleserializer.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "dsp/downchannelizer.h"
#include "device/devicesourceapi.h"
#include "channel/sdrdaemonchannelsinkthread.h"
#include "sdrdaemonchannelsink.h"

const QString SDRDaemonChannelSink::m_channelIdURI = "sdrangel.channel.sdrdaemonsink";
const QString SDRDaemonChannelSink::m_channelId = "SDRDaemonChannelSink";

SDRDaemonChannelSink::SDRDaemonChannelSink(DeviceSourceAPI *deviceAPI) :
        ChannelSinkAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_running(false),
        m_sinkThread(0),
        m_txBlockIndex(0),
        m_frameCount(0),
        m_sampleIndex(0)
{
    setObjectName(m_channelId);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    m_cm256p = m_cm256.isInitialized() ? &m_cm256 : 0;
}

SDRDaemonChannelSink::~SDRDaemonChannelSink()
{
    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void SDRDaemonChannelSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst __attribute__((unused)))
{
    SampleVector::const_iterator it = begin;

    while (it != end)
    {
        int inSamplesIndex = it - begin;
        int inRemainingSamples = end - it;

        if (m_txBlockIndex == 0)
        {
            struct timeval tv;
            SDRDaemonMetaDataFEC metaData;
            gettimeofday(&tv, 0);

            metaData.m_centerFrequency = 0; // TODO
            metaData.m_sampleRate = 48000; // TODO
            metaData.m_sampleBytes = SDR_RX_SAMP_SZ/8;
            metaData.m_sampleBits = SDR_RX_SAMP_SZ;
            metaData.m_nbOriginalBlocks = SDRDaemonNbOrginalBlocks;
            metaData.m_nbFECBlocks = 8; // TODO
            metaData.m_tv_sec = tv.tv_sec;
            metaData.m_tv_usec = tv.tv_usec;

            boost::crc_32_type crc32;
            crc32.process_bytes(&metaData, 20);
            metaData.m_crc32 = crc32.checksum();
            SDRDaemonSuperBlock& superBlock = m_dataBlock.m_superBlocks[0]; // first block

            memset((void *) &superBlock, 0, SDRDaemonUdpSize);

            superBlock.m_header.m_frameIndex = m_frameCount;
            superBlock.m_header.m_blockIndex = m_txBlockIndex;
            memcpy((void *) &superBlock.m_protectedBlock, (const void *) &metaData, sizeof(SDRDaemonMetaDataFEC));

            if (!(metaData == m_currentMetaFEC))
            {
                qDebug() << "SDRDaemonChannelSink::feed: meta: "
                        << "|" << metaData.m_centerFrequency
                        << ":" << metaData.m_sampleRate
                        << ":" << (int) (metaData.m_sampleBytes & 0xF)
                        << ":" << (int) metaData.m_sampleBits
                        << "|" << (int) metaData.m_nbOriginalBlocks
                        << ":" << (int) metaData.m_nbFECBlocks
                        << "|" << metaData.m_tv_sec
                        << ":" << metaData.m_tv_usec;

                m_currentMetaFEC = metaData;
            }

            m_txBlockIndex = 1; // next Tx block with data
        }
    }
}

void SDRDaemonChannelSink::start()
{
    qDebug("SDRDaemonChannelSink::start");
    memset((void *) &m_currentMetaFEC, 0, sizeof(SDRDaemonMetaDataFEC));
    if (m_running) { stop(); }
    m_sinkThread = new SDRDaemonChannelSinkThread(&m_dataQueue, m_cm256p);
    m_running = true;
}

void SDRDaemonChannelSink::stop()
{
    qDebug("SDRDaemonChannelSink::stop");
    m_running = false;
}

bool SDRDaemonChannelSink::handleMessage(const Message& cmd __attribute__((unused)))
{
    return false;
}

QByteArray SDRDaemonChannelSink::serialize() const
{
    SimpleSerializer s(1);
    return s.final();
}

bool SDRDaemonChannelSink::deserialize(const QByteArray& data __attribute__((unused)))
{
    return false;
}
