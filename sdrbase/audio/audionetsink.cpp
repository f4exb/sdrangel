///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <algorithm>

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
    m_stereo(false),
    m_decimation(1),
    m_decimationCount(0),
    m_codecInputSize(960),
    m_codecInputIndex(0),
    m_bufferIndex(0),
    m_port(9998)
{
    std::fill(m_data, m_data+m_dataBlockSize, 0);
    std::fill(m_opusIn, m_opusIn+m_opusBlockSize, 0);
    m_codecRatio = (m_sampleRate / m_decimation) / (AudioOpus::m_bitrate / 8); // compressor ratio
    m_udpSocket = new QUdpSocket(parent);
}

AudioNetSink::AudioNetSink(QObject *parent, int sampleRate, bool stereo) :
    m_type(SinkUDP),
    m_codec(CodecL16),
    m_rtpBufferAudio(0),
    m_sampleRate(48000),
    m_stereo(false),
    m_decimation(1),
    m_decimationCount(0),
    m_codecInputSize(960),
    m_codecInputIndex(0),
    m_bufferIndex(0),
    m_port(9998)
{
    std::fill(m_data, m_data+m_dataBlockSize, 0);
    std::fill(m_opusIn, m_opusIn+m_opusBlockSize, 0);
    m_codecRatio = (m_sampleRate / m_decimation) / (AudioOpus::m_bitrate / 8); // compressor ratio
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
    m_stereo = stereo;
    m_sampleRate = sampleRate;

    setNewCodecData();

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
        case CodecG722:
            m_rtpBufferAudio->setPayloadInformation(RTPSink::PayloadG722, sampleRate/2);
            break;
        case CodecOpus:
            m_rtpBufferAudio->setPayloadInformation(RTPSink::PayloadOpus, sampleRate);
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
    setNewCodecData();
    m_decimationCount = 0;
}

void AudioNetSink::setNewCodecData()
{
    if (m_codec == CodecOpus)
    {
        m_codecInputSize = m_sampleRate / (m_decimation * 50); // 20ms = 1/50s - size is per channel
        m_codecInputSize = m_codecInputSize > 960 ? 960 : m_codecInputSize; // hard limit of 48 kS/s
        m_codecRatio = (m_sampleRate / m_decimation) / (AudioOpus::m_bitrate / 8); // compressor ratio
        qDebug() << "AudioNetSink::setNewCodecData: CodecOpus:"
            << " m_codecInputSize: " << m_codecInputSize
            << " m_codecRatio: " << m_codecRatio
            << " Fs: " << m_sampleRate/m_decimation
            << " stereo: " << m_stereo;
        m_opus.setEncoder(m_sampleRate/m_decimation, m_stereo ? 2 : 1);
        m_codecInputIndex = 0;
        m_bufferIndex = 0;
    }

    setDecimationFilters();
}

void AudioNetSink::setDecimationFilters()
{
    int decimatedSampleRate = m_sampleRate / m_decimation;

    switch (m_codec)
    {
    case CodecPCMA:
    case CodecPCMU:
        m_audioFilter.setDecimFilters(m_sampleRate, decimatedSampleRate, 3300.0, 300.0);
        break;
    case CodecG722:
        m_audioFilter.setDecimFilters(m_sampleRate, decimatedSampleRate, 7000.0, 50.0);
        break;
    case CodecOpus:
    case CodecL8:
    case CodecL16:
    default:
        m_audioFilter.setDecimFilters(m_sampleRate, decimatedSampleRate, 0.45*decimatedSampleRate, 50.0);
        break;
    }
}

void AudioNetSink::write(qint16 isample)
{
    qint16& sample = isample;

    if (m_decimation > 1)
    {
        float lpSample = m_audioFilter.run(sample / 32768.0f);

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
        if (m_codec == CodecG722)
        {
            if (m_bufferIndex >= 2*m_udpBlockSize)
            {
                m_udpSocket->writeDatagram((const char*) m_data, (qint64 ) m_udpBlockSize, m_address, m_port);
                m_bufferIndex = 0;
            }
        }
        else
        {
            if (m_bufferIndex >= m_udpBlockSize)
            {
                m_udpSocket->writeDatagram((const char*) m_data, (qint64 ) m_udpBlockSize, m_address, m_port);
                m_bufferIndex = 0;
            }
        }

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
        case CodecG722:
        {
            qint16 *p = (qint16*) &m_data[m_udpBlockSize + 2*m_bufferIndex];
            *p = sample;
            m_bufferIndex += 1;

            if (m_bufferIndex == 2*m_udpBlockSize) {
                m_g722.encode((uint8_t *) m_data, (const int16_t*) &m_data[m_udpBlockSize], 2*m_udpBlockSize);
            }
        }
            break;
        case CodecOpus:
        {
            if (m_codecInputIndex == m_codecInputSize)
            {
                int nbBytes = m_opus.encode(m_codecInputSize, m_opusIn, (uint8_t *) m_data);
                nbBytes = nbBytes > m_udpBlockSize ? m_udpBlockSize : nbBytes;
                m_udpSocket->writeDatagram((const char*) m_data, (qint64 ) nbBytes, m_address, m_port);
                m_codecInputIndex = 0;
            }

            m_opusIn[m_codecInputIndex++] = sample;
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
        case CodecG722:
        {

            if (m_bufferIndex >= 2*m_g722BlockSize)
            {
                m_g722.encode((uint8_t *) m_data, (const int16_t*) &m_data[m_g722BlockSize], 2*m_g722BlockSize);
                m_bufferIndex = 0;
            }

            if (m_bufferIndex % 2 == 0) {
                m_rtpBufferAudio->write((uint8_t *) &m_data[m_bufferIndex/2]);
            }

            qint16 *p = (qint16*) &m_data[m_g722BlockSize + 2*m_bufferIndex];
            *p = sample;
            m_bufferIndex += 1;
        }
            break;
        case CodecOpus:
        {
            if (m_codecInputIndex == m_codecInputSize)
            {
                int nbBytes = m_opus.encode(m_codecInputSize, m_opusIn, (uint8_t *) m_data);
                if (nbBytes != AudioOpus::m_bitrate/400) { // 8 bits for 1/50s (20ms)
                    qWarning("AudioNetSink::write: CodecOpus mono: unexpected output frame size: %d bytes", nbBytes);
                }
                m_bufferIndex = 0;
                m_codecInputIndex = 0;
            }

            if (m_codecInputIndex % m_codecRatio == 0) {
                m_rtpBufferAudio->write((uint8_t *) &m_data[m_bufferIndex++]);
            }

            m_opusIn[m_codecInputIndex++] = sample;
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
            m_udpSocket->writeDatagram((const char*) m_data, (qint64 ) m_udpBlockSize, m_address, m_port);
            m_bufferIndex = 0;
        }

        switch(m_codec)
        {
        case CodecPCMA:
        case CodecPCMU:
        case CodecG722:
            break; // mono modes - do nothing
        case CodecOpus:
        {
            if (m_codecInputIndex == m_codecInputSize)
            {
                int nbBytes = m_opus.encode(m_codecInputSize, m_opusIn, (uint8_t *) m_data);
                nbBytes = nbBytes > m_udpBlockSize ? m_udpBlockSize : nbBytes;
                m_udpSocket->writeDatagram((const char*) m_data, (qint64 ) nbBytes, m_address, m_port);
                m_codecInputIndex = 0;
            }

            m_opusIn[2*m_codecInputIndex]   = lSample;
            m_opusIn[2*m_codecInputIndex+1] = rSample;
            m_codecInputIndex++;
        }
            break;
        case CodecL8:
        {
            qint8 *p = (qint8*) &m_data[m_bufferIndex];
            *p = lSample / 256;
            m_bufferIndex += sizeof(qint8);
            p = (qint8*) &m_data[m_bufferIndex];
            *p = rSample / 256;
            m_bufferIndex += sizeof(qint8);
        }
            break;
        case CodecL16:
        default:
        {
            qint16 *p = (qint16*) &m_data[m_bufferIndex];
            *p = lSample;
            m_bufferIndex += sizeof(qint16);
            p = (qint16*) &m_data[m_bufferIndex];
            *p = rSample;
            m_bufferIndex += sizeof(qint16);
        }
            break;
        }
    }
    else if (m_type == SinkRTP)
    {
        switch(m_codec)
        {
        case CodecPCMA:
        case CodecPCMU:
        case CodecG722:
            break; // mono modes - do nothing
        case CodecOpus:
        {
            if (m_codecInputIndex == m_codecInputSize)
            {
                int nbBytes = m_opus.encode(m_codecInputSize, m_opusIn, (uint8_t *) m_data);
                if (nbBytes != AudioOpus::m_bitrate/400) { // 8 bits for 1/50s (20ms)
                    qWarning("AudioNetSink::write: CodecOpus stereo: unexpected output frame size: %d bytes", nbBytes);
                }
                m_bufferIndex = 0;
                m_codecInputIndex = 0;
            }

            if (m_codecInputIndex % m_codecRatio == 0) {
                m_rtpBufferAudio->write((uint8_t *) &m_data[m_bufferIndex++]);
            }

            m_opusIn[2*m_codecInputIndex]   = lSample;
            m_opusIn[2*m_codecInputIndex+1] = rSample;
            m_codecInputIndex++;
        }
            break;
        case CodecL8:
        {
            qint8 pl = lSample / 256;
            qint8 pr = rSample / 256;
            m_rtpBufferAudio->write((uint8_t *) &pl, (uint8_t *) &pr);
        }
            break;
        case CodecL16:
        default:
            m_rtpBufferAudio->write((uint8_t *) &lSample, (uint8_t *) &rSample);
            break;
        }
    }
}

void AudioNetSink::moveToThread(QThread *thread)
{
    m_udpSocket->moveToThread(thread);
}

