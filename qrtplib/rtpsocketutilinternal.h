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
 * \file rtpsocketutilinternal.h
 */

#ifndef RTPSOCKETUTILINTERNAL_H

#define RTPSOCKETUTILINTERNAL_H

#ifdef RTP_SOCKETTYPE_WINSOCK
	#define RTPSOCKERR								INVALID_SOCKET
	#define RTPCLOSE(x)								closesocket(x)
	#define RTPSOCKLENTYPE							int
	#define RTPIOCTL								ioctlsocket
#else // not Win32
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <sys/ioctl.h>
	#include <net/if.h>
	#include <string.h>
	#include <netdb.h>
	#include <unistd.h>

	#ifdef RTP_HAVE_SYS_FILIO
		#include <sys/filio.h>
	#endif // RTP_HAVE_SYS_FILIO
	#ifdef RTP_HAVE_SYS_SOCKIO
		#include <sys/sockio.h>
	#endif // RTP_HAVE_SYS_SOCKIO
	#ifdef RTP_SUPPORT_IFADDRS
		#include <ifaddrs.h>
	#endif // RTP_SUPPORT_IFADDRS

	#define RTPSOCKERR								-1
	#define RTPCLOSE(x)								close(x)

	#ifdef RTP_SOCKLENTYPE_UINT
		#define RTPSOCKLENTYPE						unsigned int
	#else
		#define RTPSOCKLENTYPE						int
	#endif // RTP_SOCKLENTYPE_UINT

	#define RTPIOCTL								ioctl
#endif // RTP_SOCKETTYPE_WINSOCK

#endif // RTPSOCKETUTILINTERNAL_H

