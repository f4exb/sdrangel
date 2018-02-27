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
 * \file rtpmemoryobject.h
 */

#ifndef RTPMEMORYOBJECT_H

#define RTPMEMORYOBJECT_H

#include "rtpconfig.h"
#include "rtpmemorymanager.h"

namespace qrtplib
{

class JRTPLIB_IMPORTEXPORT RTPMemoryObject
{
protected:
#ifdef RTP_SUPPORT_MEMORYMANAGEMENT
	RTPMemoryObject(RTPMemoryManager *memmgr) : mgr(memmgr)			{ }
#else
	RTPMemoryObject(RTPMemoryManager *memmgr)						{ JRTPLIB_UNUSED(memmgr); }
#endif // RTP_SUPPORT_MEMORYMANAGEMENT
	virtual ~RTPMemoryObject()										{ }

#ifdef RTP_SUPPORT_MEMORYMANAGEMENT
	RTPMemoryManager *GetMemoryManager() const						{ return mgr; }
	void SetMemoryManager(RTPMemoryManager *m)						{ mgr = m; }
#else
	RTPMemoryManager *GetMemoryManager() const						{ return 0; }
	void SetMemoryManager(RTPMemoryManager *m)						{ JRTPLIB_UNUSED(m); }
#endif // RTP_SUPPORT_MEMORYMANAGEMENT

#ifdef RTP_SUPPORT_MEMORYMANAGEMENT
private:
	RTPMemoryManager *mgr;
#endif // RTP_SUPPORT_MEMORYMANAGEMENT
};

} // end namespace

#endif // RTPMEMORYOBJECT_H

