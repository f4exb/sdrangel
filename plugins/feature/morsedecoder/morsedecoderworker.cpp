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

#include "dsp/scopevis.h"
#include "dsp/datafifo.h"

#include "morsedecoderworker.h"

MESSAGE_CLASS_DEFINITION(MorseDecoderWorker::MsgConfigureMorseDecoderWorker, Message)
MESSAGE_CLASS_DEFINITION(MorseDecoderWorker::MsgConnectFifo, Message)

MorseDecoderWorker::MorseDecoderWorker() :
    m_dataFifo(nullptr),
    m_msgQueueToFeature(nullptr),
    m_sampleBufferSize(0),
    m_nbBytes(0)
{
    qDebug("MorseDecoderWorker::MorseDecoderWorker");
}

MorseDecoderWorker::~MorseDecoderWorker()
{
    m_inputMessageQueue.clear();
}

void MorseDecoderWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

void MorseDecoderWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

void MorseDecoderWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

void MorseDecoderWorker::feedPart(
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

    m_nbBytes = nbBytes;
    int countSamples = (end - begin) / nbBytes;

    if (countSamples > m_sampleBufferSize)
    {
        m_sampleBuffer.resize(countSamples);
        m_sampleBufferSize = countSamples;
    }

    // TODO
    // for (int i = 0; i < countSamples; i++) {
    //     processSample(dataType, begin, countSamples, i);
    // }
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

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

}

void MorseDecoderWorker::applySampleRate(int sampleRate)
{
    m_sinkSampleRate = sampleRate;
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
