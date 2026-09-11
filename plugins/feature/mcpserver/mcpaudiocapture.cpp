///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QThread>

#include "SWGChannelReport.h"
#include "SWGErrorResponse.h"

#include "maincore.h"
#include "channel/channelapi.h"
#include "device/deviceset.h"
#include "dsp/datafifo.h"
#include "dsp/wavfilerecord.h"
#include "pipes/datapipes.h"
#include "pipes/messagepipes.h"
#include "pipes/objectpipe.h"
#include "util/messagequeue.h"
#include "webapi/webapiadapterinterface.h"

#include "mcperror.h"
#include "mcpcapture.h"

namespace {

QJsonObject reportToJson(SWGSDRangel::SWGChannelReport& report)
{
    QJsonObject *obj = report.asJsonObject();
    QJsonObject result = *obj;
    delete obj;
    return result;
}

} // namespace

MCPAudioCapture::MCPAudioCapture(WebAPIAdapterInterface *adapter, QObject *parent) :
    QObject(parent),
    m_adapter(adapter),
    m_pendingChannel(nullptr),
    m_pendingFifo(nullptr),
    m_pendingReportQueue(nullptr),
    m_capturingChannel(nullptr),
    m_captureAborted(0)
{
}

// Registers the demod data pipe and the report message pipe on the channel. Runs on the main
// thread so the pipe objects are created with the same thread affinity as everything else.
bool MCPAudioCapture::attach(int deviceSetIndex, int channelIndex)
{
    m_pendingChannel = nullptr;
    m_pendingFifo = nullptr;
    m_pendingReportQueue = nullptr;
    m_attachError.clear();

    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();

    if ((deviceSetIndex < 0) || (deviceSetIndex >= (int) deviceSets.size()))
    {
        m_attachError = QString("There is no device set with index %1").arg(deviceSetIndex);
        return false;
    }

    ChannelAPI *channel = deviceSets[deviceSetIndex]->getChannelAt(channelIndex);

    if (!channel)
    {
        m_attachError = QString("There is no channel with index %1 in device set %2").arg(channelIndex).arg(deviceSetIndex);
        return false;
    }

    Attachment attachment;
    {
        QMutexLocker lock(&m_attachMutex);
        attachment = m_attachments.value(channel);
    }

    if (!attachment.m_fifo)
    {
        ObjectPipe *dataPipe = mainCore->getDataPipes().registerProducerToConsumer(channel, this, "demod");
        attachment.m_fifo = dataPipe ? qobject_cast<DataFifo*>(dataPipe->m_element) : nullptr;

        if (!attachment.m_fifo)
        {
            m_attachError = "Could not register the demod data pipe on the channel";
            return false;
        }

        connect(dataPipe, &ObjectPipe::toBeDeleted, this, &MCPAudioCapture::handlePipeToBeDeleted);
        attachment.m_fifo->setSize(96000 * 4);

        ObjectPipe *messagePipe = mainCore->getMessagePipes().registerProducerToConsumer(channel, this, "reportdemod");
        attachment.m_reportQueue = messagePipe ? qobject_cast<MessageQueue*>(messagePipe->m_element) : nullptr;

        QMutexLocker lock(&m_attachMutex);
        m_attachments.insert(channel, attachment);
    }

    // Discard whatever the channel wrote between captures
    attachment.m_fifo->reset();

    m_pendingChannel = channel;
    m_pendingFifo = attachment.m_fifo;
    m_pendingReportQueue = attachment.m_reportQueue;

    {
        QMutexLocker lock(&m_attachMutex);
        m_capturingChannel = channel;
        m_captureAborted.storeRelease(0);
    }

    // Ask the channel to report its audio sample rate on the pipe
    channel->getInputMessageQueue()->push(MainCore::MsgChannelDemodQuery::create());
    return true;
}

// The channel that produces the samples has gone away, so drop the pipes registered on it.
// Registrations are otherwise kept for the life of this object.
void MCPAudioCapture::handlePipeToBeDeleted(int reason, QObject *object)
{
    if (reason != 0) { // 0: producer (the channel) is being deleted
        return;
    }

    QMutexLocker lock(&m_attachMutex);

    if (m_attachments.remove(object) > 0) {
        qDebug("MCPAudioCapture::handlePipeToBeDeleted: channel %p removed", object);
    }

    // A capture in progress holds a raw pointer to this channel's FIFO, which the pipe
    // registry is about to destroy, so tell it to stop reading
    if (m_capturingChannel == object)
    {
        m_captureAborted.storeRelease(1);
        m_capturingChannel = nullptr;

        // Wait for any read already in progress to finish. This runs before the pipe registry
        // deletes the FIFO, so once it returns the capture will never touch it again.
        QMutexLocker fifoLock(&m_fifoMutex);
    }
}

// The channel answers MsgChannelDemodQuery with MsgChannelDemodReport carrying the audio sample rate
int MCPAudioCapture::reportedSampleRate(MessageQueue *queue, int timeoutMs)
{
    if (!queue) {
        return 0;
    }

    int sampleRate = 0;
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs)
    {
        {
            // The queue belongs to the report pipe and is freed with it when the channel goes,
            // so read it under the same lock and abort flag as the sample FIFO
            QMutexLocker fifoLock(&m_fifoMutex);

            if (m_captureAborted.loadAcquire()) {
                return 0;
            }

            Message *message;

            while ((message = queue->pop()) != nullptr)
            {
                if (MainCore::MsgChannelDemodReport::match(*message)) {
                    sampleRate = ((MainCore::MsgChannelDemodReport *) message)->getSampleRate();
                }

                delete message;
            }
        }

        if (sampleRate > 0) {
            return sampleRate;
        }

        QThread::msleep(20);
    }

    return 0;
}

// Fallback for channels that do not answer the query: most demodulator reports carry the rate
int MCPAudioCapture::sampleRateFromReport(int deviceSetIndex, int channelIndex)
{
    SWGSDRangel::SWGChannelReport response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();

    if (m_adapter->devicesetChannelReportGet(deviceSetIndex, channelIndex, response, error) / 100 != 2) {
        return 0;
    }

    QJsonObject json = reportToJson(response);

    for (const QString& key : json.keys())
    {
        if (!key.endsWith("Report") || !json[key].isObject()) {
            continue;
        }

        QJsonObject sub = json[key].toObject();

        for (const QString& rateKey : {"audioSampleRate", "channelSampleRate", "sampleRate"})
        {
            if (sub.contains(rateKey) && (sub[rateKey].toInt(0) > 0)) {
                return sub[rateKey].toInt();
            }
        }
    }

    return 0;
}

QJsonObject MCPAudioCapture::capture(int deviceSetIndex, int channelIndex, double seconds, const QString& path, bool inlineAudio)
{
    QMutexLocker captureLock(&m_captureMutex);
    bool attached = false;
    QMetaObject::invokeMethod(this, "attach", Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(bool, attached), Q_ARG(int, deviceSetIndex), Q_ARG(int, channelIndex));

    if (!attached) {
        throw MCPToolError(m_attachError);
    }

    DataFifo *fifo = m_pendingFifo;
    MessageQueue *reportQueue = m_pendingReportQueue;
    QObject *channel = m_pendingChannel;
    QByteArray samples;
    int nbBytesPerSample = 2;
    int sampleRate = reportedSampleRate(reportQueue, 1000);

    if (sampleRate <= 0) {
        sampleRate = sampleRateFromReport(deviceSetIndex, channelIndex);
    }

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < (qint64) (seconds * 1000.0))
    {
        bool idle = false;
        {
            // The abort flag is tested under the same lock the teardown handler takes, so the
            // FIFO cannot be freed between the test and the reads below
            QMutexLocker fifoLock(&m_fifoMutex);

            if (m_captureAborted.loadAcquire()) {
                break;
            }

            unsigned int fill = fifo->fill();

            if (fill == 0)
            {
                idle = true;
            }
            else
            {
                QByteArray::iterator part1begin, part1end, part2begin, part2end;
                DataFifo::DataType dataType;
                unsigned int count = fifo->readBegin(fill, &part1begin, &part1end, &part2begin, &part2end, dataType);
                nbBytesPerSample = (dataType == DataFifo::DataTypeCI16) ? 4 : 2;

                if (part1begin != part1end) {
                    samples.append(&(*part1begin), (int) (part1end - part1begin));
                }
                if (part2begin != part2end) {
                    samples.append(&(*part2begin), (int) (part2end - part2begin));
                }

                fifo->readCommit(count);
            }
        }

        if (idle) {
            QThread::msleep(10);
        }
    }

    bool aborted = m_captureAborted.loadAcquire() != 0;
    {
        QMutexLocker lock(&m_attachMutex);

        if (m_capturingChannel == channel) {
            m_capturingChannel = nullptr;
        }
    }

    if (aborted)
    {
        throw MCPToolError(QString("Channel %1:%2 was removed while its audio was being captured, so the recording was abandoned")
            .arg(deviceSetIndex).arg(channelIndex));
    }

    if (samples.isEmpty())
    {
        throw MCPToolError(QString("No audio was produced by channel %1:%2 in %3 seconds. Only demodulators that feed the Demod "
            "Analyzer provide audio (NFMDemod, AMDemod, SSBDemod, WFMDemod, BFMDemod, DSDDemod, M17Demod and similar). "
            "Check also that the device is running.").arg(deviceSetIndex).arg(channelIndex).arg(seconds));
    }

    if (sampleRate <= 0) {
        throw MCPToolError("The channel did not report its audio sample rate, so the audio cannot be written");
    }

    bool mono = nbBytesPerSample == 2;
    int nbSamples = samples.size() / nbBytesPerSample;
    const qint16 *data = (const qint16 *) samples.constData();
    int nbShorts = samples.size() / 2;
    int peak = 0;

    // Peak level lets the caller tell silence (a closed squelch) from a real recording
    for (int i = 0; i < nbShorts; i++)
    {
        int value = qAbs((int) data[i]);

        if (value > peak) {
            peak = value;
        }
    }

    QString fileBase = path;

    if (fileBase.endsWith(".wav", Qt::CaseInsensitive)) {
        fileBase.chop(4);
    }

    WavFileRecord writer(fileBase);
    writer.setFileBaseIsFileName(true);
    writer.setSampleRate(sampleRate);
    writer.setMono(mono);

    if (!writer.startRecording()) {
        throw MCPToolError(QString("Could not write the audio file %1.wav").arg(fileBase));
    }

    if (mono)
    {
        writer.writeMono((qint16 *) samples.data(), nbSamples);
    }
    else
    {
        for (int i = 0; i < nbSamples; i++) {
            writer.write(data[2 * i], data[2 * i + 1]);
        }
    }

    writer.stopRecording();

    QString fileName = fileBase + ".wav";
    QFileInfo info(fileName);
    QJsonObject result;
    result["deviceSetIndex"] = deviceSetIndex;
    result["channelIndex"] = channelIndex;
    result["path"] = info.absoluteFilePath();
    result["sizeBytes"] = (double) info.size();
    result["sampleRate"] = sampleRate;
    result["channels"] = mono ? 1 : 2;
    result["samples"] = nbSamples;
    result["seconds"] = nbSamples / (double) sampleRate;
    result["peakLevel"] = peak / 32768.0;

    if (peak < 32) {
        result["warning"] = "The audio is silent. The squelch may be closed, the channel may be muted, or there may be no signal.";
    }

    if (inlineAudio)
    {
        QFile file(fileName);

        if (file.open(QIODevice::ReadOnly))
        {
            result["audioBase64"] = QString(file.readAll().toBase64());
            result["audioMimeType"] = "audio/wav";
        }
    }

    return result;
}
