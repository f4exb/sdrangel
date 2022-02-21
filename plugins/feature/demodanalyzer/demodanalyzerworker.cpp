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

        applySettings(cfg.getSettings(), cfg.getForce());

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

void DemodAnalyzerWorker::applySettings(const DemodAnalyzerSettings& settings, bool force)
{
    qDebug() << "DemodAnalyzerWorker::applySettings:"
            << " m_title: " << settings.m_title
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_log2Decim: " << settings.m_log2Decim
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
