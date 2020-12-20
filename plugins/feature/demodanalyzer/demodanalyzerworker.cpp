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

#include "dsp/basebandsamplesink.h"
#include "dsp/datafifo.h"

#include "demodanalyzerworker.h"

MESSAGE_CLASS_DEFINITION(DemodAnalyzerWorker::MsgConfigureDemodAnalyzerWorker, Message)
MESSAGE_CLASS_DEFINITION(DemodAnalyzerWorker::MsgConnectFifo, Message)

DemodAnalyzerWorker::DemodAnalyzerWorker() :
    m_dataFifo(nullptr),
    m_msgQueueToFeature(nullptr),
    m_sampleBufferSize(0),
    m_running(false),
    m_mutex(QMutex::Recursive)
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

bool DemodAnalyzerWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = true;
    return m_running;
}

void DemodAnalyzerWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = false;
}

void DemodAnalyzerWorker::feedPart(const QByteArray::const_iterator& begin, const QByteArray::const_iterator& end)
{
    int countSamples = (end - begin) / sizeof(int16_t);
    int16_t *s = (int16_t*) begin;

    if (countSamples > m_sampleBufferSize)
    {
        m_sampleBuffer.resize(countSamples);
        m_sampleBufferSize = countSamples;
    }

    for(int i = 0; i < countSamples; i++)
    {
        double re = s[i] / (double) std::numeric_limits<int16_t>::max();
        m_magsq = re*re;
        m_channelPowerAvg(m_magsq);
        m_sampleBuffer[i].setReal(re * SDR_RX_SCALEF);
        m_sampleBuffer[i].setImag(0);
    }

	if (m_sampleSink) {
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.begin() + countSamples, false);
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

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgConnectFifo::match(cmd))
    {
        qDebug("DemodAnalyzerWorker::handleMessage: MsgConnectFifo");
        QMutexLocker mutexLocker(&m_mutex);
        MsgConnectFifo& msg = (MsgConnectFifo&) cmd;
        m_dataFifo = msg.getFifo();
        bool doConnect = msg.getConnect();

        if (doConnect) {
            QObject::connect(
                m_dataFifo,
                &DataFifo::dataReady,
                this,
                &DemodAnalyzerWorker::handleData,
                Qt::QueuedConnection
            );
        } else {
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

void DemodAnalyzerWorker::applySettings(const DemodAnalyzerSettings& settings, bool force)
{
    qDebug() << "DemodAnalyzerWorker::applySettings:"
            << " m_title: " << settings.m_title
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_deviceIndex: " << settings.m_deviceIndex
            << " m_channelIndex: " << settings.m_channelIndex
            << " force: " << force;

    m_settings = settings;
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

        std::size_t count = m_dataFifo->readBegin(m_dataFifo->fill(), &part1begin, &part1end, &part2begin, &part2end);

		// first part of FIFO data
        if (part1begin != part1end) {
            feedPart(part1begin, part1end);
        }

		// second part of FIFO data (used when block wraps around)
		if (part2begin != part2end) {
            feedPart(part2begin, part2end);
        }

		m_dataFifo->readCommit((unsigned int) count);
    }
}
