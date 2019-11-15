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

#include "nfmmodbaseband.h"

MESSAGE_CLASS_DEFINITION(NFMModBaseband::MsgConfigureNFMModBaseband, Message)
MESSAGE_CLASS_DEFINITION(NFMModBaseband::MsgConfigureChannelizer, Message)

NFMModBaseband::NFMModBaseband() :
    m_mutex(QMutex::Recursive)
{
    m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(48000));
    m_channelizer = new UpChannelizer(&m_source);

    qDebug("NFMModBaseband::NFMModBaseband");
    QObject::connect(
        &m_sampleFifo,
        &SampleSourceFifo::dataRead,
        this,
        &NFMModBaseband::handleData,
        Qt::QueuedConnection
    );

	DSPEngine::instance()->getAudioDeviceManager()->addAudioSource(m_source.getAudioFifo(), getInputMessageQueue());
    m_source.applyAudioSampleRate(DSPEngine::instance()->getAudioDeviceManager()->getInputSampleRate());

    DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(m_source.getFeedbackAudioFifo(), getInputMessageQueue());
    m_source.applyFeedbackAudioSampleRate(DSPEngine::instance()->getAudioDeviceManager()->getOutputSampleRate());

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

NFMModBaseband::~NFMModBaseband()
{
    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(m_source.getFeedbackAudioFifo());
    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSource(m_source.getAudioFifo());
    delete m_channelizer;
}

void NFMModBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
}

void NFMModBaseband::pull(const SampleVector::iterator& begin, unsigned int nbSamples)
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

void NFMModBaseband::handleData()
{
    QMutexLocker mutexLocker(&m_mutex);
    SampleVector& data = m_sampleFifo.getData();
    unsigned int ipart1begin;
    unsigned int ipart1end;
    unsigned int ipart2begin;
    unsigned int ipart2end;
    Real rmsLevel, peakLevel, numSamples;

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

    m_source.getLevels(rmsLevel, peakLevel, numSamples);
    emit levelChanged(rmsLevel, peakLevel, numSamples);
}

void NFMModBaseband::processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    m_channelizer->prefetch(iEnd - iBegin);
    m_channelizer->pull(data.begin() + iBegin, iEnd - iBegin);
}

void NFMModBaseband::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool NFMModBaseband::handleMessage(const Message& cmd)
{
    if (DSPConfigureAudio::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPConfigureAudio& cfg = (DSPConfigureAudio&) cmd;
        uint32_t sampleRate = cfg.getSampleRate();
        DSPConfigureAudio::AudioType audioType = cfg.getAudioType();

        qDebug() << "NFMModBaseband::handleMessage: DSPConfigureAudio:"
                << " sampleRate: " << sampleRate
                << " audioType: " << audioType;

        if (audioType == DSPConfigureAudio::AudioInput)
        {
            if (sampleRate != m_source.getAudioSampleRate()) {
                m_source.applyAudioSampleRate(sampleRate);
            }
        }
        else if (audioType == DSPConfigureAudio::AudioOutput)
        {
            if (sampleRate != m_source.getFeedbackAudioSampleRate()) {
                m_source.applyFeedbackAudioSampleRate(sampleRate);
            }
        }

        return true;
    }
    else if (MsgConfigureNFMModBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureNFMModBaseband& cfg = (MsgConfigureNFMModBaseband&) cmd;
        qDebug() << "NFMModBaseband::handleMessage: MsgConfigureNFMModBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgConfigureChannelizer::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        qDebug() << "NFMModBaseband::handleMessage: MsgConfigureChannelizer"
            << "(requested) sourceSampleRate: " << cfg.getSourceSampleRate()
            << "(requested) sourceCenterFrequency: " << cfg.getSourceCenterFrequency();
        m_channelizer->setChannelization(cfg.getSourceSampleRate(), cfg.getSourceCenterFrequency());
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "NFMModBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer->setBasebandSampleRate(notif.getSampleRate());
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());

		return true;
    }
    else if (CWKeyer::MsgConfigureCWKeyer::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        const CWKeyer::MsgConfigureCWKeyer& cfg = (CWKeyer::MsgConfigureCWKeyer&) cmd;
        CWKeyer::MsgConfigureCWKeyer *notif = new CWKeyer::MsgConfigureCWKeyer(cfg);
        CWKeyer& cwKeyer = m_source.getCWKeyer();
        cwKeyer.getInputMessageQueue()->push(notif);

        return true;
    }
    else
    {
        return false;
    }
}

void NFMModBaseband::applySettings(const NFMModSettings& settings, bool force)
{
    m_source.applySettings(settings, force);
    m_settings = settings;
}

int NFMModBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}