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
 * \file rtpaddress.h
 */

#ifndef RTPADDRESS_H

#define RTPADDRESS_H

#include "rtpconfig.h"
#include <string>
#include <stdint.h>
#include <QHostAddress>

#include "export.h"

namespace qrtplib
{

/** This class is an abstract class which is used to specify destinations, multicast groups etc. */
class QRTPLIB_API RTPAddress
{
public:
    /** Default constructor. Address and port set via setters */
    RTPAddress() : port(0), rtcpsendport(0)
    {}

    /** Constructor with address and port */
    RTPAddress(const QHostAddress& address, uint16_t port) :
        address(address),
        port(port),
        rtcpsendport(0)
    {}

    /** Returns the type of address the actual implementation represents. */
    QAbstractSocket::NetworkLayerProtocol GetAddressType() const
    {
        return address.protocol();
    }

    /** Creates a copy of the RTPAddress instance.
     *  Creates a copy of the RTPAddress instance. If \c mgr is not NULL, the
     *  corresponding memory manager will be used to allocate the memory for the address
     *  copy.
     */
    RTPAddress *CreateCopy() const;

    /** Checks if the address \c addr is the same address as the one this instance represents.
     *  Checks if the address \c addr is the same address as the one this instance represents.
     *  Implementations must be able to handle a NULL argument.
     *
     *  Note that this function is only used for received packets, and for those
     *  the rtcpsendport variable is not important and should be ignored.
     */
    bool IsSameAddress(const RTPAddress *addr) const;

    /** Checks if the address \c addr represents the same host as this instance.
     *  Checks if the address \c addr represents the same host as this instance. Implementations
     *  must be able to handle a NULL argument.
     *
     *  Note that this function is only used for received packets.
     */
    bool IsFromSameHost(const RTPAddress *addr) const;

    /** Equality */
    bool operator==(const RTPAddress& otherAddress) const;

    /** Get host address */
    const QHostAddress& getAddress() const
    {
        return address;
    }

    /** Set host address */
    void setAddress(const QHostAddress& address)
    {
        this->address = address;
    }

    /** Get RTP port */
    uint16_t getPort() const
    {
        return port;
    }

    /** Set RTP port */
    void setPort(uint16_t port)
    {
        this->port = port;
    }

    /** Get RTCP port */
    uint16_t getRtcpsendport() const
    {
        return rtcpsendport;
    }

    /** Set RTCP port */
    void setRtcpsendport(uint16_t rtcpsendport)
    {
        this->rtcpsendport = rtcpsendport;
    }

private:
    QHostAddress address;
    uint16_t port;
    uint16_t rtcpsendport;
};

} // end namespace

#endif // RTPADDRESS_H

