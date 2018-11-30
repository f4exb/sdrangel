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
#include "export.h"

#include <QObject>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QQueue>
#include <QMutex>

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
class QRTPLIB_API RTPUDPTransmissionParams: public RTPTransmissionParams
{
public:
    RTPUDPTransmissionParams();

    /** Sets the IP address which is used to bind the sockets to \c bindAddress. */
    void SetBindIP(const QHostAddress& bindAddress) {
        m_bindAddress = bindAddress;
    }

    /** Sets the multicast interface IP address. */
    void SetMulticastInterface(const QNetworkInterface& mcastInterface) {
        m_mcastInterface = mcastInterface;
    }

    /** Sets the RTP portbase to \c pbase, which has to be an even number
     *  unless RTPUDPv4TransmissionParams::SetAllowOddPortbase was called;
     *  a port number of zero will cause a port to be chosen automatically. */
    void SetPortbase(uint16_t pbase)
    {
        m_portbase = pbase;
    }

    /** Returns the IP address which will be used to bind the sockets. */
    QHostAddress GetBindIP() const
    {
        return m_bindAddress;
    }

    /** Returns the multicast interface IP address. */
    QNetworkInterface GetMulticastInterface() const
    {
        return m_mcastInterface;
    }

    /** Returns the RTP portbase which will be used (default is 5000). */
    uint16_t GetPortbase() const
    {
        return m_portbase;
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
    QNetworkInterface m_mcastInterface;
    uint16_t m_portbase;
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
    m_rtpsock = 0;
    m_rtcpsock = 0;
    m_useexistingsockets = false;
}

/** Additional information about the UDP over IPv4 transmitter. */
class QRTPLIB_API RTPUDPTransmissionInfo: public RTPTransmissionInfo
{
public:
    RTPUDPTransmissionInfo(
            QHostAddress localIP,
            QUdpSocket *rtpsock,
            QUdpSocket *rtcpsock,
            uint16_t rtpport,
            uint16_t rtcpport) :
        RTPTransmissionInfo(RTPTransmitter::IPv4UDPProto)
    {
        m_localIP = localIP;
        m_rtpsocket = rtpsock;
        m_rtcpsocket = rtcpsock;
        m_rtpPort = rtpport;
        m_rtcpPort = rtcpport;
    }

    ~RTPUDPTransmissionInfo()
    {
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
    QHostAddress m_localIP;
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
class QRTPLIB_API RTPUDPTransmitter: public QObject, public RTPTransmitter
{
    Q_OBJECT
public:
    RTPUDPTransmitter();
    virtual ~RTPUDPTransmitter();

    virtual int Init();
    virtual int Create(std::size_t maxpacksize, const RTPTransmissionParams *transparams);
    virtual int BindSockets();
    void moveToThread(QThread *thread);
    virtual void Destroy();
    virtual RTPTransmissionInfo *GetTransmissionInfo();
    virtual void DeleteTransmissionInfo(RTPTransmissionInfo *inf);

    virtual bool ComesFromThisTransmitter(const RTPAddress& addr);
    virtual std::size_t GetHeaderOverhead()
    {
        return RTPUDPTRANS_HEADERSIZE;
    }

    virtual int SendRTPData(const void *data, std::size_t len);
    virtual int SendRTCPData(const void *data, std::size_t len);

    virtual int AddDestination(const RTPAddress &addr);
    virtual int DeleteDestination(const RTPAddress &addr);
    virtual void ClearDestinations();

    virtual bool SupportsMulticasting();
    virtual int JoinMulticastGroup(const RTPAddress &addr);
    virtual int LeaveMulticastGroup(const RTPAddress &addr);

    virtual int SetReceiveMode(RTPTransmitter::ReceiveMode m);
    virtual int AddToIgnoreList(const RTPAddress &addr);
    virtual int DeleteFromIgnoreList(const RTPAddress &addr);
    virtual void ClearIgnoreList();
    virtual int AddToAcceptList(const RTPAddress &addr);
    virtual int DeleteFromAcceptList(const RTPAddress &addr);
    virtual void ClearAcceptList();
    virtual int SetMaximumPacketSize(std::size_t s);

    virtual RTPRawPacket *GetNextPacket();


private:
    bool m_init;
    bool m_created;
    bool m_waitingfordata;
    QUdpSocket *m_rtpsock, *m_rtcpsock;
    bool m_deletesocketswhendone;
    QHostAddress m_localIP; //!< from parameters bind IP
    QNetworkInterface m_multicastInterface; //!< from parameters multicast interface
    uint16_t m_rtpPort, m_rtcpPort;
    RTPTransmitter::ReceiveMode m_receivemode;

    std::size_t m_maxpacksize;
    static const std::size_t m_absoluteMaxPackSize = 65535;
    char m_rtpBuffer[m_absoluteMaxPackSize];
    char m_rtcpBuffer[m_absoluteMaxPackSize];

    std::list<RTPAddress> m_destinations;
    std::list<RTPAddress> m_acceptList;
    std::list<RTPAddress> m_ignoreList;
    QQueue<RTPRawPacket*> m_rawPacketQueue;
    QMutex m_rawPacketQueueLock;

    bool ShouldAcceptData(const RTPAddress& address);

private slots:
    void readRTPPendingDatagrams();
    void readRTCPPendingDatagrams();

signals:
    void NewDataAvailable();
};


} // namespace

#endif /* QRTPLIB_RTPUDPTRANSMITTER_H_ */
