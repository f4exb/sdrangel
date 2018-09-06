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

#include "SWGChannelSettings.h"

#include "util/simpleserializer.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "dsp/downchannelizer.h"
#include "dsp/dspcommands.h"
#include "device/devicesourceapi.h"
#include "daemonsinkthread.h"
#include "daemonsink.h"

MESSAGE_CLASS_DEFINITION(DaemonSink::MsgConfigureDaemonSink, Message)
MESSAGE_CLASS_DEFINITION(DaemonSink::MsgSampleRateNotification, Message)

const QString DaemonSink::m_channelIdURI = "sdrangel.channel.daemonsink";
const QString DaemonSink::m_channelId = "DaemonSink";

DaemonSink::DaemonSink(DeviceSourceAPI *deviceAPI) :
        ChannelSinkAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_running(false),
        m_sinkThread(0),
        m_txBlockIndex(0),
        m_frameCount(0),
        m_sampleIndex(0),
        m_dataBlock(0),
        m_centerFrequency(0),
        m_sampleRate(48000),
        m_sampleBytes(SDR_RX_SAMP_SZ == 24 ? 4 : 2),
        m_nbBlocksFEC(0),
        m_txDelay(35),
        m_dataAddress("127.0.0.1"),
        m_dataPort(9090)
{
    setObjectName(m_channelId);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);
}

DaemonSink::~DaemonSink()
{
    m_dataBlockMutex.lock();
    if (m_dataBlock && !m_dataBlock->m_txControlBlock.m_complete) {
        delete m_dataBlock;
    }
    m_dataBlockMutex.unlock();
    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void DaemonSink::setTxDelay(int txDelay, int nbBlocksFEC)
{
    double txDelayRatio = txDelay / 100.0;
    double delay = m_sampleRate == 0 ? 1.0 : (127*127*txDelayRatio) / m_sampleRate;
    delay /= 128 + nbBlocksFEC;
    m_txDelay = roundf(delay*1e6); // microseconds
    qDebug() << "DaemonSink::setTxDelay:"
            << " " << txDelay
            << "% m_txDelay: " << m_txDelay << "us"
            << " m_sampleRate: " << m_sampleRate << "S/s";
}

void DaemonSink::setNbBlocksFEC(int nbBlocksFEC)
{
    qDebug() << "DaemonSink::setNbBlocksFEC: nbBlocksFEC: " << nbBlocksFEC;
    m_nbBlocksFEC = nbBlocksFEC;
}

void DaemonSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst __attribute__((unused)))
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

            metaData.m_centerFrequency = m_centerFrequency;
            metaData.m_sampleRate = m_sampleRate;
            metaData.m_sampleBytes = m_sampleBytes;
            metaData.m_sampleBits = 0; // TODO: deprecated
            metaData.m_nbOriginalBlocks = SDRDaemonNbOrginalBlocks;
            metaData.m_nbFECBlocks = m_nbBlocksFEC;
            metaData.m_tv_sec = tv.tv_sec;
            metaData.m_tv_usec = tv.tv_usec;

            if (!m_dataBlock) { // on the very first cycle there is no data block allocated
                m_dataBlock = new SDRDaemonDataBlock();
            }

            boost::crc_32_type crc32;
            crc32.process_bytes(&metaData, 20);
            metaData.m_crc32 = crc32.checksum();
            SDRDaemonSuperBlock& superBlock = m_dataBlock->m_superBlocks[0]; // first block
            superBlock.init();
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
        } // block zero

        // TODO: handle different sample sizes...
        if (m_sampleIndex + inRemainingSamples < SDRDaemonSamplesPerBlock) // there is still room in the current super block
        {
            memcpy((void *) &m_superBlock.m_protectedBlock.m_samples[m_sampleIndex],
                    (const void *) &(*(begin+inSamplesIndex)),
                    inRemainingSamples * sizeof(Sample));
            m_sampleIndex += inRemainingSamples;
            it = end; // all input samples are consumed
        }
        else // complete super block and initiate the next if not end of frame
        {
            memcpy((void *) &m_superBlock.m_protectedBlock.m_samples[m_sampleIndex],
                    (const void *) &(*(begin+inSamplesIndex)),
                    (SDRDaemonSamplesPerBlock - m_sampleIndex) * sizeof(Sample));
            it += SDRDaemonSamplesPerBlock - m_sampleIndex;
            m_sampleIndex = 0;

            m_superBlock.m_header.m_frameIndex = m_frameCount;
            m_superBlock.m_header.m_blockIndex = m_txBlockIndex;
            m_dataBlock->m_superBlocks[m_txBlockIndex] = m_superBlock;

            if (m_txBlockIndex == SDRDaemonNbOrginalBlocks - 1) // frame complete
            {
                m_dataBlockMutex.lock();
                m_dataBlock->m_txControlBlock.m_frameIndex = m_frameCount;
                m_dataBlock->m_txControlBlock.m_processed = false;
                m_dataBlock->m_txControlBlock.m_complete = true;
                m_dataBlock->m_txControlBlock.m_nbBlocksFEC = m_nbBlocksFEC;
                m_dataBlock->m_txControlBlock.m_txDelay = m_txDelay;
                m_dataBlock->m_txControlBlock.m_dataAddress = m_dataAddress;
                m_dataBlock->m_txControlBlock.m_dataPort = m_dataPort;

                //qDebug("DaemonSink::feed: m_dataBlock: %p m_dataQueue.sz: %d", m_dataBlock, m_dataQueue.size());
                emit dataBlockAvailable(m_dataBlock);
                //m_dataQueue.push(m_dataBlock);
                m_dataBlock = new SDRDaemonDataBlock(); // create a new one immediately
                m_dataBlockMutex.unlock();

                m_txBlockIndex = 0;
                m_frameCount++;
            }
            else
            {
                m_txBlockIndex++;
            }
        }
    }
}

void DaemonSink::start()
{
    qDebug("DaemonSink::start");

    memset((void *) &m_currentMetaFEC, 0, sizeof(SDRDaemonMetaDataFEC));

    if (m_running) {
        stop();
    }

    m_sinkThread = new DaemonSinkThread(&m_dataQueue);
    connect(this,
            SIGNAL(dataBlockAvailable(SDRDaemonDataBlock *)),
            m_sinkThread,
            SLOT(processDataBlock(SDRDaemonDataBlock *)),
            Qt::QueuedConnection);
    m_sinkThread->startStop(true);
    m_running = true;
}

void DaemonSink::stop()
{
    qDebug("DaemonSink::stop");

    if (m_sinkThread != 0)
    {
        m_sinkThread->startStop(false);
        m_sinkThread->deleteLater();
        m_sinkThread = 0;
    }

    m_running = false;
}

bool DaemonSink::handleMessage(const Message& cmd __attribute__((unused)))
{
	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;

        qDebug() << "DaemonSink::handleMessage: MsgChannelizerNotification:"
                << " channelSampleRate: " << notif.getSampleRate()
                << " offsetFrequency: " << notif.getFrequencyOffset();

        if (notif.getSampleRate() > 0) {
            setSampleRate(notif.getSampleRate());
        }

        setTxDelay(m_settings.m_txDelay, m_settings.m_nbFECBlocks);

        if (m_guiMessageQueue)
        {
            MsgSampleRateNotification *msg = MsgSampleRateNotification::create(notif.getSampleRate());
            m_guiMessageQueue->push(msg);
        }

		return true;
	}
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;

        qDebug() << "DaemonSink::handleMessage: DSPSignalNotification:"
                << " inputSampleRate: " << notif.getSampleRate()
                << " centerFrequency: " << notif.getCenterFrequency();

        setCenterFrequency(notif.getCenterFrequency());

        return true;
    }
    else if (MsgConfigureDaemonSink::match(cmd))
    {
        MsgConfigureDaemonSink& cfg = (MsgConfigureDaemonSink&) cmd;
        qDebug() << "DaemonSink::handleMessage: MsgConfigureDaemonSink";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else
    {
        return false;
    }
}

QByteArray DaemonSink::serialize() const
{
    return m_settings.serialize();
}

bool DaemonSink::deserialize(const QByteArray& data __attribute__((unused)))
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureDaemonSink *msg = MsgConfigureDaemonSink::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureDaemonSink *msg = MsgConfigureDaemonSink::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void DaemonSink::applySettings(const DaemonSinkSettings& settings, bool force)
{
    qDebug() << "DaemonSink::applySettings:"
            << " m_nbFECBlocks: " << settings.m_nbFECBlocks
            << " m_txDelay: " << settings.m_txDelay
            << " m_dataAddress: " << settings.m_dataAddress
            << " m_dataPort: " << settings.m_dataPort
            << " force: " << force;

    if ((m_settings.m_nbFECBlocks != settings.m_nbFECBlocks) || force) {
        setNbBlocksFEC(settings.m_nbFECBlocks);
        setTxDelay(settings.m_txDelay, settings.m_nbFECBlocks);
    }

    if ((m_settings.m_txDelay != settings.m_txDelay) || force) {
        setTxDelay(settings.m_txDelay, settings.m_nbFECBlocks);
    }

    if ((m_settings.m_dataAddress != settings.m_dataAddress) || force) {
        m_dataAddress = settings.m_dataAddress;
    }

    if ((m_settings.m_dataPort != settings.m_dataPort) || force) {
        m_dataPort = settings.m_dataPort;
    }

    m_settings = settings;
}

int DaemonSink::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setDaemonSinkSettings(new SWGSDRangel::SWGDaemonSinkSettings());
    response.getDaemonSinkSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int DaemonSink::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage __attribute__((unused)))
{
    DaemonSinkSettings settings = m_settings;

    if (channelSettingsKeys.contains("nbFECBlocks"))
    {
        int nbFECBlocks = response.getDaemonSinkSettings()->getNbFecBlocks();

        if ((nbFECBlocks < 0) || (nbFECBlocks > 127)) {
            settings.m_nbFECBlocks = 8;
        } else {
            settings.m_nbFECBlocks = response.getDaemonSinkSettings()->getNbFecBlocks();
        }
    }

    if (channelSettingsKeys.contains("txDelay"))
    {
        int txDelay = response.getDaemonSinkSettings()->getTxDelay();

        if (txDelay < 0) {
            settings.m_txDelay = 35;
        } else {
            settings.m_txDelay = txDelay;
        }
    }

    if (channelSettingsKeys.contains("dataAddress")) {
        settings.m_dataAddress = *response.getDaemonSinkSettings()->getDataAddress();
    }

    if (channelSettingsKeys.contains("dataPort"))
    {
        int dataPort = response.getDaemonSinkSettings()->getDataPort();

        if ((dataPort < 1024) || (dataPort > 65535)) {
            settings.m_dataPort = 9090;
        } else {
            settings.m_dataPort = dataPort;
        }
    }

    MsgConfigureDaemonSink *msg = MsgConfigureDaemonSink::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("DaemonSink::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDaemonSink *msgToGUI = MsgConfigureDaemonSink::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void DaemonSink::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const DaemonSinkSettings& settings)
{
    response.getDaemonSinkSettings()->setNbFecBlocks(settings.m_nbFECBlocks);
    response.getDaemonSinkSettings()->setTxDelay(settings.m_txDelay);

    if (response.getDaemonSinkSettings()->getDataAddress()) {
        *response.getDaemonSinkSettings()->getDataAddress() = settings.m_dataAddress;
    } else {
        response.getDaemonSinkSettings()->setDataAddress(new QString(settings.m_dataAddress));
    }

    response.getDaemonSinkSettings()->setDataPort(settings.m_dataPort);
}
