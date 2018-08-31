///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "device/devicesinkapi.h"
#include "dsp/upchannelizer.h"
#include "dsp/threadedbasebandsamplesource.h"

#include "daemonsrc.h"

const QString DaemonSrc::m_channelIdURI = "sdrangel.channeltx.daemonsrc";
const QString DaemonSrc::m_channelId ="DaemonSrc";

DaemonSrc::DaemonSrc(DeviceSinkAPI *deviceAPI) :
    ChannelSourceAPI(m_channelIdURI),
    m_deviceAPI(deviceAPI)
{
    setObjectName(m_channelId);

    m_channelizer = new UpChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSource(m_channelizer, this);
    m_deviceAPI->addThreadedSource(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);
}

DaemonSrc::~DaemonSrc()
{
    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSource(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void DaemonSrc::pull(Sample& sample)
{
    sample.m_real = 0.0f;
    sample.m_imag = 0.0f;
}

void DaemonSrc::pullAudio(int nbSamples __attribute__((unused)))
{
}

void DaemonSrc::start()
{
}

void DaemonSrc::stop()
{
}

bool DaemonSrc::handleMessage(const Message& cmd __attribute__((unused)))
{
    return false;
}

QByteArray DaemonSrc::serialize() const
{
    return m_settings.serialize();
}

bool DaemonSrc::deserialize(const QByteArray& data __attribute__((unused)))
{
    if (m_settings.deserialize(data))
    {
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        return false;
    }
}
