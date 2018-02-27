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

#include "rtpipv6address.h"
#include "rtpmemorymanager.h"

#ifdef RTP_SUPPORT_IPV6


namespace qrtplib
{

RTPAddress *RTPIPv6Address::CreateCopy(RTPMemoryManager *mgr) const
{
	JRTPLIB_UNUSED(mgr); // possibly unused
	RTPIPv6Address *newaddr = RTPNew(mgr,RTPMEM_TYPE_CLASS_RTPADDRESS) RTPIPv6Address(ip,port);
	return newaddr;
}

bool RTPIPv6Address::IsSameAddress(const RTPAddress *addr) const
{
	if (addr == 0)
		return false;
	if (addr->GetAddressType() != RTPAddress::IPv6Address)
		return false;

	const RTPIPv6Address *addr2 = (const RTPIPv6Address *)addr;
	const uint8_t *ip2 = addr2->ip.s6_addr;

	if (port != addr2->port)
		return false;

	for (int i = 0 ; i < 16 ; i++)
	{
		if (ip.s6_addr[i] != ip2[i])
			return false;
	}
	return true;
}

bool RTPIPv6Address::IsFromSameHost(const RTPAddress *addr) const
{
	if (addr == 0)
		return false;
	if (addr->GetAddressType() != RTPAddress::IPv6Address)
		return false;

	const RTPIPv6Address *addr2 = (const RTPIPv6Address *)addr;
	const uint8_t *ip2 = addr2->ip.s6_addr;
	for (int i = 0 ; i < 16 ; i++)
	{
		if (ip.s6_addr[i] != ip2[i])
			return false;
	}
	return true;
}


} // end namespace

#endif // RTP_SUPPORT_IPV6

