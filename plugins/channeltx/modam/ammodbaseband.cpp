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

#include "ammodbaseband.h"

MESSAGE_CLASS_DEFINITION(AMModBaseband::MsgConfigureAMModBaseband, Message)

AMModBaseband::AMModBaseband() :
    m_mutex(QMutex::Recursive)
{
    m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(48000));
    m_channelizer = new UpChannelizer(&m_source);

    qDebug("AMModBaseband::AMModBaseband");
    QObject::connect(
        &m_sampleFifo,
        &SampleSourceFifo::dataRead,
        this,
        &AMModBaseband::handleData,
        Qt::QueuedConnection
    );

    DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(m_source.getFeedbackAudioFifo(), getInputMessageQueue());
    m_source.applyFeedbackAudioSampleRate(DSPEngine::instance()->getAudioDeviceManager()->getOutputSampleRate());

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

AMModBaseband::~AMModBaseband()
{
    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(m_source.getFeedbackAudioFifo());
    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSource(m_source.getAudioFifo());
    delete m_channelizer;
}

void AMModBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
}

void AMModBaseband::setChannel(ChannelAPI *channel)
{
    m_source.setChannel(channel);
}

void AMModBaseband::pull(const SampleVector::iterator& begin, unsigned int nbSamples)
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

void AMModBaseband::handleData()
{
    QMutexLocker mutexLocker(&m_mutex);
    SampleVector& data = m_sampleFifo.getData();
    unsigned int ipart1begin;
    unsigned int ipart1end;
    unsigned int ipart2begin;
    unsigned int ipart2end;
    qreal rmsLevel, peakLevel;
    int numSamples;

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

void AMModBaseband::processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    m_channelizer->prefetch(iEnd - iBegin);
    m_channelizer->pull(data.begin() + iBegin, iEnd - iBegin);
}

void AMModBaseband::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool AMModBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureAMModBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureAMModBaseband& cfg = (MsgConfigureAMModBaseband&) cmd;
        qDebug() << "AMModBaseband::handleMessage: MsgConfigureAMModBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "AMModBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer->setBasebandSampleRate(notif.getSampleRate());
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
        m_source.applyAudioSampleRate(m_source.getAudioSampleRate()); // reapply in case of channel sample rate change

		return true;
    }
    else if (CWKeyer::MsgConfigureCWKeyer::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        qDebug() << "AMModBaseband::handleMessage: MsgConfigureCWKeyer";
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

void AMModBaseband::applySettings(const AMModSettings& settings, bool force)
{
    if ((m_settings.m_inputFrequencyOffset != settings.m_inputFrequencyOffset) || force)
    {
        m_channelizer->setChannelization(m_source.getAudioSampleRate(), settings.m_inputFrequencyOffset);
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
        m_source.applyAudioSampleRate(m_source.getAudioSampleRate()); // reapply in case of channel sample rate change
    }

    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getInputDeviceIndex(settings.m_audioDeviceName);
        audioDeviceManager->removeAudioSource(getAudioFifo());
        int audioSampleRate = audioDeviceManager->getInputSampleRate(audioDeviceIndex);

        if (getAudioSampleRate() != audioSampleRate)
        {
            m_channelizer->setChannelization(audioSampleRate, settings.m_inputFrequencyOffset);
            m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
            m_source.applyAudioSampleRate(audioSampleRate);
        }
    }

    if ((settings.m_modAFInput != m_settings.m_modAFInput) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getInputDeviceIndex(settings.m_audioDeviceName);

        if (settings.m_modAFInput == AMModSettings::AMModInputAudio) {
            audioDeviceManager->addAudioSource(getAudioFifo(), getInputMessageQueue(), audioDeviceIndex);
        } else {
            audioDeviceManager->removeAudioSource(getAudioFifo());
        }
    }

    if ((settings.m_feedbackAudioDeviceName != m_settings.m_feedbackAudioDeviceName) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getOutputDeviceIndex(settings.m_feedbackAudioDeviceName);
        audioDeviceManager->removeAudioSink(getFeedbackAudioFifo());
        audioDeviceManager->addAudioSink(getFeedbackAudioFifo(), getInputMessageQueue(), audioDeviceIndex);
        int audioSampleRate = audioDeviceManager->getOutputSampleRate(audioDeviceIndex);

        if (getFeedbackAudioSampleRate() != audioSampleRate) {
            m_source.applyFeedbackAudioSampleRate(audioSampleRate);
        }
    }

    m_source.applySettings(settings, force);

    m_settings = settings;
}

int AMModBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}
