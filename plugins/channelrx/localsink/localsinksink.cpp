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

#include <QDebug>

#include <boost/crc.hpp>
#include <boost/cstdint.hpp>
#include "dsp/devicesamplesource.h"
#include "dsp/hbfilterchainconverter.h"

#include "localsinkworker.h"
#include "localsinksink.h"

LocalSinkSink::LocalSinkSink() :
    m_deviceSource(nullptr),
    m_sinkWorker(nullptr),
    m_running(false),
    m_centerFrequency(0),
    m_frequencyOffset(0),
    m_sampleRate(48000),
    m_deviceSampleRate(48000)
{
    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(4000000));
    applySettings(m_settings, true);
}

LocalSinkSink::~LocalSinkSink()
{
}

void LocalSinkSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    if (m_running && m_deviceSource) {
        m_deviceSource->getSampleFifo()->write(begin, end);
    }
    // m_sampleFifo.write(begin, end);
}

void LocalSinkSink::start(DeviceSampleSource *deviceSource)
{
    qDebug("LocalSinkSink::start: deviceSource: %p", deviceSource);

    if (m_running) {
        stop();
    }

    m_deviceSource = deviceSource;
    // TODO: We'll see later if a worker is really needed
    // m_sinkWorker = new LocalSinkWorker();
    // m_sinkWorker->moveToThread(&m_sinkWorkerThread);
    // m_sinkWorker->setSampleFifo(&m_sampleFifo);

    // if (deviceSource) {
    //     m_sinkWorker->setDeviceSampleFifo(deviceSource->getSampleFifo());
    // }

    // QObject::connect(
    //     &m_sampleFifo,
    //     &SampleSinkFifo::dataReady,
    //     m_sinkWorker,
    //     &LocalSinkWorker::handleData,
    //     Qt::QueuedConnection
    // );

    // startWorker();
    m_running = true;
}

void LocalSinkSink::stop()
{
    qDebug("LocalSinkSink::stop");

    // TODO: We'll see later if a worker is really needed
    // QObject::disconnect(
    //     &m_sampleFifo,
    //     &SampleSinkFifo::dataReady,
    //     m_sinkWorker,
    //     &LocalSinkWorker::handleData
    // );

    // if (m_sinkWorker != 0)
    // {
    //     stopWorker();
    //     m_sinkWorker->deleteLater();
    //     m_sinkWorker = nullptr;
    // }

    m_running = false;
    m_deviceSource = nullptr;
}

void LocalSinkSink::startWorker()
{
	m_sinkWorker->startStop(true);
	m_sinkWorkerThread.start();
}

void LocalSinkSink::stopWorker()
{
	m_sinkWorker->startStop(false);
	m_sinkWorkerThread.quit();
	m_sinkWorkerThread.wait();
}

void LocalSinkSink::applySettings(const LocalSinkSettings& settings, bool force)
{
    qDebug() << "LocalSinkSink::applySettings:"
            << " m_localDeviceIndex: " << settings.m_localDeviceIndex
            << " m_streamIndex: " << settings.m_streamIndex
            << " force: " << force;

    m_settings = settings;
}

void LocalSinkSink::setSampleRate(int sampleRate)
{
    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(sampleRate));
}
