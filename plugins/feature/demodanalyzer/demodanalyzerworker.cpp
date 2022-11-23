///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include "dsp/scopevis.h"
#include "dsp/datafifo.h"
#include "dsp/wavfilerecord.h"

#include "demodanalyzerworker.h"

MESSAGE_CLASS_DEFINITION(DemodAnalyzerWorker::MsgConfigureDemodAnalyzerWorker, Message)
MESSAGE_CLASS_DEFINITION(DemodAnalyzerWorker::MsgConnectFifo, Message)

DemodAnalyzerWorker::DemodAnalyzerWorker() :
    m_dataFifo(nullptr),
    m_msgQueueToFeature(nullptr),
    m_sampleBufferSize(0),
    m_wavFileRecord(nullptr),
    m_recordSilenceNbSamples(0),
    m_recordSilenceCount(0),
    m_nbBytes(0)
{
    qDebug("DemodAnalyzerWorker::DemodAnalyzerWorker");
}

DemodAnalyzerWorker::~DemodAnalyzerWorker()
{
    m_inputMessageQueue.clear();
}

void DemodAnalyzerWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

void DemodAnalyzerWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_wavFileRecord = new WavFileRecord(m_sinkSampleRate);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

void DemodAnalyzerWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    delete m_wavFileRecord;
    m_wavFileRecord = nullptr;
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

void DemodAnalyzerWorker::feedPart(
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
        m_convBuffer.resize(2*countSamples);
        m_sampleBufferSize = countSamples;
    }

    for (int i = 0; i < countSamples; i++) {
        processSample(dataType, begin, countSamples, i);
    }

	if (m_scopeVis)
    {
        std::vector<SampleVector::const_iterator> vbegin;
        vbegin.push_back(m_sampleBuffer.begin());
        m_scopeVis->feed(vbegin, countSamples/(1<<m_settings.m_log2Decim));
	}

    if (m_settings.m_recordToFile && m_wavFileRecord)
    {
        for (int is = 0; is < countSamples/(1<<m_settings.m_log2Decim); is++)
        {
            const Sample& sample = m_sampleBuffer[is];

            if ((sample.m_real == 0) && (sample.m_imag == 0))
            {
                if (m_recordSilenceNbSamples <= 0)
                {
                    writeSampleToFile(sample);
                    m_recordSilenceCount = 0;
                }
                else if (m_recordSilenceCount < m_recordSilenceNbSamples)
                {
                    writeSampleToFile(sample);
                    m_recordSilenceCount++;
                }
                else
                {
                    if (m_wavFileRecord->isRecording()) {
                        m_wavFileRecord->stopRecording();
                    }
                }
            }
            else
            {
                if (!m_wavFileRecord->isRecording()) {
                    m_wavFileRecord->startRecording();
                }
                writeSampleToFile(sample);
                m_recordSilenceCount = 0;
            }
        }
    }
}

void DemodAnalyzerWorker::writeSampleToFile(const Sample& sample)
{
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

void DemodAnalyzerWorker::decimate(int countSamples)
{
    qint16 *buf = m_convBuffer.data();
    SampleVector::iterator it = m_sampleBuffer.begin();

    switch (m_settings.m_log2Decim)
    {
    case 1:
        m_decimators.decimate2_cen(&it, buf, countSamples*2);
        break;
    case 2:
        m_decimators.decimate4_cen(&it, buf, countSamples*2);
        break;
    case 3:
        m_decimators.decimate8_cen(&it, buf, countSamples*2);
        break;
    case 4:
        m_decimators.decimate16_cen(&it, buf, countSamples*2);
        break;
    case 5:
        m_decimators.decimate32_cen(&it, buf, countSamples*2);
        break;
    case 6:
        m_decimators.decimate64_cen(&it, buf, countSamples*2);
        break;
    default:
        break;
    }
}

void DemodAnalyzerWorker::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool DemodAnalyzerWorker::handleMessage(const Message& cmd)
{
    if (MsgConfigureDemodAnalyzerWorker::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureDemodAnalyzerWorker& cfg = (MsgConfigureDemodAnalyzerWorker&) cmd;
        qDebug("DemodAnalyzerWorker::handleMessage: MsgConfigureDemodAnalyzerWorker");

        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (MsgConnectFifo::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConnectFifo& msg = (MsgConnectFifo&) cmd;
        m_dataFifo = msg.getFifo();
        bool doConnect = msg.getConnect();
        qDebug("DemodAnalyzerWorker::handleMessage: MsgConnectFifo: %s", (doConnect ? "connect" : "disconnect"));

        if (doConnect) {
            QObject::connect(
                m_dataFifo,
                &DataFifo::dataReady,
                this,
                &DemodAnalyzerWorker::handleData,
                Qt::QueuedConnection
            );
        }
        else
        {
            QObject::disconnect(
                m_dataFifo,
                &DataFifo::dataReady,
                this,
                &DemodAnalyzerWorker::handleData
            );
        }

        return true;
    }
    else
    {
        return false;
    }
}

void DemodAnalyzerWorker::applySettings(const DemodAnalyzerSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "DemodAnalyzerWorker::applySettings:" << settings.getDebugString(settingsKeys, force) << force;

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

    if (settingsKeys.contains("recordSilenceTime")
     || settingsKeys.contains("log2Decim")  || force)
    {
        m_recordSilenceNbSamples = (settings.m_recordSilenceTime * (m_sinkSampleRate / (1<<settings.m_log2Decim))) / 10; // time in 100'ś ms
        m_recordSilenceCount = 0;

        if (m_wavFileRecord)
        {
            if (m_wavFileRecord->isRecording()) {
                m_wavFileRecord->stopRecording();
            }

            m_wavFileRecord->setSampleRate(m_sinkSampleRate / (1<<settings.m_log2Decim));
        }
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

}

void DemodAnalyzerWorker::applySampleRate(int sampleRate)
{
    m_sinkSampleRate = sampleRate;

    m_recordSilenceNbSamples = (m_settings.m_recordSilenceTime * (m_sinkSampleRate / (1<<m_settings.m_log2Decim))) / 10; // time in 100'ś ms
    m_recordSilenceCount = 0;

    if (m_wavFileRecord)
    {
        if (m_wavFileRecord->isRecording()) {
            m_wavFileRecord->stopRecording();
        }

        m_wavFileRecord->setSampleRate(m_sinkSampleRate / (1<<m_settings.m_log2Decim));
    }
}

void DemodAnalyzerWorker::handleData()
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
