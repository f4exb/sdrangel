///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include "bfmmodbaseband.h"

MESSAGE_CLASS_DEFINITION(BFMModBaseband::MsgConfigureBFMModBaseband, Message)
MESSAGE_CLASS_DEFINITION(BFMModBaseband::MsgConfigureFileSampleRate, Message)

BFMModBaseband::BFMModBaseband() :
    m_fileSampleRate(48000)
{
    m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(48000));
    m_channelizer = new UpChannelizer(&m_source);

    qDebug("BFMModBaseband::BFMModBaseband");
    QObject::connect(
        &m_sampleFifo,
        &SampleSourceFifo::dataRead,
        this,
        &BFMModBaseband::handleData,
        Qt::QueuedConnection
    );

    DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(m_source.getFeedbackAudioFifo(), getInputMessageQueue());
    m_source.applyFeedbackAudioSampleRate(DSPEngine::instance()->getAudioDeviceManager()->getOutputSampleRate());

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

BFMModBaseband::~BFMModBaseband()
{
    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(m_source.getFeedbackAudioFifo());
    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSource(m_source.getAudioFifo());
    delete m_channelizer;
}

void BFMModBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
}

void BFMModBaseband::setChannel(ChannelAPI *channel)
{
    m_source.setChannel(channel);
}

void BFMModBaseband::pull(const SampleVector::iterator& begin, unsigned int nbSamples)
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

void BFMModBaseband::handleData()
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

void BFMModBaseband::processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    m_channelizer->prefetch(iEnd - iBegin);
    m_channelizer->pull(data.begin() + iBegin, iEnd - iBegin);
}

void BFMModBaseband::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()))
	{
		if (!handleMessage(*message)) {
			qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
		}
		delete message;
	}
}

bool BFMModBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureBFMModBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureBFMModBaseband& cfg = (MsgConfigureBFMModBaseband&) cmd;
        qDebug() << "BFMModBaseband::handleMessage: MsgConfigureBFMModBaseband";

        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgConfigureFileSampleRate::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        const MsgConfigureFileSampleRate& cfg = (const MsgConfigureFileSampleRate&) cmd;
        m_fileSampleRate = cfg.getSampleRate();

        if ((m_settings.m_modAFInput == BFMModSettings::BFMModInputFile) &&
            (m_source.getAudioSampleRate() != m_fileSampleRate))
        {
            m_source.applyAudioSampleRate(m_fileSampleRate);
        }

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "BFMModBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer->setBasebandSampleRate(notif.getSampleRate());
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
        m_source.applyAudioSampleRate(m_source.getAudioSampleRate()); // reapply in case of channel sample rate change

		return true;
    }
    else if (DSPConfigureAudio::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        const DSPConfigureAudio& cfg = (const DSPConfigureAudio&) cmd;
        if (cfg.getAudioType() == DSPConfigureAudio::AudioOutput) {
            m_source.applyFeedbackAudioSampleRate(cfg.getSampleRate());
        } else if (m_settings.m_modAFInput == BFMModSettings::BFMModInputAudio) {
            m_source.applyAudioSampleRate(cfg.getSampleRate());
        }
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

void BFMModBaseband::applySettings(const QStringList& settingsKeys, const BFMModSettings& settings, bool force)
{
    if ((settingsKeys.contains("rfBandwidth") && m_settings.m_rfBandwidth != settings.m_rfBandwidth)
     || (settingsKeys.contains("inputFrequencyOffset") && m_settings.m_inputFrequencyOffset != settings.m_inputFrequencyOffset) || force)
    {
        m_channelizer->setChannelization(settings.m_rfBandwidth, settings.m_inputFrequencyOffset);
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
        m_source.applyAudioSampleRate(m_source.getAudioSampleRate()); // reapply in case of channel sample rate change
    }

    if ((settingsKeys.contains("audioDeviceName") && settings.m_audioDeviceName != m_settings.m_audioDeviceName)
     || (settingsKeys.contains("modAFInput") && settings.m_modAFInput != m_settings.m_modAFInput) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getInputDeviceIndex(settings.m_audioDeviceName);

        audioDeviceManager->removeAudioSource(getAudioFifo());
        if (settings.m_modAFInput == BFMModSettings::BFMModInputAudio) {
            audioDeviceManager->addAudioSource(getAudioFifo(), getInputMessageQueue(), audioDeviceIndex);
        }

        int inputSampleRate = m_source.getAudioSampleRate();

        if (settings.m_modAFInput == BFMModSettings::BFMModInputAudio) {
            inputSampleRate = audioDeviceManager->getInputSampleRate(audioDeviceIndex);
        } else if (settings.m_modAFInput == BFMModSettings::BFMModInputFile) {
            inputSampleRate = m_fileSampleRate;
        }

        if (m_source.getAudioSampleRate() != inputSampleRate) {
            m_source.applyAudioSampleRate(inputSampleRate);
        }
    }

    if ((settingsKeys.contains("feedbackAudioDeviceName") && settings.m_feedbackAudioDeviceName != m_settings.m_feedbackAudioDeviceName) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        const int audioDeviceIndex = audioDeviceManager->getOutputDeviceIndex(settings.m_feedbackAudioDeviceName);
        audioDeviceManager->removeAudioSink(m_source.getFeedbackAudioFifo());
        audioDeviceManager->addAudioSink(m_source.getFeedbackAudioFifo(), getInputMessageQueue(), audioDeviceIndex);
        const int audioSampleRate = audioDeviceManager->getOutputSampleRate(audioDeviceIndex);
        if (m_source.getFeedbackAudioSampleRate() != audioSampleRate) {
            m_source.applyFeedbackAudioSampleRate(audioSampleRate);
        }
    }

    m_source.applySettings(settingsKeys, settings, force);

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

int BFMModBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}
