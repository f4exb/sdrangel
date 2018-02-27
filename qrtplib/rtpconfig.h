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

#ifndef RTPCONFIG_UNIX_H

#define RTPCONFIG_UNIX_H

#ifndef JRTPLIB_UNUSED
/**
 * Provide a macro to use for marking method parameters as unused.
 */
#define JRTPLIB_UNUSED(x) (void)(x)
#endif // JRTPLIB_UNUSED

#define JRTPLIB_IMPORT 
#define JRTPLIB_EXPORT 
#ifdef JRTPLIB_COMPILING
	#define JRTPLIB_IMPORTEXPORT JRTPLIB_EXPORT
#else
	#define JRTPLIB_IMPORTEXPORT JRTPLIB_IMPORT
#endif // JRTPLIB_COMPILING

// Don't have <sys/filio.h>

// Don't have <sys/sockio.h>

// Little endian system

#define RTP_SOCKLENTYPE_UINT

// No sa_len member in struct sockaddr

#define RTP_SUPPORT_IPV4MULTICAST

// No support for JThread was enabled

#define RTP_SUPPORT_SDESPRIV

#define RTP_SUPPORT_PROBATION

#define RTP_SUPPORT_GETLOGINR

#define RTP_SUPPORT_IPV6

#define RTP_SUPPORT_IPV6MULTICAST

#define RTP_SUPPORT_IFADDRS

#define RTP_SUPPORT_SENDAPP

#define RTP_SUPPORT_MEMORYMANAGEMENT

// No support for sending unknown RTCP packets

#define RTP_SUPPORT_NETINET_IN

// Not using winsock sockets

// No QueryPerformanceCounter support

// No ui64 suffix

// Stdio snprintf version

#define RTP_HAVE_ARRAYALLOC

// No rand_s support

// No strncpy_s support

// No SRTP support

#define RTP_HAVE_CLOCK_GETTIME

#define RTP_HAVE_POLL

// No 'WSAPoll' support

#define RTP_HAVE_MSG_NOSIGNAL

#endif // RTPCONFIG_UNIX_H

