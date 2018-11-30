/*

 This file is a part of JRTPLIB
 Copyright (c) 1999-2017 Jori Liesenborgs

 Contact: jori.liesenborgs@gmail.com

 This library was developed at the Expertise Centre for Digital Media
 (http://www.edm.uhasselt.be), a research center of the Hasselt University
 (http://www.uhasselt.be). The library is based upon work done for
 my thesis at the School for Knowledge Technology (Belgium/The Netherlands).

 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 IN THE SOFTWARE.

 */

#include "rtpudptransmitter.h"
#include "rtperrors.h"
#include "rtpaddress.h"
#include "rtpstructs.h"
#include "rtprawpacket.h"

#include <QUdpSocket>

namespace qrtplib
{

RTPUDPTransmitter::RTPUDPTransmitter() :
    m_rawPacketQueueLock(QMutex::Recursive)
{
    m_created = false;
    m_init = false;
    m_rtcpsock = 0;
    m_rtpsock = 0;
    m_deletesocketswhendone = false;
    m_waitingfordata = false;
    m_rtcpPort = 0;
    m_rtpPort = 0;
    m_receivemode = RTPTransmitter::AcceptAll;
    m_maxpacksize = 0;
    memset(m_rtpBuffer, 0, m_absoluteMaxPackSize);
    memset(m_rtcpBuffer, 0, m_absoluteMaxPackSize);
}

RTPUDPTransmitter::~RTPUDPTransmitter()
{
    Destroy();
}

int RTPUDPTransmitter::Init()
{
    if (m_init) {
        return ERR_RTP_UDPV4TRANS_ALREADYINIT;
    }

    m_init = true;
    return 0;
}

int RTPUDPTransmitter::Create(std::size_t maximumpacketsize, const RTPTransmissionParams *transparams)
{
    const RTPUDPTransmissionParams *params, defaultparams;
    qint64 size;

    if (maximumpacketsize > m_absoluteMaxPackSize) {
        return ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG;
    }

    if (!m_init) {
        return ERR_RTP_UDPV4TRANS_NOTINIT;
    }

    if (m_created) {
        return ERR_RTP_UDPV4TRANS_ALREADYCREATED;
    }

    // Obtain transmission parameters

    if (transparams == 0) {
        params = &defaultparams;
    }
    else
    {
        if (transparams->GetTransmissionProtocol() != RTPTransmitter::IPv4UDPProto)
        {
            return ERR_RTP_UDPV4TRANS_ILLEGALPARAMETERS;
        }
        params = (const RTPUDPTransmissionParams *) transparams;
    }

    // Determine the port numbers

    m_localIP = params->GetBindIP();

    if (params->GetAllowOddPortbase())
    {
        m_rtpPort = params->GetPortbase();
        m_rtcpPort = params->GetForcedRTCPPort();
    }
    else
    {
        if (params->GetPortbase() % 2 == 0)
        {
            m_rtpPort = params->GetPortbase();
            m_rtcpPort = m_rtpPort + 1;
        }
        else
        {
            return ERR_RTP_UDPV4TRANS_PORTBASENOTEVEN;
        }
    }

    if (params->GetUseExistingSockets(&m_rtpsock, &m_rtcpsock))
    {
        m_deletesocketswhendone = false;
    }
    else
    {
        m_deletesocketswhendone = true;

        m_rtpsock = new QUdpSocket();

        // If we're multiplexing, we're just going to set the RTCP socket to equal the RTP socket
        if (params->GetRTCPMultiplexing())
        {
            m_rtcpsock = m_rtpsock;
            m_rtcpPort = m_rtpPort;
        } else {
            m_rtcpsock = new QUdpSocket();
        }
    }

    // set socket buffer sizes

    size = params->GetRTPReceiveBufferSize();
    m_rtpsock->setReadBufferSize(size);

    if (m_rtpsock != m_rtcpsock)
    {
        size = params->GetRTCPReceiveBufferSize();
        m_rtcpsock->setReadBufferSize(size);
    }

    m_maxpacksize = maximumpacketsize;
    m_multicastInterface = params->GetMulticastInterface();
    m_receivemode = RTPTransmitter::AcceptAll;

    m_waitingfordata = false;
    m_created = true;

    return 0;
}

int RTPUDPTransmitter::BindSockets()
{
    if (!m_rtpsock->bind(m_localIP, m_rtpPort)) {
        return ERR_RTP_UDPV4TRANS_CANTBINDRTPSOCKET;
    }

    connect(m_rtpsock, SIGNAL(readyRead()), this, SLOT(readRTPPendingDatagrams()));

    if (m_rtpsock != m_rtcpsock)
    {
        if (!m_rtcpsock->bind(m_localIP, m_rtcpPort)) {
            return ERR_RTP_UDPV4TRANS_CANTBINDRTCPSOCKET;
        }

        connect(m_rtcpsock, SIGNAL(readyRead()), this, SLOT(readRTCPPendingDatagrams()));
    }

    return 0;
}

void RTPUDPTransmitter::Destroy()
{
    if (!m_init) {
        return;
    }

    if (!m_created)
    {
        return;
    }

    if (m_deletesocketswhendone)
    {
        if (m_rtpsock != m_rtcpsock) {
            delete m_rtcpsock;
        }

        delete m_rtpsock;
    }

    m_created = false;
}

RTPTransmissionInfo *RTPUDPTransmitter::GetTransmissionInfo()
{
    if (!m_init) {
        return 0;
    }

    RTPTransmissionInfo *tinf = new RTPUDPTransmissionInfo(m_localIP, m_rtpsock, m_rtcpsock, m_rtpPort, m_rtcpPort);

    return tinf;
}

void RTPUDPTransmitter::DeleteTransmissionInfo(RTPTransmissionInfo *inf)
{
    if (!m_init) {
        return;
    }

    delete inf;
}

bool RTPUDPTransmitter::ComesFromThisTransmitter(const RTPAddress& addr)
{
    if (addr.getAddress() != m_localIP) {
        return false;
    }

    return (addr.getPort() == m_rtpPort) && (addr.getRtcpsendport() == m_rtcpPort);
}

int RTPUDPTransmitter::SendRTPData(const void *data, std::size_t len)
{
    if (!m_init) {
        return ERR_RTP_UDPV4TRANS_NOTINIT;
    }

    if (!m_created) {
        return ERR_RTP_UDPV4TRANS_NOTCREATED;
    }

    if (len > m_maxpacksize)
    {
        return ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG;
    }

    std::list<RTPAddress>::const_iterator it = m_destinations.begin();

    for (; it != m_destinations.end(); ++it)
    {
        m_rtpsock->writeDatagram((const char*) data, (qint64) len, it->getAddress(), it->getPort());
    }

    return 0;
}

int RTPUDPTransmitter::SendRTCPData(const void *data, std::size_t len)
{
    if (!m_init) {
        return ERR_RTP_UDPV4TRANS_NOTINIT;
    }

    if (!m_created) {
        return ERR_RTP_UDPV4TRANS_NOTCREATED;
    }

    if (len > m_maxpacksize) {
        return ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG;
    }

    std::list<RTPAddress>::const_iterator it = m_destinations.begin();

    for (; it != m_destinations.end(); ++it)
    {
        m_rtcpsock->writeDatagram((const char*) data, (qint64) len, it->getAddress(), it->getRtcpsendport());
    }

    return 0;
}

int RTPUDPTransmitter::AddDestination(const RTPAddress &addr)
{
    m_destinations.push_back(addr);
    return 0;
}

int RTPUDPTransmitter::DeleteDestination(const RTPAddress &addr)
{
    m_destinations.remove(addr);
    return 0;
}

void RTPUDPTransmitter::ClearDestinations()
{
    m_destinations.clear();
}

bool RTPUDPTransmitter::SupportsMulticasting()
{
    QNetworkInterface::InterfaceFlags flags = m_multicastInterface.flags();
    QAbstractSocket::SocketState rtpSocketState = m_rtpsock->state();
    QAbstractSocket::SocketState rtcpSocketState = m_rtcpsock->state();
    return m_multicastInterface.isValid()
            && (rtpSocketState & QAbstractSocket::BoundState)
            && (rtcpSocketState & QAbstractSocket::BoundState)
            && (flags & QNetworkInterface::CanMulticast)
            && (flags & QNetworkInterface::IsRunning)
            && !(flags & QNetworkInterface::IsLoopBack);
}

int RTPUDPTransmitter::JoinMulticastGroup(const RTPAddress &addr)
{
    if (!m_init) {
        return ERR_RTP_UDPV4TRANS_NOTINIT;
    }

    if (!m_created) {
        return ERR_RTP_UDPV4TRANS_NOTCREATED;
    }

    if (!SupportsMulticasting()) {
        return ERR_RTP_UDPV6TRANS_NOMULTICASTSUPPORT;
    }

    if (m_rtpsock->joinMulticastGroup(addr.getAddress(), m_multicastInterface))
    {
        if (m_rtpsock != m_rtcpsock)
        {
            if (!m_rtcpsock->joinMulticastGroup(addr.getAddress(), m_multicastInterface)) {
                return ERR_RTP_UDPV4TRANS_COULDNTJOINMULTICASTGROUP;
            }
        }
    }
    else
    {
        return ERR_RTP_UDPV4TRANS_COULDNTJOINMULTICASTGROUP;
    }

    return 0;
}

int RTPUDPTransmitter::LeaveMulticastGroup(const RTPAddress &addr)
{
    if (!m_init) {
        return ERR_RTP_UDPV4TRANS_NOTINIT;
    }

    if (!m_created) {
        return ERR_RTP_UDPV4TRANS_NOTCREATED;
    }

    if (!SupportsMulticasting()) {
        return ERR_RTP_UDPV6TRANS_NOMULTICASTSUPPORT;
    }

    m_rtpsock->leaveMulticastGroup(addr.getAddress());

    if (m_rtpsock != m_rtcpsock)
    {
        m_rtcpsock->leaveMulticastGroup(addr.getAddress());
    }

    return 0;
}

int RTPUDPTransmitter::SetReceiveMode(RTPTransmitter::ReceiveMode m)
{
    if (!m_init) {
        return ERR_RTP_UDPV4TRANS_NOTINIT;
    }

    if (!m_created) {
        return ERR_RTP_UDPV4TRANS_NOTCREATED;
    }

    if (m != m_receivemode) {
        m_receivemode = m;
    }

    return 0;
}

int RTPUDPTransmitter::AddToIgnoreList(const RTPAddress &addr)
{
    m_ignoreList.push_back(addr);
    return 0;
}

int RTPUDPTransmitter::DeleteFromIgnoreList(const RTPAddress &addr)
{
    m_ignoreList.remove(addr);
    return 0;
}

void RTPUDPTransmitter::ClearIgnoreList()
{
    m_ignoreList.clear();
}

int RTPUDPTransmitter::AddToAcceptList(const RTPAddress &addr)
{
    m_acceptList.push_back(addr);
    return 0;
}

int RTPUDPTransmitter::DeleteFromAcceptList(const RTPAddress &addr)
{
    m_acceptList.remove(addr);
    return 0;
}

void RTPUDPTransmitter::ClearAcceptList()
{
    m_acceptList.clear();
}

int RTPUDPTransmitter::SetMaximumPacketSize(std::size_t s)
{
    if (!m_init) {
        return ERR_RTP_UDPV4TRANS_NOTINIT;
    }

    if (!m_created) {
        return ERR_RTP_UDPV4TRANS_NOTCREATED;
    }

    if (s > m_absoluteMaxPackSize) {
        return ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG;
    }

    m_maxpacksize = s;
    return 0;
}

RTPRawPacket *RTPUDPTransmitter::GetNextPacket()
{
    QMutexLocker locker(&m_rawPacketQueueLock);

    if (m_rawPacketQueue.isEmpty()) {
        return 0;
    } else {
        return m_rawPacketQueue.takeFirst();
    }
}

void RTPUDPTransmitter::readRTPPendingDatagrams()
{
    while (m_rtpsock->hasPendingDatagrams())
    {
        RTPTime curtime = RTPTime::CurrentTime();
        QHostAddress remoteAddress;
        quint16 remotePort;
        qint64 pendingDataSize = m_rtpsock->pendingDatagramSize();
        qint64 bytesRead = m_rtpsock->readDatagram(m_rtpBuffer, pendingDataSize, &remoteAddress, &remotePort);
        qDebug("RTPUDPTransmitter::readRTPPendingDatagrams: %lld bytes read from %s:%d",
                bytesRead,
                qPrintable(remoteAddress.toString()),
                remotePort);

        RTPAddress rtpAddress;
        rtpAddress.setAddress(remoteAddress);
        rtpAddress.setPort(remotePort);

        if (ShouldAcceptData(rtpAddress))
        {
            bool isrtp = true;

            if (m_rtpsock == m_rtcpsock) // check payload type when multiplexing
            {
                if ((std::size_t) bytesRead > sizeof(RTCPCommonHeader))
                {
                    RTCPCommonHeader *rtcpheader = (RTCPCommonHeader *) m_rtpBuffer;
                    uint8_t packettype = rtcpheader->packettype;

                    if (packettype >= 200 && packettype <= 204) {
                        isrtp = false;
                    }
                }
            }

            RTPRawPacket *pack = new RTPRawPacket((uint8_t *) m_rtpBuffer, bytesRead, rtpAddress, curtime, isrtp);

            m_rawPacketQueueLock.lock();
            m_rawPacketQueue.append(pack);
            m_rawPacketQueueLock.unlock();

            emit NewDataAvailable();
        }
    }
}

void RTPUDPTransmitter::readRTCPPendingDatagrams()
{
    while (m_rtcpsock->hasPendingDatagrams())
    {
        RTPTime curtime = RTPTime::CurrentTime();
        QHostAddress remoteAddress;
        quint16 remotePort;
        qint64 pendingDataSize = m_rtcpsock->pendingDatagramSize();
        qint64 bytesRead = m_rtcpsock->readDatagram(m_rtcpBuffer, pendingDataSize, &remoteAddress, &remotePort);
        qDebug("RTPUDPTransmitter::readRTCPPendingDatagrams: %lld bytes read from %s:%d",
                bytesRead,
                qPrintable(remoteAddress.toString()),
                remotePort);

        RTPAddress rtpAddress;
        rtpAddress.setAddress(remoteAddress);
        rtpAddress.setPort(remotePort);

        if (ShouldAcceptData(rtpAddress))
        {
            RTPRawPacket *pack = new RTPRawPacket((uint8_t *) m_rtcpBuffer, bytesRead, rtpAddress, curtime, false);

            m_rawPacketQueueLock.lock();
            m_rawPacketQueue.append(pack);
            m_rawPacketQueueLock.unlock();

            emit NewDataAvailable();
        }
    }
}

bool RTPUDPTransmitter::ShouldAcceptData(const RTPAddress& rtpAddress)
{
    if (m_receivemode == RTPTransmitter::AcceptAll)
    {
        return true;
    }
    else if (m_receivemode == RTPTransmitter::AcceptSome)
    {
        std::list<RTPAddress>::iterator findIt = std::find(m_acceptList.begin(), m_acceptList.end(), rtpAddress);
        return findIt != m_acceptList.end();
    }
    else // this is RTPTransmitter::IgnoreSome
    {
        std::list<RTPAddress>::iterator findIt = std::find(m_ignoreList.begin(), m_ignoreList.end(), rtpAddress);
        return findIt == m_ignoreList.end();
    }
}

} // namespace


