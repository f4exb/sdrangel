///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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
#include "dsp/spectrumvis.h"

#include "ssbmodbaseband.h"

MESSAGE_CLASS_DEFINITION(SSBModBaseband::MsgConfigureSSBModBaseband, Message)

SSBModBaseband::SSBModBaseband()
{
    m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(48000));
    m_channelizer = new UpChannelizer(&m_source);

    qDebug("SSBModBaseband::SSBModBaseband");
    QObject::connect(
        &m_sampleFifo,
        &SampleSourceFifo::dataRead,
        this,
        &SSBModBaseband::handleData,
        Qt::QueuedConnection
    );

    DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(m_source.getFeedbackAudioFifo(), getInputMessageQueue());
    m_source.applyFeedbackAudioSampleRate(DSPEngine::instance()->getAudioDeviceManager()->getOutputSampleRate());

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

SSBModBaseband::~SSBModBaseband()
{
    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(m_source.getFeedbackAudioFifo());
    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSource(m_source.getAudioFifo());
    delete m_channelizer;
}

void SSBModBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
}

void SSBModBaseband::setChannel(ChannelAPI *channel)
{
    m_source.setChannel(channel);
}

void SSBModBaseband::pull(const SampleVector::iterator& begin, unsigned int nbSamples)
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

void SSBModBaseband::handleData()
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

void SSBModBaseband::processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    m_channelizer->prefetch(iEnd - iBegin);
    m_channelizer->pull(data.begin() + iBegin, iEnd - iBegin);
}

void SSBModBaseband::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool SSBModBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureSSBModBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureSSBModBaseband& cfg = (MsgConfigureSSBModBaseband&) cmd;
        qDebug() << "SSBModBaseband::handleMessage: MsgConfigureSSBModBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "SSBModBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer->setBasebandSampleRate(notif.getSampleRate());
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
        m_source.applyAudioSampleRate(m_source.getAudioSampleRate()); // reapply in case of channel sample rate change

		return true;
    }
    else if (CWKeyer::MsgConfigureCWKeyer::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        const CWKeyer::MsgConfigureCWKeyer& cfg = (CWKeyer::MsgConfigureCWKeyer&) cmd;
        CWKeyer::MsgConfigureCWKeyer *notif = new CWKeyer::MsgConfigureCWKeyer(cfg);
        CWKeyer *cwKeyer = m_source.getCWKeyer();
        cwKeyer->getInputMessageQueue()->push(notif);

        return true;
    }
    else
    {
        return false;
    }
}

void SSBModBaseband::applySettings(const SSBModSettings& settings, bool force)
{
    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force)
    {
        m_channelizer->setChannelization(m_source.getAudioSampleRate(), settings.m_inputFrequencyOffset);
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
        m_source.applyAudioSampleRate(m_source.getAudioSampleRate()); // reapply in case of channel sample rate change
    }

    if ((settings.m_spanLog2 != m_settings.m_spanLog2) || force)
    {
        DSPSignalNotification *msg = new DSPSignalNotification(getAudioSampleRate()/(1<<settings.m_spanLog2), 0);
        m_spectrumVis->getInputMessageQueue()->push(msg);
    }

    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getInputDeviceIndex(settings.m_audioDeviceName);
        audioDeviceManager->removeAudioSource(getAudioFifo());
        int audioSampleRate = audioDeviceManager->getInputSampleRate(audioDeviceIndex);

        if (getAudioSampleRate() != audioSampleRate)
        {
            m_channelizer->setChannelization(audioSampleRate, m_settings.m_inputFrequencyOffset);
            m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
            m_source.applyAudioSampleRate(audioSampleRate);
            DSPSignalNotification *msg = new DSPSignalNotification(getAudioSampleRate()/(1<<m_settings.m_spanLog2), 0);
            m_spectrumVis->getInputMessageQueue()->push(msg);
        }
    }

    if ((settings.m_modAFInput != m_settings.m_modAFInput) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getInputDeviceIndex(settings.m_audioDeviceName);

        if (settings.m_modAFInput == SSBModSettings::SSBModInputAudio) {
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

int SSBModBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}
