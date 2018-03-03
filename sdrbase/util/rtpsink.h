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
#include <stdint.h>

// jrtplib includes
#include "rtpsession.h"
#include "rtpudpv4transmitternobind.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtplibraryversion.h"

#include "util/export.h"

class SDRBASE_API RTPSinkMemoryManager : public jrtplib::RTPMemoryManager
{
public:
    RTPSinkMemoryManager()
    {
        alloccount = 0;
        freecount = 0;
    }
    ~RTPSinkMemoryManager()
    {
        qDebug() << "RTPSinkMemoryManager::~RTPSinkMemoryManager: alloc: " << alloccount << " free: " << freecount;
    }
    void *AllocateBuffer(size_t numbytes, int memtype)
    {
        void *buf = malloc(numbytes);
        qDebug() << "RTPSinkMemoryManager::AllocateBuffer: Allocated " << numbytes << " bytes at location " << buf << " (memtype = " << memtype << ")";
        alloccount++;
        return buf;
    }

    void FreeBuffer(void *p)
    {
        qDebug() << "RTPSinkMemoryManager::FreeBuffer: Freeing block " << p;
        freecount++;
        free(p);
    }
private:
    int alloccount,freecount;
};

class RTPSink
{
public:
    typedef enum
    {
        PayloadL16Mono,
        PayloadL16Stereo,
    } PayloadType;

    RTPSink(const QString& address, uint16_t port, PayloadType payloadType = PayloadL16Mono);
    ~RTPSink();

    bool isValid() const { return m_valid; }
    void setPayloadType(PayloadType payloadType);

    void setDestination(const QString& address, uint16_t port);
    void deleteDestination(const QString& address, uint16_t port);
    void addDestination(const QString& address, uint16_t port);

    void write(const uint8_t *sampleByte);
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
    uint32_t m_destip;
    uint16_t m_destport;
    jrtplib::RTPSession m_rtpSession;
    jrtplib::RTPSessionParams m_rtpSessionParams;
    jrtplib::RTPUDPv4TransmissionNoBindParams m_rtpTransmissionParams;
    jrtplib::RTPUDPv4TransmitterNoBind m_rtpTransmitter;
    RTPSinkMemoryManager m_rtpMemoryManager;
    bool m_endianReverse;
    QMutex m_mutex;
};


#endif /* SDRBASE_UTIL_RTPSINK_H_ */
