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

#include "rtpexternaltransmitter.h"
#include "rtprawpacket.h"
#include "rtptimeutilities.h"
#include "rtpdefines.h"
#include "rtperrors.h"
#include "rtpsocketutilinternal.h"
#include "rtpselect.h"
#include <stdio.h>
#include <string.h>

#include <iostream>

#define MAINMUTEX_LOCK
#define MAINMUTEX_UNLOCK
#define WAITMUTEX_LOCK
#define WAITMUTEX_UNLOCK

namespace qrtplib
{

RTPExternalTransmitter::RTPExternalTransmitter() :
        packetinjector((RTPExternalTransmitter *) this)
{
    created = false;
    init = false;
}

RTPExternalTransmitter::~RTPExternalTransmitter()
{
    Destroy();
}

int RTPExternalTransmitter::Init(bool tsafe)
{
    if (init)
        return ERR_RTP_EXTERNALTRANS_ALREADYINIT;

    if (tsafe)
        return ERR_RTP_NOTHREADSUPPORT;

    init = true;
    return 0;
}

int RTPExternalTransmitter::Create(size_t maximumpacketsize, const RTPTransmissionParams *transparams)
{
    const RTPExternalTransmissionParams *params;
    int status;

    if (!init)
        return ERR_RTP_EXTERNALTRANS_NOTINIT;

    MAINMUTEX_LOCK

    if (created)
    {
        MAINMUTEX_UNLOCK
        return ERR_RTP_EXTERNALTRANS_ALREADYCREATED;
    }

    // Obtain transmission parameters

    if (transparams == 0)
    {
        MAINMUTEX_UNLOCK
        return ERR_RTP_EXTERNALTRANS_ILLEGALPARAMETERS;
    }
    if (transparams->GetTransmissionProtocol() != RTPTransmitter::ExternalProto)
    {
        MAINMUTEX_UNLOCK
        return ERR_RTP_EXTERNALTRANS_ILLEGALPARAMETERS;
    }

    params = (const RTPExternalTransmissionParams *) transparams;

    if ((status = m_abortDesc.Init()) < 0)
    {
        MAINMUTEX_UNLOCK
        return status;
    }
    m_abortCount = 0;

    maxpacksize = maximumpacketsize;
    sender = params->GetSender();
    headersize = params->GetAdditionalHeaderSize();

    localhostname = 0;
    localhostnamelength = 0;

    waitingfordata = false;
    created = true;
    MAINMUTEX_UNLOCK
    return 0;
}

void RTPExternalTransmitter::Destroy()
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
        delete[] localhostname;
        localhostname = 0;
        localhostnamelength = 0;
    }

    FlushPackets();
    created = false;

    if (waitingfordata)
    {
        m_abortDesc.SendAbortSignal();
        m_abortCount++;
        m_abortDesc.Destroy();
    MAINMUTEX_UNLOCK
    WAITMUTEX_LOCK // to make sure that the WaitForIncomingData function ended
    WAITMUTEX_UNLOCK
}
else
    m_abortDesc.Destroy();

MAINMUTEX_UNLOCK
}

RTPTransmissionInfo *RTPExternalTransmitter::GetTransmissionInfo()
{
if (!init)
return 0;

MAINMUTEX_LOCK
RTPTransmissionInfo *tinf = new RTPExternalTransmissionInfo(&packetinjector);
MAINMUTEX_UNLOCK
return tinf;
}

void RTPExternalTransmitter::DeleteTransmissionInfo(RTPTransmissionInfo *i)
{
if (!init)
return;

delete i;
}

int RTPExternalTransmitter::GetLocalHostName(uint8_t *buffer, size_t *bufferlength)
{
if (!init)
return ERR_RTP_EXTERNALTRANS_NOTINIT;

MAINMUTEX_LOCK
if (!created)
{
MAINMUTEX_UNLOCK
return ERR_RTP_EXTERNALTRANS_NOTCREATED;
}

if (localhostname == 0)
{
// We'll just use 'gethostname' for simplicity

char name[1024];

if (gethostname(name, 1023) != 0)
    strcpy(name, "localhost"); // failsafe
else
    name[1023] = 0; // ensure null-termination

localhostnamelength = strlen(name);
localhostname = new uint8_t[localhostnamelength + 1];

memcpy(localhostname, name, localhostnamelength);
localhostname[localhostnamelength] = 0;
}

if ((*bufferlength) < localhostnamelength)
{
*bufferlength = localhostnamelength; // tell the application the required size of the buffer
MAINMUTEX_UNLOCK
return ERR_RTP_TRANS_BUFFERLENGTHTOOSMALL;
}

memcpy(buffer, localhostname, localhostnamelength);
*bufferlength = localhostnamelength;

MAINMUTEX_UNLOCK
return 0;
}

bool RTPExternalTransmitter::ComesFromThisTransmitter(const RTPAddress *addr)
{
MAINMUTEX_LOCK
bool value = false;
if (sender)
value = sender->ComesFromThisSender(addr);
MAINMUTEX_UNLOCK
return value;
}

int RTPExternalTransmitter::Poll()
{
return 0;
}

int RTPExternalTransmitter::WaitForIncomingData(const RTPTime &delay, bool *dataavailable)
{
if (!init)
return ERR_RTP_EXTERNALTRANS_NOTINIT;

MAINMUTEX_LOCK

if (!created)
{
MAINMUTEX_UNLOCK
return ERR_RTP_EXTERNALTRANS_NOTCREATED;
}
if (waitingfordata)
{
MAINMUTEX_UNLOCK
return ERR_RTP_EXTERNALTRANS_ALREADYWAITING;
}

waitingfordata = true;

if (!rawpacketlist.empty())
{
if (dataavailable != 0)
    *dataavailable = true;
waitingfordata = false;
MAINMUTEX_UNLOCK
return 0;
}

WAITMUTEX_LOCK
MAINMUTEX_UNLOCK

int8_t isset = 0;
SocketType abortSock = m_abortDesc.GetAbortSocket();
int status = RTPSelect(&abortSock, &isset, 1, delay);
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
if (isset)
{
m_abortDesc.ClearAbortSignal();
m_abortCount = 0;
}

if (dataavailable != 0)
{
if (rawpacketlist.empty())
    *dataavailable = false;
else
    *dataavailable = true;
}

MAINMUTEX_UNLOCK
WAITMUTEX_UNLOCK
return 0;
}

int RTPExternalTransmitter::AbortWait()
{
if (!init)
return ERR_RTP_EXTERNALTRANS_NOTINIT;

MAINMUTEX_LOCK
if (!created)
{
MAINMUTEX_UNLOCK
return ERR_RTP_EXTERNALTRANS_NOTCREATED;
}
if (!waitingfordata)
{
MAINMUTEX_UNLOCK
return ERR_RTP_EXTERNALTRANS_NOTWAITING;
}

m_abortDesc.SendAbortSignal();
m_abortCount++;

MAINMUTEX_UNLOCK
return 0;
}

int RTPExternalTransmitter::SendRTPData(const void *data, size_t len)
{
if (!init)
return ERR_RTP_EXTERNALTRANS_NOTINIT;

MAINMUTEX_LOCK

if (!created)
{
MAINMUTEX_UNLOCK
return ERR_RTP_EXTERNALTRANS_NOTCREATED;
}
if (len > maxpacksize)
{
MAINMUTEX_UNLOCK
return ERR_RTP_EXTERNALTRANS_SPECIFIEDSIZETOOBIG;
}

if (!sender)
{
MAINMUTEX_UNLOCK
return ERR_RTP_EXTERNALTRANS_NOSENDER;
}

MAINMUTEX_UNLOCK

if (!sender->SendRTP(data, len))
return ERR_RTP_EXTERNALTRANS_SENDERROR;

return 0;
}

int RTPExternalTransmitter::SendRTCPData(const void *data, size_t len)
{
if (!init)
return ERR_RTP_EXTERNALTRANS_NOTINIT;

MAINMUTEX_LOCK

if (!created)
{
MAINMUTEX_UNLOCK
return ERR_RTP_EXTERNALTRANS_NOTCREATED;
}
if (len > maxpacksize)
{
MAINMUTEX_UNLOCK
return ERR_RTP_EXTERNALTRANS_SPECIFIEDSIZETOOBIG;
}

if (!sender)
{
MAINMUTEX_UNLOCK
return ERR_RTP_EXTERNALTRANS_NOSENDER;
}
MAINMUTEX_UNLOCK

if (!sender->SendRTCP(data, len))
return ERR_RTP_EXTERNALTRANS_SENDERROR;

return 0;
}

int RTPExternalTransmitter::AddDestination(const RTPAddress &)
{
return ERR_RTP_EXTERNALTRANS_NODESTINATIONSSUPPORTED;
}

int RTPExternalTransmitter::DeleteDestination(const RTPAddress &)
{
return ERR_RTP_EXTERNALTRANS_NODESTINATIONSSUPPORTED;
}

void RTPExternalTransmitter::ClearDestinations()
{
}

bool RTPExternalTransmitter::SupportsMulticasting()
{
return false;
}

int RTPExternalTransmitter::JoinMulticastGroup(const RTPAddress &)
{
return ERR_RTP_EXTERNALTRANS_NOMULTICASTSUPPORT;
}

int RTPExternalTransmitter::LeaveMulticastGroup(const RTPAddress &)
{
return ERR_RTP_EXTERNALTRANS_NOMULTICASTSUPPORT;
}

void RTPExternalTransmitter::LeaveAllMulticastGroups()
{
}

int RTPExternalTransmitter::SetReceiveMode(RTPTransmitter::ReceiveMode m)
{
if (!init)
return ERR_RTP_EXTERNALTRANS_NOTINIT;

MAINMUTEX_LOCK
if (!created)
{
MAINMUTEX_UNLOCK
return ERR_RTP_EXTERNALTRANS_NOTCREATED;
}
if (m != RTPTransmitter::AcceptAll)
{
MAINMUTEX_UNLOCK
return ERR_RTP_EXTERNALTRANS_BADRECEIVEMODE;
}
MAINMUTEX_UNLOCK
return 0;
}

int RTPExternalTransmitter::AddToIgnoreList(const RTPAddress &)
{
return ERR_RTP_EXTERNALTRANS_NOIGNORELIST;
}

int RTPExternalTransmitter::DeleteFromIgnoreList(const RTPAddress &)
{
return ERR_RTP_EXTERNALTRANS_NOIGNORELIST;
}

void RTPExternalTransmitter::ClearIgnoreList()
{
}

int RTPExternalTransmitter::AddToAcceptList(const RTPAddress &)
{
return ERR_RTP_EXTERNALTRANS_NOACCEPTLIST;
}

int RTPExternalTransmitter::DeleteFromAcceptList(const RTPAddress &)
{
return ERR_RTP_EXTERNALTRANS_NOACCEPTLIST;
}

void RTPExternalTransmitter::ClearAcceptList()
{
}

int RTPExternalTransmitter::SetMaximumPacketSize(size_t s)
{
if (!init)
return ERR_RTP_EXTERNALTRANS_NOTINIT;

MAINMUTEX_LOCK
if (!created)
{
MAINMUTEX_UNLOCK
return ERR_RTP_EXTERNALTRANS_NOTCREATED;
}
maxpacksize = s;
MAINMUTEX_UNLOCK
return 0;
}

bool RTPExternalTransmitter::NewDataAvailable()
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

RTPRawPacket *RTPExternalTransmitter::GetNextPacket()
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

void RTPExternalTransmitter::FlushPackets()
{
std::list<RTPRawPacket*>::const_iterator it;

for (it = rawpacketlist.begin(); it != rawpacketlist.end(); ++it)
delete *it;
rawpacketlist.clear();
}

void RTPExternalTransmitter::InjectRTP(const void *data, size_t len, const RTPAddress &a)
{
if (!init)
return;

MAINMUTEX_LOCK
if (!created)
{
MAINMUTEX_UNLOCK
return;
}

RTPAddress *addr = a.CreateCopy();
if (addr == 0)
return;

uint8_t *datacopy;

datacopy = new uint8_t[len];
if (datacopy == 0)
{
delete addr;
return;
}
memcpy(datacopy, data, len);

RTPTime curtime = RTPTime::CurrentTime();
RTPRawPacket *pack;

pack = new RTPRawPacket(datacopy, len, addr, curtime, true);
if (pack == 0)
{
delete addr;
delete[] localhostname;
return;
}
rawpacketlist.push_back(pack);

if (m_abortCount == 0)
{
m_abortDesc.SendAbortSignal();
m_abortCount++;
}

MAINMUTEX_UNLOCK
}

void RTPExternalTransmitter::InjectRTCP(const void *data, size_t len, const RTPAddress &a)
{
if (!init)
return;

MAINMUTEX_LOCK
if (!created)
{
MAINMUTEX_UNLOCK
return;
}

RTPAddress *addr = a.CreateCopy();
if (addr == 0)
return;

uint8_t *datacopy;

datacopy = new uint8_t[len];
if (datacopy == 0)
{
delete addr;
return;
}
memcpy(datacopy, data, len);

RTPTime curtime = RTPTime::CurrentTime();
RTPRawPacket *pack;

pack = new RTPRawPacket(datacopy, len, addr, curtime, false);
if (pack == 0)
{
delete addr;
delete[] localhostname;
return;
}
rawpacketlist.push_back(pack);

if (m_abortCount == 0)
{
m_abortDesc.SendAbortSignal();
m_abortCount++;
}

MAINMUTEX_UNLOCK
}

void RTPExternalTransmitter::InjectRTPorRTCP(const void *data, size_t len, const RTPAddress &a)
{
if (!init)
return;

MAINMUTEX_LOCK
if (!created)
{
MAINMUTEX_UNLOCK
return;
}

RTPAddress *addr = a.CreateCopy();
if (addr == 0)
return;

uint8_t *datacopy;
bool rtp = true;

if (len >= 2)
{
const uint8_t *pData = (const uint8_t *) data;
if (pData[1] >= 200 && pData[1] <= 204)
rtp = false;
}

datacopy = new uint8_t[len];
if (datacopy == 0)
{
delete addr;
return;
}
memcpy(datacopy, data, len);

RTPTime curtime = RTPTime::CurrentTime();
RTPRawPacket *pack;

pack = new RTPRawPacket(datacopy, len, addr, curtime, rtp);
if (pack == 0)
{
delete addr;
delete[] localhostname;
return;
}
rawpacketlist.push_back(pack);

if (m_abortCount == 0)
{
m_abortDesc.SendAbortSignal();
m_abortCount++;
}

MAINMUTEX_UNLOCK

}

} // end namespace

