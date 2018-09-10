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
#include "channel/sdrdaemonchannelsinkthread.h"
#include "sdrdaemonchannelsink.h"

MESSAGE_CLASS_DEFINITION(SDRDaemonChannelSink::MsgConfigureSDRDaemonChannelSink, Message)

const QString SDRDaemonChannelSink::m_channelIdURI = "sdrangel.channel.sdrdaemonsink";
const QString SDRDaemonChannelSink::m_channelId = "SDRDaemonChannelSink";

SDRDaemonChannelSink::SDRDaemonChannelSink(DeviceSourceAPI *deviceAPI) :
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
        m_txDelay(100),
        m_dataAddress("127.0.0.1"),
        m_dataPort(9090)
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

void SDRDaemonChannelSink::setTxDelay(int txDelay)
{
    qDebug() << "SDRDaemonChannelSink::setTxDelay: txDelay: " << txDelay;
    m_txDelay = txDelay;
}

void SDRDaemonChannelSink::setNbBlocksFEC(int nbBlocksFEC)
{
    qDebug() << "SDRDaemonChannelSink::setNbBlocksFEC: nbBlocksFEC: " << nbBlocksFEC;
    m_nbBlocksFEC = nbBlocksFEC;
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

        // handle different sample sizes...
        int samplesPerBlock = SDRDaemonNbBytesPerBlock / sizeof(Sample);
        if (m_sampleIndex + inRemainingSamples < samplesPerBlock) // there is still room in the current super block
        {
            memcpy((void *) &m_superBlock.m_protectedBlock.buf[m_sampleIndex*sizeof(Sample)],
                    (const void *) &(*(begin+inSamplesIndex)),
                    inRemainingSamples * sizeof(Sample));
            m_sampleIndex += inRemainingSamples;
            it = end; // all input samples are consumed
        }
        else // complete super block and initiate the next if not end of frame
        {
            memcpy((void *) &m_superBlock.m_protectedBlock.buf[m_sampleIndex*sizeof(Sample)],
                    (const void *) &(*(begin+inSamplesIndex)),
                    (samplesPerBlock - m_sampleIndex) * sizeof(Sample));
            it += samplesPerBlock - m_sampleIndex;
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

                m_dataQueue.push(m_dataBlock);
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

void SDRDaemonChannelSink::start()
{
    qDebug("SDRDaemonChannelSink::start");

    memset((void *) &m_currentMetaFEC, 0, sizeof(SDRDaemonMetaDataFEC));

    if (m_running) {
        stop();
    }

    m_sinkThread = new SDRDaemonChannelSinkThread(&m_dataQueue, m_cm256p);
    m_sinkThread->startStop(true);
    m_running = true;
}

void SDRDaemonChannelSink::stop()
{
    qDebug("SDRDaemonChannelSink::stop");

    if (m_sinkThread != 0)
    {
        m_sinkThread->startStop(false);
        m_sinkThread->deleteLater();
        m_sinkThread = 0;
    }

    m_running = false;
}

bool SDRDaemonChannelSink::handleMessage(const Message& cmd __attribute__((unused)))
{
	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;

        qDebug() << "SDRDaemonChannelSink::handleMessage: MsgChannelizerNotification:"
                << " channelSampleRate: " << notif.getSampleRate()
                << " offsetFrequency: " << notif.getFrequencyOffset();

        if (notif.getSampleRate() > 0) {
            setSampleRate(notif.getSampleRate());
        }

		return true;
	}
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;

        qDebug() << "SDRDaemonChannelSink::handleMessage: DSPSignalNotification:"
                << " inputSampleRate: " << notif.getSampleRate()
                << " centerFrequency: " << notif.getCenterFrequency();

        setCenterFrequency(notif.getCenterFrequency());

        return true;
    }
    else if (MsgConfigureSDRDaemonChannelSink::match(cmd))
    {
        MsgConfigureSDRDaemonChannelSink& cfg = (MsgConfigureSDRDaemonChannelSink&) cmd;
        qDebug() << "SDRDaemonChannelSink::handleMessage: MsgConfigureSDRDaemonChannelSink";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else
    {
        return false;
    }
}

QByteArray SDRDaemonChannelSink::serialize() const
{
    return m_settings.serialize();
}

bool SDRDaemonChannelSink::deserialize(const QByteArray& data __attribute__((unused)))
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureSDRDaemonChannelSink *msg = MsgConfigureSDRDaemonChannelSink::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureSDRDaemonChannelSink *msg = MsgConfigureSDRDaemonChannelSink::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void SDRDaemonChannelSink::applySettings(const SDRDaemonChannelSinkSettings& settings, bool force)
{
    qDebug() << "SDRDaemonChannelSink::applySettings:"
            << " m_nbFECBlocks: " << settings.m_nbFECBlocks
            << " m_txDelay: " << settings.m_txDelay
            << " m_dataAddress: " << settings.m_dataAddress
            << " m_dataPort: " << settings.m_dataPort
            << " force: " << force;

    if ((m_settings.m_nbFECBlocks != settings.m_nbFECBlocks) || force) {
        m_nbBlocksFEC = settings.m_nbFECBlocks;
    }

    if ((m_settings.m_txDelay != settings.m_txDelay) || force) {
        m_txDelay = settings.m_txDelay;
    }

    if ((m_settings.m_dataAddress != settings.m_dataAddress) || force) {
        m_dataAddress = settings.m_dataAddress;
    }

    if ((m_settings.m_dataPort != settings.m_dataPort) || force) {
        m_dataPort = settings.m_dataPort;
    }

    m_settings = settings;
}

int SDRDaemonChannelSink::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setDaemonSinkSettings(new SWGSDRangel::SWGDaemonSinkSettings());
    response.getDaemonSinkSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int SDRDaemonChannelSink::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage __attribute__((unused)))
{
    SDRDaemonChannelSinkSettings settings = m_settings;

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
            settings.m_txDelay = 100;
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

    MsgConfigureSDRDaemonChannelSink *msg = MsgConfigureSDRDaemonChannelSink::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("SDRDaemonChannelSink::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSDRDaemonChannelSink *msgToGUI = MsgConfigureSDRDaemonChannelSink::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void SDRDaemonChannelSink::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const SDRDaemonChannelSinkSettings& settings)
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
