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

#include "denoiserworker.h"

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
    m_nbBytes(0)
{
}

DenoiserWorker::~DenoiserWorker()
{
    m_inputMessageQueue.clear();
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
        for (int is = 0; is < countSamples; is++)
        {
            const Sample& sample = m_sampleBuffer[is];

            if ((sample.m_real == 0) && (sample.m_imag == 0))
            {
                if (m_wavFileRecord->isRecording()) {
                    m_wavFileRecord->stopRecording();
                }
            }
            else
            {
                if (!m_wavFileRecord->isRecording()) {
                    m_wavFileRecord->startRecording();
                }
                writeSampleToFile(sample);
            }
        }
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
    Q_UNUSED(settingsKeys)
    Q_UNUSED(force)

    m_settings = settings;
}

void DenoiserWorker::applySampleRate(int sampleRate)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sinkSampleRate = sampleRate;
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
}
