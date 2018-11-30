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

#include "rtpinternalsourcedata.h"
#include "rtppacket.h"
#include <string.h>

#define RTPINTERNALSOURCEDATA_MAXPROBATIONPACKETS		32

namespace qrtplib
{

RTPInternalSourceData::RTPInternalSourceData(uint32_t ssrc) :
        RTPSourceData(ssrc)
{
}

RTPInternalSourceData::~RTPInternalSourceData()
{
}

// The following function should delete rtppack if necessary
int RTPInternalSourceData::ProcessRTPPacket(RTPPacket *rtppack, const RTPTime &receivetime, bool *stored, RTPSources *sources)
{
    bool accept;
    double tsunit;

    *stored = false;

    if (timestampunit < 0)
        tsunit = INF_GetEstimatedTimestampUnit();
    else
        tsunit = timestampunit;

    stats.ProcessPacket(rtppack, receivetime, tsunit, ownssrc, &accept);

    if (!accept)
        return 0;
    validated = true;

    if (validated && !ownssrc) // for own ssrc these variables depend on the outgoing packets, not on the incoming
        issender = true;

    bool isonprobation = !validated;
    bool ispackethandled = false;

    sources->OnValidatedRTPPacket(this, rtppack, isonprobation, &ispackethandled);
    if (ispackethandled) // Packet is already handled in the callback, no need to store it in the list
    {
        // Set 'stored' to true to avoid the packet being deallocated
        *stored = true;
        return 0;
    }

    // Now, we can place the packet in the queue

    if (packetlist.empty())
    {
        *stored = true;
        packetlist.push_back(rtppack);
        return 0;
    }

    if (!validated) // still on probation
    {
        // Make sure that we don't buffer too much packets to avoid wasting memory
        // on a bad source. Delete the packet in the queue with the lowest sequence
        // number.
        if (packetlist.size() == RTPINTERNALSOURCEDATA_MAXPROBATIONPACKETS)
        {
            RTPPacket *p = *(packetlist.begin());
            packetlist.pop_front();
            delete p;
        }
    }

    // find the right position to insert the packet

    std::list<RTPPacket*>::iterator it, start;
    bool done = false;
    uint32_t newseqnr = rtppack->GetExtendedSequenceNumber();

    it = packetlist.end();
    --it;
    start = packetlist.begin();

    while (!done)
    {
        RTPPacket *p;
        uint32_t seqnr;

        p = *it;
        seqnr = p->GetExtendedSequenceNumber();
        if (seqnr > newseqnr)
        {
            if (it != start)
                --it;
            else // we're at the start of the list
            {
                *stored = true;
                done = true;
                packetlist.push_front(rtppack);
            }
        }
        else if (seqnr < newseqnr) // insert after this packet
        {
            ++it;
            packetlist.insert(it, rtppack);
            done = true;
            *stored = true;
        }
        else // they're equal !! Drop packet
        {
            done = true;
        }
    }

    return 0;
}

int RTPInternalSourceData::ProcessSDESItem(uint8_t sdesid, const uint8_t *data, std::size_t itemlen, const RTPTime &receivetime, bool *cnamecollis)
{
    *cnamecollis = false;

    stats.SetLastMessageTime(receivetime);

    switch (sdesid)
    {
    case RTCP_SDES_ID_CNAME:
    {
        std::size_t curlen;
        uint8_t *oldcname;

        // NOTE: we're going to make sure that the CNAME is only set once.
        oldcname = SDESinf.GetCNAME(&curlen);
        if (curlen == 0)
        {
            // if CNAME is set, the source is validated
            SDESinf.SetCNAME(data, itemlen);
            validated = true;
        }
        else // check if this CNAME is equal to the one that is already present
        {
            if (curlen != itemlen)
                *cnamecollis = true;
            else
            {
                if (memcmp(data, oldcname, itemlen) != 0)
                    *cnamecollis = true;
            }
        }
    }
        break;
    case RTCP_SDES_ID_NAME:
    {
        std::size_t oldlen;

        SDESinf.GetName(&oldlen);
        if (oldlen == 0) // Name not set
            return SDESinf.SetName(data, itemlen);
    }
        break;
    case RTCP_SDES_ID_EMAIL:
    {
        std::size_t oldlen;

        SDESinf.GetEMail(&oldlen);
        if (oldlen == 0)
            return SDESinf.SetEMail(data, itemlen);
    }
        break;
    case RTCP_SDES_ID_PHONE:
        return SDESinf.SetPhone(data, itemlen);
    case RTCP_SDES_ID_LOCATION:
        return SDESinf.SetLocation(data, itemlen);
    case RTCP_SDES_ID_TOOL:
    {
        std::size_t oldlen;

        SDESinf.GetTool(&oldlen);
        if (oldlen == 0)
            return SDESinf.SetTool(data, itemlen);
    }
        break;
    case RTCP_SDES_ID_NOTE:
        stats.SetLastNoteTime(receivetime);
        return SDESinf.SetNote(data, itemlen);
    }
    return 0;
}

#ifdef RTP_SUPPORT_SDESPRIV

int RTPInternalSourceData::ProcessPrivateSDESItem(const uint8_t *prefix, std::size_t prefixlen, const uint8_t *value, std::size_t valuelen, const RTPTime &receivetime)
{
    int status;

    stats.SetLastMessageTime(receivetime);
    status = SDESinf.SetPrivateValue(prefix, prefixlen, value, valuelen);
    if (status == ERR_RTP_SDES_MAXPRIVITEMS)
        return 0; // don't stop processing just because the number of items is full
    return status;
}

#endif // RTP_SUPPORT_SDESPRIV

int RTPInternalSourceData::ProcessBYEPacket(const uint8_t *reason, std::size_t reasonlen, const RTPTime &receivetime)
{
    if (byereason)
    {
        delete[] byereason;
        byereason = 0;
        byereasonlen = 0;
    }

    byetime = receivetime;
    byereason = new uint8_t[reasonlen];
    memcpy(byereason, reason, reasonlen);
    byereasonlen = reasonlen;
    receivedbye = true;
    stats.SetLastMessageTime(receivetime);
    return 0;
}

} // end namespace

