///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGSDRDaemonChannelSourceReport.h"

#include "device/devicesinkapi.h"
#include "dsp/upchannelizer.h"
#include "dsp/threadedbasebandsamplesource.h"

#include "daemonsrcthread.h"
#include "daemonsrc.h"

MESSAGE_CLASS_DEFINITION(DaemonSrc::MsgSampleRateNotification, Message)
MESSAGE_CLASS_DEFINITION(DaemonSrc::MsgConfigureDaemonSrc, Message)

const QString DaemonSrc::m_channelIdURI = "sdrangel.channeltx.daemonsrc";
const QString DaemonSrc::m_channelId ="DaemonSrc";

DaemonSrc::DaemonSrc(DeviceSinkAPI *deviceAPI) :
    ChannelSourceAPI(m_channelIdURI),
    m_deviceAPI(deviceAPI),
    m_sourceThread(0),
    m_running(false),
    m_nbCorrectableErrors(0),
    m_nbUncorrectableErrors(0)
{
    setObjectName(m_channelId);

    m_channelizer = new UpChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSource(m_channelizer, this);
    m_deviceAPI->addThreadedSource(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    connect(&m_dataQueue, SIGNAL(dataBlockEnqueued()), this, SLOT(handleData()), Qt::QueuedConnection);
    m_cm256p = m_cm256.isInitialized() ? &m_cm256 : 0;
    m_currentMeta.init();
}

DaemonSrc::~DaemonSrc()
{
    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSource(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void DaemonSrc::pull(Sample& sample)
{
    m_dataReadQueue.readSample(sample);
}

void DaemonSrc::pullAudio(int nbSamples __attribute__((unused)))
{
}

void DaemonSrc::start()
{
    qDebug("DaemonSrc::start");

    if (m_running) {
        stop();
    }

    m_sourceThread = new DaemonSrcThread(&m_dataQueue);
    m_sourceThread->startStop(true);
    m_sourceThread->dataBind(m_settings.m_dataAddress, m_settings.m_dataPort);
    m_running = true;
}

void DaemonSrc::stop()
{
    qDebug("DaemonSrc::stop");

    if (m_sourceThread != 0)
    {
        m_sourceThread->startStop(false);
        m_sourceThread->deleteLater();
        m_sourceThread = 0;
    }

    m_running = false;
}

void DaemonSrc::setDataLink(const QString& dataAddress, uint16_t dataPort)
{
    DaemonSrcSettings settings = m_settings;
    settings.m_dataAddress = dataAddress;
    settings.m_dataPort = dataPort;

    MsgConfigureDaemonSrc *msg = MsgConfigureDaemonSrc::create(settings, false);
    m_inputMessageQueue.push(msg);
}

bool DaemonSrc::handleMessage(const Message& cmd)
{
    if (UpChannelizer::MsgChannelizerNotification::match(cmd))
    {
        UpChannelizer::MsgChannelizerNotification& notif = (UpChannelizer::MsgChannelizerNotification&) cmd;
        qDebug() << "DaemonSrc::handleMessage: MsgChannelizerNotification:"
                << " basebandSampleRate: " << notif.getBasebandSampleRate()
                << " outputSampleRate: " << notif.getSampleRate()
                << " inputFrequencyOffset: " << notif.getFrequencyOffset();

        if (m_guiMessageQueue)
        {
            MsgSampleRateNotification *msg = MsgSampleRateNotification::create(notif.getBasebandSampleRate());
            m_guiMessageQueue->push(msg);
        }

        return true;
    }
    else if (MsgConfigureDaemonSrc::match(cmd))
    {
        MsgConfigureDaemonSrc& cfg = (MsgConfigureDaemonSrc&) cmd;
        qDebug() << "MsgConfigureDaemonSrc::handleMessage: MsgConfigureDaemonSrc";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    return false;
}

QByteArray DaemonSrc::serialize() const
{
    return m_settings.serialize();
}

bool DaemonSrc::deserialize(const QByteArray& data __attribute__((unused)))
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureDaemonSrc *msg = MsgConfigureDaemonSrc::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureDaemonSrc *msg = MsgConfigureDaemonSrc::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void DaemonSrc::applySettings(const DaemonSrcSettings& settings, bool force)
{
    qDebug() << "DaemonSrc::applySettings:"
            << " m_dataAddress: " << settings.m_dataAddress
            << " m_dataPort: " << settings.m_dataPort
            << " force: " << force;

    bool change = false;

    if ((m_settings.m_dataAddress != settings.m_dataAddress) || force) {
        change = true;
    }

    if ((m_settings.m_dataPort != settings.m_dataPort) || force) {
        change = true;
    }

    if (change && m_sourceThread) {
        m_sourceThread->dataBind(settings.m_dataAddress, settings.m_dataPort);
    }

    m_settings = settings;
}

void DaemonSrc::handleDataBlock(SDRDaemonDataBlock* dataBlock __attribute__((unused)))
{
}

void DaemonSrc::handleData()
{
    SDRDaemonDataBlock* dataBlock;

    while (m_running && ((dataBlock = m_dataQueue.pop()) != 0)) {
        handleDataBlock(dataBlock);
    }
}

void DaemonSrc::printMeta(const QString& header, SDRDaemonMetaDataFEC *metaData)
{
    qDebug().noquote() << header << ": "
            << "|" << metaData->m_centerFrequency
            << ":" << metaData->m_sampleRate
            << ":" << (int) (metaData->m_sampleBytes & 0xF)
            << ":" << (int) metaData->m_sampleBits
            << ":" << (int) metaData->m_nbOriginalBlocks
            << ":" << (int) metaData->m_nbFECBlocks
            << "|" << metaData->m_tv_sec
            << ":" << metaData->m_tv_usec
            << "|";
}

uint32_t DaemonSrc::calculateDataReadQueueSize(int sampleRate)
{
    // scale for 20 blocks at 48 kS/s. Take next even number.
    uint32_t maxSize = sampleRate / 2400;
    maxSize = (maxSize % 2 == 0) ? maxSize : maxSize + 1;
    qDebug("SDRDaemonChannelSource::calculateDataReadQueueSize: set max queue size to %u blocks", maxSize);
    return maxSize;
}

int DaemonSrc::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setSdrDaemonChannelSourceSettings(new SWGSDRangel::SWGSDRDaemonChannelSourceSettings());
    response.getSdrDaemonChannelSourceSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int DaemonSrc::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage __attribute__((unused)))
{
    DaemonSrcSettings settings = m_settings;

    if (channelSettingsKeys.contains("dataAddress")) {
        settings.m_dataAddress = *response.getSdrDaemonChannelSourceSettings()->getDataAddress();
    }

    if (channelSettingsKeys.contains("dataPort"))
    {
        int dataPort = response.getSdrDaemonChannelSourceSettings()->getDataPort();

        if ((dataPort < 1024) || (dataPort > 65535)) {
            settings.m_dataPort = 9090;
        } else {
            settings.m_dataPort = dataPort;
        }
    }

    MsgConfigureDaemonSrc *msg = MsgConfigureDaemonSrc::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("DaemonSrc::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDaemonSrc *msgToGUI = MsgConfigureDaemonSrc::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int DaemonSrc::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setSdrDaemonChannelSourceReport(new SWGSDRangel::SWGSDRDaemonChannelSourceReport());
    response.getSdrDaemonChannelSourceReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void DaemonSrc::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const DaemonSrcSettings& settings)
{
    if (response.getSdrDaemonChannelSourceSettings()->getDataAddress()) {
        *response.getSdrDaemonChannelSourceSettings()->getDataAddress() = settings.m_dataAddress;
    } else {
        response.getSdrDaemonChannelSourceSettings()->setDataAddress(new QString(settings.m_dataAddress));
    }

    response.getSdrDaemonChannelSourceSettings()->setDataPort(settings.m_dataPort);
}

void DaemonSrc::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    struct timeval tv;
    gettimeofday(&tv, 0);

    response.getSdrDaemonChannelSourceReport()->setTvSec(tv.tv_sec);
    response.getSdrDaemonChannelSourceReport()->setTvUSec(tv.tv_usec);
    response.getSdrDaemonChannelSourceReport()->setQueueSize(m_dataReadQueue.size());
    response.getSdrDaemonChannelSourceReport()->setQueueLength(m_dataReadQueue.length());
    response.getSdrDaemonChannelSourceReport()->setSamplesCount(m_dataReadQueue.readSampleCount());
    response.getSdrDaemonChannelSourceReport()->setCorrectableErrorsCount(m_nbCorrectableErrors);
    response.getSdrDaemonChannelSourceReport()->setUncorrectableErrorsCount(m_nbUncorrectableErrors);
}
