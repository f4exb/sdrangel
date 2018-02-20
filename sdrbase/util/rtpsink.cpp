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

#include "rtpsink.h"
#include "dsp/dsptypes.h"
#include <algorithm>
#include <sys/socket.h>

RTPSink::RTPSink(const QString& address, uint16_t port, PayloadType payloadType) :
    m_sampleRate(48000),
    m_sampleBytes(0),
    m_packetSamples(0),
    m_bufferSize(0),
    m_sampleBufferIndex(0),
    m_byteBuffer(0),
    m_destport(port),
    m_mutex(QMutex::Recursive)
{
	// Here we use JRTPLIB in a bit funny way since we do not want the socket to bind because we are only sending
	// data to a remote party and we don't want to waste a port on the local machine for each possible connection that may not be used.
	// Therefore we create a socket and assign it through the SetUseExistingSockets method of the RTPUDPv4TransmissionParams object
	// By doing this the socket is left unbound but sending RTP packets with the library is still possible. Other functions may
	// not work but we don't care

	m_rtpsock = socket(PF_INET,SOCK_DGRAM,0);

	if (m_rtpsock < 0) {
		qCritical("RTPSink::RTPSink: cannot allocate socket");
		m_valid = false;
	}

    uint32_t endianTest32 = 1;
    uint8_t *ptr = (uint8_t*) &endianTest32;
    m_endianReverse = (*ptr == 1);

    m_destip = inet_addr(address.toStdString().c_str());
    m_destip = ntohl(m_destip);

    m_rtpSessionParams.SetOwnTimestampUnit(1.0 / (double) m_sampleRate);
    m_rtpTransmissionParams.SetUseExistingSockets(m_rtpsock, m_rtpsock);

    int status = m_rtpSession.Create(m_rtpSessionParams, &m_rtpTransmissionParams);

    if (status < 0) {
        qCritical("RTPSink::RTPSink: cannot create session: %s", jrtplib::RTPGetErrorString(status).c_str());
        m_valid = false;
    } else {
        qDebug("RTPSink::RTPSink: created session: %s", jrtplib::RTPGetErrorString(status).c_str());
    }

    setPayloadType(payloadType);
    m_valid = true;
}

RTPSink::~RTPSink()
{
    jrtplib::RTPTime delay = jrtplib::RTPTime(10.0);
    m_rtpSession.BYEDestroy(delay, "Time's up", 9);

    if (m_byteBuffer) {
        delete[] m_byteBuffer;
    }
}

void RTPSink::setPayloadType(PayloadType payloadType)
{
    QMutexLocker locker(&m_mutex);

    qDebug("RTPSink::setPayloadType: %d", payloadType);

    switch (payloadType)
    {
    case PayloadL16Stereo:
        m_sampleRate = 48000;
        m_sampleBytes = sizeof(AudioSample);
        m_rtpSession.SetDefaultPayloadType(96);
        break;
    case PayloadL16Mono:
    default:
        m_sampleRate = 48000;
        m_sampleBytes = sizeof(int16_t);
        m_rtpSession.SetDefaultPayloadType(96);
        break;
    }

    m_packetSamples = m_sampleRate/50; // 20ms packet samples
    m_bufferSize = m_packetSamples * m_sampleBytes;

    if (m_byteBuffer) {
        delete[] m_byteBuffer;
    }

    m_byteBuffer = new uint8_t[m_bufferSize];
    m_sampleBufferIndex = 0;
    m_payloadType = payloadType;

    int status = m_rtpSession.SetTimestampUnit(1.0 / (double) m_sampleRate);

    if (status < 0) {
        qCritical("RTPSink::setPayloadType: cannot set timestamp unit: %s", jrtplib::RTPGetErrorString(status).c_str());
    } else {
        qDebug("RTPSink::setPayloadType: timestamp unit set to %f: %s",
               1.0 / (double) m_sampleRate,
               jrtplib::RTPGetErrorString(status).c_str());
    }

    status = m_rtpSession.SetDefaultMark(false);

    if (status < 0) {
        qCritical("RTPSink::setPayloadType: cannot set default mark: %s", jrtplib::RTPGetErrorString(status).c_str());
    } else {
        qDebug("RTPSink::setPayloadType: set default mark to false: %s", jrtplib::RTPGetErrorString(status).c_str());
    }

    status = m_rtpSession.SetDefaultTimestampIncrement(m_packetSamples);

    if (status < 0) {
        qCritical("RTPSink::setPayloadType: cannot set default timestamp increment: %s", jrtplib::RTPGetErrorString(status).c_str());
    } else {
        qDebug("RTPSink::setPayloadType: set default timestamp increment to %d: %s", m_packetSamples, jrtplib::RTPGetErrorString(status).c_str());
    }

    status = m_rtpSession.SetMaximumPacketSize(m_bufferSize+40);

    if (status < 0) {
        qCritical("RTPSink::setPayloadType: cannot set maximum packet size: %s", jrtplib::RTPGetErrorString(status).c_str());
    } else {
        qDebug("RTPSink::setPayloadType: set maximum packet size to %d bytes: %s", m_bufferSize+40, jrtplib::RTPGetErrorString(status).c_str());
    }
}

void RTPSink::setDestination(const QString& address, uint16_t port)
{
    m_rtpSession.ClearDestinations();
    m_rtpSession.DeleteDestination(jrtplib::RTPIPv4Address(m_destip, m_destport));
    m_destip = inet_addr(address.toStdString().c_str());
    m_destip = ntohl(m_destip);
    m_destport = port;

    int status = m_rtpSession.AddDestination(jrtplib::RTPIPv4Address(m_destip, m_destport));

    if (status < 0) {
        qCritical("RTPSink::setDestination: cannot set destination address: %s", jrtplib::RTPGetErrorString(status).c_str());
    }
}

void RTPSink::deleteDestination(const QString& address, uint16_t port)
{
    uint32_t destip = inet_addr(address.toStdString().c_str());
    destip = ntohl(m_destip);

    int status =  m_rtpSession.DeleteDestination(jrtplib::RTPIPv4Address(destip, port));

    if (status < 0) {
        qCritical("RTPSink::deleteDestination: cannot delete destination address: %s", jrtplib::RTPGetErrorString(status).c_str());
    }
}

void RTPSink::addDestination(const QString& address, uint16_t port)
{
    uint32_t destip = inet_addr(address.toStdString().c_str());
    destip = ntohl(m_destip);

    int status = m_rtpSession.AddDestination(jrtplib::RTPIPv4Address(destip, port));

    if (status < 0) {
        qCritical("RTPSink::addDestination: cannot add destination address: %s", jrtplib::RTPGetErrorString(status).c_str());
    } else {
        qDebug("RTPSink::addDestination: destination address set to %s:%d: %s",
                address.toStdString().c_str(),
                port,
                jrtplib::RTPGetErrorString(status).c_str());
    }
}

void RTPSink::write(const uint8_t *sampleByte)
{
    QMutexLocker locker(&m_mutex);

    if (m_sampleBufferIndex < m_packetSamples)
    {
        writeNetBuf(&m_byteBuffer[m_sampleBufferIndex*m_sampleBytes],
                sampleByte,
                elemLength(m_payloadType),
                m_sampleBytes,
                m_endianReverse);
        m_sampleBufferIndex++;
    }
    else
    {
        int status = m_rtpSession.SendPacket((const void *) m_byteBuffer, (std::size_t) m_bufferSize);

        if (status < 0) {
            qCritical("RTPSink::write: cannot write packet: %s", jrtplib::RTPGetErrorString(status).c_str());
        }

        writeNetBuf(&m_byteBuffer[0], sampleByte,  elemLength(m_payloadType), m_sampleBytes, m_endianReverse);
        m_sampleBufferIndex = 1;
    }
}

void RTPSink::write(const uint8_t *samples, int nbSamples)
{
    int samplesIndex = 0;
    QMutexLocker locker(&m_mutex);

    // fill remainder of buffer and send it
    if (m_sampleBufferIndex + nbSamples > m_packetSamples)
    {
        writeNetBuf(&m_byteBuffer[m_sampleBufferIndex*m_sampleBytes],
                &samples[samplesIndex*m_sampleBytes],
                elemLength(m_payloadType),
                (m_packetSamples - m_sampleBufferIndex)*m_sampleBytes,
                m_endianReverse);
        m_rtpSession.SendPacket((const void *) m_byteBuffer, (std::size_t) m_bufferSize);
        nbSamples -= (m_packetSamples - m_sampleBufferIndex);
        m_sampleBufferIndex = 0;
    }

    // send complete packets
    while (nbSamples > m_packetSamples)
    {
        writeNetBuf(m_byteBuffer,
                samples,
                elemLength(m_payloadType),
                m_bufferSize,
                m_endianReverse);
        m_rtpSession.SendPacket((const void *) m_byteBuffer, (std::size_t) m_bufferSize);
        samplesIndex += m_packetSamples;
        nbSamples -= m_packetSamples;
    }

    // copy remainder of input to buffer
    writeNetBuf(&m_byteBuffer[m_sampleBufferIndex*m_sampleBytes],
            &samples[samplesIndex*m_sampleBytes],
            elemLength(m_payloadType),
            nbSamples*m_sampleBytes,m_endianReverse);
}

void RTPSink::writeNetBuf(uint8_t *dest, const uint8_t *src, unsigned int elemLen, unsigned int bytesLen, bool endianReverse)
{
    for (unsigned int i = 0; i < bytesLen; i += elemLen)
    {
        memcpy(&dest[i], &src[i], elemLen);

        if (endianReverse) {
            std::reverse(&dest[i], &dest[i+elemLen]);
        }
    }
}

unsigned int RTPSink::elemLength(PayloadType payloadType)
{
    switch (payloadType)
    {
    case PayloadL16Stereo:
        return sizeof(int16_t);
        break;
    case PayloadL16Mono:
    default:
        return sizeof(int16_t);
        break;
    }
}

