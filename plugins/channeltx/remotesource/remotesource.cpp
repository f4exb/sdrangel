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

#include "remotesource.h"

#include <sys/time.h>
#include <unistd.h>
#include <boost/crc.hpp>
#include <boost/cstdint.hpp>

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGRemoteSourceReport.h"

#include "dsp/devicesamplesink.h"
#include "device/devicesinkapi.h"
#include "dsp/upchannelizer.h"
#include "dsp/threadedbasebandsamplesource.h"

#include "remotesourcethread.h"

MESSAGE_CLASS_DEFINITION(RemoteSource::MsgSampleRateNotification, Message)
MESSAGE_CLASS_DEFINITION(RemoteSource::MsgConfigureRemoteSource, Message)
MESSAGE_CLASS_DEFINITION(RemoteSource::MsgQueryStreamData, Message)
MESSAGE_CLASS_DEFINITION(RemoteSource::MsgReportStreamData, Message)

const QString RemoteSource::m_channelIdURI = "sdrangel.channeltx.remotesource";
const QString RemoteSource::m_channelId ="RemoteSource";

RemoteSource::RemoteSource(DeviceSinkAPI *deviceAPI) :
    ChannelSourceAPI(m_channelIdURI),
    m_deviceAPI(deviceAPI),
    m_sourceThread(0),
    m_running(false),
    m_nbCorrectableErrors(0),
    m_nbUncorrectableErrors(0)
{
    setObjectName(m_channelId);

    connect(&m_dataQueue, SIGNAL(dataBlockEnqueued()), this, SLOT(handleData()), Qt::QueuedConnection);
    m_cm256p = m_cm256.isInitialized() ? &m_cm256 : 0;
    m_currentMeta.init();

    m_channelizer = new UpChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSource(m_channelizer, this);
    m_deviceAPI->addThreadedSource(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

RemoteSource::~RemoteSource()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSource(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void RemoteSource::pull(Sample& sample)
{
    m_dataReadQueue.readSample(sample, true); // true is scale for Tx
}

void RemoteSource::pullAudio(int nbSamples)
{
    (void) nbSamples;
}

void RemoteSource::start()
{
    qDebug("RemoteSource::start");

    if (m_running) {
        stop();
    }

    m_sourceThread = new RemoteSourceThread(&m_dataQueue);
    m_sourceThread->startStop(true);
    m_sourceThread->dataBind(m_settings.m_dataAddress, m_settings.m_dataPort);
    m_running = true;
}

void RemoteSource::stop()
{
    qDebug("RemoteSource::stop");

    if (m_sourceThread != 0)
    {
        m_sourceThread->startStop(false);
        m_sourceThread->deleteLater();
        m_sourceThread = 0;
    }

    m_running = false;
}

void RemoteSource::setDataLink(const QString& dataAddress, uint16_t dataPort)
{
    RemoteSourceSettings settings = m_settings;
    settings.m_dataAddress = dataAddress;
    settings.m_dataPort = dataPort;

    MsgConfigureRemoteSource *msg = MsgConfigureRemoteSource::create(settings, false);
    m_inputMessageQueue.push(msg);
}

bool RemoteSource::handleMessage(const Message& cmd)
{
    if (UpChannelizer::MsgChannelizerNotification::match(cmd))
    {
        UpChannelizer::MsgChannelizerNotification& notif = (UpChannelizer::MsgChannelizerNotification&) cmd;
        qDebug() << "RemoteSource::handleMessage: MsgChannelizerNotification:"
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
    else if (MsgConfigureRemoteSource::match(cmd))
    {
        MsgConfigureRemoteSource& cfg = (MsgConfigureRemoteSource&) cmd;
        qDebug() << "MsgConfigureRemoteSource::handleMessage: MsgConfigureRemoteSource";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgQueryStreamData::match(cmd))
    {
        if (m_guiMessageQueue)
        {
            struct timeval tv;
            gettimeofday(&tv, 0);

            MsgReportStreamData *msg = MsgReportStreamData::create(
                    tv.tv_sec,
                    tv.tv_usec,
                    m_dataReadQueue.size(),
                    m_dataReadQueue.length(),
                    m_dataReadQueue.readSampleCount(),
                    m_nbCorrectableErrors,
                    m_nbUncorrectableErrors,
                    m_currentMeta.m_nbOriginalBlocks,
                    m_currentMeta.m_nbFECBlocks,
                    m_currentMeta.m_centerFrequency,
                    m_currentMeta.m_sampleRate);
            m_guiMessageQueue->push(msg);
        }

        return true;
    }
    return false;
}

QByteArray RemoteSource::serialize() const
{
    return m_settings.serialize();
}

bool RemoteSource::deserialize(const QByteArray& data)
{
    (void) data;
    if (m_settings.deserialize(data))
    {
        MsgConfigureRemoteSource *msg = MsgConfigureRemoteSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureRemoteSource *msg = MsgConfigureRemoteSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void RemoteSource::applySettings(const RemoteSourceSettings& settings, bool force)
{
    qDebug() << "RemoteSource::applySettings:"
            << " m_dataAddress: " << settings.m_dataAddress
            << " m_dataPort: " << settings.m_dataPort
            << " force: " << force;

    bool change = false;
    QList<QString> reverseAPIKeys;

    if ((m_settings.m_dataAddress != settings.m_dataAddress) || force)
    {
        reverseAPIKeys.append("dataAddress");
        change = true;
    }

    if ((m_settings.m_dataPort != settings.m_dataPort) || force)
    {
        reverseAPIKeys.append("dataPort");
        change = true;
    }

    if (change && m_sourceThread)
    {
        reverseAPIKeys.append("sourceThread");
        m_sourceThread->dataBind(settings.m_dataAddress, settings.m_dataPort);
    }

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

void RemoteSource::handleDataBlock(RemoteDataBlock* dataBlock)
{
    (void) dataBlock;
    if (dataBlock->m_rxControlBlock.m_blockCount < RemoteNbOrginalBlocks)
    {
        qWarning("RemoteSource::handleDataBlock: incomplete data block: not processing");
    }
    else
    {
        int blockCount = 0;

        for (int blockIndex = 0; blockIndex < 256; blockIndex++)
        {
            if ((blockIndex == 0) && (dataBlock->m_rxControlBlock.m_metaRetrieved))
            {
                m_cm256DescriptorBlocks[blockCount].Index = 0;
                m_cm256DescriptorBlocks[blockCount].Block = (void *) &(dataBlock->m_superBlocks[0].m_protectedBlock);
                blockCount++;
            }
            else if (dataBlock->m_superBlocks[blockIndex].m_header.m_blockIndex != 0)
            {
                m_cm256DescriptorBlocks[blockCount].Index = dataBlock->m_superBlocks[blockIndex].m_header.m_blockIndex;
                m_cm256DescriptorBlocks[blockCount].Block = (void *) &(dataBlock->m_superBlocks[blockIndex].m_protectedBlock);
                blockCount++;
            }
        }

        //qDebug("RemoteSource::handleDataBlock: frame: %u blocks: %d", dataBlock.m_rxControlBlock.m_frameIndex, blockCount);

        // Need to use the CM256 recovery
        if (m_cm256p &&(dataBlock->m_rxControlBlock.m_originalCount < RemoteNbOrginalBlocks))
        {
            qDebug("RemoteSource::handleDataBlock: %d recovery blocks", dataBlock->m_rxControlBlock.m_recoveryCount);
            CM256::cm256_encoder_params paramsCM256;
            paramsCM256.BlockBytes = sizeof(RemoteProtectedBlock); // never changes
            paramsCM256.OriginalCount = RemoteNbOrginalBlocks;  // never changes

            if (m_currentMeta.m_tv_sec == 0) {
                paramsCM256.RecoveryCount = dataBlock->m_rxControlBlock.m_recoveryCount;
            } else {
                paramsCM256.RecoveryCount = m_currentMeta.m_nbFECBlocks;
            }

            // update counters
            if (dataBlock->m_rxControlBlock.m_originalCount < RemoteNbOrginalBlocks - paramsCM256.RecoveryCount) {
                m_nbUncorrectableErrors += RemoteNbOrginalBlocks - paramsCM256.RecoveryCount - dataBlock->m_rxControlBlock.m_originalCount;
            } else {
                m_nbCorrectableErrors += dataBlock->m_rxControlBlock.m_recoveryCount;
            }

            if (m_cm256.cm256_decode(paramsCM256, m_cm256DescriptorBlocks)) // CM256 decode
            {
                qWarning() << "RemoteSource::handleDataBlock: decode CM256 error:"
                        << " m_originalCount: " << dataBlock->m_rxControlBlock.m_originalCount
                        << " m_recoveryCount: " << dataBlock->m_rxControlBlock.m_recoveryCount;
            }
            else
            {
                for (int ir = 0; ir < dataBlock->m_rxControlBlock.m_recoveryCount; ir++) // restore missing blocks
                {
                    int recoveryIndex = RemoteNbOrginalBlocks - dataBlock->m_rxControlBlock.m_recoveryCount + ir;
                    int blockIndex = m_cm256DescriptorBlocks[recoveryIndex].Index;
                    RemoteProtectedBlock *recoveredBlock =
                            (RemoteProtectedBlock *) m_cm256DescriptorBlocks[recoveryIndex].Block;
                    memcpy((void *) &(dataBlock->m_superBlocks[blockIndex].m_protectedBlock), recoveredBlock, sizeof(RemoteProtectedBlock));
                    if ((blockIndex == 0) && !dataBlock->m_rxControlBlock.m_metaRetrieved) {
                        dataBlock->m_rxControlBlock.m_metaRetrieved = true;
                    }
                }
            }
        }

        // Validate block zero and retrieve its data
        if (dataBlock->m_rxControlBlock.m_metaRetrieved)
        {
            RemoteMetaDataFEC *metaData = (RemoteMetaDataFEC *) &(dataBlock->m_superBlocks[0].m_protectedBlock);
            boost::crc_32_type crc32;
            crc32.process_bytes(metaData, 20);

            if (crc32.checksum() == metaData->m_crc32)
            {
                if (!(m_currentMeta == *metaData))
                {
                    printMeta("RemoteSource::handleDataBlock", metaData);

                    if (m_currentMeta.m_sampleRate != metaData->m_sampleRate)
                    {
                        m_channelizer->configure(m_channelizer->getInputMessageQueue(), metaData->m_sampleRate, 0);
                        m_dataReadQueue.setSize(calculateDataReadQueueSize(metaData->m_sampleRate));
                    }
                }

                m_currentMeta = *metaData;
            }
            else
            {
                qWarning() << "RemoteSource::handleDataBlock: recovered meta: invalid CRC32";
            }
        }

        m_dataReadQueue.push(dataBlock); // Push into R/W buffer
    }
}

void RemoteSource::handleData()
{
    RemoteDataBlock* dataBlock;

    while (m_running && ((dataBlock = m_dataQueue.pop()) != 0)) {
        handleDataBlock(dataBlock);
    }
}

void RemoteSource::printMeta(const QString& header, RemoteMetaDataFEC *metaData)
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

uint32_t RemoteSource::calculateDataReadQueueSize(int sampleRate)
{
    // scale for 20 blocks at 48 kS/s. Take next even number.
    uint32_t maxSize = sampleRate / 2400;
    maxSize = (maxSize % 2 == 0) ? maxSize : maxSize + 1;
    qDebug("RemoteSource::calculateDataReadQueueSize: set max queue size to %u blocks", maxSize);
    return maxSize;
}

int RemoteSource::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setRemoteSourceSettings(new SWGSDRangel::SWGRemoteSourceSettings());
    response.getRemoteSourceSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int RemoteSource::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    RemoteSourceSettings settings = m_settings;

    if (channelSettingsKeys.contains("dataAddress")) {
        settings.m_dataAddress = *response.getRemoteSourceSettings()->getDataAddress();
    }
    if (channelSettingsKeys.contains("dataPort"))
    {
        int dataPort = response.getRemoteSourceSettings()->getDataPort();

        if ((dataPort < 1024) || (dataPort > 65535)) {
            settings.m_dataPort = 9090;
        } else {
            settings.m_dataPort = dataPort;
        }
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getRemoteSourceSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getRemoteSourceSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getRemoteSourceSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getRemoteSourceSettings()->getReverseApiAddress() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getRemoteSourceSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getRemoteSourceSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getRemoteSourceSettings()->getReverseApiChannelIndex();
    }

    MsgConfigureRemoteSource *msg = MsgConfigureRemoteSource::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("RemoteSource::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRemoteSource *msgToGUI = MsgConfigureRemoteSource::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int RemoteSource::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setRemoteSourceReport(new SWGSDRangel::SWGRemoteSourceReport());
    response.getRemoteSourceReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void RemoteSource::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const RemoteSourceSettings& settings)
{
    if (response.getRemoteSourceSettings()->getDataAddress()) {
        *response.getRemoteSourceSettings()->getDataAddress() = settings.m_dataAddress;
    } else {
        response.getRemoteSourceSettings()->setDataAddress(new QString(settings.m_dataAddress));
    }

    response.getRemoteSourceSettings()->setDataPort(settings.m_dataPort);
    response.getRemoteSourceSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getRemoteSourceSettings()->getTitle()) {
        *response.getRemoteSourceSettings()->getTitle() = settings.m_title;
    } else {
        response.getRemoteSourceSettings()->setTitle(new QString(settings.m_title));
    }

    response.getRemoteSourceSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getRemoteSourceSettings()->getReverseApiAddress()) {
        *response.getRemoteSourceSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getRemoteSourceSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getRemoteSourceSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getRemoteSourceSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getRemoteSourceSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void RemoteSource::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    struct timeval tv;
    gettimeofday(&tv, 0);

    response.getRemoteSourceReport()->setTvSec(tv.tv_sec);
    response.getRemoteSourceReport()->setTvUSec(tv.tv_usec);
    response.getRemoteSourceReport()->setQueueSize(m_dataReadQueue.size());
    response.getRemoteSourceReport()->setQueueLength(m_dataReadQueue.length());
    response.getRemoteSourceReport()->setSamplesCount(m_dataReadQueue.readSampleCount());
    response.getRemoteSourceReport()->setCorrectableErrorsCount(m_nbCorrectableErrors);
    response.getRemoteSourceReport()->setUncorrectableErrorsCount(m_nbUncorrectableErrors);
    response.getRemoteSourceReport()->setNbOriginalBlocks(m_currentMeta.m_nbOriginalBlocks);
    response.getRemoteSourceReport()->setNbFecBlocks(m_currentMeta.m_nbFECBlocks);
    response.getRemoteSourceReport()->setCenterFreq(m_currentMeta.m_centerFrequency);
    response.getRemoteSourceReport()->setSampleRate(m_currentMeta.m_sampleRate);
    response.getRemoteSourceReport()->setDeviceCenterFreq(m_deviceAPI->getSampleSink()->getCenterFrequency()/1000);
    response.getRemoteSourceReport()->setDeviceSampleRate(m_deviceAPI->getSampleSink()->getSampleRate());
}

void RemoteSource::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const RemoteSourceSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setTx(1);
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("RemoteSource"));
    swgChannelSettings->setRemoteSourceSettings(new SWGSDRangel::SWGRemoteSourceSettings());
    SWGSDRangel::SWGRemoteSourceSettings *swgRemoteSourceSettings = swgChannelSettings->getRemoteSourceSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("dataAddress") || force) {
        swgRemoteSourceSettings->setDataAddress(new QString(settings.m_dataAddress));
    }
    if (channelSettingsKeys.contains("dataPort") || force) {
        swgRemoteSourceSettings->setDataPort(settings.m_dataPort);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgRemoteSourceSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgRemoteSourceSettings->setTitle(new QString(settings.m_title));
    }

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex)
            .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer=new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgChannelSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);

    delete swgChannelSettings;
}

void RemoteSource::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "RemoteSource::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("RemoteSource::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
