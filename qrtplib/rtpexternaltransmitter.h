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
 * \file rtpexternaltransmitter.h
 */

#ifndef RTPEXTERNALTRANSMITTER_H

#define RTPEXTERNALTRANSMITTER_H

#include "rtpconfig.h"
#include "rtptransmitter.h"
#include "rtpabortdescriptors.h"
#include <list>

namespace qrtplib
{

class RTPExternalTransmitter;

/** Base class to specify a mechanism to transmit RTP packets outside of this library.
 *  Base class to specify a mechanism to transmit RTP packets outside of this library. When
 *  you want to use your own mechanism to transmit RTP packets, you need to specify that
 *  you'll be using the external transmission component, and derive a class from this base
 *  class. An instance should then be specified in the RTPExternalTransmissionParams object,
 *  so that the transmitter will call the \c SendRTP, \c SendRTCP and \c ComesFromThisSender
 *  methods of this instance when needed.
 */
class RTPExternalSender
{
public:
    RTPExternalSender()
    {
    }
    virtual ~RTPExternalSender()
    {
    }

    /** This member function will be called when RTP data needs to be transmitted. */
    virtual bool SendRTP(const void *data, std::size_t len) = 0;

    /** This member function will be called when an RTCP packet needs to be transmitted. */
    virtual bool SendRTCP(const void *data, std::size_t len) = 0;

    /** Used to identify if an RTPAddress instance originated from this sender (to be able to detect own packets). */
    virtual bool ComesFromThisSender(const RTPAddress *a) = 0;
};

/** Interface to inject incoming RTP and RTCP packets into the library.
 *  Interface to inject incoming RTP and RTCP packets into the library. When you have your own
 *  mechanism to receive incoming RTP/RTCP data, you'll need to pass these packets to the library.
 *  By first retrieving the RTPExternalTransmissionInfo instance for the external transmitter you'll
 *  be using, you can obtain the associated RTPExternalPacketInjecter instance. By calling it's
 *  member functions, you can then inject RTP or RTCP data into the library for further processing.
 */
class RTPExternalPacketInjecter
{
public:
    RTPExternalPacketInjecter(RTPExternalTransmitter *trans)
    {
        transmitter = trans;
    }
    ~RTPExternalPacketInjecter()
    {
    }

    /** This function can be called to insert an RTP packet into the transmission component. */
    void InjectRTP(const void *data, std::size_t len, const RTPAddress &a);

    /** This function can be called to insert an RTCP packet into the transmission component. */
    void InjectRTCP(const void *data, std::size_t len, const RTPAddress &a);

    /** Use this function to inject an RTP or RTCP packet and the transmitter will try to figure out which type of packet it is. */
    void InjectRTPorRTCP(const void *data, std::size_t len, const RTPAddress &a);
private:
    RTPExternalTransmitter *transmitter;
};

/** Parameters to initialize a transmitter of type RTPExternalTransmitter. */
class RTPExternalTransmissionParams: public RTPTransmissionParams
{
public:
    /** Using this constructor you can specify which RTPExternalSender object you'll be using
     *  and how much the additional header overhead for each packet will be. */
    RTPExternalTransmissionParams(RTPExternalSender *s, int headeroverhead) :
            RTPTransmissionParams(RTPTransmitter::ExternalProto)
    {
        sender = s;
        headersize = headeroverhead;
    }

    RTPExternalSender *GetSender() const
    {
        return sender;
    }
    int GetAdditionalHeaderSize() const
    {
        return headersize;
    }
private:
    RTPExternalSender *sender;
    int headersize;
};

/** Additional information about the external transmission component. */
class RTPExternalTransmissionInfo: public RTPTransmissionInfo
{
public:
    RTPExternalTransmissionInfo(RTPExternalPacketInjecter *p) :
            RTPTransmissionInfo(RTPTransmitter::ExternalProto)
    {
        packetinjector = p;
    }

    /** Tells you which RTPExternalPacketInjecter you need to use to pass RTP or RTCP
     *  data on to the transmission component. */
    RTPExternalPacketInjecter *GetPacketInjector() const
    {
        return packetinjector;
    }
private:
    RTPExternalPacketInjecter *packetinjector;
};

/** A transmission component which will use user specified functions to transmit the data and
 *  which will expose functions to inject received RTP or RTCP data into this component.
 *  A transmission component which will use user specified functions to transmit the data and
 *  which will expose functions to inject received RTP or RTCP data into this component. Use
 *  a class derived from RTPExternalSender to specify the functions which need to be used for
 *  sending the data. Obtain the RTPExternalTransmissionInfo object associated with this
 *  transmitter to obtain the functions needed to pass RTP/RTCP packets on to the transmitter.
 */
class RTPExternalTransmitter: public RTPTransmitter
{
public:
    RTPExternalTransmitter();
    ~RTPExternalTransmitter();

    int Init(bool treadsafe);
    int Create(std::size_t maxpacksize, const RTPTransmissionParams *transparams);
    void Destroy();
    RTPTransmissionInfo *GetTransmissionInfo();
    void DeleteTransmissionInfo(RTPTransmissionInfo *inf);

    int GetLocalHostName(uint8_t *buffer, std::size_t *bufferlength);
    bool ComesFromThisTransmitter(const RTPAddress *addr);
    std::size_t GetHeaderOverhead()
    {
        return headersize;
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

    void InjectRTP(const void *data, std::size_t len, const RTPAddress &a);
    void InjectRTCP(const void *data, std::size_t len, const RTPAddress &a);
    void InjectRTPorRTCP(const void *data, std::size_t len, const RTPAddress &a);
private:
    void FlushPackets();

    bool init;
    bool created;
    bool waitingfordata;
    RTPExternalSender *sender;
    RTPExternalPacketInjecter packetinjector;

    std::list<RTPRawPacket*> rawpacketlist;

    uint8_t *localhostname;
    std::size_t localhostnamelength;

    std::size_t maxpacksize;
    int headersize;

    RTPAbortDescriptors m_abortDesc;
    int m_abortCount;
};

inline void RTPExternalPacketInjecter::InjectRTP(const void *data, std::size_t len, const RTPAddress &a)
{
    transmitter->InjectRTP(data, len, a);
}

inline void RTPExternalPacketInjecter::InjectRTCP(const void *data, std::size_t len, const RTPAddress &a)
{
    transmitter->InjectRTCP(data, len, a);
}

inline void RTPExternalPacketInjecter::InjectRTPorRTCP(const void *data, std::size_t len, const RTPAddress &a)
{
    transmitter->InjectRTPorRTCP(data, len, a);
}

} // end namespace

#endif // RTPTCPSOCKETTRANSMITTER_H

