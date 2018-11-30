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
 * \file rtptransmitter.h
 */

#ifndef RTPTRANSMITTER_H

#define RTPTRANSMITTER_H

#include "rtpconfig.h"
#include "rtptypes.h"
#include "rtptimeutilities.h"
#include <cstddef>

namespace qrtplib
{

class RTPRawPacket;
class RTPAddress;
class RTPTransmissionParams;
class RTPTime;
class RTPTransmissionInfo;

/** Abstract class from which actual transmission components should be derived.
 *  Abstract class from which actual transmission components should be derived.
 *  The abstract class RTPTransmitter specifies the interface for
 *  actual transmission components. Currently, three implementations exist:
 *  an UDP over IPv4 transmitter, an UDP over IPv6 transmitter and a transmitter
 *  which can be used to use an external transmission mechanism.
 */
class RTPTransmitter
{
public:
    /** Used to identify a specific transmitter.
     *  If UserDefinedProto is used in the RTPSession::Create function, the RTPSession
     *  virtual member function NewUserDefinedTransmitter will be called to create
     *  a transmission component.
     */
    enum TransmissionProtocol
    {
        IPv4UDPProto, /**< Specifies the internal UDP over IPv4 transmitter. */
        IPv6UDPProto, /**< Specifies the internal UDP over IPv6 transmitter. */
        TCPProto, /**< Specifies the internal TCP transmitter. */
        ExternalProto, /**< Specifies the transmitter which can send packets using an external mechanism, and which can have received packets injected into it - see RTPExternalTransmitter for additional information. */
        UserDefinedProto /**< Specifies a user defined, external transmitter. */
    };

    /** Three kind of receive modes can be specified. */
    enum ReceiveMode
    {
        AcceptAll, /**< All incoming data is accepted, no matter where it originated from. */
        AcceptSome, /**< Only data coming from specific sources will be accepted. */
        IgnoreSome /**< All incoming data is accepted, except for data coming from a specific set of sources. */
    };
protected:
    /** Constructor in which you can specify a memory manager to use. */
    RTPTransmitter()
    {
        timeinit.Dummy();
    }
public:
    virtual ~RTPTransmitter()
    {
    }

    /** This function must be called before the transmission component can be used. */
    virtual int Init() = 0;

    /** Prepares the component to be used.
     *  Prepares the component to be used. The parameter \c maxpacksize specifies the maximum size
     *  a packet can have: if the packet is larger it will not be transmitted. The \c transparams
     *  parameter specifies a pointer to an RTPTransmissionParams instance. This is also an abstract
     *  class and each actual component will define its own parameters by inheriting a class
     *  from RTPTransmissionParams. If \c transparams is NULL, the default transmission parameters
     *  for the component will be used.
     */
    virtual int Create(std::size_t maxpacksize, const RTPTransmissionParams *transparams) = 0;

    /** Bind the RTP and RTCP sockets to ports that were set at creation time */
    virtual int BindSockets() = 0;

    /** By calling this function, buffers are cleared and the component cannot be used anymore.
     *  By calling this function, buffers are cleared and the component cannot be used anymore.
     *  Only when the Create function is called again can the component be used again. */
    virtual void Destroy() = 0;

    /** Returns additional information about the transmitter.
     *  This function returns an instance of a subclass of RTPTransmissionInfo which will give
     *  some additional information about the transmitter (a list of local IP addresses for example).
     *  Currently, either an instance of RTPUDPv4TransmissionInfo or RTPUDPv6TransmissionInfo is
     *  returned, depending on the type of the transmitter. The user has to deallocate the returned
     *  instance when it is no longer needed, which can be done using RTPTransmitter::DeleteTransmissionInfo.
     */
    virtual RTPTransmissionInfo *GetTransmissionInfo() = 0;

    /** Deallocates the information returned by RTPTransmitter::GetTransmissionInfo .
     *  Deallocates the information returned by RTPTransmitter::GetTransmissionInfo .
     */
    virtual void DeleteTransmissionInfo(RTPTransmissionInfo *inf) = 0;

    /** Returns \c true if the address specified by \c addr is one of the addresses of the transmitter. */
    virtual bool ComesFromThisTransmitter(const RTPAddress& addr) = 0;

    /** Returns the amount of bytes that will be added to the RTP packet by the underlying layers (excluding
     *  the link layer). */
    virtual std::size_t GetHeaderOverhead() = 0;

    /** Send a packet with length \c len containing \c data	to all RTP addresses of the current destination list. */
    virtual int SendRTPData(const void *data, std::size_t len) = 0;

    /** Send a packet with length \c len containing \c data to all RTCP addresses of the current destination list. */
    virtual int SendRTCPData(const void *data, std::size_t len) = 0;

    /** Adds the address specified by \c addr to the list of destinations. */
    virtual int AddDestination(const RTPAddress &addr) = 0;

    /** Deletes the address specified by \c addr from the list of destinations. */
    virtual int DeleteDestination(const RTPAddress &addr) = 0;

    /** Clears the list of destinations. */
    virtual void ClearDestinations() = 0;

    /** Returns \c true if the transmission component supports multicasting. */
    virtual bool SupportsMulticasting() = 0;

    /** Joins the multicast group specified by \c addr. */
    virtual int JoinMulticastGroup(const RTPAddress &addr) = 0;

    /** Leaves the multicast group specified by \c addr. */
    virtual int LeaveMulticastGroup(const RTPAddress &addr) = 0;

    /** Sets the receive mode.
     *  Sets the receive mode to \c m, which is one of the following: RTPTransmitter::AcceptAll,
     *  RTPTransmitter::AcceptSome or RTPTransmitter::IgnoreSome. Note that if the receive
     *  mode is changed, all information about the addresses to ignore to accept is lost.
     */
    virtual int SetReceiveMode(RTPTransmitter::ReceiveMode m) = 0;

    /** Adds \c addr to the list of addresses to ignore. */
    virtual int AddToIgnoreList(const RTPAddress &addr) = 0;

    /** Deletes \c addr from the list of addresses to accept. */
    virtual int DeleteFromIgnoreList(const RTPAddress &addr)= 0;

    /** Clears the list of addresses to ignore. */
    virtual void ClearIgnoreList() = 0;

    /** Adds \c addr to the list of addresses to accept. */
    virtual int AddToAcceptList(const RTPAddress &addr) = 0;

    /** Deletes \c addr from the list of addresses to accept. */
    virtual int DeleteFromAcceptList(const RTPAddress &addr) = 0;

    /** Clears the list of addresses to accept. */
    virtual void ClearAcceptList() = 0;

    /** Sets the maximum packet size which the transmitter should allow to \c s. */
    virtual int SetMaximumPacketSize(std::size_t s) = 0;

    /** Returns the raw data of a received RTP packet
     *  in an RTPRawPacket instance. */
    virtual RTPRawPacket *GetNextPacket() = 0;
};

/** Base class for transmission parameters.
 *  This class is an abstract class which will have a specific implementation for a
 *  specific kind of transmission component. All actual implementations inherit the
 *  GetTransmissionProtocol function which identifies the component type for which
 *  these parameters are valid.
 */
class RTPTransmissionParams
{
protected:
    RTPTransmissionParams(RTPTransmitter::TransmissionProtocol p)
    {
        protocol = p;
    }
public:
    virtual ~RTPTransmissionParams()
    {
    }

    /** Returns the transmitter type for which these parameters are valid. */
    RTPTransmitter::TransmissionProtocol GetTransmissionProtocol() const
    {
        return protocol;
    }
private:
    RTPTransmitter::TransmissionProtocol protocol;
};

/** Base class for additional information about the transmitter.
 *  This class is an abstract class which will have a specific implementation for a
 *  specific kind of transmission component. All actual implementations inherit the
 *  GetTransmissionProtocol function which identifies the component type for which
 *  these parameters are valid.
 */
class RTPTransmissionInfo
{
protected:
    RTPTransmissionInfo(RTPTransmitter::TransmissionProtocol p)
    {
        protocol = p;
    }
public:
    virtual ~RTPTransmissionInfo()
    {
    }
    /** Returns the transmitter type for which these parameters are valid. */
    RTPTransmitter::TransmissionProtocol GetTransmissionProtocol() const
    {
        return protocol;
    }
private:
    RTPTransmitter::TransmissionProtocol protocol;
};

} // end namespace

#endif // RTPTRANSMITTER_H

