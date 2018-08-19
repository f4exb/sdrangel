///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon sink channel (Rx)                                                   //
//                                                                               //
// SDRdaemon is a detached SDR front end that handles the interface with a       //
// physical device and sends or receives the I/Q samples stream to or from a     //
// SDRangel instance via UDP. It is controlled via a Web REST API.               //
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

#include "util/simpleserializer.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "dsp/downchannelizer.h"
#include "device/devicesourceapi.h"
#include "sdrdaemonchannelsink.h"

const QString SDRDaemonChannelSink::m_channelIdURI = "sdrangel.channel.sdrdaemonsink";
const QString SDRDaemonChannelSink::m_channelId = "SDRDaemonChannelSink";

SDRDaemonChannelSink::SDRDaemonChannelSink(DeviceSourceAPI *deviceAPI) :
        ChannelSinkAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_running(false)
{
    setObjectName(m_channelId);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);
}

SDRDaemonChannelSink::~SDRDaemonChannelSink()
{
    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void SDRDaemonChannelSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst __attribute__((unused)))
{
    qDebug("SDRDaemonChannelSink::feed: received %d samples", (int) (end - begin));
}

void SDRDaemonChannelSink::start()
{
    qDebug("SDRDaemonChannelSink::start");
    m_running = true;
}

void SDRDaemonChannelSink::stop()
{
    qDebug("SDRDaemonChannelSink::stop");
    m_running = false;
}

bool SDRDaemonChannelSink::handleMessage(const Message& cmd __attribute__((unused)))
{
    return false;
}

QByteArray SDRDaemonChannelSink::serialize() const
{
    SimpleSerializer s(1);
    return s.final();
}

bool SDRDaemonChannelSink::deserialize(const QByteArray& data __attribute__((unused)))
{
    return false;
}
