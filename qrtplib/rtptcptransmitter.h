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

/**
 * \file rtptcptransmitter.h
 */

#ifndef RTPTCPTRANSMITTER_H

#define RTPTCPTRANSMITTER_H

#include "rtpconfig.h"
#include "rtptransmitter.h"
#include "rtpsocketutil.h"
#include "rtpabortdescriptors.h"
#include <map>
#include <list>
#include <vector>

namespace qrtplib
{

/** Parameters for the TCP transmitter. */
class RTPTCPTransmissionParams: public RTPTransmissionParams
{
public:
    RTPTCPTransmissionParams();

    /** If non null, the specified abort descriptors will be used to cancel
     *  the function that's waiting for packets to arrive; set to null (the default)
     *  to let the transmitter create its own instance. */
    void SetCreatedAbortDescriptors(RTPAbortDescriptors *desc)
    {
        m_pAbortDesc = desc;
    }

    /** If non-null, this RTPAbortDescriptors instance will be used internally,
     *  which can be useful when creating your own poll thread for multiple
     *  sessions. */
    RTPAbortDescriptors *GetCreatedAbortDescriptors() const
    {
        return m_pAbortDesc;
    }
private:
    RTPAbortDescriptors *m_pAbortDesc;
};

inline RTPTCPTransmissionParams::RTPTCPTransmissionParams() :
        RTPTransmissionParams(RTPTransmitter::TCPProto)
{
    m_pAbortDesc = 0;
}

/** Additional information about the TCP transmitter. */
class RTPTCPTransmissionInfo: public RTPTransmissionInfo
{
public:
    RTPTCPTransmissionInfo() :
            RTPTransmissionInfo(RTPTransmitter::TCPProto)
    {
    }
    ~RTPTCPTransmissionInfo()
    {
    }
};

// TODO: this is for IPv4, and will only be valid if one  rtp packet is in one tcp frame
#define RTPTCPTRANS_HEADERSIZE						(20+20+2) // 20 IP, 20 TCP, 2 for framing (RFC 4571)

/** A TCP transmission component.
 *
 *  This class inherits the RTPTransmitter interface and implements a transmission component
 *  which uses TCP to send and receive RTP and RTCP data. The component's parameters
 *  are described by the class RTPTCPTransmissionParams. The functions which have an RTPAddress
 *  argument require an argument of RTPTCPAddress. The RTPTransmitter::GetTransmissionInfo member function
 *  returns an instance of type RTPTCPTransmissionInfo.
 *
 *  After this transmission component was created, no data will actually be sent or received
 *  yet. You can specify over which TCP connections (which must be established first) data
 *  should be transmitted by using the RTPTransmitter::AddDestination member function. This
 *  takes an argument of type RTPTCPAddress, with which relevant the socket descriptor can
 *  be passed to the transmitter.
 *
 *  These sockets will also be used to check for incoming RTP or RTCP data. The RTPTCPAddress
 *  instance that's associated with a received packet, will contain the socket descriptor
 *  on which the data was received. This descriptor can be obtained using RTPTCPAddress::GetSocket.
 *
 *  To get notified of an error when sending over or receiving from a socket, override the
 *  RTPTCPTransmitter::OnSendError and RTPTCPTransmitter::OnReceiveError member functions.
 */
class RTPTCPTransmitter : public RTPTransmitter
{
public:
    RTPTCPTransmitter();
    ~RTPTCPTransmitter();

    int Init(bool treadsafe);
    int Create(size_t maxpacksize, const RTPTransmissionParams *transparams);
    void Destroy();
    RTPTransmissionInfo *GetTransmissionInfo();
    void DeleteTransmissionInfo(RTPTransmissionInfo *inf);

    int GetLocalHostName(uint8_t *buffer, size_t *bufferlength);
    bool ComesFromThisTransmitter(const RTPAddress *addr);
    size_t GetHeaderOverhead()
    {
        return RTPTCPTRANS_HEADERSIZE;
    }

    int Poll();
    int WaitForIncomingData(const RTPTime &delay, bool *dataavailable = 0);
    int AbortWait();

    int SendRTPData(const void *data, size_t len);
    int SendRTCPData(const void *data, size_t len);

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
    int SetMaximumPacketSize(size_t s);

    bool NewDataAvailable();
    RTPRawPacket *GetNextPacket();
protected:
    /** By overriding this function you can be notified of an error when sending over a socket. */
    virtual void OnSendError(SocketType sock);
    /** By overriding this function you can be notified of an error when receiving from a socket. */
    virtual void OnReceiveError(SocketType sock);
private:
    class SocketData
    {
    public:
        SocketData();
        ~SocketData();
        void Reset();

        uint8_t m_lengthBuffer[2];
        int m_lengthBufferOffset;
        int m_dataLength;
        int m_dataBufferOffset;
        uint8_t *m_pDataBuffer;

        uint8_t *ExtractDataBuffer()
        {
            uint8_t *pTmp = m_pDataBuffer;
            m_pDataBuffer = 0;
            return pTmp;
        }
        int ProcessAvailableBytes(SocketType sock, int availLen, bool &complete);
    };

    int SendRTPRTCPData(const void *data, size_t len);
    void FlushPackets();
    int PollSocket(SocketType sock, SocketData &sdata);
    void ClearDestSockets();
    int ValidateSocket(SocketType s);

    bool m_init;
    bool m_created;
    bool m_waitingForData;

    std::map<SocketType, SocketData> m_destSockets;
    std::vector<SocketType> m_tmpSocks;
    std::vector<int8_t> m_tmpFlags;
    std::vector<uint8_t> m_localHostname;
    size_t m_maxPackSize;

    std::list<RTPRawPacket*> m_rawpacketlist;

    RTPAbortDescriptors m_abortDesc;
    RTPAbortDescriptors *m_pAbortDesc; // in case an external one was specified

};

inline void RTPTCPTransmitter::OnSendError(SocketType)
{
}
inline void RTPTCPTransmitter::OnReceiveError(SocketType)
{
}

} // end namespace

#endif // RTPTCPTRANSMITTER_H

