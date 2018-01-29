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
#include <stdint.h>

// jrtplib includes
#include "rtpsession.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtplibraryversion.h"

template<typename SampleType>
class RTPSink
{
public:
    RTPSink(const QString& address, uint16_t port, int sampleRate) :
        m_destport(port),
        m_sampleBuffer(0),
        m_sampleBufferIndex(0),
        m_mutex(QMutex::Recursive)
    {
        m_destip = inet_addr(address.toStdString().c_str());
        m_destip = ntohl(m_destip);
        m_rtpSessionParams.SetOwnTimestampUnit(1.0/(double) sampleRate);
        m_rtpTransmissionParams.SetPortbase(8092); // FIXME: sort this out

        int status = m_rtpSession.Create(m_rtpSessionParams, &m_rtpTransmissionParams);

        if (status < 0)
        {
            qCritical("RTPSink::RTPSink: cannot create session: %s", jrtplib::RTPGetErrorString(status).c_str());
            return;
        }

        status = m_rtpSession.AddDestination(jrtplib::RTPIPv4Address(m_destip, m_destport));

        if (status < 0)
        {
            qCritical("RTPSink::RTPSink: cannot set destination address: %s", jrtplib::RTPGetErrorString(status).c_str());
            return;
        }

        m_sampleBuffer = new SampleType[sampleRate]; // store 1 second
    }

    ~RTPSink()
    {
        jrtplib::RTPTime delay = jrtplib::RTPTime(10.0);
        m_rtpSession.BYEDestroy(delay, "Time's up", 9);

        if (m_sampleBuffer) { delete[] m_sampleBuffer; }
    }

    void setDestination(const QString& address, uint16_t port)
    {
        if (!m_sampleBuffer) { return; }

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

    void deleteDestination(const QString& address, uint16_t port)
    {
        uint32_t destip = inet_addr(address.toStdString().c_str());
        destip = ntohl(m_destip);

        int status =  m_rtpSession.DeleteDestination(jrtplib::RTPIPv4Address(destip, port));

        if (status < 0) {
            qCritical("RTPSink::deleteDestination: cannot delete destination address: %s", jrtplib::RTPGetErrorString(status).c_str());
        }
    }

    void addDestination(const QString& address, uint16_t port)
    {
        uint32_t destip = inet_addr(address.toStdString().c_str());
        destip = ntohl(m_destip);

        int status = m_rtpSession.AddDestination(jrtplib::RTPIPv4Address(destip, port));

        if (status < 0) {
            qCritical("RTPSink::addDestination: cannot add destination address: %s", jrtplib::RTPGetErrorString(status).c_str());
        }
    }

    void setSampleRate(int sampleRate)
    {
        QMutexLocker locker(&m_mutex);
        if (m_sampleBuffer) { delete[] m_sampleBuffer; }
        m_sampleBuffer = new SampleType[sampleRate]; // store 1 second

        int status = m_rtpSession.SetTimestampUnit(1.0 / (double) sampleRate);

        if (status < 0)
        {
            qCritical("RTPSink::setSampleRate: cannot set timestamp unit: %s", jrtplib::RTPGetErrorString(status).c_str());
            return;
        }
    }

protected:
    uint32_t m_destip;
    uint16_t m_destport;
    jrtplib::RTPSession m_rtpSession;
    jrtplib::RTPSessionParams m_rtpSessionParams;
    jrtplib::RTPUDPv4TransmissionParams m_rtpTransmissionParams;
    SampleType *m_sampleBuffer;
    int m_sampleBufferIndex;
    QMutex m_mutex;
};


#endif /* SDRBASE_UTIL_RTPSINK_H_ */
