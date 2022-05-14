///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
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

#include <QTime>
#include <QDebug>

#include <stdio.h>
#include <complex.h>

#include "dsp/dspengine.h"
#include "device/deviceapi.h"

#include "atvdemod.h"

MESSAGE_CLASS_DEFINITION(ATVDemod::MsgConfigureATVDemod, Message)

const char* const ATVDemod::m_channelIdURI = "sdrangel.channel.demodatv";
const char* const ATVDemod::m_channelId = "ATVDemod";

ATVDemod::ATVDemod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
    m_deviceAPI(deviceAPI),
    m_centerFrequency(0),
    m_basebandSampleRate(0)
{
    qDebug("ATVDemod::ATVDemod");
    setObjectName(m_channelId);

    m_basebandSink = new ATVDemodBaseband();
    m_basebandSink->setFifoLabel(QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(getIndexInDeviceSet())
    );
    m_basebandSink->moveToThread(&m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &ATVDemod::handleIndexInDeviceSetChanged
    );
}

ATVDemod::~ATVDemod()
{
    qDebug("ATVDemod::~ATVDemod");
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void ATVDemod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSink(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

uint32_t ATVDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void ATVDemod::start()
{
	qDebug("ATVDemod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    // re-apply essential messages

    DSPSignalNotification* notifToSink = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(notifToSink);

    ATVDemodBaseband::MsgConfigureATVDemodBaseband *msg = ATVDemodBaseband::MsgConfigureATVDemodBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void ATVDemod::stop()
{
    qDebug("ATVDemod::stop");
    m_basebandSink->stopWork();
	m_thread.exit();
	m_thread.wait();
}

void ATVDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

bool ATVDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureATVDemod::match(cmd))
    {
        MsgConfigureATVDemod& cfg = (MsgConfigureATVDemod&) cmd;
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_centerFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate(); // store for init at start
        qDebug() << "ATVDemod::handleMessage: DSPSignalNotification" << m_basebandSampleRate;

        // Forward to the sink
        DSPSignalNotification* notifToSink = new DSPSignalNotification(notif); // make a copy
        m_basebandSink->getInputMessageQueue()->push(notifToSink);

        // Forward to GUI
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else
    {
        return false;
    }
}

void ATVDemod::setCenterFrequency(qint64 frequency)
{
    ATVDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (getMessageQueueToGUI())
    {
        MsgConfigureATVDemod *msg = MsgConfigureATVDemod::create(settings, false);
        getMessageQueueToGUI()->push(msg);
    }
}

void ATVDemod::applySettings(const ATVDemodSettings& settings, bool force)
{
    qDebug() << "ATVDemod::applySettings:"
            << "m_inputFrequencyOffset:" << settings.m_inputFrequencyOffset
            << "m_bfoFrequency:" << settings.m_bfoFrequency
            << "m_atvModulation:" << settings.m_atvModulation
            << "m_fmDeviation:" << settings.m_fmDeviation
            << "m_fftFiltering:" << settings.m_fftFiltering
            << "m_fftOppBandwidth:" << settings.m_fftOppBandwidth
            << "m_fftBandwidth:" << settings.m_fftBandwidth
            << "m_nbLines:" << settings.m_nbLines
            << "m_fps:" << settings.m_fps
            << "m_atvStd:" << settings.m_atvStd
            << "m_hSync:" << settings.m_hSync
            << "m_vSync:" << settings.m_vSync
            << "m_invertVideo:" << settings.m_invertVideo
            << "m_halfFrames:" << settings.m_halfFrames
            << "m_levelSynchroTop:" << settings.m_levelSynchroTop
            << "m_levelBlack:" << settings.m_levelBlack
            << "m_rgbColor:" << settings.m_rgbColor
            << "m_title:" << settings.m_title
            << "m_udpAddress:" << settings.m_udpAddress
            << "m_udpPort:" << settings.m_udpPort
            << "force:" << force;

    ATVDemodBaseband::MsgConfigureATVDemodBaseband *msg = ATVDemodBaseband::MsgConfigureATVDemodBaseband::create(settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    m_settings = settings;
}

void ATVDemod::handleIndexInDeviceSetChanged(int index)
{
    if (index < 0) {
        return;
    }

    QString fifoLabel = QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
}
