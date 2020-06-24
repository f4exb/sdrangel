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

#include "localsinkthread.h"
#include "localsinksink.h"

LocalSinkSink::LocalSinkSink() :
        m_sinkThread(nullptr),
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
    m_sampleFifo.write(begin, end);
}

void LocalSinkSink::start(DeviceSampleSource *deviceSource)
{
    qDebug("LocalSinkSink::start");

    if (m_running) {
        stop();
    }

    m_sinkThread = new LocalSinkThread();
    m_sinkThread->setSampleFifo(&m_sampleFifo);

    if (deviceSource) {
        m_sinkThread->setDeviceSampleFifo(deviceSource->getSampleFifo());
    }

    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        m_sinkThread,
        &LocalSinkThread::handleData,
        Qt::QueuedConnection
    );

    m_sinkThread->startStop(true);
    m_running = true;
}

void LocalSinkSink::stop()
{
    qDebug("LocalSinkSink::stop");

    QObject::disconnect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        m_sinkThread,
        &LocalSinkThread::handleData
    );

    if (m_sinkThread != 0)
    {
        m_sinkThread->startStop(false);
        m_sinkThread->deleteLater();
        m_sinkThread = 0;
    }

    m_running = false;
}

void LocalSinkSink::applySettings(const LocalSinkSettings& settings, bool force)
{
    qDebug() << "LocalSinkSink::applySettings:"
            << " m_localDeviceIndex: " << settings.m_localDeviceIndex
            << " m_streamIndex: " << settings.m_streamIndex
            << " force: " << force;

    m_settings = settings;
}
