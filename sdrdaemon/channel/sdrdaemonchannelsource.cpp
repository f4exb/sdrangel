///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon source channel (Tx)                                                 //
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

#include <QDebug>

#include "util/simpleserializer.h"
#include "dsp/threadedbasebandsamplesource.h"
#include "dsp/upchannelizer.h"
#include "device/devicesinkapi.h"
#include "sdrdaemonchannelsource.h"

MESSAGE_CLASS_DEFINITION(SDRDaemonChannelSource::MsgConfigureSDRDaemonChannelSource, Message)

const QString SDRDaemonChannelSource::m_channelIdURI = "sdrangel.channel.sdrdaemonsource";
const QString SDRDaemonChannelSource::m_channelId = "SDRDaemonChannelSource";

SDRDaemonChannelSource::SDRDaemonChannelSource(DeviceSinkAPI *deviceAPI) :
        ChannelSourceAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_running(false),
        m_samplesCount(0),
        m_dataAddress("127.0.0.1"),
        m_dataPort(9090)
{
    setObjectName(m_channelId);

    m_channelizer = new UpChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSource(m_channelizer, this);
    m_deviceAPI->addThreadedSource(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);
}

SDRDaemonChannelSource::~SDRDaemonChannelSource()
{
    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSource(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void SDRDaemonChannelSource::pull(Sample& sample)
{
    sample.m_real = 0.0f;
    sample.m_imag = 0.0f;

    if (m_samplesCount < 1023) {
        m_samplesCount++;
    } else {
        qDebug("SDRDaemonChannelSource::pull: 1024 samples pulled");
        m_samplesCount = 0;
    }
}

void SDRDaemonChannelSource::start()
{
    qDebug("SDRDaemonChannelSink::start");
    m_running = true;
}

void SDRDaemonChannelSource::stop()
{
    qDebug("SDRDaemonChannelSink::stop");
    m_running = false;
}

bool SDRDaemonChannelSource::handleMessage(const Message& cmd __attribute__((unused)))
{
    if (MsgConfigureSDRDaemonChannelSource::match(cmd))
    {
        MsgConfigureSDRDaemonChannelSource& cfg = (MsgConfigureSDRDaemonChannelSource&) cmd;
        qDebug() << "SDRDaemonChannelSource::handleMessage: MsgConfigureSDRDaemonChannelSource";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else
    {
        return false;
    }
}

QByteArray SDRDaemonChannelSource::serialize() const
{
    return m_settings.serialize();
}

bool SDRDaemonChannelSource::deserialize(const QByteArray& data __attribute__((unused)))
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureSDRDaemonChannelSource *msg = MsgConfigureSDRDaemonChannelSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureSDRDaemonChannelSource *msg = MsgConfigureSDRDaemonChannelSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void SDRDaemonChannelSource::applySettings(const SDRDaemonChannelSourceSettings& settings, bool force)
{
    qDebug() << "SDRDaemonChannelSource::applySettings:"
            << " m_dataAddress: " << settings.m_dataAddress
            << " m_dataPort: " << settings.m_dataPort
            << " force: " << force;

    if ((m_settings.m_dataAddress != settings.m_dataAddress) || force) {
        m_dataAddress = settings.m_dataAddress;
    }

    if ((m_settings.m_dataPort != settings.m_dataPort) || force) {
        m_dataPort = settings.m_dataPort;
    }

    m_settings = settings;
}


