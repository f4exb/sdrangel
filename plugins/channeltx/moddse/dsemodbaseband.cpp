///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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
#include "audio/audiodevicemanager.h"

#include "dsemodbaseband.h"
#include "dsemodprocessor.h"

MESSAGE_CLASS_DEFINITION(DSEModBaseband::MsgConfigureDSEModBaseband, Message)

DSEModBaseband::DSEModBaseband()
{
    m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(DSEModSource::m_basebandSampleRate));
    m_channelizer = new UpChannelizer(&m_source);

    qDebug("DSEModBaseband::DSEModBaseband");
    QObject::connect(
        &m_sampleFifo,
        &SampleSourceFifo::dataRead,
        this,
        &DSEModBaseband::handleData,
        Qt::QueuedConnection
    );

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    // The operator's monitor for local input and decoded Angel61 gateway audio
    DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(m_source.getFeedbackAudioFifo(), getInputMessageQueue());
    m_source.applyFeedbackAudioSampleRate(DSPEngine::instance()->getAudioDeviceManager()->getOutputSampleRate());
}

DSEModBaseband::~DSEModBaseband()
{
    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSource(m_source.getAudioFifo());
    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(m_source.getFeedbackAudioFifo());
    delete m_channelizer;
}

void DSEModBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
    m_apiProduced = m_apiPulled = m_apiDrainAt = 0;
}

void DSEModBaseband::setChannel(ChannelAPI *channel)
{
    m_source.setChannel(channel);
}

void DSEModBaseband::pull(const SampleVector::iterator& begin, unsigned int nbSamples)
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
    m_apiPulled += nbSamples;
    const auto drainAt = m_apiDrainAt.load();
    if (drainAt && m_source.getProcessor()->isAPIDraining() && m_apiPulled >= drainAt) { m_source.getProcessor()->outputDrained(); }
}

void DSEModBaseband::handleData()
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

void DSEModBaseband::processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    m_channelizer->prefetch(iEnd - iBegin);
    m_channelizer->pull(data.begin() + iBegin, iEnd - iBegin);
    m_apiProduced += iEnd - iBegin;
    auto processor = m_source.getProcessor();
    if (!processor->isAPIDraining()) { m_apiDrainAt = 0; }
    else if (!m_apiDrainAt && processor->getBasebandFifo().getFill() == 0) {
        // Include an extra channel FIFO cycle to flush the channelizer's filter
        // and interpolation state before reporting completion.
        m_apiDrainAt = m_apiProduced.load() + m_sampleFifo.size();
    }
}

void DSEModBaseband::handleInputMessages()
{
    Message* message;

    // Every message deleted, handled or not: the audio device manager sends to this queue too, and
    // what was not handled leaked
    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        handleMessage(*message);
        delete message;
    }
}

bool DSEModBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureDSEModBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureDSEModBaseband& cfg = (MsgConfigureDSEModBaseband&) cmd;
        qDebug() << "DSEModBaseband::handleMessage: MsgConfigureDSEModBaseband";

        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "DSEModBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer->setBasebandSampleRate(notif.getSampleRate());
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());

        return true;
    }
    else if (DSEModSource::MsgLinkState::match(cmd))
    {
        const DSEModSource::MsgLinkState& msg = (DSEModSource::MsgLinkState&) cmd;
        m_source.setLinkState(msg.getRequestedTier(), msg.getSendTier(), msg.getTimestampMs());
        return true;
    }
    else if (DSPConfigureAudio::match(cmd))
    {
        // The audio device's rate changed in the preferences: the microphone's or the feedback's
        // (the FIFOs were added with this queue), which otherwise went on at the old rate
        QMutexLocker mutexLocker(&m_mutex);
        const DSPConfigureAudio& cfg = (const DSPConfigureAudio&) cmd;

        if (cfg.getSampleRate() > 0)
        {
            if (cfg.getAudioType() == DSPConfigureAudio::AudioInput) {
                m_source.applyAudioSampleRate(cfg.getSampleRate());
            } else {
                m_source.applyFeedbackAudioSampleRate(cfg.getSampleRate());
            }
        }

        return true;
    }
    else if (DSEModSource::MsgSendMessage::match(cmd))
    {
        const DSEModSource::MsgSendMessage& msg = (DSEModSource::MsgSendMessage&) cmd;
        m_source.sendMessage(msg.getText(), msg.getAutoTransmit());
        return true;
    }
    else if (DSEModSource::MsgSendAPRS::match(cmd))
    {
        const DSEModSource::MsgSendAPRS& msg = (DSEModSource::MsgSendAPRS&) cmd;
        m_source.sendAPRS(msg.getInfo());
        return true;
    }
    else if (DSEModSource::MsgSendDatagram::match(cmd))
    {
        const DSEModSource::MsgSendDatagram& msg = (DSEModSource::MsgSendDatagram&) cmd;
        m_source.sendDatagram(msg.getDatagram(), msg.getAutoTransmit());
        return true;
    }
    else if (DSEModSource::MsgTunnelState::match(cmd))
    {
        const DSEModSource::MsgTunnelState& msg = (DSEModSource::MsgTunnelState&) cmd;
        m_source.tunnelState(msg.getEnabled(), msg.getAdvertise());
        return true;
    }
    else if (DSEModSource::MsgTunnelControl::match(cmd))
    {
        const DSEModSource::MsgTunnelControl& msg = (DSEModSource::MsgTunnelControl&) cmd;
        m_source.tunnelControl(msg.getControl(), msg.getSend());
        return true;
    }
    else if (DSEModSource::MsgSendFile::match(cmd))
    {
        const DSEModSource::MsgSendFile& msg = (DSEModSource::MsgSendFile&) cmd;
        m_source.sendFile(msg.getName(), msg.getData());
        return true;
    }
    else if (DSEModSource::MsgFileControl::match(cmd))
    {
        const DSEModSource::MsgFileControl& msg = (DSEModSource::MsgFileControl&) cmd;
        m_source.fileControl(msg.getControl(), msg.getSend(), msg.getDestination(), msg.getAutoTransmit());
        return true;
    }
    else if (DSEModSource::MsgFileNoAnswer::match(cmd))
    {
        m_source.fileNoAnswer();
        return true;
    }
    else if (DSEModSource::MsgSendDTMF::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        const DSEModSource::MsgSendDTMF& msg = (DSEModSource::MsgSendDTMF&) cmd;
        m_source.sendDTMF(msg.getDigits(), msg.getAutoTransmit());
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

void DSEModBaseband::applySettings(const QStringList& settingsKeys, const DSEModSettings& settings, bool force)
{
    if ((settingsKeys.contains("inputFrequencyOffset") && (settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset)) || force)
    {
        m_channelizer->setChannelization(DSEModSource::m_basebandSampleRate, settings.m_inputFrequencyOffset);
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
    }

    if ((settingsKeys.contains("audioDeviceName") && (settings.m_audioDeviceName != m_settings.m_audioDeviceName)) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getInputDeviceIndex(settings.m_audioDeviceName);
        audioDeviceManager->removeAudioSource(getAudioFifo());
        int audioSampleRate = audioDeviceManager->getInputSampleRate(audioDeviceIndex);

        if (getAudioSampleRate() != audioSampleRate) {
            m_source.applyAudioSampleRate(audioSampleRate);
        }

        if (settings.m_modAFInput == DSEModSettings::DSEModInputAudio) {
            audioDeviceManager->addAudioSource(getAudioFifo(), getInputMessageQueue(), audioDeviceIndex);
        }
    }

    if ((settingsKeys.contains("modAFInput") && (settings.m_modAFInput != m_settings.m_modAFInput)) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getInputDeviceIndex(settings.m_audioDeviceName);

        if (settings.m_modAFInput == DSEModSettings::DSEModInputAudio) {
            audioDeviceManager->addAudioSource(getAudioFifo(), getInputMessageQueue(), audioDeviceIndex);
        } else {
            audioDeviceManager->removeAudioSource(getAudioFifo());
        }
    }

    if ((settingsKeys.contains("feedbackAudioDeviceName")
        && (settings.m_feedbackAudioDeviceName != m_settings.m_feedbackAudioDeviceName)) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getOutputDeviceIndex(settings.m_feedbackAudioDeviceName);
        audioDeviceManager->removeAudioSink(m_source.getFeedbackAudioFifo());
        audioDeviceManager->addAudioSink(m_source.getFeedbackAudioFifo(), getInputMessageQueue(), audioDeviceIndex);
        int audioSampleRate = audioDeviceManager->getOutputSampleRate(audioDeviceIndex);

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

int DSEModBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}
