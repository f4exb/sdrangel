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
#include "rnnoise.h"

#include "denoiserworker.h"

const int DenoiserWorker::m_levelNbSamples = 480; // 10 ms at 48 kHz

MESSAGE_CLASS_DEFINITION(DenoiserWorker::MsgConfigureDenoiserWorker, Message)
MESSAGE_CLASS_DEFINITION(DenoiserWorker::MsgConnectFifo, Message)

DenoiserWorker::DenoiserWorker(QObject *parent) :
    QObject(parent),
    m_dataFifo(nullptr),
    m_sinkSampleRate(0),
    m_msgQueueToFeature(nullptr),
    m_magsq(0.0),
    m_sampleBufferSize(0),
    m_channelPowerAvg(),
    m_wavFileRecord(nullptr),
    m_recordSilenceNbSamples(0),
    m_recordSilenceCount(0),
    m_nbBytes(0),
    m_rnnoiseFill(0)
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

    if (countSamples > m_sampleBufferSize)
    {
        m_sampleBuffer.resize(countSamples);
        m_sampleBufferSize = countSamples;
    }

    for (int i = 0; i < countSamples; i++) {
        processSample(dataType, begin, i);
    }

    if (m_settings.m_recordToFile && m_wavFileRecord)
    {
        for (const auto& sample : m_sampleBuffer) {
            writeSampleToFile(sample);
        }

        m_sampleBuffer.clear();
    }
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
    Message* message = nullptr;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        const Message& cmd = *message;
        handleMessage(cmd);
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

    if (settingsKeys.contains("enableDenoiser") || settingsKeys.contains("denoiserType") || force) {
        m_rnnoiseFill = 0;
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void DenoiserWorker::applySampleRate(int sampleRate)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sinkSampleRate = sampleRate;

    if (m_wavFileRecord)
    {
        if (m_wavFileRecord->isRecording()) {
            m_wavFileRecord->stopRecording();
        }

        m_wavFileRecord->setSampleRate(m_sinkSampleRate);
    }
}

void DenoiserWorker::handleData()
{
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
    // Periodic debug to verify runtime settings and branch selection
    // static uint32_t s_dbgCount = 0;
    // if ((s_dbgCount++ % 48000) == 0) { // approx. once per second at 48 kS/s
    //     qDebug() << "DenoiserWorker::processSample: dataType=" << (int)dataType
    //              << " enable=" << m_settings.m_enableDenoiser
    //              << " type=" << static_cast<int>(m_settings.m_denoiserType);
    // }

    switch(dataType)
    {
        case DataFifo::DataTypeI16: {
            int16_t *s = (int16_t*) begin;
            double re = s[i] / (double) std::numeric_limits<int16_t>::max();
            calculateLevel(re * (m_settings.m_volumeTenths / 10.0));
            m_magsq = re*re;
            m_channelPowerAvg(m_magsq);

            if ((!m_settings.m_enableDenoiser || m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_None) && !m_settings.m_audioMute)
            {
                // if ((s_dbgCount % 48000) == 1) {
                //     qDebug() << "DenoiserWorker::processSample[I16]: passthrough branch";
                // }
                m_sampleBuffer.push_back(Sample(re * SDR_RX_SCALEF, 0));
                m_audioBuffer[m_audioBufferFill].l = static_cast<int16_t>(s[i]*(m_settings.m_volumeTenths / 10.0f));
                m_audioBuffer[m_audioBufferFill].r = static_cast<int16_t>(s[i]*(m_settings.m_volumeTenths / 10.0f));
                ++m_audioBufferFill;

                if (m_audioBufferFill >= m_audioBuffer.size())
                {
                    std::size_t res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);
                    if (res != m_audioBufferFill)
                    {
                        qDebug("DenoiserWorker::processSample: %lu/%lu audio samples written", res, m_audioBufferFill);
                        m_audioFifo.clear();
                    }
                    m_audioBufferFill = 0;
                }
            }
            else if ((m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_RNnoise) && !m_settings.m_audioMute)
            {
                // if ((s_dbgCount % 48000) == 1) {
                //     qDebug() << "DenoiserWorker::processSample[I16]: RNNoise branch";
                // }
                // feed RNNoise input buffer
                m_rnnoiseIn[m_rnnoiseFill] = static_cast<float>(s[i])*(m_settings.m_volumeTenths / 10.0f); // already in [-32768..32767] range
                m_rnnoiseFill++;

                if (m_rnnoiseFill >= 480)
                {
                    // process RNNoise frame
                    rnnoise_process_frame(m_rnnoiseState, m_rnnoiseOut, m_rnnoiseIn);

                    // output RNNoise processed samples
                    for (int j = 0; j < 480; j++)
                    {
                        float outSample = m_rnnoiseOut[j];
                        m_sampleBuffer.push_back(Sample(outSample * 181, 0)); // 181 = sqrt(32768)
                        int16_t audioSample = static_cast<int16_t>(outSample);
                        m_audioBuffer[m_audioBufferFill].l = audioSample;
                        m_audioBuffer[m_audioBufferFill].r = audioSample;
                        ++m_audioBufferFill;

                        if (m_audioBufferFill >= m_audioBuffer.size())
                        {
                            std::size_t res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);
                            if (res != m_audioBufferFill)
                            {
                                qDebug("DenoiserWorker::processSample: %lu/%lu audio samples written", res, m_audioBufferFill);
                                m_audioFifo.clear();
                            }
                            m_audioBufferFill = 0;
                        }
                    }

                    m_rnnoiseFill = 0;
                }
            }
        }
        break;
        case DataFifo::DataTypeCI16: {
            int16_t *s = (int16_t*) begin;
            double re = s[2*i]   / (double) std::numeric_limits<int16_t>::max();
            double im = s[2*i+1] / (double) std::numeric_limits<int16_t>::max();
            calculateLevel((re + im) * (m_settings.m_volumeTenths / 20.0));
            m_magsq = re*re + im*im;
            m_channelPowerAvg(m_magsq);

            if ((!m_settings.m_enableDenoiser || m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_None) && !m_settings.m_audioMute)
            {
                // if ((s_dbgCount % 48000) == 1) {
                //     qDebug() << "DenoiserWorker::processSample[CI16]: passthrough branch";
                // }
                m_sampleBuffer.push_back(Sample(re * SDR_RX_SCALEF, im * SDR_RX_SCALEF));
                m_audioBuffer[m_audioBufferFill].l = static_cast<int16_t>(s[2*i]*(m_settings.m_volumeTenths / 10.0f));
                m_audioBuffer[m_audioBufferFill].r = static_cast<int16_t>(s[2*i+1]*(m_settings.m_volumeTenths / 10.0f));
                ++m_audioBufferFill;

                if (m_audioBufferFill >= m_audioBuffer.size())
                {
                    std::size_t res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);
                    if (res != m_audioBufferFill)
                    {
                        qDebug("DenoiserWorker::processSample: %lu/%lu audio samples written", res, m_audioBufferFill);
                        m_audioFifo.clear();
                    }
                    m_audioBufferFill = 0;
                }
            }
            else if ((m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_RNnoise) && !m_settings.m_audioMute)
            {
                // if ((s_dbgCount % 48000) == 1) {
                //     qDebug() << "DenoiserWorker::processSample[CI16]: RNNoise branch";
                // }
                // feed RNNoise input buffer
                m_rnnoiseIn[m_rnnoiseFill] = static_cast<float>(s[2*i] + s[2*i+1]) * (m_settings.m_volumeTenths / 20.0f); // average I/Q in [-32768..32767] range
                m_rnnoiseFill++;

                if (m_rnnoiseFill >= 480)
                {
                    // process RNNoise frame
                    rnnoise_process_frame(m_rnnoiseState, m_rnnoiseOut, m_rnnoiseIn);

                    // output RNNoise processed samples
                    for (int j = 0; j < 480; j++)
                    {
                        float outSample = m_rnnoiseOut[j];
                        m_sampleBuffer.push_back(Sample(outSample * 181, outSample * 181)); // 181 = sqrt(32768)
                        int16_t audioSample = static_cast<int16_t>(outSample);
                        m_audioBuffer[m_audioBufferFill].l = audioSample;
                        m_audioBuffer[m_audioBufferFill].r = audioSample;
                        ++m_audioBufferFill;

                        if (m_audioBufferFill >= m_audioBuffer.size())
                        {
                            std::size_t res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);
                            if (res != m_audioBufferFill)
                            {
                                qDebug("DenoiserWorker::processSample: %lu/%lu audio samples written", res, m_audioBufferFill);
                                m_audioFifo.clear();
                            }
                            m_audioBufferFill = 0;
                        }
                    }

                    m_rnnoiseFill = 0;
                }
            }
        }
        break;
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
