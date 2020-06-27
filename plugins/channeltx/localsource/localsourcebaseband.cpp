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

#include "dsp/upchannelizer.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

#include "localsourcebaseband.h"

MESSAGE_CLASS_DEFINITION(LocalSourceBaseband::MsgConfigureLocalSourceBaseband, Message)
MESSAGE_CLASS_DEFINITION(LocalSourceBaseband::MsgConfigureLocalSourceWork, Message)
MESSAGE_CLASS_DEFINITION(LocalSourceBaseband::MsgConfigureLocalDeviceSampleSink, Message)

LocalSourceBaseband::LocalSourceBaseband() :
    m_localSampleSink(nullptr),
    m_mutex(QMutex::Recursive)
{
    m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(48000));
    m_channelizer = new UpChannelizer(&m_source);

    qDebug("LocalSourceBaseband::LocalSourceBaseband");
    QObject::connect(
        &m_sampleFifo,
        &SampleSourceFifo::dataRead,
        this,
        &LocalSourceBaseband::handleData,
        Qt::QueuedConnection
    );

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

LocalSourceBaseband::~LocalSourceBaseband()
{
    delete m_channelizer;
}

void LocalSourceBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
}

void LocalSourceBaseband::pull(const SampleVector::iterator& begin, unsigned int nbSamples)
{
    unsigned int part1Begin, part1End, part2Begin, part2End;
    m_sampleFifo.read(nbSamples, part1Begin, part1End, part2Begin, part2End);
    SampleVector& data = m_sampleFifo.getData();

    if (part1Begin != part1End)
    {
        std::copy(
            data.begin() + part1Begin,
            data.begin() + part1End,
            begin
        );
    }

    unsigned int shift = part1End - part1Begin;

    if (part2Begin != part2End)
    {
        std::copy(
            data.begin() + part2Begin,
            data.begin() + part2End,
            begin + shift
        );
    }
}

void LocalSourceBaseband::handleData()
{
    QMutexLocker mutexLocker(&m_mutex);
    SampleVector& data = m_sampleFifo.getData();
    unsigned int ipart1begin;
    unsigned int ipart1end;
    unsigned int ipart2begin;
    unsigned int ipart2end;

    unsigned int remainder = m_sampleFifo.remainder();

    while ((remainder > 0) && (m_inputMessageQueue.size() == 0))
    {
        m_sampleFifo.write(remainder, ipart1begin, ipart1end, ipart2begin, ipart2end);

        if (ipart1begin != ipart1end) { // first part of FIFO data
            processFifo(data, ipart1begin, ipart1end);
        }

        if (ipart2begin != ipart2end) { // second part of FIFO data (used when block wraps around)
            processFifo(data, ipart2begin, ipart2end);
        }

        remainder = m_sampleFifo.remainder();
    }
}

void LocalSourceBaseband::processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    m_channelizer->prefetch(iEnd - iBegin);
    m_channelizer->pull(data.begin() + iBegin, iEnd - iBegin);
}

void LocalSourceBaseband::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool LocalSourceBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureLocalSourceBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureLocalSourceBaseband& cfg = (MsgConfigureLocalSourceBaseband&) cmd;
        qDebug() << "LocalSourceBaseband::handleMessage: MsgConfigureLocalSourceBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "LocalSourceBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer->setBasebandSampleRate(notif.getSampleRate());

		return true;
    }
    else if (MsgConfigureLocalSourceWork::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
		MsgConfigureLocalSourceWork& conf = (MsgConfigureLocalSourceWork&) cmd;
        qDebug() << "LocalSourceBaseband::handleMessage: MsgConfigureLocalSourceWork: " << conf.isWorking();

        if (conf.isWorking()) {
            m_source.start(m_localSampleSink);
        } else {
            m_source.stop();
        }

		return true;
    }
    else if (MsgConfigureLocalDeviceSampleSink::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureLocalDeviceSampleSink& notif  = (MsgConfigureLocalDeviceSampleSink&) cmd;
        qDebug() << "LocalSourceBaseband::handleMessage: MsgConfigureLocalDeviceSampleSink: " << notif.getDeviceSampleSink();
        m_localSampleSink = notif.getDeviceSampleSink();

        if (m_source.isRunning()) {
            m_source.start(m_localSampleSink);
        }

        return  true;
    }
    else
    {
        return false;
    }
}

void LocalSourceBaseband::applySettings(const LocalSourceSettings& settings, bool force)
{
    qDebug() << "LocalSourceBaseband::applySettings:"
        << "m_localDeviceIndex:" << settings.m_localDeviceIndex
        << "m_log2Interp:" << settings.m_log2Interp
        << "m_filterChainHash:" << settings.m_filterChainHash
        << "m_play:" << settings.m_play
        << " force: " << force;

    if ((settings.m_log2Interp != m_settings.m_log2Interp)
     || (settings.m_filterChainHash != m_settings.m_filterChainHash) || force)
    {
        m_channelizer->setInterpolation(m_settings.m_log2Interp, m_settings.m_filterChainHash);
    }

    //m_source.applySettings(settings, force);
    m_settings = settings;
}

int LocalSourceBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}
