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

#define RTPUDPTRANS_MAXPACKSIZE 65535

namespace qrtplib
{

RTPUDPTransmitter::RTPUDPTransmitter()
{
    m_created = false;
    m_init = false;
    m_rtcpsock = 0;
    m_rtpsock = 0;
    m_waitingfordata = false;
    m_closesocketswhendone = false;
    m_rtcpPort = 0;
    m_rtpPort = 0;
    m_supportsmulticasting = false;
    m_receivemode = RTPTransmitter::AcceptAll;
    m_localhostname = 0;
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
    struct sockaddr_in addr;
    qint64 size;
    int status;

    if (maximumpacketsize > RTPUDPTRANS_MAXPACKSIZE) {
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
        params = (const RTPUDPv4TransmissionParams *) transparams;
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
        m_closesocketswhendone = false;
    }
    else
    {
        m_rtpsock = new QUdpSocket();

        // If we're multiplexing, we're just going to set the RTCP socket to equal the RTP socket
        if (params->GetRTCPMultiplexing())
        {
            m_rtcpsock = m_rtpsock;
            m_rtcpPort = m_rtpPort;
        } else {
            m_rtcpsock = new QUdpSocket();
        }

        m_closesocketswhendone = true;

        // set socket buffer sizes

        size = params->GetRTPReceiveBufferSize();
        m_rtpsock->setReadBufferSize(size);

        if (m_rtpsock != m_rtcpsock)
        {
            size = params->GetRTCPReceiveBufferSize();
            m_rtcpsock->setReadBufferSize(size);
        }
    }

    m_maxpacksize = maximumpacketsize;
    m_mcastifaceIP = params->GetMulticastInterfaceIP();
    m_receivemode = RTPTransmitter::AcceptAll;

    m_localhostname = 0;
    m_localhostnamelength = 0;

    m_waitingfordata = false;
    m_created = true;

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

    if (m_localhostname)
    {
        delete[] m_localhostname;
        m_localhostname = 0;
        m_localhostnamelength = 0;
    }

    FlushPackets();
    ClearAcceptIgnoreInfo();

    if (m_closesocketswhendone)
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

    RTPTransmissionInfo *tinf = new RTPUDPv4TransmissionNoBindInfo(
            m_localIP, m_rtpsock, m_rtcpsock, m_rtpPort, m_rtcpPort);

    return tinf;
}

void RTPUDPTransmitter::DeleteTransmissionInfo(RTPTransmissionInfo *inf)
{
    if (!m_init) {
        return;
    }

    delete inf;
}

bool RTPUDPTransmitter::ComesFromThisTransmitter(const RTPAddress *addr)
{
    if (addr->getAddress() != m_localIP) {
        return false;
    }

    return (addr->getPort() == m_rtpPort) && (addr->getRtcpsendport() == m_rtcpPort);
}

} // namespace


