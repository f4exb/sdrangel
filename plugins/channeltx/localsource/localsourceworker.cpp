///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#include "dsp/samplesourcefifo.h"

#include "localsourceworker.h"

LocalSourceWorker::LocalSourceWorker(QObject* parent) :
    QObject(parent),
    m_running(false),
    m_sampleFifo(nullptr)
{
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

LocalSourceWorker::~LocalSourceWorker()
{
    qDebug("LocalSourceWorker::~LocalSourceWorker");
}

void LocalSourceWorker::setSampleFifo(SampleSourceFifo *sampleFifo)
{
    m_sampleFifo = sampleFifo;
}

void LocalSourceWorker::pullSamples(unsigned int count)
{
    unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
    m_sampleFifo->read(count, iPart1Begin, iPart1End, iPart2Begin, iPart2End);
    emit samplesAvailable(iPart1Begin, iPart1End, iPart2Begin, iPart2End);
}

void LocalSourceWorker::startWork()
{
    qDebug("LocalSourceWorker::startWork");
    m_running = true;
}

void LocalSourceWorker::stopWork()
{
	qDebug("LocalSourceWorker::stopWork");
	m_running = false;
}

void LocalSourceWorker::handleInputMessages()
{
}
