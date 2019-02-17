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

#include <QDebug>
#include <QUdpSocket>

const int AudioNetSink::m_udpBlockSize = 512;

AudioNetSink::AudioNetSink(QObject *parent) :
    m_type(SinkUDP),
    m_codec(CodecL16),
    m_rtpBufferAudio(0),
    m_sampleRate(48000),
    m_decimation(1),
    m_decimationCount(0),
    m_bufferIndex(0),
    m_port(9998)
{
    memset(m_data, 0, 65536);
    m_udpSocket = new QUdpSocket(parent);
}

AudioNetSink::AudioNetSink(QObject *parent, int sampleRate, bool stereo) :
    m_type(SinkUDP),
    m_codec(CodecL16),
    m_rtpBufferAudio(0),
    m_sampleRate(48000),
    m_decimation(1),
    m_decimationCount(0),
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

void AudioNetSink::setParameters(Codec codec, bool stereo, int sampleRate)
{
    qDebug() << "AudioNetSink::setParameters:"
            << " codec: " << codec
            << " stereo: " << stereo
            << " sampleRate: " << sampleRate;

    m_codec = codec;
    m_sampleRate = sampleRate;
    m_audioFilter.setDecimFilters(m_sampleRate, m_decimation);

    if (m_rtpBufferAudio)
    {
        switch (m_codec)
        {
        case CodecPCMA:
            m_audioCompressor.fillALaw();
            m_rtpBufferAudio->setPayloadInformation(RTPSink::PayloadPCMA8, sampleRate);
            break;
        case CodecPCMU:
            m_audioCompressor.fillULaw();
            m_rtpBufferAudio->setPayloadInformation(RTPSink::PayloadPCMU8, sampleRate);
            break;
        case CodecL8:
            m_rtpBufferAudio->setPayloadInformation(RTPSink::PayloadL8, sampleRate);
            break;
        case CodecL16: // actually no codec
        default:
            m_rtpBufferAudio->setPayloadInformation(stereo ? RTPSink::PayloadL16Stereo : RTPSink::PayloadL16Mono, sampleRate);
            break;
        }
    }
}

void AudioNetSink::setDecimation(uint32_t decimation)
{
    m_decimation = decimation < 1 ? 1 : decimation > 6 ? 6 : decimation;
    qDebug() << "AudioNetSink::setDecimation: " << m_decimation << " from: " << decimation;
    m_audioFilter.setDecimFilters(m_sampleRate, m_decimation);
    m_decimationCount = 0;
}

void AudioNetSink::write(qint16 isample)
{
    qint16& sample = isample;

    if (m_decimation > 1)
    {
        float lpSample = m_audioFilter.runLP(sample / 32768.0f);

        if (m_decimationCount >= m_decimation - 1)
        {
            sample = lpSample * 32768.0f;
            m_decimationCount = 0;
        }
        else
        {
            m_decimationCount++;
            return;
        }
    }

    if (m_type == SinkUDP)
    {
        if (m_bufferIndex >= m_udpBlockSize)
        {
            m_udpSocket->writeDatagram((const char*)m_data, (qint64 ) m_udpBlockSize, m_address, m_port);
            m_bufferIndex = 0;
        }
        else
        {
            switch(m_codec)
            {
            case CodecPCMA:
            case CodecPCMU:
            {
                qint8 *p = (qint8*) &m_data[m_bufferIndex];
                *p = m_audioCompressor.compress8(sample);
                m_bufferIndex += sizeof(qint8);
            }
                break;
            case CodecL8:
            {
                qint8 *p = (qint8*) &m_data[m_bufferIndex];
                *p = sample / 256;
                m_bufferIndex += sizeof(qint8);
            }
                break;
            case CodecL16:
            default:
            {
                qint16 *p = (qint16*) &m_data[m_bufferIndex];
                *p = sample;
                m_bufferIndex += sizeof(qint16);
            }
                break;
            }
        }
    }
    else if (m_type == SinkRTP)
    {
        switch(m_codec)
        {
        case CodecPCMA:
        case CodecPCMU:
        {
            qint8 p = m_audioCompressor.compress8(sample);
            m_rtpBufferAudio->write((uint8_t *) &p);
        }
            break;
        case CodecL8:
        {
            qint8 p = sample / 256;
            m_rtpBufferAudio->write((uint8_t *) &p);
        }
            break;
        case CodecL16:
        default:
            m_rtpBufferAudio->write((uint8_t *) &sample);
            break;
        }
    }
}

void AudioNetSink::write(qint16 ilSample, qint16 irSample)
{
    qint16& lSample = ilSample;
    qint16& rSample = irSample;

    if (m_decimation > 1)
    {
        float lpLSample = m_audioFilter.runLP(lSample / 32768.0f);
        float lpRSample = m_audioFilter.runLP(rSample / 32768.0f);

        if (m_decimationCount >= m_decimation - 1)
        {
            lSample = lpLSample * 32768.0f;
            rSample = lpRSample * 32768.0f;
            m_decimationCount = 0;
        }
        else
        {
            m_decimationCount++;
            return;
        }
    }

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

void AudioNetSink::moveToThread(QThread *thread)
{
    m_udpSocket->moveToThread(thread);
}

