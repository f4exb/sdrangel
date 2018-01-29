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

const int AudioNetSink::m_udpBlockSize = 512;

AudioNetSink::AudioNetSink(QObject *parent, bool stereo) :
    m_type(SinkUDP),
    m_udpBufferAudioMono(0),
    m_udpBufferAudioStereo(0)
{
    if (stereo)
    {
        m_udpBufferAudioStereo = new UDPSink<AudioSample>(parent, m_udpBlockSize);
#ifdef HAS_JRTPLIB
        m_rtpBufferAudioStereo = new RTPSink<AudioSample>("127.0.0.1", 9999, 48000);
        m_rtpBufferAudioMono = 0;
#endif
    }
    else
    {
        m_udpBufferAudioMono = new UDPSink<int16_t>(parent, m_udpBlockSize);
#ifdef HAS_JRTPLIB
        m_rtpBufferAudioMono = new RTPSink<int16_t>("127.0.0.1", 9999, 48000);
        m_rtpBufferAudioStereo = 0;
#endif
    }
}

AudioNetSink::~AudioNetSink()
{
    if (m_udpBufferAudioMono) { delete m_udpBufferAudioMono; }
    if (m_udpBufferAudioStereo) { delete m_udpBufferAudioStereo; }
#ifdef HAS_JRTPLIB
    if (m_rtpBufferAudioMono) { delete m_rtpBufferAudioMono; }
    if (m_rtpBufferAudioStereo) { delete m_rtpBufferAudioStereo; }
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
    if (m_rtpBufferAudioMono) {
        m_rtpBufferAudioMono->setDestination(address, port);
    }
    if (m_rtpBufferAudioStereo) {
        m_rtpBufferAudioStereo->setDestination(address, port);
    }
#endif
}

void AudioNetSink::addDestination(const QString& address, uint16_t port)
{
#ifdef HAS_JRTPLIB
    if (m_rtpBufferAudioMono) {
        m_rtpBufferAudioMono->addDestination(address, port);
    }
    if (m_rtpBufferAudioStereo) {
        m_rtpBufferAudioStereo->addDestination(address, port);
    }
#endif
}

void AudioNetSink::deleteDestination(const QString& address, uint16_t port)
{
#ifdef HAS_JRTPLIB
    if (m_rtpBufferAudioMono) {
        m_rtpBufferAudioMono->deleteDestination(address, port);
    }
    if (m_rtpBufferAudioStereo) {
        m_rtpBufferAudioStereo->deleteDestination(address, port);
    }
#endif
}

void AudioNetSink::setSampleRate(int sampleRate)
{
#ifdef HAS_JRTPLIB
    if (m_rtpBufferAudioMono) {
        m_rtpBufferAudioMono->setSampleRate(sampleRate);
    }
    if (m_rtpBufferAudioStereo) {
        m_rtpBufferAudioStereo->setSampleRate(sampleRate);
    }
#endif
}

void AudioNetSink::write(qint16 sample)
{
    if (m_udpBufferAudioMono == 0) {
        return;
    }

    if (m_type == SinkUDP) {
        m_udpBufferAudioMono->write(sample);
    } else if (m_type == SinkRTP) {
#ifdef HAS_JRTPLIB
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
#endif
    }
}


