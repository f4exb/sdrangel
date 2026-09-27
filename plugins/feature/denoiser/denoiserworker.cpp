///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include "dsp/wavfilerecord.h"
#include "audio/audiodevicemanager.h"
#include "dsp/dspengine.h"
#include "pipes/datapipes.h"
#include "maincore.h"
#include "rnnoise.h"
#include <algorithm>
#include <cmath>

#include "denoiserworker.h"
#include "vst3effect.h"

const int DenoiserWorker::m_levelNbSamples = 480; // 10 ms at 48 kHz

MESSAGE_CLASS_DEFINITION(DenoiserWorker::MsgConfigureDenoiserWorker, Message)
MESSAGE_CLASS_DEFINITION(DenoiserWorker::MsgConnectFifo, Message)
MESSAGE_CLASS_DEFINITION(DenoiserWorker::MsgReportNvidiaStatus, Message)
MESSAGE_CLASS_DEFINITION(DenoiserWorker::MsgReportVst3Status, Message)

DenoiserWorker::DenoiserWorker(QObject *parent) :
    QObject(parent),
    m_dataFifo(nullptr),
    m_demodProducer(nullptr),
    m_sinkSampleRate(0),
    m_msgQueueToFeature(nullptr),
    m_magsq(0.0),
    m_channelPowerAvg(),
    m_wavFileRecord(nullptr),
    m_recordSilenceNbSamples(0),
    m_recordSilenceCount(0),
    m_nbBytes(0),
    m_rnnoiseFill(0),
    m_nvidiaFill(0),
    m_nvidiaInitAttempted(false),
    m_nvidiaReady(false),
    m_nvidiaResetPending(false),
    m_nvidiaStatusReported(false),
    m_nvidiaGeneration(0),
    m_vst3Effect(nullptr),
    m_vst3Fill(0),
    m_vst3FailureReported(false)
{
	m_audioBuffer.resize(4800);
	m_audioBufferFill = 0;
    m_audioFifo.setSize(4800 * 4);
    DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(getAudioFifo(), getInputMessageQueue());
    m_rnnoiseState = rnnoise_create(nullptr);
}

DenoiserWorker::~DenoiserWorker()
{
    m_inputMessageQueue.clear();
    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(getAudioFifo());
    rnnoise_destroy(m_rnnoiseState);
    m_nvidiaDenoiser.shutdown();
}

void DenoiserWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

void DenoiserWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_wavFileRecord = new WavFileRecord(m_sinkSampleRate);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

void DenoiserWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    if (m_wavFileRecord)
    {
        delete m_wavFileRecord;
        m_wavFileRecord = nullptr;
    }
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

void DenoiserWorker::feedPart(
    const QByteArray::const_iterator& begin,
    const QByteArray::const_iterator& end,
    DataFifo::DataType dataType
)
{
    int nbBytes;

    switch(dataType)
    {
    case DataFifo::DataTypeCI16:
        nbBytes = 4;
        break;
    case DataFifo::DataTypeI16:
    default:
        nbBytes = 2;
    }

    if ((nbBytes != m_nbBytes) && m_wavFileRecord)
    {
        m_wavFileRecord->stopRecording();
        m_wavFileRecord->setMono(nbBytes == 2);
    }

    m_nbBytes = nbBytes;
    int countSamples = (end - begin) / nbBytes;

    m_sampleBuffer.clear();
    m_sampleBuffer.reserve(countSamples);

    for (int i = 0; i < countSamples; i++) {
        processSample(dataType, begin, i);
    }

    flushAudio();

    if (m_settings.m_recordToFile && m_wavFileRecord)
    {
        for (const auto& sample : m_sampleBuffer) {
            writeSampleToFile(sample);
        }
    }

    m_sampleBuffer.clear();
}

void DenoiserWorker::flushAudio()
{
    if (m_audioBufferFill == 0) return;

    if (m_demodProducer)
    {
        QList<ObjectPipe*> pipes;
        MainCore::instance()->getDataPipes().getDataPipes(m_demodProducer, "demod", pipes);
        const auto *data = reinterpret_cast<const quint8*>(m_audioBuffer.data());
        const unsigned int byteCount = static_cast<unsigned int>(m_audioBufferFill * sizeof(AudioSample));

        for (const auto& pipe : pipes)
        {
            DataFifo *fifo = qobject_cast<DataFifo*>(pipe->m_element);
            if (fifo) {
                fifo->write(data, byteCount, DataFifo::DataTypeCI16);
            }
        }
    }

    if (m_settings.m_audioMute) {
        std::fill_n(m_audioBuffer.begin(), m_audioBufferFill, AudioSample{0, 0});
    }

    const std::size_t written = m_audioFifo.write(reinterpret_cast<const quint8*>(m_audioBuffer.data()), m_audioBufferFill);
    if (written != m_audioBufferFill) 
    {
        qDebug("DenoiserWorker::flushAudio: %lu/%lu audio samples written", written, m_audioBufferFill);
        m_audioFifo.clear();
    }
    m_audioBufferFill = 0;
}

void DenoiserWorker::writeSampleToFile(const Sample& sample)
{
    if (!m_wavFileRecord) {
        return;
    }

    if (SDR_RX_SAMP_SZ == 16)
    {
        if (m_nbBytes == 2) {
            m_wavFileRecord->writeMono(sample.m_real);
        } else {
            m_wavFileRecord->write(sample.m_real, sample.m_imag);
        }
    }
    else
    {
        if (m_nbBytes == 2) {
            m_wavFileRecord->writeMono(sample.m_real >> 8);
        } else {
            m_wavFileRecord->write(sample.m_real >> 8, sample.m_imag >> 8);
        }
    }
}

void DenoiserWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()))
    {
        const Message& cmd = *message;
        if (!handleMessage(cmd)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }
        delete message;
    }
}

bool DenoiserWorker::handleMessage(const Message& cmd)
{
    if (MsgConfigureDenoiserWorker::match(cmd))
    {
        const MsgConfigureDenoiserWorker& conf = static_cast<const MsgConfigureDenoiserWorker&>(cmd);
        qDebug("DenoiserWorker::handleMessage: MsgConfigureDenoiserWorker");
        applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());
        return true;
    }
    else if (MsgConnectFifo::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConnectFifo& msg = (MsgConnectFifo&) cmd;
        m_dataFifo = msg.getFifo();
        bool doConnect = msg.getConnect();
        qDebug("DenoiserWorker::handleMessage: MsgConnectFifo: %s", (doConnect ? "connect" : "disconnect"));

        if (doConnect) {
            QObject::connect(
                m_dataFifo,
                &DataFifo::dataReady,
                this,
                &DenoiserWorker::handleData,
                Qt::QueuedConnection
            );
        }
        else
        {
            QObject::disconnect(
                m_dataFifo,
                &DataFifo::dataReady,
                this,
                &DenoiserWorker::handleData
            );
        }

        return true;
    }

    return false;
}

void DenoiserWorker::applySettings(const DenoiserSettings& settings, const QStringList& settingsKeys, bool force)
{
    QMutexLocker mutexLocker(&m_mutex);
    qDebug() << "DenoiserWorker::applySettings" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    if (settingsKeys.contains("fileRecordName")  || force)
    {
        if (m_wavFileRecord)
        {
            QStringList dotBreakout = settings.m_fileRecordName.split(QLatin1Char('.'));

            if (dotBreakout.size() > 1)
            {
                QString extension = dotBreakout.last();

                if (extension != "wav") {
                    dotBreakout.last() = "wav";
                }
            }
            else
            {
                dotBreakout.append("wav");
            }

            QString newFileRecordName = dotBreakout.join(QLatin1Char('.'));
            QString fileBase;
            FileRecordInterface::guessTypeFromFileName(newFileRecordName, fileBase);
            qDebug("DemodAnalyzerWorker::applySettings: newFileRecordName: %s fileBase: %s", qPrintable(newFileRecordName), qPrintable(fileBase));
            m_wavFileRecord->setFileName(fileBase);
        }
    }

    if (settingsKeys.contains("recordToFile")  || force)
    {
        if (m_wavFileRecord)
        {
            if (settings.m_recordToFile)
            {
                if (!m_wavFileRecord->isRecording()) {
                    m_wavFileRecord->startRecording();
                }
            }
            else
            {
                if (m_wavFileRecord->isRecording()) {
                    m_wavFileRecord->stopRecording();
                }
            }

            m_recordSilenceCount = 0;
        }
    }

    if ((settingsKeys.contains("audioDeviceName") && (settings.m_audioDeviceName != m_settings.m_audioDeviceName)) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getOutputDeviceIndex(settings.m_audioDeviceName);
        audioDeviceManager->removeAudioSink(getAudioFifo());
        audioDeviceManager->addAudioSink(getAudioFifo(), getInputMessageQueue(), audioDeviceIndex);
        unsigned int audioSampleRate = audioDeviceManager->getOutputSampleRate(audioDeviceIndex);
        qDebug() << "DenoiserWorker::applySettings: audio device name:" << settings.m_audioDeviceName
                    << " index:" << audioDeviceIndex << " sample rate:" << audioSampleRate;
        // TODO: handle sample rate change
    }

    if (settingsKeys.contains("enableDenoiser") || settingsKeys.contains("denoiserType") ||
        settingsKeys.contains("nvidiaIntensity") ||
        settingsKeys.contains("nvidiaVad") || force)
    {
        m_rnnoiseFill = 0;
        resetNvidia();
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
    if (force || settingsKeys.contains("denoiserType") || settingsKeys.contains("vst3ModulePath") ||
        settingsKeys.contains("vst3ClassId") || settingsKeys.contains("enableDenoiser"))
    {
        m_vst3Fill = 0;
        m_vst3FailureReported = false;
    }

    // applySettings runs on the worker thread. Release the CUDA effect as soon as
    // this backend is no longer in use, even if no more NVIDIA samples arrive.
    if (m_nvidiaResetPending && (!m_settings.m_enableDenoiser ||
        m_settings.m_denoiserType != DenoiserSettings::DenoiserType::DenoiserType_Nvidia))
    {
        m_nvidiaDenoiser.shutdown();
        m_nvidiaResetPending = false;
    }
}

void DenoiserWorker::applySampleRate(int sampleRate)
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_sinkSampleRate == sampleRate) {
        return;
    }

    m_sinkSampleRate = sampleRate;
    resetNvidia();
    m_vst3Fill = 0;
    m_vst3FailureReported = false;

    if (m_wavFileRecord)
    {
        if (m_wavFileRecord->isRecording()) {
            m_wavFileRecord->stopRecording();
        }

        m_wavFileRecord->setSampleRate(m_sinkSampleRate);
    }
}

void DenoiserWorker::setVst3Effect(Vst3Effect *effect)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_vst3Effect = effect;
    m_vst3Fill = 0;
    m_vst3FailureReported = false;
}

void DenoiserWorker::resetNvidia()
{
    // Caller holds m_mutex. The model itself is released on the worker thread.
    m_nvidiaFill = 0;
    m_nvidiaInitAttempted = false;
    m_nvidiaReady = false;
    m_nvidiaResetPending = true;
    m_nvidiaStatusReported = false;
    ++m_nvidiaGeneration;
}

void DenoiserWorker::prepareNvidia()
{
    int sampleRate;
    float intensity;
    bool vad;
    quint32 generation;

    {
        QMutexLocker mutexLocker(&m_mutex);

        if (m_nvidiaInitAttempted || !m_settings.m_enableDenoiser ||
            m_settings.m_denoiserType != DenoiserSettings::DenoiserType::DenoiserType_Nvidia) {
            return;
        }

        if (m_nvidiaResetPending)
        {
            m_nvidiaDenoiser.shutdown();
            m_nvidiaResetPending = false;
        }

        m_nvidiaInitAttempted = true;
        sampleRate = m_sinkSampleRate;
        intensity = qBound(0, m_settings.m_nvidiaIntensity, 100) / 100.0f;
        vad = m_settings.m_nvidiaVad;
        generation = m_nvidiaGeneration;
    }

    // Loading the model can take seconds. Do it without m_mutex so the feature
    // thread is not blocked by applySampleRate() or setVst3Effect() meanwhile.
    // m_nvidiaDenoiser is only used on this thread, as NVIDIA requires.
    QString error;
    bool ready = false;

    if (sampleRate != 48000) {
        error = QStringLiteral("NVIDIA Noise Removal requires 48 kHz input; got %1 Hz.").arg(sampleRate);
    } else {
        ready = m_nvidiaDenoiser.initialize(error, intensity, vad);
    }

    QMutexLocker mutexLocker(&m_mutex);

    if (generation != m_nvidiaGeneration) {
        return; // Settings or rate changed while loading; reset and retry on the next data.
    }

    m_nvidiaReady = ready;

    if (!ready) {
        reportNvidiaError(error + QStringLiteral(" Audio is passing through."));
    }
}

void DenoiserWorker::handleData()
{
    prepareNvidia();

    QMutexLocker mutexLocker(&m_mutex);

    while ((m_dataFifo->fill() > 0) && (m_inputMessageQueue.size() == 0))
    {
		QByteArray::iterator part1begin;
		QByteArray::iterator part1end;
		QByteArray::iterator part2begin;
		QByteArray::iterator part2end;
        DataFifo::DataType dataType;

        std::size_t count = m_dataFifo->readBegin(m_dataFifo->fill(), &part1begin, &part1end, &part2begin, &part2end, dataType);

		// first part of FIFO data
        if (part1begin != part1end) {
            feedPart(part1begin, part1end, dataType);
        }

		// second part of FIFO data (used when block wraps around)
		if (part2begin != part2end) {
            feedPart(part2begin, part2end, dataType);
        }

		m_dataFifo->readCommit((unsigned int) count);
    }

    qreal rmsLevel, peakLevel;
    int numSamples;
    getLevels(rmsLevel, peakLevel, numSamples);
    emit levelChanged(rmsLevel, peakLevel, numSamples);
}

void DenoiserWorker::processSample(
    DataFifo::DataType dataType,
    const QByteArray::const_iterator& begin,
    int i
)
{
    switch(dataType)
    {
        case DataFifo::DataTypeI16:
        {
            int16_t *s = (int16_t*) begin;
            double samplefp = s[i] * (m_settings.m_volumeTenths / 10.0);
            double re = samplefp / (double) std::numeric_limits<int16_t>::max();
            calculateLevel(re);
            m_magsq = re*re;
            m_channelPowerAvg(m_magsq);

            if (!m_settings.m_enableDenoiser || m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_None) {
                processI16DenoiserNone(samplefp);
            } else if (m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_RNnoise) {
                processI16DenoiserRNNoise(samplefp);
            } else if (m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_Nvidia) {
                processNvidiaSample(samplefp);
            } else if (m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_Vst3) {
                processVst3Sample(samplefp, samplefp);
            }
        }
        break;
        case DataFifo::DataTypeCI16:
        {
            int16_t *s = (int16_t*) begin;
            double samplefpRe = s[2*i]   * (m_settings.m_volumeTenths / 10.0);
            double samplefpIm = s[2*i+1] * (m_settings.m_volumeTenths / 10.0);
            double re = samplefpRe / (double) std::numeric_limits<int16_t>::max();
            double im = samplefpIm / (double) std::numeric_limits<int16_t>::max();
            calculateLevel((re + im) / 2.0);
            m_magsq = re*re + im*im;
            m_channelPowerAvg(m_magsq);

            if (!m_settings.m_enableDenoiser || m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_None) {
                processCI16DenoiserNone(samplefpRe, samplefpIm);
            } else if (m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_RNnoise) {
                processCI16DenoiserRNNoise(samplefpRe, samplefpIm);
            } else if (m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_Nvidia) {
                processNvidiaSample((samplefpRe + samplefpIm) / 2.0);
            } else if (m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_Vst3) {
                processVst3Sample(samplefpRe, samplefpIm);
            }
        }
        break;
    }
}

void DenoiserWorker::processI16DenoiserNone(const double& samplefp)
{
    if (m_channelPowerAvg.asDouble() > 1e-4) { // -40 dB threshold
        m_sampleBuffer.push_back(Sample(samplefp, 0));
    }

    m_audioBuffer[m_audioBufferFill].l = static_cast<int16_t>(samplefp);
    m_audioBuffer[m_audioBufferFill].r = static_cast<int16_t>(samplefp);
    ++m_audioBufferFill;

    if (m_audioBufferFill >= m_audioBuffer.size())
    {
        flushAudio();
    }
}

void DenoiserWorker::processI16DenoiserRNNoise(const double& samplefp)
{
    // feed RNNoise input buffer
    m_rnnoiseIn[m_rnnoiseFill] = static_cast<float>(samplefp); // already in [-32768..32767] range
    m_rnnoiseFill++;

    if (m_rnnoiseFill >= 480)
    {
        // process RNNoise frame
        rnnoise_process_frame(m_rnnoiseState, m_rnnoiseOut, m_rnnoiseIn);

        // output RNNoise processed samples
        for (int j = 0; j < 480; j++)
        {
            float outSample = m_rnnoiseOut[j];

            if (m_channelPowerAvg.asDouble() > 1e-4) { // -40 dB threshold
                m_sampleBuffer.push_back(Sample(outSample * 181, 0)); // 181 = sqrt(32768)
            }

            int16_t audioSample = static_cast<int16_t>(outSample);
            m_audioBuffer[m_audioBufferFill].l = audioSample;
            m_audioBuffer[m_audioBufferFill].r = audioSample;
            ++m_audioBufferFill;

            if (m_audioBufferFill >= m_audioBuffer.size())
            {
                flushAudio();
            }
        }

        m_rnnoiseFill = 0;
    }
}

void DenoiserWorker::processNvidiaSample(const double& samplefp)
{
    if (m_nvidiaResetPending)
    {
        m_nvidiaDenoiser.shutdown();
        m_nvidiaResetPending = false;
    }

    // NvAFX_Run uses mono, normalized floating point audio in 10 ms frames.
    m_nvidiaIn[m_nvidiaFill++] = static_cast<float>(std::max(-1.0, std::min(1.0, samplefp / 32768.0)));

    if (m_nvidiaFill < m_nvidiaFrameSize) {
        return;
    }

    // The model is loaded by prepareNvidia() outside m_mutex; pass through until it is ready.
    bool processed = false;

    if (m_nvidiaReady)
    {
        QString error;
        processed = m_nvidiaDenoiser.process(m_nvidiaIn, m_nvidiaOut, error);

        if (!processed)
        {
            m_nvidiaReady = false;
            reportNvidiaError(error + QStringLiteral(" Audio is passing through."));
        }
    }

    if (processed && !m_nvidiaStatusReported)
    {
        if (m_msgQueueToFeature) {
            m_msgQueueToFeature->push(MsgReportNvidiaStatus::create(QString()));
        }
        m_nvidiaStatusReported = true;
    }

    for (int j = 0; j < m_nvidiaFrameSize; ++j)
    {
        const double normalized = processed && std::isfinite(m_nvidiaOut[j]) ? m_nvidiaOut[j] : m_nvidiaIn[j];
        const double sample = std::max(-32768.0, std::min(32767.0, normalized * 32768.0));
        const int16_t audioSample = static_cast<int16_t>(sample);

        if (m_channelPowerAvg.asDouble() > 1e-4) {
            m_sampleBuffer.push_back(Sample(static_cast<FixReal>(audioSample) * (SDR_RX_SAMP_SZ == 24 ? 256 : 1), 0));
        }

        m_audioBuffer[m_audioBufferFill].l = audioSample;
        m_audioBuffer[m_audioBufferFill].r = audioSample;
        ++m_audioBufferFill;

        if (m_audioBufferFill >= m_audioBuffer.size())
        {
            flushAudio();
        }
    }

    m_nvidiaFill = 0;
}

void DenoiserWorker::reportNvidiaError(const QString& error)
{
    qWarning() << "DenoiserWorker:" << error;
    m_nvidiaStatusReported = false;
    if (m_msgQueueToFeature) {
        m_msgQueueToFeature->push(MsgReportNvidiaStatus::create(error));
    }
}

void DenoiserWorker::processVst3Sample(double left, double right)
{
    const int i = m_vst3Fill++;
    m_vst3Input[0][i] = static_cast<float>(std::max(-1.0, std::min(1.0, left / 32768.0)));
    m_vst3Input[1][i] = static_cast<float>(std::max(-1.0, std::min(1.0, right / 32768.0)));
    if (m_vst3Fill != m_vst3BlockSize) {
        return;
    }

    bool processed = false;
    if (m_vst3Effect)
    {
        QString error;
        processed = m_vst3Effect->process(m_vst3Input[0], m_vst3Input[1],
            m_vst3Output[0], m_vst3Output[1], m_vst3BlockSize, error);
        if (!processed && !m_vst3FailureReported)
        {
            qWarning() << "DenoiserWorker:" << error;
            if (m_msgQueueToFeature)
            {
                m_msgQueueToFeature->push(MsgReportVst3Status::create(
                    QStringLiteral("VST3: %1. Audio is passing through.").arg(error)));
            }
            m_vst3FailureReported = true;
        }
    }
    for (int j = 0; j < m_vst3BlockSize; ++j)
    {
        const float l = processed && std::isfinite(m_vst3Output[0][j]) ? m_vst3Output[0][j] : m_vst3Input[0][j];
        const float r = processed && std::isfinite(m_vst3Output[1][j]) ? m_vst3Output[1][j] : m_vst3Input[1][j];
        const int16_t leftSample = static_cast<int16_t>(std::max(-32768.0f, std::min(32767.0f, l * 32768.0f)));
        const int16_t rightSample = static_cast<int16_t>(std::max(-32768.0f, std::min(32767.0f, r * 32768.0f)));
        if (m_channelPowerAvg.asDouble() > 1e-4)
        {
            const int scale = SDR_RX_SAMP_SZ == 24 ? 256 : 1;
            m_sampleBuffer.push_back(Sample(static_cast<FixReal>(leftSample) * scale,
                static_cast<FixReal>(rightSample) * scale));
        }
        m_audioBuffer[m_audioBufferFill].l = leftSample;
        m_audioBuffer[m_audioBufferFill].r = rightSample;
        if (++m_audioBufferFill >= m_audioBuffer.size())
        {
            flushAudio();
        }
    }
    m_vst3Fill = 0;
}

void DenoiserWorker::processCI16DenoiserNone(const double& samplefpRe, const double& samplefpIm)
{
    if (m_channelPowerAvg.asDouble() > 1e-4) { // -40 dB threshold
        m_sampleBuffer.push_back(Sample(samplefpRe, samplefpIm));
    }

    m_audioBuffer[m_audioBufferFill].l = static_cast<int16_t>(samplefpRe);
    m_audioBuffer[m_audioBufferFill].r = static_cast<int16_t>(samplefpIm);
    ++m_audioBufferFill;

    if (m_audioBufferFill >= m_audioBuffer.size())
    {
        flushAudio();
    }
}

void DenoiserWorker::processCI16DenoiserRNNoise(const double& samplefpRe, const double& samplefpIm)
{
    Q_UNUSED(samplefpRe);
    Q_UNUSED(samplefpIm);
    // feed RNNoise input buffer
    m_rnnoiseIn[m_rnnoiseFill] = static_cast<float>((samplefpRe + samplefpIm) / 2.0f); // average I/Q in [-32768..32767] range
    m_rnnoiseFill++;

    if (m_rnnoiseFill >= 480)
    {
        // process RNNoise frame
        rnnoise_process_frame(m_rnnoiseState, m_rnnoiseOut, m_rnnoiseIn);

        // output RNNoise processed samples
        for (int j = 0; j < 480; j++)
        {
            float outSample = m_rnnoiseOut[j];

            if (m_channelPowerAvg.asDouble() > 1e-4) { // -40 dB threshold
                m_sampleBuffer.push_back(Sample(outSample * 181, outSample * 181)); // 181 = sqrt(32768)
            }

            int16_t audioSample = static_cast<int16_t>(outSample);
            m_audioBuffer[m_audioBufferFill].l = audioSample;
            m_audioBuffer[m_audioBufferFill].r = audioSample;
            ++m_audioBufferFill;

            if (m_audioBufferFill >= m_audioBuffer.size())
            {
                flushAudio();
            }
        }

        m_rnnoiseFill = 0;
    }
}

void DenoiserWorker::calculateLevel(const Real& sample)
{
    if (m_levelCalcCount < m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), sample);
        m_levelSum += sample * sample;
        m_levelCalcCount++;
    }
    else
    {
        m_rmsLevel = sqrt(m_levelSum / m_levelNbSamples);
        m_peakLevelOut = m_peakLevel;
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}
