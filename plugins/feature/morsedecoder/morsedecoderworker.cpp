///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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
#include <QTimer>

#include "dsp/scopevis.h"
#include "dsp/datafifo.h"

#include "morsedecoder.h"
#include "morsedecoderworker.h"

MESSAGE_CLASS_DEFINITION(MorseDecoderWorker::MsgConfigureMorseDecoderWorker, Message)
MESSAGE_CLASS_DEFINITION(MorseDecoderWorker::MsgConfigureSampleRate, Message)
MESSAGE_CLASS_DEFINITION(MorseDecoderWorker::MsgConnectFifo, Message)

MorseDecoderWorker::MorseDecoderWorker() :
    m_dataFifo(nullptr),
    m_msgQueueToFeature(nullptr),
    m_auto(false),
    m_pitchHz(-1),
    m_speedWPM(-1),
    m_scopeVis(nullptr),
    m_pollTimer(nullptr)
{
    qDebug("MorseDecoderWorker::MorseDecoderWorker");
    m_ggMorseParameters = new GGMorse::Parameters{
        48000.0f,                         // sampleRateInp: capture sample rate
        48000.0f,                         // sampleRateOut:  playback sample rate
        GGMorse::kDefaultSamplesPerFrame, // samplesPerFrame: number of samples per audio frame (128)
        GGMORSE_SAMPLE_FORMAT_I16,        // sampleFormatInp: format of the captured audio samples
        GGMORSE_SAMPLE_FORMAT_I16         // sampleFormatOut: format of the playback audio samples
    };
    m_ggMorse = new GGMorse(*m_ggMorseParameters);
    auto parametersDecode = m_ggMorse->getDefaultParametersDecode(); // auto pitch [200, 1200 Hz], auto speed, apply low pass and high pass
    parametersDecode.applyFilterHighPass = false;
    m_ggMorse->setParametersDecode(parametersDecode);
    applySampleRate(48000);
}

MorseDecoderWorker::~MorseDecoderWorker()
{

    m_inputMessageQueue.clear();
    delete m_ggMorse;
    delete m_ggMorseParameters;
}

void MorseDecoderWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

void MorseDecoderWorker::startWork()
{
    qDebug("MorseDecoderWorker::startWork");
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    m_pollTimer = new QTimer();
    connect(m_pollTimer, SIGNAL(timeout()), this, SLOT(pollingTick()));
    m_pollTimer->start(1000);
}

void MorseDecoderWorker::stopWork()
{
    qDebug("MorseDecoderWorker::stopWork");
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    disconnect(m_pollTimer, SIGNAL(timeout()), this, SLOT(pollingTick()));
    m_pollTimer->stop();
    delete m_pollTimer;
}

void MorseDecoderWorker::feedPart(
    const QByteArray::const_iterator& begin,
    const QByteArray::const_iterator& end,
    DataFifo::DataType dataType
)
{
    if (dataType != DataFifo::DataTypeI16) {
        return;
    }

    int countBytes = end - begin;
    int bytesLeft = m_bytesBufferSize - m_bytesBufferCount;

    if (countBytes > m_bytesBufferSize) {
        qDebug("MorseDecoderWorker::feedPart: countBytes: %d m_bytesBufferSize: %d",
            countBytes, m_bytesBufferSize);
        return;
    }

    if (countBytes >= bytesLeft)
    {
        std::copy(begin, begin + bytesLeft, m_bytesBuffer.begin() + m_bytesBufferCount); // fill buffer
        int unprocessedBytes = processBuffer(m_bytesBuffer);
        std::copy(begin + bytesLeft - unprocessedBytes, end, m_bytesBuffer.begin());
        if (m_bytesBufferCount + bytesLeft != (int) m_bytesBuffer.size()) {
            qDebug("MorseDecoderWorker::feedPart: countBytes: %d bytesLeft: %d m_bytesBufferCount: %d unprocessedBytes: %d",
                countBytes, bytesLeft, m_bytesBufferCount, unprocessedBytes);
        }
        int remBytes = end - (begin + bytesLeft - unprocessedBytes);
        m_bytesBufferCount = remBytes;
    }
    else
    {
        std::copy(begin, end, m_bytesBuffer.begin() + m_bytesBufferCount);
        m_bytesBufferCount += countBytes;
    }
}

int MorseDecoderWorker::processBuffer(QByteArray& bytesBuffer)
{
    uint32_t samplesHave = bytesBuffer.size() / 2;
    uint32_t samplesTotal = bytesBuffer.size() / 2;
    int bytesLeft = 0;

    // // Input waveform
    // if (m_scopeVis)
    // {
    //     SampleVector strace;
    //     strace.resize(4800, 0);
    //     int nSamples = bytesBuffer.size() / 2;
    //     float ftrace = nSamples / 4800.0f;
    //     int i = 0;

    //     std::for_each(
    //         strace.begin(),
    //         strace.end(),
    //         [&](Sample& s)
    //         {
    //             int itrace = ftrace * i;
    //             int16_t *ps = (int16_t *) (bytesBuffer.data() + 2*itrace);
    //             float re = *ps / 32768.0f;
    //             i++;
    //             s = Sample(re*SDR_RX_SCALEF, 0);
    //         }
    //     );
    //     std::vector<SampleVector::const_iterator> vbegin;
    //     vbegin.push_back(strace.begin());
    //     m_scopeVis->feed(vbegin, 4800);
    // }

    GGMorse::CBWaveformInp cbWaveformInp = [&](void * data, uint32_t nMaxBytes)
    {
        if (samplesHave*2 < nMaxBytes)
        {
            if (samplesHave != 0)
            {
                bytesLeft = samplesHave*2;
                // qDebug("MorseDecoderWorker::processBuffer::cbWaveformInp: nMaxBytes: %u / %u samples left buffer size: %u countBytes: %d",
                //     nMaxBytes, samplesHave, bytesBuffer.size(), countBytes);
            }

            return 0;
        }

        samplesHave -= nMaxBytes/2;
        // qDebug("MorseDecoderWorker::processBuffer::cbWaveformInp: samplesTotal: %u samplesHave: %u nMaxBytes: %u",
        //    samplesTotal, samplesHave, nMaxBytes);
        memcpy(data, bytesBuffer.data() + (samplesTotal - samplesHave)*2, nMaxBytes);
        return (int) nMaxBytes;
    };

    bool result = m_ggMorse->decode(cbWaveformInp);

    if (result)
    {
        GGMorse::TxRx dst;
        m_ggMorse->takeRxData(dst);
        QString text;
        std::for_each(
            dst.begin(),
            dst.end(),
            [&](const uint8_t c) { text.append((char) c); }
        );

        const GGMorse::Statistics& stats = m_ggMorse->getStatistics();
        m_pitchHz = stats.estimatedPitch_Hz;
        m_speedWPM = stats.estimatedSpeed_wpm;

        if (m_msgQueueToFeature)
        {
            MorseDecoder::MsgReportText *msg = MorseDecoder::MsgReportText::create(text);
            msg->m_costFunction = stats.costFunction;
            msg->m_estimatedPitchHz = m_settings.m_auto ? stats.estimatedPitch_Hz : m_pitchHz;
            msg->m_estimatedSpeedWPM = m_settings.m_auto ? stats.estimatedSpeed_wpm : m_speedWPM;
            msg->m_signalThreshold = stats.signalThreshold;
            m_msgQueueToFeature->push(msg);
        }

        // Goertzel output and thresholds
        if (m_scopeVis)
        {
            std::vector<float> trace;
            int traceSize = m_ggMorse->takeSignalF(trace);
            std::vector<float> thresholds;
            int thrSize = m_ggMorse->takeThresholdF(thresholds);
            // qDebug("MorseDecoderWorker::processBuffer: traceSize: %d thrSize: %d", traceSize, thrSize);
            int i = 0;
            float ftrace = traceSize / 4800.0f; // interpolation to scope basic trace size
            float fthres = thrSize / 4800.0f;

            if (traceSize != 0)
            {
                SampleVector strace;
                strace.resize(4800, 0);
                std::for_each(
                    strace.begin(),
                    strace.end(),
                    [&](Sample& s)
                    {
                        int itrace = ftrace * i;
                        int ithres = fthres * i;
                        float re = trace[itrace];
                        float im = m_settings.m_showThreshold ? thresholds[ithres] : 3.16227766017e-2; // -30dB
                        i++;
                        s = Sample(re*SDR_RX_SCALEF, im*SDR_RX_SCALEF);
                    }
                );
                std::vector<SampleVector::const_iterator> vbegin;
                vbegin.push_back(strace.begin());
                m_scopeVis->feed(vbegin, 4800);
            }
        }
    }

    return bytesLeft;
}

void MorseDecoderWorker::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool MorseDecoderWorker::handleMessage(const Message& cmd)
{
    if (MsgConfigureMorseDecoderWorker::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureMorseDecoderWorker& cfg = (MsgConfigureMorseDecoderWorker&) cmd;
        qDebug("MorseDecoderWorker::handleMessage: MsgConfigureMorseDecoderWorker");

        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (MsgConfigureSampleRate::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureSampleRate& cfg = (MsgConfigureSampleRate&) cmd;
        qDebug("MorseDecoderWorker::handleMessage: MsgConfigureSampleRate");
        applySampleRate(cfg.getSampleRate());

        return true;
    }
    else if (MsgConnectFifo::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConnectFifo& msg = (MsgConnectFifo&) cmd;
        m_dataFifo = msg.getFifo();
        bool doConnect = msg.getConnect();
        qDebug("MorseDecoderWorker::handleMessage: MsgConnectFifo: %s", (doConnect ? "connect" : "disconnect"));

        if (doConnect) {
            QObject::connect(
                m_dataFifo,
                &DataFifo::dataReady,
                this,
                &MorseDecoderWorker::handleData,
                Qt::QueuedConnection
            );
        }
        else
        {
            QObject::disconnect(
                m_dataFifo,
                &DataFifo::dataReady,
                this,
                &MorseDecoderWorker::handleData
            );
        }

        return true;
    }
    else
    {
        return false;
    }
}

void MorseDecoderWorker::applySettings(const MorseDecoderSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "MorseDecoderWorker::applySettings:" << settings.getDebugString(settingsKeys, force) << force;

    if (settingsKeys.contains("auto") || force)
    {
        auto parametersDecode = m_ggMorse->getDefaultParametersDecode();
        parametersDecode.applyFilterHighPass = false;

        if (settings.m_auto)
        {
            parametersDecode.frequency_hz = -1.0f; // auto
            parametersDecode.speed_wpm = -1.0f; // auto
        }
        else
        {
            parametersDecode.frequency_hz = m_pitchHz;
            parametersDecode.speed_wpm = m_speedWPM;
        }

        m_ggMorse->setParametersDecode(parametersDecode);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

}

void MorseDecoderWorker::applySampleRate(int sampleRate)
{
    m_sinkSampleRate = sampleRate;
    m_ggMorseParameters->sampleRateInp = sampleRate;
    int ggMorseBlockSize = (sampleRate / GGMorse::kBaseSampleRate)*GGMorse::kDefaultSamplesPerFrame;
    m_bytesBufferSize = sampleRate * GGMorse::kMaxWindowToAnalyze_s * 2;
    m_bytesBuffer.resize(m_bytesBufferSize);
    m_bytesBufferCount = 0;
    float seconds = m_bytesBufferSize/(sampleRate*2.0f);

    if (m_scopeVis) {
        m_scopeVis->setLiveRate(4800/seconds);
    }

    qDebug("MorseDecoderWorker::applySampleRate: m_sinkSampleRate: %d ggMorseBlockSize: %d m_bytesBufferSize: %d",
        m_sinkSampleRate, ggMorseBlockSize, m_bytesBufferSize);
}

void MorseDecoderWorker::handleData()
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
}

void MorseDecoderWorker::pollingTick()
{
}
