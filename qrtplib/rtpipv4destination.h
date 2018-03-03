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
 * \file rtpipv4destination.h
 */

#ifndef RTPIPV4DESTINATION_H

#define RTPIPV4DESTINATION_H

#include "rtpconfig.h"
#include "rtptypes.h"
#include "rtpipv4address.h"
#ifndef RTP_SOCKETTYPE_WINSOCK
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif // RTP_SOCKETTYPE_WINSOCK
#include <string.h>
#include <string>

#include "util/export.h"

namespace qrtplib
{

class QRTPLIB_API RTPIPv4Destination
{
public:
    RTPIPv4Destination()
    {
        ip = 0;
        memset(&rtpaddr, 0, sizeof(struct sockaddr_in));
        memset(&rtcpaddr, 0, sizeof(struct sockaddr_in));
    }

    RTPIPv4Destination(uint32_t ip, uint16_t rtpport, uint16_t rtcpport)
    {
        memset(&rtpaddr, 0, sizeof(struct sockaddr_in));
        memset(&rtcpaddr, 0, sizeof(struct sockaddr_in));

        rtpaddr.sin_family = AF_INET;
        rtpaddr.sin_port = htons(rtpport);
        rtpaddr.sin_addr.s_addr = htonl(ip);

        rtcpaddr.sin_family = AF_INET;
        rtcpaddr.sin_port = htons(rtcpport);
        rtcpaddr.sin_addr.s_addr = htonl(ip);

        RTPIPv4Destination::ip = ip;
    }

    bool operator==(const RTPIPv4Destination &src) const
    {
        if (rtpaddr.sin_addr.s_addr == src.rtpaddr.sin_addr.s_addr && rtpaddr.sin_port == src.rtpaddr.sin_port)
            return true;
        return false;
    }
    uint32_t GetIP() const
    {
        return ip;
    }
    // nbo = network byte order
    uint32_t GetIP_NBO() const
    {
        return rtpaddr.sin_addr.s_addr;
    }
    uint16_t GetRTPPort_NBO() const
    {
        return rtpaddr.sin_port;
    }
    uint16_t GetRTCPPort_NBO() const
    {
        return rtcpaddr.sin_port;
    }
    const struct sockaddr_in *GetRTPSockAddr() const
    {
        return &rtpaddr;
    }
    const struct sockaddr_in *GetRTCPSockAddr() const
    {
        return &rtcpaddr;
    }
    std::string GetDestinationString() const;

    static bool AddressToDestination(const RTPAddress &addr, RTPIPv4Destination &dest)
    {
        if (addr.GetAddressType() != RTPAddress::IPv4Address)
            return false;

        const RTPIPv4Address &address = (const RTPIPv4Address &) addr;
        uint16_t rtpport = address.GetPort();
        uint16_t rtcpport = address.GetRTCPSendPort();

        dest = RTPIPv4Destination(address.GetIP(), rtpport, rtcpport);
        return true;
    }

private:
    uint32_t ip;
    struct sockaddr_in rtpaddr;
    struct sockaddr_in rtcpaddr;
};

} // end namespace

#endif // RTPIPV4DESTINATION_H

