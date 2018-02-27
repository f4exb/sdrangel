/*

  This file is a part of JRTPLIB
  Copyright (c) 1999-2011 Jori Liesenborgs

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

#include "rtpipv6destination.h"

#ifdef RTP_SUPPORT_IPV6

#include "rtpinternalutils.h"

namespace qrtplib
{

std::string RTPIPv6Destination::GetDestinationString() const
{
	uint16_t ip16[8];
	char str[48];
	uint16_t portbase = ntohs(rtpaddr.sin6_port);
	int i,j;
	for (i = 0,j = 0 ; j < 8 ; j++,i += 2)
	{
		ip16[j] = (((uint16_t)rtpaddr.sin6_addr.s6_addr[i])<<8);
		ip16[j] |= ((uint16_t)rtpaddr.sin6_addr.s6_addr[i+1]);
	}
	RTP_SNPRINTF(str,48,"%04X:%04X:%04X:%04X:%04X:%04X:%04X:%04X/%d",(int)ip16[0],(int)ip16[1],(int)ip16[2],(int)ip16[3],(int)ip16[4],(int)ip16[5],(int)ip16[6],(int)ip16[7],(int)portbase);
	return std::string(str);
}

} // end namespace

#endif // RTP_SUPPORT_IPV6


