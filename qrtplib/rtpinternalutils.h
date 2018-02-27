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

#ifndef RTPINTERNALUTILS_H

#define RTPINTERNALUTILS_H

#include "rtpconfig.h"

#if defined(RTP_HAVE_SNPRINTF_S)
	#include <windows.h>
	#include <stdio.h>
	#define RTP_SNPRINTF _snprintf_s
#elif defined(RTP_HAVE_SNPRINTF)
	#include <windows.h>
	#include <stdio.h>
	#define RTP_SNPRINTF _snprintf
#else
	#include <stdio.h>
	#define RTP_SNPRINTF snprintf
#endif 

#ifdef RTP_HAVE_STRNCPY_S
	#define RTP_STRNCPY(dest, src, len) strncpy_s((dest), (len), (src), _TRUNCATE)
#else
	#define RTP_STRNCPY(dest, src, len) strncpy((dest), (src), (len))
#endif // RTP_HAVE_STRNCPY_S

#endif // RTPINTERNALUTILS_H

