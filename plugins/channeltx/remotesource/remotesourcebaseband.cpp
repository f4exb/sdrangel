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

#include "remotesourcebaseband.h"

MESSAGE_CLASS_DEFINITION(RemoteSourceBaseband::MsgConfigureRemoteSourceBaseband, Message)
MESSAGE_CLASS_DEFINITION(RemoteSourceBaseband::MsgConfigureRemoteSourceWork, Message)

RemoteSourceBaseband::RemoteSourceBaseband() :
    m_mutex(QMutex::Recursive)
{
    m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(48000));
    m_channelizer = new UpChannelizer(&m_source);

    qDebug("RemoteSourceBaseband::RemoteSourceBaseband");
    QObject::connect(
        &m_sampleFifo,
        &SampleSourceFifo::dataRead,
        this,
        &RemoteSourceBaseband::handleData,
        Qt::QueuedConnection
    );

    connect(&m_source, SIGNAL(newChannelSampleRate(unsigned int)), this, SLOT(newChannelSampleRate(unsigned int)));
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

RemoteSourceBaseband::~RemoteSourceBaseband()
{
    delete m_channelizer;
}

void RemoteSourceBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
}

void RemoteSourceBaseband::pull(const SampleVector::iterator& begin, unsigned int nbSamples)
{
    //qDebug("RemoteSourceBaseband::pull: %u", nbSamples);
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

void RemoteSourceBaseband::handleData()
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

void RemoteSourceBaseband::processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    m_channelizer->prefetch(iEnd - iBegin);
    m_channelizer->pull(data.begin() + iBegin, iEnd - iBegin);
}

void RemoteSourceBaseband::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool RemoteSourceBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureRemoteSourceBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureRemoteSourceBaseband& cfg = (MsgConfigureRemoteSourceBaseband&) cmd;
        qDebug() << "RemoteSourceBaseband::handleMessage: MsgConfigureRemoteSourceBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgConfigureRemoteSourceWork::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
		MsgConfigureRemoteSourceWork& conf = (MsgConfigureRemoteSourceWork&) cmd;
        qDebug() << "RemoteSourceBaseband::handleMessage: MsgConfigureRemoteSourceWork: " << conf.isWorking();

        if (conf.isWorking()) {
            m_source.start();
        } else {
            m_source.stop();
        }

		return true;
    }
    else
    {
        return false;
    }
}

void RemoteSourceBaseband::applySettings(const RemoteSourceSettings& settings, bool force)
{
    qDebug() << "RemoteSourceBaseband::applySettings:"
        << "m_log2Interp:" << settings.m_log2Interp
        << "m_filterChainHash:" << settings.m_filterChainHash
        << "m_dataAddress:" << settings.m_dataAddress
        << "m_dataPort:" << settings.m_dataPort
        << "force:" << force;

    if ((settings.m_dataAddress != m_settings.m_dataAddress)
     || (settings.m_dataPort != m_settings.m_dataPort) || force) {
        m_source.dataBind(settings.m_dataAddress, settings.m_dataPort);
    }

    if ((m_settings.m_filterChainHash != settings.m_filterChainHash)
     || (m_settings.m_log2Interp != settings.m_log2Interp) || force)
    {
        m_channelizer->setInterpolation(settings.m_log2Interp, settings.m_filterChainHash);
    }

    m_settings = settings;
}

int RemoteSourceBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}

void RemoteSourceBaseband::newRemoteSampleRate(unsigned int sampleRate)
{
    m_channelizer->setChannelization(sampleRate, 0); // Adjust channelizer to match remote sample rate
}
