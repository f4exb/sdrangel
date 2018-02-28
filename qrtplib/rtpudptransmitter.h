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

#ifndef QRTPLIB_RTPUDPTRANSMITTER_H_
#define QRTPLIB_RTPUDPTRANSMITTER_H_

#include "rtptransmitter.h"
#include <QHostAddress>
#include <stdint.h>
#include <list>

#define RTPUDPV4TRANS_HASHSIZE                                  8317
#define RTPUDPV4TRANS_DEFAULTPORTBASE                           5000
#define RTPUDPV4TRANS_RTPRECEIVEBUFFER                          32768
#define RTPUDPV4TRANS_RTCPRECEIVEBUFFER                         32768
#define RTPUDPV4TRANS_RTPTRANSMITBUFFER                         32768
#define RTPUDPV4TRANS_RTCPTRANSMITBUFFER                        32768

class QUdpSocket;

namespace qrtplib
{

/** Parameters for the UDP transmitter. */
class RTPUDPTransmissionParams: public RTPTransmissionParams
{
public:
    RTPUDPTransmissionParams();

    /** Sets the IP address which is used to bind the sockets to \c bindAddress. */
    void SetBindIP(const QHostAddress& bindAddress) {
        m_bindAddress = bindAddress;
    }

    /** Sets the multicast interface IP address. */
    void SetMulticastInterfaceIP(const QHostAddress& mcastGroupAddress) {
        m_mcastGroupAddress = mcastGroupAddress;
    }

    /** Sets the RTP portbase to \c pbase, which has to be an even number
     *  unless RTPUDPv4TransmissionParams::SetAllowOddPortbase was called;
     *  a port number of zero will cause a port to be chosen automatically. */
    void SetPortbase(uint16_t pbase)
    {
        m_portbase = pbase;
    }

    /** Passes a list of IP addresses which will be used as the local IP addresses. */
    void SetLocalIPList(const std::list<QHostAddress>& iplist)
    {
        m_localIPs = iplist;
    }

    /** Clears the list of local IP addresses.
     *  Clears the list of local IP addresses. An empty list will make the transmission
     *  component itself determine the local IP addresses.
     */
    void ClearLocalIPList()
    {
        m_localIPs.clear();
    }

    /** Returns the IP address which will be used to bind the sockets. */
    QHostAddress GetBindIP() const
    {
        return m_bindAddress;
    }

    /** Returns the multicast interface IP address. */
    QHostAddress GetMulticastInterfaceIP() const
    {
        return m_mcastGroupAddress;
    }

    /** Returns the RTP portbase which will be used (default is 5000). */
    uint16_t GetPortbase() const
    {
        return m_portbase;
    }

    /** Returns the list of local IP addresses. */
    const std::list<QHostAddress> &GetLocalIPList() const
    {
        return m_localIPs;
    }

    /** Sets the RTP socket's send buffer size. */
    void SetRTPSendBufferSize(int s)
    {
        m_rtpsendbufsz = s;
    }

    /** Sets the RTP socket's receive buffer size. */
    void SetRTPReceiveBufferSize(int s)
    {
        m_rtprecvbufsz = s;
    }

    /** Sets the RTCP socket's send buffer size. */
    void SetRTCPSendBufferSize(int s)
    {
        m_rtcpsendbufsz = s;
    }

    /** Sets the RTCP socket's receive buffer size. */
    void SetRTCPReceiveBufferSize(int s)
    {
        m_rtcprecvbufsz = s;
    }

    /** Enables or disables multiplexing RTCP traffic over the RTP channel, so that only a single port is used. */
    void SetRTCPMultiplexing(bool f)
    {
        m_rtcpmux = f;
    }

    /** Can be used to allow the RTP port base to be any number, not just even numbers. */
    void SetAllowOddPortbase(bool f)
    {
        m_allowoddportbase = f;
    }

    /** Force the RTCP socket to use a specific port, not necessarily one more than
     *  the RTP port (set this to zero to disable). */
    void SetForcedRTCPPort(uint16_t rtcpport)
    {
        m_forcedrtcpport = rtcpport;
    }

    /** Use sockets that have already been created, no checks on port numbers
     *  will be done, and no buffer sizes will be set; you'll need to close
     *  the sockets yourself when done, it will **not** be done automatically. */
    void SetUseExistingSockets(QUdpSocket *rtpsocket, QUdpSocket *rtcpsocket)
    {
        m_rtpsock = rtpsocket;
        m_rtcpsock = rtcpsocket;
        m_useexistingsockets = true;
    }

    /** Returns the RTP socket's send buffer size. */
    int GetRTPSendBufferSize() const
    {
        return m_rtpsendbufsz;
    }

    /** Returns the RTP socket's receive buffer size. */
    int GetRTPReceiveBufferSize() const
    {
        return m_rtprecvbufsz;
    }

    /** Returns the RTCP socket's send buffer size. */
    int GetRTCPSendBufferSize() const
    {
        return m_rtcpsendbufsz;
    }

    /** Returns the RTCP socket's receive buffer size. */
    int GetRTCPReceiveBufferSize() const
    {
        return m_rtcprecvbufsz;
    }

    /** Returns a flag indicating if RTCP traffic will be multiplexed over the RTP channel. */
    bool GetRTCPMultiplexing() const
    {
        return m_rtcpmux;
    }

    /** If true, any RTP portbase will be allowed, not just even numbers. */
    bool GetAllowOddPortbase() const
    {
        return m_allowoddportbase;
    }

    /** If non-zero, the specified port will be used to receive RTCP traffic. */
    uint16_t GetForcedRTCPPort() const
    {
        return m_forcedrtcpport;
    }

    /** Returns true and fills in sockets if existing sockets were set
     *  using RTPUDPv4TransmissionParams::SetUseExistingSockets. */
    bool GetUseExistingSockets(QUdpSocket **rtpsocket, QUdpSocket **rtcpsocket) const
    {
        if (!m_useexistingsockets) {
            return false;
        }

        *rtpsocket = m_rtpsock;
        *rtcpsocket = m_rtcpsock;
        return true;
    }

private:
    QHostAddress m_bindAddress;
    QHostAddress m_mcastGroupAddress;
    uint16_t m_portbase;
    std::list<QHostAddress> m_localIPs;
    int m_rtpsendbufsz, m_rtprecvbufsz;
    int m_rtcpsendbufsz, m_rtcprecvbufsz;
    bool m_rtcpmux;
    bool m_allowoddportbase;
    uint16_t m_forcedrtcpport;

    QUdpSocket *m_rtpsock, *m_rtcpsock;
    bool m_useexistingsockets;
};

inline RTPUDPTransmissionParams::RTPUDPTransmissionParams() :
        RTPTransmissionParams(RTPTransmitter::IPv4UDPProto)
{
    m_portbase = RTPUDPV4TRANS_DEFAULTPORTBASE;
    m_rtpsendbufsz = RTPUDPV4TRANS_RTPTRANSMITBUFFER;
    m_rtprecvbufsz = RTPUDPV4TRANS_RTPRECEIVEBUFFER;
    m_rtcpsendbufsz = RTPUDPV4TRANS_RTCPTRANSMITBUFFER;
    m_rtcprecvbufsz = RTPUDPV4TRANS_RTCPRECEIVEBUFFER;
    m_rtcpmux = false;
    m_allowoddportbase = false;
    m_forcedrtcpport = 0;
    m_useexistingsockets = false;
    m_rtpsock = 0;
    m_rtcpsock = 0;
}

/** Additional information about the UDP over IPv4 transmitter. */
class RTPUDPTransmissionInfo: public RTPTransmissionInfo
{
public:
    RTPUDPTransmissionInfo(const std::list<QHostAddress>& iplist, QUdpSocket *rtpsock, QUdpSocket *rtcpsock, uint16_t rtpport, uint16_t rtcpport) :
            RTPTransmissionInfo(RTPTransmitter::IPv4UDPProto)
    {
        m_localIPlist = iplist;
        m_rtpsocket = rtpsock;
        m_rtcpsocket = rtcpsock;
        m_rtpPort = rtpport;
        m_rtcpPort = rtcpport;
    }

    ~RTPUDPTransmissionInfo()
    {
    }

    /** Returns the list of IPv4 addresses the transmitter considers to be the local IP addresses. */
    std::list<uint32_t> GetLocalIPList() const
    {
        return m_localIPlist;
    }

    /** Returns the socket descriptor used for receiving and transmitting RTP packets. */
    QUdpSocket *GetRTPSocket() const
    {
        return m_rtpsocket;
    }

    /** Returns the socket descriptor used for receiving and transmitting RTCP packets. */
    QUdpSocket *GetRTCPSocket() const
    {
        return m_rtcpsocket;
    }

    /** Returns the port number that the RTP socket receives packets on. */
    uint16_t GetRTPPort() const
    {
        return m_rtpPort;
    }

    /** Returns the port number that the RTCP socket receives packets on. */
    uint16_t GetRTCPPort() const
    {
        return m_rtcpPort;
    }
private:
    std::list<QHostAddress> m_localIPlist;
    QUdpSocket *m_rtpsocket, *m_rtcpsocket;
    uint16_t m_rtpPort, m_rtcpPort;
};

#define RTPUDPTRANS_HEADERSIZE                        (20+8)

/** An UDP transmission component.
 *  This class inherits the RTPTransmitter interface and implements a transmission component
 *  which uses UDP to send and receive RTP and RTCP data. The component's parameters
 *  are described by the class RTPUDPTransmissionParams. The GetTransmissionInfo member function
 *  returns an instance of type RTPUDPTransmissionInfo.
 */
class RTPUDPTransmitter: public RTPTransmitter
{
public:
    RTPUDPTransmitter();
    ~RTPUDPTransmitter();

    int Init();
    int Create(std::size_t maxpacksize, const RTPTransmissionParams *transparams);
    void Destroy();
    RTPTransmissionInfo *GetTransmissionInfo();
    void DeleteTransmissionInfo(RTPTransmissionInfo *inf);

    int GetLocalHostName(uint8_t *buffer, std::size_t *bufferlength);
    bool ComesFromThisTransmitter(const RTPAddress *addr);
    std::size_t GetHeaderOverhead()
    {
        return RTPUDPTRANS_HEADERSIZE;
    }

    int Poll();
    int WaitForIncomingData(const RTPTime &delay, bool *dataavailable = 0);
    int AbortWait();

    int SendRTPData(const void *data, std::size_t len);
    int SendRTCPData(const void *data, std::size_t len);

    int AddDestination(const RTPAddress &addr);
    int DeleteDestination(const RTPAddress &addr);
    void ClearDestinations();

    bool SupportsMulticasting();
    int JoinMulticastGroup(const RTPAddress &addr);
    int LeaveMulticastGroup(const RTPAddress &addr);
    void LeaveAllMulticastGroups();

    int SetReceiveMode(RTPTransmitter::ReceiveMode m);
    int AddToIgnoreList(const RTPAddress &addr);
    int DeleteFromIgnoreList(const RTPAddress &addr);
    void ClearIgnoreList();
    int AddToAcceptList(const RTPAddress &addr);
    int DeleteFromAcceptList(const RTPAddress &addr);
    void ClearAcceptList();
    int SetMaximumPacketSize(std::size_t s);

    bool NewDataAvailable();
    RTPRawPacket *GetNextPacket();

private:
    int CreateLocalIPList();
    bool GetLocalIPList_Interfaces();
    void GetLocalIPList_DNS();
    void AddLoopbackAddress();
    void FlushPackets();
    int ProcessAddAcceptIgnoreEntry(uint32_t ip, uint16_t port);
    int ProcessDeleteAcceptIgnoreEntry(uint32_t ip, uint16_t port);
    bool ShouldAcceptData(uint32_t srcip, uint16_t srcport);
    void ClearAcceptIgnoreInfo();

    bool m_init;
    bool m_created;
    bool m_waitingfordata;
    QUdpSocket *m_rtpsock, *m_rtcpsock;
    QHostAddress m_mcastifaceIP;
    std::list<QHostAddress> m_localIPs;
    uint16_t m_rtpPort, m_rtcpPort;
    uint8_t m_multicastTTL;
    RTPTransmitter::ReceiveMode m_receivemode;

    uint8_t *m_localhostname;
    std::size_t m_localhostnamelength;

    std::list<RTPRawPacket*> m_rawpacketlist;

    bool m_supportsmulticasting;
    std::size_t m_maxpacksize;

    bool m_closesocketswhendone;
};


} // namespace

#endif /* QRTPLIB_RTPUDPTRANSMITTER_H_ */
