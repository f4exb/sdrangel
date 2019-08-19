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

#include "device/deviceapi.h"
#include "dsp/downchannelizer.h"
#include "dsp/threadedbasebandsamplesink.h"

#include "interferometer.h"

const QString Interferometer::m_channelIdURI = "sdrangel.channel.interferometer";
const QString Interferometer::m_channelId = "Interferometer";

Interferometer::Interferometer(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
    m_deviceAPI(deviceAPI),
    m_correlator(4096),
    m_spectrumSink(nullptr),
    m_scopeSink(nullptr)
{
    m_correlator.setCorrIndex(1);
    connect(&m_correlator, SIGNAL(dataReady(int, int)), this, SLOT(handleData(int, int)));

    for (int i = 0; i < m_deviceAPI->getNbSourceStreams(); i++)
    {
        m_sinks.push_back(InterferometerSink(&m_correlator));
        m_channelizers.push_back(new DownChannelizer(&m_sinks.back()));
        m_threadedBasebandSampleSinks.push_back(new ThreadedBasebandSampleSink(m_channelizers.back(), &m_sinks.back()));
        m_deviceAPI->addChannelSink(m_threadedBasebandSampleSinks.back(), i);

        if (i == 2) { // 2 way interferometer
            break;
        }
    }

    m_deviceAPI->addChannelSinkAPI(this);
    m_sinks.back().setProcessingUnit(true); // The last one is processed last by the engine
}

Interferometer::~Interferometer()
{
    while (!m_threadedBasebandSampleSinks.empty())
    {
        m_deviceAPI->removeChannelSink(m_threadedBasebandSampleSinks.back());
        delete m_threadedBasebandSampleSinks.back();
        m_threadedBasebandSampleSinks.pop_back();
    }

    while (!m_channelizers.empty())
    {
        delete m_channelizers.back();
        m_channelizers.pop_back();
    }

    m_deviceAPI->removeChannelSinkAPI(this);
}

void Interferometer::handleData(int start, int stop)
{
    if ((m_settings.m_correlationType == InterferometerSettings::CorrelationAdd)
     || (m_settings.m_correlationType == InterferometerSettings::CorrelationMultiply))
    {


    }
}