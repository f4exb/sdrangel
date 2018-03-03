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
 * \file rtpipv4address.h
 */

#ifndef RTPIPV4ADDRESS_H

#define RTPIPV4ADDRESS_H

#include "rtpconfig.h"
#include "rtpaddress.h"
#include "rtptypes.h"

#include "util/export.h"

namespace qrtplib
{

/** Represents an IPv4 IP address and port.
 *  This class is used by the UDP over IPv4 transmission component.
 *  When an RTPIPv4Address is used in one of the multicast functions of the transmitter, the port
 *  number is ignored. When an instance is used in one of the accept or ignore functions of the
 *  transmitter, a zero port number represents all ports for the specified IP address.
 */
class QRTPLIB_API RTPIPv4Address: public RTPAddress
{
public:
    /** Creates an instance with IP address \c ip and port number \c port (both
     *  are interpreted in host byte order), and possibly sets the RTCP multiplex flag
     *  (see RTPIPv4Address::UseRTCPMultiplexingOnTransmission). */
    RTPIPv4Address(uint32_t ip = 0, uint16_t port = 0, bool rtcpmux = false) :
            RTPAddress(IPv4Address)
    {
        RTPIPv4Address::ip = ip;
        RTPIPv4Address::port = port;
        if (rtcpmux)
            rtcpsendport = port;
        else
            rtcpsendport = port + 1;
    }

    /** Creates an instance with IP address \c ip and port number \c port (both
     *  are interpreted in host byte order), and sets a specific port to
     *  send RTCP packets to (see RTPIPv4Address::GetRTCPSendPort). */
    RTPIPv4Address(uint32_t ip, uint16_t port, uint16_t rtcpsendport) :
            RTPAddress(IPv4Address)
    {
        RTPIPv4Address::ip = ip;
        RTPIPv4Address::port = port;
        RTPIPv4Address::rtcpsendport = rtcpsendport;
    }

    /** Creates an instance with IP address \c ip and port number \c port (\c port is
     *  interpreted in host byte order) and possibly sets the RTCP multiplex flag
     *  (see RTPIPv4Address::UseRTCPMultiplexingOnTransmission). */
    RTPIPv4Address(const uint8_t ip[4], uint16_t port = 0, bool rtcpmux = false) :
            RTPAddress(IPv4Address)
    {
        RTPIPv4Address::ip = (uint32_t) ip[3];
        RTPIPv4Address::ip |= (((uint32_t) ip[2]) << 8);
        RTPIPv4Address::ip |= (((uint32_t) ip[1]) << 16);
        RTPIPv4Address::ip |= (((uint32_t) ip[0]) << 24);

        RTPIPv4Address::port = port;
        if (rtcpmux)
            rtcpsendport = port;
        else
            rtcpsendport = port + 1;
    }

    /** Creates an instance with IP address \c ip and port number \c port (both
     *  are interpreted in host byte order), and sets a specific port to
     *  send RTCP packets to (see RTPIPv4Address::GetRTCPSendPort). */
    RTPIPv4Address(const uint8_t ip[4], uint16_t port, uint16_t rtcpsendport) :
            RTPAddress(IPv4Address)
    {
        RTPIPv4Address::ip = (uint32_t) ip[3];
        RTPIPv4Address::ip |= (((uint32_t) ip[2]) << 8);
        RTPIPv4Address::ip |= (((uint32_t) ip[1]) << 16);
        RTPIPv4Address::ip |= (((uint32_t) ip[0]) << 24);

        RTPIPv4Address::port = port;
        RTPIPv4Address::rtcpsendport = rtcpsendport;
    }

    ~RTPIPv4Address()
    {
    }

    /** Sets the IP address for this instance to \c ip which is assumed to be in host byte order. */
    void SetIP(uint32_t ip)
    {
        RTPIPv4Address::ip = ip;
    }

    /** Sets the IP address of this instance to \c ip. */
    void SetIP(const uint8_t ip[4])
    {
        RTPIPv4Address::ip = (uint32_t) ip[3];
        RTPIPv4Address::ip |= (((uint32_t) ip[2]) << 8);
        RTPIPv4Address::ip |= (((uint32_t) ip[1]) << 16);
        RTPIPv4Address::ip |= (((uint32_t) ip[0]) << 24);
    }

    /** Sets the port number for this instance to \c port which is interpreted in host byte order. */
    void SetPort(uint16_t port)
    {
        RTPIPv4Address::port = port;
    }

    /** Returns the IP address contained in this instance in host byte order. */
    uint32_t GetIP() const
    {
        return ip;
    }

    /** Returns the port number of this instance in host byte order. */
    uint16_t GetPort() const
    {
        return port;
    }

    /** For outgoing packets, this indicates to which port RTCP packets will be sent (can,
     *  be the same port as the RTP packets in case RTCP multiplexing is used). */
    uint16_t GetRTCPSendPort() const
    {
        return rtcpsendport;
    }

    RTPAddress *CreateCopy() const;

    // Note that these functions are only used for received packets, and for those
    // the rtcpsendport variable is not important and should be ignored.
    bool IsSameAddress(const RTPAddress *addr) const;
    bool IsFromSameHost(const RTPAddress *addr) const;
private:
    uint32_t ip;
    uint16_t port;
    uint16_t rtcpsendport;
};

} // end namespace

#endif // RTPIPV4ADDRESS_H

