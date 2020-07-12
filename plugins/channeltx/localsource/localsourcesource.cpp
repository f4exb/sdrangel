///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "dsp/devicesamplesink.h"
#include "localsourceworker.h"
#include "localsourcesource.h"

LocalSourceSource::LocalSourceSource() :
    m_running(false),
    m_sinkWorker(nullptr)
{}

LocalSourceSource::~LocalSourceSource()
{}

void LocalSourceSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void LocalSourceSource::start(DeviceSampleSink *deviceSink)
{
    qDebug("LocalSourceSource::start: %p", deviceSink);

    if (m_running) {
        stop();
    }

    if (!deviceSink) {
        return;
    }

    m_sinkWorker = new LocalSourceWorker();
    m_sinkWorker->moveToThread(&m_sinkWorkerThread);

    if (deviceSink)
    {
        m_localSampleSourceFifo = deviceSink->getSampleFifo();
        m_chunkSize = deviceSink->getSampleRate() / 10;
        m_localSamples.resize(2*m_chunkSize);
        m_localSamplesIndex = 0;
        m_localSamplesIndexOffset = m_chunkSize;
        m_sinkWorker->setSampleFifo(m_localSampleSourceFifo);
    }
    else
    {
        m_localSampleSourceFifo = nullptr;
    }

    connect(this,
            SIGNAL(pullSamples(unsigned int)),
            m_sinkWorker,
            SLOT(pullSamples(unsigned int)),
            Qt::QueuedConnection);

    connect(m_sinkWorker,
            SIGNAL(samplesAvailable(unsigned int, unsigned int, unsigned int, unsigned int)),
            this,
            SLOT(processSamples(unsigned int, unsigned int, unsigned int, unsigned int)),
            Qt::QueuedConnection);

    startWorker();
    m_running = true;
}

void LocalSourceSource::stop()
{
    qDebug("LocalSourceSource::stop");

    if (m_sinkWorker)
    {
        stopWorker();
        m_sinkWorker->deleteLater();
        m_sinkWorker = nullptr;
    }

    m_running = false;
}


void LocalSourceSource::startWorker()
{
    m_sinkWorker->startWork();
    m_sinkWorkerThread.start();
}

void LocalSourceSource::stopWorker()
{
	m_sinkWorker->stopWork();
	m_sinkWorkerThread.quit();
	m_sinkWorkerThread.wait();
}


void LocalSourceSource::pullOne(Sample& sample)
{
    if (m_localSampleSourceFifo)
    {
        sample = m_localSamples[m_localSamplesIndex + m_localSamplesIndexOffset];

        if (m_localSamplesIndex < m_chunkSize - 1)
        {
            m_localSamplesIndex++;
        }
        else
        {
            m_localSamplesIndex = 0;

            if (m_localSamplesIndexOffset == 0) {
                m_localSamplesIndexOffset = m_chunkSize;
            } else {
                m_localSamplesIndexOffset = 0;
            }

            emit pullSamples(m_chunkSize);
        }
    }
    else
    {
        sample = Sample{0, 0};
    }
}

void LocalSourceSource::processSamples(unsigned int iPart1Begin, unsigned int iPart1End, unsigned int iPart2Begin, unsigned int iPart2End)
{
    int destOffset = (m_localSamplesIndexOffset == 0 ? m_chunkSize : 0);
    SampleVector::iterator beginDestination = m_localSamples.begin() + destOffset;
    SampleVector& data = m_localSampleSourceFifo->getData();

    // qDebug("LocalSourceSource::processSamples: m_localSamplesIndexOffset: %d m_chunkSize: %d part1: %u part2: %u",
    //     m_localSamplesIndexOffset, m_chunkSize, (iPart1End - iPart1Begin), (iPart2End - iPart2Begin));

    if (iPart1Begin != iPart1End)
    {
        std::copy(
            data.begin() + iPart1Begin,
            data.begin() + iPart1End,
            beginDestination
        );
    }

    beginDestination += (iPart1End - iPart1Begin);

    if (iPart2Begin != iPart2End)
    {
        std::copy(
            data.begin() + iPart2Begin,
            data.begin() + iPart2End,
            beginDestination
        );
    }
}
