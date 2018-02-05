///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "audionetsink.h"
#include "util/udpsink.h"
#ifdef HAS_JRTPLIB
#include "util/rtpsink.h"
#endif

const int AudioNetSink::m_udpBlockSize = 512;

AudioNetSink::AudioNetSink(QObject *parent, bool stereo) :
    m_type(SinkUDP),
    m_udpBufferAudioMono(0),
    m_udpBufferAudioStereo(0),
    m_rtpBufferAudio(0)
{
    if (stereo) {
        m_udpBufferAudioStereo = new UDPSink<AudioSample>(parent, m_udpBlockSize);
    } else {
        m_udpBufferAudioMono = new UDPSink<int16_t>(parent, m_udpBlockSize);
    }

#ifdef HAS_JRTPLIB
    m_rtpBufferAudio = new RTPSink("127.0.0.1", 9999, stereo ? RTPSink::PayloadL16Stereo : RTPSink::PayloadL16Mono);
#endif
}

AudioNetSink::~AudioNetSink()
{
    if (m_udpBufferAudioMono) {
        delete m_udpBufferAudioMono;
    }

    if (m_udpBufferAudioStereo) {
        delete m_udpBufferAudioStereo;
    }
#ifdef HAS_JRTPLIB
    if (m_rtpBufferAudio) {
        delete m_rtpBufferAudio;
    }
#endif
}

bool AudioNetSink::isRTPCapable() const
{
#ifdef HAS_JRTPLIB
        return true;
#else
        return false;
#endif
}

bool AudioNetSink::selectType(SinkType type)
{
    if (type == SinkUDP)
    {
        m_type = SinkUDP;
        return true;
    }
    else if (type == SinkRTP)
    {
#ifdef HAS_JRTPLIB
        m_type = SinkRTP;
        return true;
#else
        m_type = SinkUDP;
        return false;
#endif
    }
    else
    {
        return false;
    }
}

void AudioNetSink::setDestination(const QString& address, uint16_t port)
{
    if (m_udpBufferAudioMono) {
        m_udpBufferAudioMono->setDestination(address, port);
    }
    if (m_udpBufferAudioStereo) {
        m_udpBufferAudioStereo->setDestination(address, port);
    }

#ifdef HAS_JRTPLIB
    if (m_rtpBufferAudio) {
        m_rtpBufferAudio->setDestination(address, port);
    }
#endif
}

#ifdef HAS_JRTPLIB
void AudioNetSink::addDestination(const QString& address, uint16_t port)
{
    if (m_rtpBufferAudio) {
        m_rtpBufferAudio->addDestination(address, port);
    }
}
#else
void AudioNetSink::addDestination(const QString& address __attribute__((unused)), uint16_t port __attribute__((unused)))
{
}
#endif

#ifdef HAS_JRTPLIB
void AudioNetSink::deleteDestination(const QString& address, uint16_t port)
{
    if (m_rtpBufferAudio) {
        m_rtpBufferAudio->deleteDestination(address, port);
    }
}
#else
void AudioNetSink::deleteDestination(const QString& address __attribute__((unused)), uint16_t port __attribute__((unused)))
{
}
#endif

void AudioNetSink::write(qint16 sample)
{
    if (m_udpBufferAudioMono == 0) {
        return;
    }

    if (m_type == SinkUDP) {
        m_udpBufferAudioMono->write(sample);
    } else if (m_type == SinkRTP) {
#ifdef HAS_JRTPLIB
        m_rtpBufferAudio->write((uint8_t *) &sample);
#endif
    }
}

void AudioNetSink::write(const AudioSample& sample)
{
    if (m_udpBufferAudioStereo == 0) {
        return;
    }

    if (m_type == SinkUDP) {
        m_udpBufferAudioStereo->write(sample);
    } else if (m_type == SinkRTP) {
#ifdef HAS_JRTPLIB
        m_rtpBufferAudio->write((uint8_t *) &sample);
#endif
    }
}


