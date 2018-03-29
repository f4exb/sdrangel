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

#ifndef SDRBASE_UTIL_RTPSINK_H_
#define SDRBASE_UTIL_RTPSINK_H_

#include <QString>
#include <QMutex>
#include <QDebug>
#include <QHostAddress>
#include <stdint.h>

// qrtplib includes
#include "rtpsession.h"
#include "rtpudptransmitter.h"
#include "rtpaddress.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"

#include "export.h"

class QUdpSocket;

class RTPSink
{
public:
    typedef enum
    {
        PayloadL16Mono,
        PayloadL16Stereo,
    } PayloadType;

    RTPSink(QUdpSocket *udpSocket, int sampleRate, bool stereo);
    ~RTPSink();

    bool isValid() const { return m_valid; }
    void setPayloadInformation(PayloadType payloadType, int sampleRate);

    void setDestination(const QString& address, uint16_t port);
    void deleteDestination(const QString& address, uint16_t port);
    void addDestination(const QString& address, uint16_t port);

    void write(const uint8_t *sampleByte);
    void write(const uint8_t *sampleByteL, const uint8_t *sampleByteR);
    void write(const uint8_t *sampleByte, int nbSamples);

protected:
    /** Reverse endianess in destination buffer */
    static void writeNetBuf(uint8_t *dest, const uint8_t *src, unsigned int elemLen, unsigned int bytesLen, bool endianReverse);
    static unsigned int elemLength(PayloadType payloadType);

    bool m_valid;
    PayloadType m_payloadType;
    int m_sampleRate;
    int m_sampleBytes;
    int m_packetSamples;
    int m_bufferSize;
    int m_sampleBufferIndex;
    uint8_t *m_byteBuffer;
    QHostAddress m_destip;
    uint16_t m_destport;
    qrtplib::RTPSession m_rtpSession;
    qrtplib::RTPSessionParams m_rtpSessionParams;
    qrtplib::RTPUDPTransmissionParams m_rtpTransmissionParams;
    qrtplib::RTPUDPTransmitter m_rtpTransmitter;
    bool m_endianReverse;
    QMutex m_mutex;
};


#endif /* SDRBASE_UTIL_RTPSINK_H_ */
