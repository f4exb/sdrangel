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
#include "util/rtpsink.h"

#include <unistd.h>
#include <QUdpSocket>

const int AudioNetSink::m_udpBlockSize = 512;

AudioNetSink::AudioNetSink(QObject *parent) :
    m_type(SinkUDP),
    m_rtpBufferAudio(0),
    m_bufferIndex(0),
    m_port(9998)
{
    memset(m_data, 0, 65536);
    m_udpSocket = new QUdpSocket(parent);
}

AudioNetSink::AudioNetSink(QObject *parent, int sampleRate, bool stereo) :
    m_type(SinkUDP),
    m_rtpBufferAudio(0),
    m_bufferIndex(0),
    m_port(9998)
{
    memset(m_data, 0, 65536);
    m_udpSocket = new QUdpSocket(parent);
    m_rtpBufferAudio = new RTPSink(m_udpSocket, sampleRate, stereo);
}

AudioNetSink::~AudioNetSink()
{
    if (m_rtpBufferAudio) {
        delete m_rtpBufferAudio;
    }

    m_udpSocket->deleteLater(); // this thread is not the owner thread (was moved)
}

bool AudioNetSink::isRTPCapable() const
{
    return m_rtpBufferAudio && m_rtpBufferAudio->isValid();
}

bool AudioNetSink::selectType(SinkType type)
{
    if (type == SinkUDP)
    {
        m_type = SinkUDP;
    }
    else // this is SinkRTP
    {
        m_type = SinkRTP;
    }

    return true;
}

void AudioNetSink::setDestination(const QString& address, uint16_t port)
{
    m_address.setAddress(const_cast<QString&>(address));
    m_port = port;

    if (m_rtpBufferAudio) {
        m_rtpBufferAudio->setDestination(address, port);
    }
}

void AudioNetSink::addDestination(const QString& address, uint16_t port)
{
    if (m_rtpBufferAudio) {
        m_rtpBufferAudio->addDestination(address, port);
    }
}

void AudioNetSink::deleteDestination(const QString& address, uint16_t port)
{
    if (m_rtpBufferAudio) {
        m_rtpBufferAudio->deleteDestination(address, port);
    }
}

void AudioNetSink::setParameters(bool stereo, int sampleRate)
{
    if (m_rtpBufferAudio) {
        m_rtpBufferAudio->setPayloadInformation(stereo ? RTPSink::PayloadL16Stereo : RTPSink::PayloadL16Mono, sampleRate);
    }
}

void AudioNetSink::write(qint16 sample)
{
    if (m_type == SinkUDP)
    {
        if (m_bufferIndex >= m_udpBlockSize)
        {
            m_udpSocket->writeDatagram((const char*)m_data, (qint64 ) m_udpBlockSize, m_address, m_port);
            m_bufferIndex = 0;
        }
        else
        {
            qint16 *p = (qint16*) &m_data[m_bufferIndex];
            *p = sample;
            m_bufferIndex += sizeof(qint16);
        }
    }
    else if (m_type == SinkRTP)
    {
        m_rtpBufferAudio->write((uint8_t *) &sample);
    }
}

void AudioNetSink::write(qint16 lSample, qint16 rSample)
{
    if (m_type == SinkUDP)
    {
        if (m_bufferIndex >= m_udpBlockSize)
        {
            m_udpSocket->writeDatagram((const char*)m_data, (qint64 ) m_udpBlockSize, m_address, m_port);
            m_bufferIndex = 0;
        }
        else
        {
            qint16 *p = (qint16*) &m_data[m_bufferIndex];
            *p = lSample;
            m_bufferIndex += sizeof(qint16);
            p = (qint16*) &m_data[m_bufferIndex];
            *p = rSample;
            m_bufferIndex += sizeof(qint16);
        }
    }
    else if (m_type == SinkRTP)
    {
        m_rtpBufferAudio->write((uint8_t *) &lSample, (uint8_t *) &rSample);
    }
}

void AudioNetSink::write(AudioSample* samples, uint32_t numSamples)
{
    if (m_type == SinkUDP)
    {
        int samplesIndex = 0;

        if (m_bufferIndex + numSamples*sizeof(AudioSample) >= m_udpBlockSize) // fill remainder of buffer and send it
        {
            memcpy(&m_data[m_bufferIndex], &samples[samplesIndex], m_udpBlockSize - m_bufferIndex); // fill remainder of buffer
            m_udpSocket->writeDatagram((const char*)m_data, (qint64 ) m_udpBlockSize, m_address, m_port);
            m_bufferIndex = 0;
            samplesIndex += (m_udpBlockSize - m_bufferIndex) / sizeof(AudioSample);
            numSamples -= (m_udpBlockSize - m_bufferIndex) / sizeof(AudioSample);
        }

        while (numSamples > m_udpBlockSize/sizeof(AudioSample)) // send directly from input without buffering
        {
            m_udpSocket->writeDatagram((const char*)&samples[samplesIndex], (qint64 ) m_udpBlockSize, m_address, m_port);
            samplesIndex += m_udpBlockSize/sizeof(AudioSample);
            numSamples -= m_udpBlockSize/sizeof(AudioSample);
        }

        memcpy(&m_data[m_bufferIndex], &samples[samplesIndex], numSamples*sizeof(AudioSample));
    }
    else if (m_type == SinkRTP)
    {
        m_rtpBufferAudio->write((uint8_t *) samples, numSamples*2); // 2 x 16 bit sample
    }
}

void AudioNetSink::moveToThread(QThread *thread)
{
    m_udpSocket->moveToThread(thread);
}

