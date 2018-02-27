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

#include "rtpabortdescriptors.h"
#include "rtpsocketutilinternal.h"
#include "rtperrors.h"
#include "rtpselect.h"

namespace qrtplib
{

RTPAbortDescriptors::RTPAbortDescriptors()
{
	m_descriptors[0] = RTPSOCKERR;
	m_descriptors[1] = RTPSOCKERR;
	m_init = false;
}

RTPAbortDescriptors::~RTPAbortDescriptors()
{
	Destroy();
}

#ifdef RTP_SOCKETTYPE_WINSOCK

int RTPAbortDescriptors::Init()
{
	if (m_init)
		return ERR_RTP_ABORTDESC_ALREADYINIT;

	SOCKET listensock;
	int size;
	struct sockaddr_in addr;

	listensock = socket(PF_INET,SOCK_STREAM,0);
	if (listensock == RTPSOCKERR)
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;

	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	if (bind(listensock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(listensock);
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	size = sizeof(struct sockaddr_in);
	if (getsockname(listensock,(struct sockaddr*)&addr,&size) != 0)
	{
		RTPCLOSE(listensock);
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
	}

	unsigned short connectport = ntohs(addr.sin_port);

	m_descriptors[0] = socket(PF_INET,SOCK_STREAM,0);
	if (m_descriptors[0] == RTPSOCKERR)
	{
		RTPCLOSE(listensock);
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	if (bind(m_descriptors[0],(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(m_descriptors[0]);
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
	}

	if (listen(listensock,1) != 0)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(m_descriptors[0]);
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(connectport);

	if (connect(m_descriptors[0],(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(m_descriptors[0]);
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	size = sizeof(struct sockaddr_in);
	m_descriptors[1] = accept(listensock,(struct sockaddr *)&addr,&size);
	if (m_descriptors[1] == RTPSOCKERR)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(m_descriptors[0]);
		return ERR_RTP_ABORTDESC_CANTCREATEABORTDESCRIPTORS;
	}

	// okay, got the connection, close the listening socket

	RTPCLOSE(listensock);

	m_init = true;
	return 0;
}

void RTPAbortDescriptors::Destroy()
{
	if (!m_init)
		return;

	RTPCLOSE(m_descriptors[0]);
	RTPCLOSE(m_descriptors[1]);
	m_descriptors[0] = RTPSOCKERR;
	m_descriptors[1] = RTPSOCKERR;

	m_init = false;
}

int RTPAbortDescriptors::SendAbortSignal()
{
	if (!m_init)
		return ERR_RTP_ABORTDESC_NOTINIT;

	send(m_descriptors[1],"*",1,0);
	return 0;
}

int RTPAbortDescriptors::ReadSignallingByte()
{
	if (!m_init)
		return ERR_RTP_ABORTDESC_NOTINIT;

	char buf[1];

	recv(m_descriptors[0],buf,1,0);
	return 0;
}

#else // unix-style

int RTPAbortDescriptors::Init()
{
	if (m_init)
		return ERR_RTP_ABORTDESC_ALREADYINIT;

	if (pipe(m_descriptors) < 0)
		return ERR_RTP_ABORTDESC_CANTCREATEPIPE;

	m_init = true;
	return 0;
}

void RTPAbortDescriptors::Destroy()
{
	if (!m_init)
		return;

	close(m_descriptors[0]);
	close(m_descriptors[1]);
	m_descriptors[0] = RTPSOCKERR;
	m_descriptors[1] = RTPSOCKERR;

	m_init = false;
}

int RTPAbortDescriptors::SendAbortSignal()
{
	if (!m_init)
		return ERR_RTP_ABORTDESC_NOTINIT;

	if (write(m_descriptors[1],"*",1))
	{
		// To get rid of __wur related compiler warnings
	}

	return 0;
}

int RTPAbortDescriptors::ReadSignallingByte()
{
	if (!m_init)
		return ERR_RTP_ABORTDESC_NOTINIT;

	unsigned char buf[1];

	if (read(m_descriptors[0],buf,1))
	{
		// To get rid of __wur related compiler warnings
	}
	return 0;
}

#endif // RTP_SOCKETTYPE_WINSOCK

// Keep calling 'ReadSignallingByte' until there's no byte left
int RTPAbortDescriptors::ClearAbortSignal()
{
	if (!m_init)
		return ERR_RTP_ABORTDESC_NOTINIT;

	bool done = false;
	while (!done)
	{
		int8_t isset = 0;

		// Not used: struct timeval tv = { 0, 0 };

		int status = RTPSelect(&m_descriptors[0], &isset, 1, RTPTime(0));
		if (status < 0)
			return status;

		if (!isset)
			done = true;
		else
		{
			int status = ReadSignallingByte();
			if (status < 0)
				return status;
		}
	}

	return 0;
}

} // end namespace
