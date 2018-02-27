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
 * \file rtpipv6destination.h
 */

#ifndef RTPIPV6DESTINATION_H

#define RTPIPV6DESTINATION_H

#include "rtpconfig.h"

#ifdef RTP_SUPPORT_IPV6

#include "rtptypes.h"
#include <string.h>
#include <string>
#ifndef RTP_SOCKETTYPE_WINSOCK
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <sys/socket.h>
#endif // RTP_SOCKETTYPE_WINSOCK

namespace qrtplib
{

class JRTPLIB_IMPORTEXPORT RTPIPv6Destination
{
public:
	RTPIPv6Destination(in6_addr ip,uint16_t portbase)
	{
		memset(&rtpaddr,0,sizeof(struct sockaddr_in6));
		memset(&rtcpaddr,0,sizeof(struct sockaddr_in6));
		rtpaddr.sin6_family = AF_INET6;
		rtpaddr.sin6_port = htons(portbase);
		rtpaddr.sin6_addr = ip;
		rtcpaddr.sin6_family = AF_INET6;
		rtcpaddr.sin6_port = htons(portbase+1);
		rtcpaddr.sin6_addr = ip;
	}
	in6_addr GetIP() const								{ return rtpaddr.sin6_addr; }
	bool operator==(const RTPIPv6Destination &src) const
	{
		if (rtpaddr.sin6_port == src.rtpaddr.sin6_port && (memcmp(&(src.rtpaddr.sin6_addr),&(rtpaddr.sin6_addr),sizeof(in6_addr)) == 0))
			return true;
		return false;
	}
	const struct sockaddr_in6 *GetRTPSockAddr() const				{ return &rtpaddr; }
	const struct sockaddr_in6 *GetRTCPSockAddr() const				{ return &rtcpaddr; }
	std::string GetDestinationString() const;
private:
	struct sockaddr_in6 rtpaddr;
	struct sockaddr_in6 rtcpaddr;
};

} // end namespace

#endif // RTP_SUPPORT_IPV6

#endif // RTPIPV6DESTINATION_H

