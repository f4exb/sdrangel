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

#include "rtpudpv4transmitter.h"
#include "rtprawpacket.h"
#include "rtpipv4address.h"
#include "rtptimeutilities.h"
#include "rtpdefines.h"
#include "rtpstructs.h"
#include "rtpsocketutilinternal.h"
#include "rtpinternalutils.h"
#include "rtpselect.h"
#include <stdio.h>
#include <assert.h>
#include <vector>

#include <iostream>

using namespace std;

#define RTPUDPV4TRANS_MAXPACKSIZE							65535
#define RTPUDPV4TRANS_IFREQBUFSIZE							8192

#define RTPUDPV4TRANS_IS_MCASTADDR(x)							(((x)&0xF0000000) == 0xE0000000)

#define RTPUDPV4TRANS_MCASTMEMBERSHIP(socket,type,mcastip,status)	{\
										struct ip_mreq mreq;\
										\
										mreq.imr_multiaddr.s_addr = htonl(mcastip);\
										mreq.imr_interface.s_addr = htonl(mcastifaceIP);\
										status = setsockopt(socket,IPPROTO_IP,type,(const char *)&mreq,sizeof(struct ip_mreq));\
									}
#define MAINMUTEX_LOCK
#define MAINMUTEX_UNLOCK
#define WAITMUTEX_LOCK
#define WAITMUTEX_UNLOCK

#define CLOSESOCKETS do { \
	if (closesocketswhendone) \
	{\
		if (rtpsock != rtcpsock) \
			RTPCLOSE(rtcpsock); \
		RTPCLOSE(rtpsock); \
	} \
} while(0)


namespace qrtplib
{

RTPUDPv4Transmitter::RTPUDPv4Transmitter(RTPMemoryManager *mgr) : RTPTransmitter(mgr),destinations(mgr,RTPMEM_TYPE_CLASS_DESTINATIONLISTHASHELEMENT),
#ifdef RTP_SUPPORT_IPV4MULTICAST
								  multicastgroups(mgr,RTPMEM_TYPE_CLASS_MULTICASTHASHELEMENT),
#endif // RTP_SUPPORT_IPV4MULTICAST
								  acceptignoreinfo(mgr,RTPMEM_TYPE_CLASS_ACCEPTIGNOREHASHELEMENT)
{
	created = false;
	init = false;
}

RTPUDPv4Transmitter::~RTPUDPv4Transmitter()
{
	Destroy();
}

int RTPUDPv4Transmitter::Init(bool tsafe)
{
	if (init)
		return ERR_RTP_UDPV4TRANS_ALREADYINIT;

	if (tsafe)
		return ERR_RTP_NOTHREADSUPPORT;

	init = true;
	return 0;
}

int RTPUDPv4Transmitter::GetIPv4SocketPort(SocketType s, uint16_t *pPort)
{
	assert(pPort != 0);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));

	RTPSOCKLENTYPE size = sizeof(struct sockaddr_in);
	if (getsockname(s,(struct sockaddr*)&addr,&size) != 0)
		return ERR_RTP_UDPV4TRANS_CANTGETSOCKETPORT;

	if (addr.sin_family != AF_INET)
		return ERR_RTP_UDPV4TRANS_NOTANIPV4SOCKET;

	uint16_t port = ntohs(addr.sin_port);
	if (port == 0)
		return ERR_RTP_UDPV4TRANS_SOCKETPORTNOTSET;

	int type = 0;
	RTPSOCKLENTYPE length = sizeof(type);

	if (getsockopt(s, SOL_SOCKET, SO_TYPE, (char*)&type, &length) != 0)
		return ERR_RTP_UDPV4TRANS_CANTGETSOCKETTYPE;

	if (type != SOCK_DGRAM)
		return ERR_RTP_UDPV4TRANS_INVALIDSOCKETTYPE;

	*pPort = port;
	return 0;
}

int RTPUDPv4Transmitter::GetAutoSockets(uint32_t bindIP, bool allowOdd, bool rtcpMux,
                   SocketType *pRtpSock, SocketType *pRtcpSock,
                   uint16_t *pRtpPort, uint16_t *pRtcpPort)
{
	const int maxAttempts = 1024;
	int attempts = 0;
	vector<SocketType> toClose;

	while (attempts++ < maxAttempts)
	{
		SocketType sock = socket(PF_INET, SOCK_DGRAM, 0);
		if (sock == RTPSOCKERR)
		{
			for (size_t i = 0 ; i < toClose.size() ; i++)
				RTPCLOSE(toClose[i]);
			return ERR_RTP_UDPV4TRANS_CANTCREATESOCKET;
		}

		// First we get an automatically chosen port

		struct sockaddr_in addr;
		memset(&addr,0,sizeof(struct sockaddr_in));

		addr.sin_family = AF_INET;
		addr.sin_port = 0;
		addr.sin_addr.s_addr = htonl(bindIP);
		if (bind(sock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
		{
			RTPCLOSE(sock);
			for (size_t i = 0 ; i < toClose.size() ; i++)
				RTPCLOSE(toClose[i]);
			return ERR_RTP_UDPV4TRANS_CANTGETVALIDSOCKET;
		}

		uint16_t basePort = 0;
		int status = GetIPv4SocketPort(sock, &basePort);
		if (status < 0)
		{
			RTPCLOSE(sock);
			for (size_t i = 0 ; i < toClose.size() ; i++)
				RTPCLOSE(toClose[i]);
			return status;
		}

		if (rtcpMux) // only need one socket
		{
			if (basePort%2 == 0 || allowOdd)
			{
				*pRtpSock = sock;
				*pRtcpSock = sock;
				*pRtpPort = basePort;
				*pRtcpPort = basePort;
				for (size_t i = 0 ; i < toClose.size() ; i++)
					RTPCLOSE(toClose[i]);

				return 0;
			}
			else
				toClose.push_back(sock);
		}
		else
		{
			SocketType sock2 = socket(PF_INET, SOCK_DGRAM, 0);
			if (sock2 == RTPSOCKERR)
			{
				RTPCLOSE(sock);
				for (size_t i = 0 ; i < toClose.size() ; i++)
					RTPCLOSE(toClose[i]);
				return ERR_RTP_UDPV4TRANS_CANTCREATESOCKET;
			}

			// Try the next port or the previous port
			uint16_t secondPort = basePort;
			bool possiblyValid = false;

			if (basePort%2 == 0)
			{
				secondPort++;
				possiblyValid = true;
			}
			else if (basePort > 1) // avoid landing on port 0
			{
				secondPort--;
				possiblyValid = true;
			}

			if (possiblyValid)
			{
				memset(&addr,0,sizeof(struct sockaddr_in));

				addr.sin_family = AF_INET;
				addr.sin_port = htons(secondPort);
				addr.sin_addr.s_addr = htonl(bindIP);
				if (bind(sock2,(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) == 0)
				{
					// In this case, we have two consecutive port numbers, the lower of
					// which is even

					if (basePort < secondPort)
					{
						*pRtpSock = sock;
						*pRtcpSock = sock2;
						*pRtpPort = basePort;
						*pRtcpPort = secondPort;
					}
					else
					{
						*pRtpSock = sock2;
						*pRtcpSock = sock;
						*pRtpPort = secondPort;
						*pRtcpPort = basePort;
					}

					for (size_t i = 0 ; i < toClose.size() ; i++)
						RTPCLOSE(toClose[i]);

					return 0;
				}
			}

			toClose.push_back(sock);
			toClose.push_back(sock2);
		}
	}

	for (size_t i = 0 ; i < toClose.size() ; i++)
		RTPCLOSE(toClose[i]);

	return ERR_RTP_UDPV4TRANS_TOOMANYATTEMPTSCHOOSINGSOCKET;
}

int RTPUDPv4Transmitter::Create(size_t maximumpacketsize,const RTPTransmissionParams *transparams)
{
	const RTPUDPv4TransmissionParams *params,defaultparams;
	struct sockaddr_in addr;
	RTPSOCKLENTYPE size;
	int status;

	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK

	if (created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_ALREADYCREATED;
	}

	// Obtain transmission parameters

	if (transparams == 0)
		params = &defaultparams;
	else
	{
		if (transparams->GetTransmissionProtocol() != RTPTransmitter::IPv4UDPProto)
		{
			MAINMUTEX_UNLOCK
			return ERR_RTP_UDPV4TRANS_ILLEGALPARAMETERS;
		}
		params = (const RTPUDPv4TransmissionParams *)transparams;
	}

	if (params->GetUseExistingSockets(rtpsock, rtcpsock))
	{
		closesocketswhendone = false;

		// Determine the port numbers
		int status = GetIPv4SocketPort(rtpsock, &m_rtpPort);
		if (status < 0)
		{
			MAINMUTEX_UNLOCK
			return status;
		}
		status = GetIPv4SocketPort(rtcpsock, &m_rtcpPort);
		if (status < 0)
		{
			MAINMUTEX_UNLOCK
			return status;
		}
	}
	else
	{
		closesocketswhendone = true;

		if (params->GetPortbase() == 0)
		{
			int status = GetAutoSockets(params->GetBindIP(), params->GetAllowOddPortbase(), params->GetRTCPMultiplexing(),
			                            &rtpsock, &rtcpsock, &m_rtpPort, &m_rtcpPort);
			if (status < 0)
			{
				MAINMUTEX_UNLOCK
				return status;
			}
		}
		else
		{
			// Check if portbase is even (if necessary)
			if (!params->GetAllowOddPortbase() && params->GetPortbase()%2 != 0)
			{
				MAINMUTEX_UNLOCK
				return ERR_RTP_UDPV4TRANS_PORTBASENOTEVEN;
			}

			// create sockets

			rtpsock = socket(PF_INET,SOCK_DGRAM,0);
			if (rtpsock == RTPSOCKERR)
			{
				MAINMUTEX_UNLOCK
				return ERR_RTP_UDPV4TRANS_CANTCREATESOCKET;
			}

			// If we're multiplexing, we're just going to set the RTCP socket to equal the RTP socket
			if (params->GetRTCPMultiplexing())
				rtcpsock = rtpsock;
			else
			{
				rtcpsock = socket(PF_INET,SOCK_DGRAM,0);
				if (rtcpsock == RTPSOCKERR)
				{
					RTPCLOSE(rtpsock);
					MAINMUTEX_UNLOCK
					return ERR_RTP_UDPV4TRANS_CANTCREATESOCKET;
				}
			}

			// bind sockets

			uint32_t bindIP = params->GetBindIP();

			m_rtpPort = params->GetPortbase();

			memset(&addr,0,sizeof(struct sockaddr_in));
			addr.sin_family = AF_INET;
			addr.sin_port = htons(params->GetPortbase());
			addr.sin_addr.s_addr = htonl(bindIP);
			if (bind(rtpsock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
			{
				CLOSESOCKETS;
				MAINMUTEX_UNLOCK
				return ERR_RTP_UDPV4TRANS_CANTBINDRTPSOCKET;
			}

			if (rtpsock != rtcpsock) // no need to bind same socket twice when multiplexing
			{
				uint16_t rtpport = params->GetPortbase();
				uint16_t rtcpport = params->GetForcedRTCPPort();

				if (rtcpport == 0)
				{
					rtcpport = rtpport;
					if (rtcpport < 0xFFFF)
						rtcpport++;
				}

				memset(&addr,0,sizeof(struct sockaddr_in));
				addr.sin_family = AF_INET;
				addr.sin_port = htons(rtcpport);
				addr.sin_addr.s_addr = htonl(bindIP);
				if (bind(rtcpsock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
				{
					CLOSESOCKETS;
					MAINMUTEX_UNLOCK
					return ERR_RTP_UDPV4TRANS_CANTBINDRTCPSOCKET;
				}

				m_rtcpPort = rtcpport;
			}
			else
				m_rtcpPort = m_rtpPort;
		}

		// set socket buffer sizes

		size = params->GetRTPReceiveBuffer();
		if (setsockopt(rtpsock,SOL_SOCKET,SO_RCVBUF,(const char *)&size,sizeof(int)) != 0)
		{
			CLOSESOCKETS;
			MAINMUTEX_UNLOCK
			return ERR_RTP_UDPV4TRANS_CANTSETRTPRECEIVEBUF;
		}
		size = params->GetRTPSendBuffer();
		if (setsockopt(rtpsock,SOL_SOCKET,SO_SNDBUF,(const char *)&size,sizeof(int)) != 0)
		{
			CLOSESOCKETS;
			MAINMUTEX_UNLOCK
			return ERR_RTP_UDPV4TRANS_CANTSETRTPTRANSMITBUF;
		}

		if (rtpsock != rtcpsock) // no need to set RTCP flags when multiplexing
		{
			size = params->GetRTCPReceiveBuffer();
			if (setsockopt(rtcpsock,SOL_SOCKET,SO_RCVBUF,(const char *)&size,sizeof(int)) != 0)
			{
				CLOSESOCKETS;
				MAINMUTEX_UNLOCK
				return ERR_RTP_UDPV4TRANS_CANTSETRTCPRECEIVEBUF;
			}
			size = params->GetRTCPSendBuffer();
			if (setsockopt(rtcpsock,SOL_SOCKET,SO_SNDBUF,(const char *)&size,sizeof(int)) != 0)
			{
				CLOSESOCKETS;
				MAINMUTEX_UNLOCK
				return ERR_RTP_UDPV4TRANS_CANTSETRTCPTRANSMITBUF;
			}
		}
	}

	// Try to obtain local IP addresses

	localIPs = params->GetLocalIPList();
	if (localIPs.empty()) // User did not provide list of local IP addresses, calculate them
	{
		int status;

		if ((status = CreateLocalIPList()) < 0)
		{
			CLOSESOCKETS;
			MAINMUTEX_UNLOCK
			return status;
		}
	}

#ifdef RTP_SUPPORT_IPV4MULTICAST
	if (SetMulticastTTL(params->GetMulticastTTL()))
		supportsmulticasting = true;
	else
		supportsmulticasting = false;
#else // no multicast support enabled
	supportsmulticasting = false;
#endif // RTP_SUPPORT_IPV4MULTICAST

	if (maximumpacketsize > RTPUDPV4TRANS_MAXPACKSIZE)
	{
		CLOSESOCKETS;
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG;
	}

	if (!params->GetCreatedAbortDescriptors())
	{
		if ((status = m_abortDesc.Init()) < 0)
		{
			CLOSESOCKETS;
			MAINMUTEX_UNLOCK
			return status;
		}
		m_pAbortDesc = &m_abortDesc;
	}
	else
	{
		m_pAbortDesc = params->GetCreatedAbortDescriptors();
		if (!m_pAbortDesc->IsInitialized())
		{
			CLOSESOCKETS;
			MAINMUTEX_UNLOCK
			return ERR_RTP_ABORTDESC_NOTINIT;
		}
	}

	maxpacksize = maximumpacketsize;
	multicastTTL = params->GetMulticastTTL();
	mcastifaceIP = params->GetMulticastInterfaceIP();
	receivemode = RTPTransmitter::AcceptAll;

	localhostname = 0;
	localhostnamelength = 0;

	waitingfordata = false;
	created = true;
	MAINMUTEX_UNLOCK
	return 0;
}

void RTPUDPv4Transmitter::Destroy()
{
	if (!init)
		return;

	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK;
		return;
	}

	if (localhostname)
	{
		RTPDeleteByteArray(localhostname,GetMemoryManager());
		localhostname = 0;
		localhostnamelength = 0;
	}

	CLOSESOCKETS;
	destinations.Clear();
#ifdef RTP_SUPPORT_IPV4MULTICAST
	multicastgroups.Clear();
#endif // RTP_SUPPORT_IPV4MULTICAST
	FlushPackets();
	ClearAcceptIgnoreInfo();
	localIPs.clear();
	created = false;

	if (waitingfordata)
	{
		m_pAbortDesc->SendAbortSignal();
		m_abortDesc.Destroy(); // Doesn't do anything if not initialized
		MAINMUTEX_UNLOCK
		WAITMUTEX_LOCK // to make sure that the WaitForIncomingData function ended
		WAITMUTEX_UNLOCK
	}
	else
		m_abortDesc.Destroy(); // Doesn't do anything if not initialized

	MAINMUTEX_UNLOCK
}

RTPTransmissionInfo *RTPUDPv4Transmitter::GetTransmissionInfo()
{
	if (!init)
		return 0;

	MAINMUTEX_LOCK
	RTPTransmissionInfo *tinf = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPTRANSMISSIONINFO) RTPUDPv4TransmissionInfo(localIPs,rtpsock,rtcpsock,m_rtpPort,m_rtcpPort);
	MAINMUTEX_UNLOCK
	return tinf;
}

void RTPUDPv4Transmitter::DeleteTransmissionInfo(RTPTransmissionInfo *i)
{
	if (!init)
		return;

	RTPDelete(i, GetMemoryManager());
}

int RTPUDPv4Transmitter::GetLocalHostName(uint8_t *buffer,size_t *bufferlength)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}

	if (localhostname == 0)
	{
		if (localIPs.empty())
		{
			MAINMUTEX_UNLOCK
			return ERR_RTP_UDPV4TRANS_NOLOCALIPS;
		}

		std::list<uint32_t>::const_iterator it;
		std::list<std::string> hostnames;

		for (it = localIPs.begin() ; it != localIPs.end() ; it++)
		{
			bool founddouble = false;
			bool foundentry = true;

			while (!founddouble && foundentry)
			{
				struct hostent *he;
				uint8_t addr[4];
				uint32_t ip = (*it);

				addr[0] = (uint8_t)((ip>>24)&0xFF);
				addr[1] = (uint8_t)((ip>>16)&0xFF);
				addr[2] = (uint8_t)((ip>>8)&0xFF);
				addr[3] = (uint8_t)(ip&0xFF);
				he = gethostbyaddr((char *)addr,4,AF_INET);
				if (he != 0)
				{
					std::string hname = std::string(he->h_name);
					std::list<std::string>::const_iterator it;

					for (it = hostnames.begin() ; !founddouble && it != hostnames.end() ; it++)
						if ((*it) == hname)
							founddouble = true;

					if (!founddouble)
						hostnames.push_back(hname);

					int i = 0;
					while (!founddouble && he->h_aliases[i] != 0)
					{
						std::string hname = std::string(he->h_aliases[i]);

						for (it = hostnames.begin() ; !founddouble && it != hostnames.end() ; it++)
							if ((*it) == hname)
								founddouble = true;

						if (!founddouble)
						{
							hostnames.push_back(hname);
							i++;
						}
					}
				}
				else
					foundentry = false;
			}
		}

		bool found  = false;

		if (!hostnames.empty())	// try to select the most appropriate hostname
		{
			std::list<std::string>::const_iterator it;

			hostnames.sort();
			for (it = hostnames.begin() ; !found && it != hostnames.end() ; it++)
			{
				if ((*it).find('.') != std::string::npos)
				{
					found = true;
					localhostnamelength = (*it).length();
					localhostname = RTPNew(GetMemoryManager(),RTPMEM_TYPE_OTHER) uint8_t [localhostnamelength+1];
					if (localhostname == 0)
					{
						MAINMUTEX_UNLOCK
						return ERR_RTP_OUTOFMEM;
					}
					memcpy(localhostname,(*it).c_str(),localhostnamelength);
					localhostname[localhostnamelength] = 0;
				}
			}
		}

		if (!found) // use an IP address
		{
			uint32_t ip;
			int len;
			char str[16];

			it = localIPs.begin();
			ip = (*it);

			RTP_SNPRINTF(str,16,"%d.%d.%d.%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),(int)(ip&0xFF));
			len = strlen(str);

			localhostnamelength = len;
			localhostname = RTPNew(GetMemoryManager(),RTPMEM_TYPE_OTHER) uint8_t [localhostnamelength + 1];
			if (localhostname == 0)
			{
				MAINMUTEX_UNLOCK
				return ERR_RTP_OUTOFMEM;
			}
			memcpy(localhostname,str,localhostnamelength);
			localhostname[localhostnamelength] = 0;
		}
	}

	if ((*bufferlength) < localhostnamelength)
	{
		*bufferlength = localhostnamelength; // tell the application the required size of the buffer
		MAINMUTEX_UNLOCK
		return ERR_RTP_TRANS_BUFFERLENGTHTOOSMALL;
	}

	memcpy(buffer,localhostname,localhostnamelength);
	*bufferlength = localhostnamelength;

	MAINMUTEX_UNLOCK
	return 0;
}

bool RTPUDPv4Transmitter::ComesFromThisTransmitter(const RTPAddress *addr)
{
	if (!init)
		return false;

	if (addr == 0)
		return false;

	MAINMUTEX_LOCK

	bool v;

	if (created && addr->GetAddressType() == RTPAddress::IPv4Address)
	{
		const RTPIPv4Address *addr2 = (const RTPIPv4Address *)addr;
		bool found = false;
		std::list<uint32_t>::const_iterator it;

		it = localIPs.begin();
		while (!found && it != localIPs.end())
		{
			if (addr2->GetIP() == *it)
				found = true;
			else
				++it;
		}

		if (!found)
			v = false;
		else
		{
			if (addr2->GetPort() == m_rtpPort || addr2->GetPort() == m_rtcpPort) // check for RTP port and RTCP port
				v = true;
			else
				v = false;
		}
	}
	else
		v = false;

	MAINMUTEX_UNLOCK
	return v;
}

int RTPUDPv4Transmitter::Poll()
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	int status;

	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	status = PollSocket(true); // poll RTP socket
	if (rtpsock != rtcpsock) // no need to poll twice when multiplexing
	{
		if (status >= 0)
			status = PollSocket(false); // poll RTCP socket
	}
	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv4Transmitter::WaitForIncomingData(const RTPTime &delay,bool *dataavailable)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (waitingfordata)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_ALREADYWAITING;
	}

	SocketType abortSocket = m_pAbortDesc->GetAbortSocket();

	SocketType socks[3] = { rtpsock, rtcpsock, abortSocket };
	int8_t readflags[3] = { 0, 0, 0 };
	const int idxRTP = 0;
	const int idxRTCP = 1;
	const int idxAbort = 2;

	waitingfordata = true;

	WAITMUTEX_LOCK
	MAINMUTEX_UNLOCK

	int status = RTPSelect(socks, readflags, 3, delay);
	if (status < 0)
	{
		MAINMUTEX_LOCK
		waitingfordata = false;
		MAINMUTEX_UNLOCK
		WAITMUTEX_UNLOCK
		return status;
	}

	MAINMUTEX_LOCK
	waitingfordata = false;
	if (!created) // destroy called
	{
		MAINMUTEX_UNLOCK;
		WAITMUTEX_UNLOCK
		return 0;
	}

	// if aborted, read from abort buffer
	if (readflags[idxAbort])
		m_pAbortDesc->ReadSignallingByte();

	if (dataavailable != 0)
	{
		if (readflags[idxRTP] || readflags[idxRTCP])
			*dataavailable = true;
		else
			*dataavailable = false;
	}

	MAINMUTEX_UNLOCK
	WAITMUTEX_UNLOCK
	return 0;
}

int RTPUDPv4Transmitter::AbortWait()
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (!waitingfordata)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTWAITING;
	}

	m_pAbortDesc->SendAbortSignal();

	MAINMUTEX_UNLOCK
	return 0;
}

int RTPUDPv4Transmitter::SendRTPData(const void *data,size_t len)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (len > maxpacksize)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG;
	}

	destinations.GotoFirstElement();
	while (destinations.HasCurrentElement())
	{
		sendto(rtpsock,(const char *)data,len,0,(const struct sockaddr *)destinations.GetCurrentElement().GetRTPSockAddr(),sizeof(struct sockaddr_in));
		destinations.GotoNextElement();
	}

	MAINMUTEX_UNLOCK
	return 0;
}

int RTPUDPv4Transmitter::SendRTCPData(const void *data,size_t len)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (len > maxpacksize)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG;
	}

	destinations.GotoFirstElement();
	while (destinations.HasCurrentElement())
	{
		sendto(rtcpsock,(const char *)data,len,0,(const struct sockaddr *)destinations.GetCurrentElement().GetRTCPSockAddr(),sizeof(struct sockaddr_in));
		destinations.GotoNextElement();
	}

	MAINMUTEX_UNLOCK
	return 0;
}

int RTPUDPv4Transmitter::AddDestination(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}

	RTPIPv4Destination dest;
	if (!RTPIPv4Destination::AddressToDestination(addr, dest))
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
	}

	int status = destinations.AddElement(dest);

	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv4Transmitter::DeleteDestination(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	RTPIPv4Destination dest;
	if (!RTPIPv4Destination::AddressToDestination(addr, dest))
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
	}

	int status = destinations.DeleteElement(dest);

	MAINMUTEX_UNLOCK
	return status;
}

void RTPUDPv4Transmitter::ClearDestinations()
{
	if (!init)
		return;

	MAINMUTEX_LOCK
	if (created)
		destinations.Clear();
	MAINMUTEX_UNLOCK
}

bool RTPUDPv4Transmitter::SupportsMulticasting()
{
	if (!init)
		return false;

	MAINMUTEX_LOCK

	bool v;

	if (!created)
		v = false;
	else
		v = supportsmulticasting;

	MAINMUTEX_UNLOCK
	return v;
}

#ifdef RTP_SUPPORT_IPV4MULTICAST

int RTPUDPv4Transmitter::JoinMulticastGroup(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK

	int status;

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
	}

	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
	uint32_t mcastIP = address.GetIP();

	if (!RTPUDPV4TRANS_IS_MCASTADDR(mcastIP))
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTAMULTICASTADDRESS;
	}

	status = multicastgroups.AddElement(mcastIP);
	if (status >= 0)
	{
		RTPUDPV4TRANS_MCASTMEMBERSHIP(rtpsock,IP_ADD_MEMBERSHIP,mcastIP,status);
		if (status != 0)
		{
			multicastgroups.DeleteElement(mcastIP);
			MAINMUTEX_UNLOCK
			return ERR_RTP_UDPV4TRANS_COULDNTJOINMULTICASTGROUP;
		}

		if (rtpsock != rtcpsock) // no need to join multicast group twice when multiplexing
		{
			RTPUDPV4TRANS_MCASTMEMBERSHIP(rtcpsock,IP_ADD_MEMBERSHIP,mcastIP,status);
			if (status != 0)
			{
				RTPUDPV4TRANS_MCASTMEMBERSHIP(rtpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
				multicastgroups.DeleteElement(mcastIP);
				MAINMUTEX_UNLOCK
				return ERR_RTP_UDPV4TRANS_COULDNTJOINMULTICASTGROUP;
			}
		}
	}
	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv4Transmitter::LeaveMulticastGroup(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK

	int status;

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
	}

	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
	uint32_t mcastIP = address.GetIP();

	if (!RTPUDPV4TRANS_IS_MCASTADDR(mcastIP))
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTAMULTICASTADDRESS;
	}

	status = multicastgroups.DeleteElement(mcastIP);
	if (status >= 0)
	{
		RTPUDPV4TRANS_MCASTMEMBERSHIP(rtpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
		if (rtpsock != rtcpsock) // no need to leave multicast group twice when multiplexing
			RTPUDPV4TRANS_MCASTMEMBERSHIP(rtcpsock,IP_DROP_MEMBERSHIP,mcastIP,status);

		status = 0;
	}

	MAINMUTEX_UNLOCK
	return status;
}

void RTPUDPv4Transmitter::LeaveAllMulticastGroups()
{
	if (!init)
		return;

	MAINMUTEX_LOCK
	if (created)
	{
		multicastgroups.GotoFirstElement();
		while (multicastgroups.HasCurrentElement())
		{
			uint32_t mcastIP;
			int status = 0;

			mcastIP = multicastgroups.GetCurrentElement();

			RTPUDPV4TRANS_MCASTMEMBERSHIP(rtpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
			if (rtpsock != rtcpsock) // no need to leave multicast group twice when multiplexing
				RTPUDPV4TRANS_MCASTMEMBERSHIP(rtcpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
			JRTPLIB_UNUSED(status);

			multicastgroups.GotoNextElement();
		}
		multicastgroups.Clear();
	}
	MAINMUTEX_UNLOCK
}

#else // no multicast support

int RTPUDPv4Transmitter::JoinMulticastGroup(const RTPAddress &addr)
{
	return ERR_RTP_UDPV4TRANS_NOMULTICASTSUPPORT;
}

int RTPUDPv4Transmitter::LeaveMulticastGroup(const RTPAddress &addr)
{
	return ERR_RTP_UDPV4TRANS_NOMULTICASTSUPPORT;
}

void RTPUDPv4Transmitter::LeaveAllMulticastGroups()
{
}

#endif // RTP_SUPPORT_IPV4MULTICAST

int RTPUDPv4Transmitter::SetReceiveMode(RTPTransmitter::ReceiveMode m)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (m != receivemode)
	{
		receivemode = m;
		acceptignoreinfo.Clear();
	}
	MAINMUTEX_UNLOCK
	return 0;
}

int RTPUDPv4Transmitter::AddToIgnoreList(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK

	int status;

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
	}
	if (receivemode != RTPTransmitter::IgnoreSome)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_DIFFERENTRECEIVEMODE;
	}

	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
	status = ProcessAddAcceptIgnoreEntry(address.GetIP(),address.GetPort());

	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv4Transmitter::DeleteFromIgnoreList(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK

	int status;

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
	}
	if (receivemode != RTPTransmitter::IgnoreSome)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_DIFFERENTRECEIVEMODE;
	}

	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
	status = ProcessDeleteAcceptIgnoreEntry(address.GetIP(),address.GetPort());

	MAINMUTEX_UNLOCK
	return status;
}

void RTPUDPv4Transmitter::ClearIgnoreList()
{
	if (!init)
		return;

	MAINMUTEX_LOCK
	if (created && receivemode == RTPTransmitter::IgnoreSome)
		ClearAcceptIgnoreInfo();
	MAINMUTEX_UNLOCK
}

int RTPUDPv4Transmitter::AddToAcceptList(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK

	int status;

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
	}
	if (receivemode != RTPTransmitter::AcceptSome)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_DIFFERENTRECEIVEMODE;
	}

	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
	status = ProcessAddAcceptIgnoreEntry(address.GetIP(),address.GetPort());

	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv4Transmitter::DeleteFromAcceptList(const RTPAddress &addr)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK

	int status;

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (addr.GetAddressType() != RTPAddress::IPv4Address)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE;
	}
	if (receivemode != RTPTransmitter::AcceptSome)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_DIFFERENTRECEIVEMODE;
	}

	const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
	status = ProcessDeleteAcceptIgnoreEntry(address.GetIP(),address.GetPort());

	MAINMUTEX_UNLOCK
	return status;
}

void RTPUDPv4Transmitter::ClearAcceptList()
{
	if (!init)
		return;

	MAINMUTEX_LOCK
	if (created && receivemode == RTPTransmitter::AcceptSome)
		ClearAcceptIgnoreInfo();
	MAINMUTEX_UNLOCK
}

int RTPUDPv4Transmitter::SetMaximumPacketSize(size_t s)
{
	if (!init)
		return ERR_RTP_UDPV4TRANS_NOTINIT;

	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_NOTCREATED;
	}
	if (s > RTPUDPV4TRANS_MAXPACKSIZE)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG;
	}
	maxpacksize = s;
	MAINMUTEX_UNLOCK
	return 0;
}

bool RTPUDPv4Transmitter::NewDataAvailable()
{
	if (!init)
		return false;

	MAINMUTEX_LOCK

	bool v;

	if (!created)
		v = false;
	else
	{
		if (rawpacketlist.empty())
			v = false;
		else
			v = true;
	}

	MAINMUTEX_UNLOCK
	return v;
}

RTPRawPacket *RTPUDPv4Transmitter::GetNextPacket()
{
	if (!init)
		return 0;

	MAINMUTEX_LOCK

	RTPRawPacket *p;

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return 0;
	}
	if (rawpacketlist.empty())
	{
		MAINMUTEX_UNLOCK
		return 0;
	}

	p = *(rawpacketlist.begin());
	rawpacketlist.pop_front();

	MAINMUTEX_UNLOCK
	return p;
}

// Here the private functions start...

#ifdef RTP_SUPPORT_IPV4MULTICAST
bool RTPUDPv4Transmitter::SetMulticastTTL(uint8_t ttl)
{
	int ttl2,status;

	ttl2 = (int)ttl;
	status = setsockopt(rtpsock,IPPROTO_IP,IP_MULTICAST_TTL,(const char *)&ttl2,sizeof(int));
	if (status != 0)
		return false;

	if (rtpsock != rtcpsock) // no need to set TTL twice when multiplexing
	{
		status = setsockopt(rtcpsock,IPPROTO_IP,IP_MULTICAST_TTL,(const char *)&ttl2,sizeof(int));
		if (status != 0)
			return false;
	}
	return true;
}
#endif // RTP_SUPPORT_IPV4MULTICAST

void RTPUDPv4Transmitter::FlushPackets()
{
	std::list<RTPRawPacket*>::const_iterator it;

	for (it = rawpacketlist.begin() ; it != rawpacketlist.end() ; ++it)
		RTPDelete(*it,GetMemoryManager());
	rawpacketlist.clear();
}

int RTPUDPv4Transmitter::PollSocket(bool rtp)
{
	RTPSOCKLENTYPE fromlen;
	int recvlen;
	char packetbuffer[RTPUDPV4TRANS_MAXPACKSIZE];
#ifdef RTP_SOCKETTYPE_WINSOCK
	SOCKET sock;
	unsigned long len;
#else
	size_t len;
	int sock;
#endif // RTP_SOCKETTYPE_WINSOCK
	struct sockaddr_in srcaddr;
	bool dataavailable;

	if (rtp)
		sock = rtpsock;
	else
		sock = rtcpsock;

	do
	{
		len = 0;
		RTPIOCTL(sock,FIONREAD,&len);

		if (len <= 0) // make sure a packet of length zero is not queued
		{
			// An alternative workaround would be to just use non-blocking sockets.
			// However, since the user does have access to the sockets and I do not
			// know how this would affect anyone else's code, I chose to do it using
			// an extra select call in case ioctl says the length is zero.

			int8_t isset = 0;
			int status = RTPSelect(&sock, &isset, 1, RTPTime(0));
			if (status < 0)
				return status;

			if (isset)
				dataavailable = true;
			else
				dataavailable = false;
		}
		else
			dataavailable = true;

		if (dataavailable)
		{
			RTPTime curtime = RTPTime::CurrentTime();
			fromlen = sizeof(struct sockaddr_in);
			recvlen = recvfrom(sock,packetbuffer,RTPUDPV4TRANS_MAXPACKSIZE,0,(struct sockaddr *)&srcaddr,&fromlen);
			if (recvlen > 0)
			{
				bool acceptdata;

				// got data, process it
				if (receivemode == RTPTransmitter::AcceptAll)
					acceptdata = true;
				else
					acceptdata = ShouldAcceptData(ntohl(srcaddr.sin_addr.s_addr),ntohs(srcaddr.sin_port));

				if (acceptdata)
				{
					RTPRawPacket *pack;
					RTPIPv4Address *addr;
					uint8_t *datacopy;

					addr = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPADDRESS) RTPIPv4Address(ntohl(srcaddr.sin_addr.s_addr),ntohs(srcaddr.sin_port));
					if (addr == 0)
						return ERR_RTP_OUTOFMEM;
					datacopy = RTPNew(GetMemoryManager(),(rtp)?RTPMEM_TYPE_BUFFER_RECEIVEDRTPPACKET:RTPMEM_TYPE_BUFFER_RECEIVEDRTCPPACKET) uint8_t[recvlen];
					if (datacopy == 0)
					{
						RTPDelete(addr,GetMemoryManager());
						return ERR_RTP_OUTOFMEM;
					}
					memcpy(datacopy,packetbuffer,recvlen);

					bool isrtp = rtp;
					if (rtpsock == rtcpsock) // check payload type when multiplexing
					{
						isrtp = true;

						if ((size_t)recvlen > sizeof(RTCPCommonHeader))
						{
							RTCPCommonHeader *rtcpheader = (RTCPCommonHeader *)datacopy;
							uint8_t packettype = rtcpheader->packettype;

    						if (packettype >= 200 && packettype <= 204)
								isrtp = false;
						}
					}

					pack = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPRAWPACKET) RTPRawPacket(datacopy,recvlen,addr,curtime,isrtp,GetMemoryManager());
					if (pack == 0)
					{
						RTPDelete(addr,GetMemoryManager());
						RTPDeleteByteArray(datacopy,GetMemoryManager());
						return ERR_RTP_OUTOFMEM;
					}
					rawpacketlist.push_back(pack);
				}
			}
		}
	} while (dataavailable);

	return 0;
}

int RTPUDPv4Transmitter::ProcessAddAcceptIgnoreEntry(uint32_t ip,uint16_t port)
{
	acceptignoreinfo.GotoElement(ip);
	if (acceptignoreinfo.HasCurrentElement()) // An entry for this IP address already exists
	{
		PortInfo *portinf = acceptignoreinfo.GetCurrentElement();

		if (port == 0) // select all ports
		{
			portinf->all = true;
			portinf->portlist.clear();
		}
		else if (!portinf->all)
		{
			std::list<uint16_t>::const_iterator it,begin,end;

			begin = portinf->portlist.begin();
			end = portinf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == port) // already in list
					return 0;
			}
			portinf->portlist.push_front(port);
		}
	}
	else // got to create an entry for this IP address
	{
		PortInfo *portinf;
		int status;

		portinf = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_ACCEPTIGNOREPORTINFO) PortInfo();
		if (port == 0) // select all ports
			portinf->all = true;
		else
			portinf->portlist.push_front(port);

		status = acceptignoreinfo.AddElement(ip,portinf);
		if (status < 0)
		{
			RTPDelete(portinf,GetMemoryManager());
			return status;
		}
	}

	return 0;
}

void RTPUDPv4Transmitter::ClearAcceptIgnoreInfo()
{
	acceptignoreinfo.GotoFirstElement();
	while (acceptignoreinfo.HasCurrentElement())
	{
		PortInfo *inf;

		inf = acceptignoreinfo.GetCurrentElement();
		RTPDelete(inf,GetMemoryManager());
		acceptignoreinfo.GotoNextElement();
	}
	acceptignoreinfo.Clear();
}

int RTPUDPv4Transmitter::ProcessDeleteAcceptIgnoreEntry(uint32_t ip,uint16_t port)
{
	acceptignoreinfo.GotoElement(ip);
	if (!acceptignoreinfo.HasCurrentElement())
		return ERR_RTP_UDPV4TRANS_NOSUCHENTRY;

	PortInfo *inf;

	inf = acceptignoreinfo.GetCurrentElement();
	if (port == 0) // delete all entries
	{
		inf->all = false;
		inf->portlist.clear();
	}
	else // a specific port was selected
	{
		if (inf->all) // currently, all ports are selected. Add the one to remove to the list
		{
			// we have to check if the list doesn't contain the port already
			std::list<uint16_t>::const_iterator it,begin,end;

			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == port) // already in list: this means we already deleted the entry
					return ERR_RTP_UDPV4TRANS_NOSUCHENTRY;
			}
			inf->portlist.push_front(port);
		}
		else // check if we can find the port in the list
		{
			std::list<uint16_t>::iterator it,begin,end;

			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; ++it)
			{
				if (*it == port) // found it!
				{
					inf->portlist.erase(it);
					return 0;
				}
			}
			// didn't find it
			return ERR_RTP_UDPV4TRANS_NOSUCHENTRY;
		}
	}
	return 0;
}

bool RTPUDPv4Transmitter::ShouldAcceptData(uint32_t srcip,uint16_t srcport)
{
	if (receivemode == RTPTransmitter::AcceptSome)
	{
		PortInfo *inf;

		acceptignoreinfo.GotoElement(srcip);
		if (!acceptignoreinfo.HasCurrentElement())
			return false;

		inf = acceptignoreinfo.GetCurrentElement();
		if (!inf->all) // only accept the ones in the list
		{
			std::list<uint16_t>::const_iterator it,begin,end;

			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == srcport)
					return true;
			}
			return false;
		}
		else // accept all, except the ones in the list
		{
			std::list<uint16_t>::const_iterator it,begin,end;

			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == srcport)
					return false;
			}
			return true;
		}
	}
	else // IgnoreSome
	{
		PortInfo *inf;

		acceptignoreinfo.GotoElement(srcip);
		if (!acceptignoreinfo.HasCurrentElement())
			return true;

		inf = acceptignoreinfo.GetCurrentElement();
		if (!inf->all) // ignore the ports in the list
		{
			std::list<uint16_t>::const_iterator it,begin,end;

			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == srcport)
					return false;
			}
			return true;
		}
		else // ignore all, except the ones in the list
		{
			std::list<uint16_t>::const_iterator it,begin,end;

			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == srcport)
					return true;
			}
			return false;
		}
	}
	return true;
}

int RTPUDPv4Transmitter::CreateLocalIPList()
{
	 // first try to obtain the list from the network interface info

	if (!GetLocalIPList_Interfaces())
	{
		// If this fails, we'll have to depend on DNS info
		GetLocalIPList_DNS();
	}
	AddLoopbackAddress();
	return 0;
}

#ifdef RTP_SOCKETTYPE_WINSOCK

bool RTPUDPv4Transmitter::GetLocalIPList_Interfaces()
{
	unsigned char buffer[RTPUDPV4TRANS_IFREQBUFSIZE];
	DWORD outputsize;
	DWORD numaddresses,i;
	SOCKET_ADDRESS_LIST *addrlist;

	if (WSAIoctl(rtpsock,SIO_ADDRESS_LIST_QUERY,NULL,0,&buffer,RTPUDPV4TRANS_IFREQBUFSIZE,&outputsize,NULL,NULL))
		return false;

	addrlist = (SOCKET_ADDRESS_LIST *)buffer;
	numaddresses = addrlist->iAddressCount;
	for (i = 0 ; i < numaddresses ; i++)
	{
		SOCKET_ADDRESS *sockaddr = &(addrlist->Address[i]);
		if (sockaddr->iSockaddrLength == sizeof(struct sockaddr_in)) // IPv4 address
		{
			struct sockaddr_in *addr = (struct sockaddr_in *)sockaddr->lpSockaddr;

			localIPs.push_back(ntohl(addr->sin_addr.s_addr));
		}
	}

	if (localIPs.empty())
		return false;

	return true;
}

#else // use either getifaddrs or ioctl

#ifdef RTP_SUPPORT_IFADDRS

bool RTPUDPv4Transmitter::GetLocalIPList_Interfaces()
{
	struct ifaddrs *addrs,*tmp;

	getifaddrs(&addrs);
	tmp = addrs;

	while (tmp != 0)
	{
		if (tmp->ifa_addr != 0 && tmp->ifa_addr->sa_family == AF_INET)
		{
			struct sockaddr_in *inaddr = (struct sockaddr_in *)tmp->ifa_addr;
			localIPs.push_back(ntohl(inaddr->sin_addr.s_addr));
		}
		tmp = tmp->ifa_next;
	}

	freeifaddrs(addrs);

	if (localIPs.empty())
		return false;
	return true;
}

#else // user ioctl

bool RTPUDPv4Transmitter::GetLocalIPList_Interfaces()
{
	int status;
	char buffer[RTPUDPV4TRANS_IFREQBUFSIZE];
	struct ifconf ifc;
	struct ifreq *ifr;
	struct sockaddr *sa;
	char *startptr,*endptr;
	int remlen;

	ifc.ifc_len = RTPUDPV4TRANS_IFREQBUFSIZE;
	ifc.ifc_buf = buffer;
	status = ioctl(rtpsock,SIOCGIFCONF,&ifc);
	if (status < 0)
		return false;

	startptr = (char *)ifc.ifc_req;
	endptr = startptr + ifc.ifc_len;
	remlen = ifc.ifc_len;
	while((startptr < endptr) && remlen >= (int)sizeof(struct ifreq))
	{
		ifr = (struct ifreq *)startptr;
		sa = &(ifr->ifr_addr);
#ifdef RTP_HAVE_SOCKADDR_LEN
		if (sa->sa_len <= sizeof(struct sockaddr))
		{
			if (sa->sa_len == sizeof(struct sockaddr_in) && sa->sa_family == PF_INET)
			{
				uint32_t ip;
				struct sockaddr_in *addr = (struct sockaddr_in *)sa;

				ip = ntohl(addr->sin_addr.s_addr);
				localIPs.push_back(ip);
			}
			remlen -= sizeof(struct ifreq);
			startptr += sizeof(struct ifreq);
		}
		else
		{
			int l = sa->sa_len-sizeof(struct sockaddr)+sizeof(struct ifreq);

			remlen -= l;
			startptr += l;
		}
#else // don't have sa_len in struct sockaddr
		if (sa->sa_family == PF_INET)
		{
			uint32_t ip;
			struct sockaddr_in *addr = (struct sockaddr_in *)sa;

			ip = ntohl(addr->sin_addr.s_addr);
			localIPs.push_back(ip);
		}
		remlen -= sizeof(struct ifreq);
		startptr += sizeof(struct ifreq);

#endif // RTP_HAVE_SOCKADDR_LEN
	}

	if (localIPs.empty())
		return false;
	return true;
}

#endif // RTP_SUPPORT_IFADDRS

#endif // RTP_SOCKETTYPE_WINSOCK

void RTPUDPv4Transmitter::GetLocalIPList_DNS()
{
	struct hostent *he;
	char name[1024];
	bool done;
	int i,j;

	gethostname(name,1023);
	name[1023] = 0;
	he = gethostbyname(name);
	if (he == 0)
		return;

	i = 0;
	done = false;
	while (!done)
	{
		if (he->h_addr_list[i] == NULL)
			done = true;
		else
		{
			uint32_t ip = 0;

			for (j = 0 ; j < 4 ; j++)
				ip |= ((uint32_t)((unsigned char)he->h_addr_list[i][j])<<((3-j)*8));
			localIPs.push_back(ip);
			i++;
		}
	}
}

void RTPUDPv4Transmitter::AddLoopbackAddress()
{
	uint32_t loopbackaddr = (((uint32_t)127)<<24)|((uint32_t)1);
	std::list<uint32_t>::const_iterator it;
	bool found = false;

	for (it = localIPs.begin() ; !found && it != localIPs.end() ; it++)
	{
		if (*it == loopbackaddr)
			found = true;
	}

	if (!found)
		localIPs.push_back(loopbackaddr);
}

} // end namespace

