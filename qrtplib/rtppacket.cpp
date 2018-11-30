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

#include "rtppacket.h"
#include "rtpstructs.h"
#include "rtpdefines.h"
#include "rtperrors.h"
#include "rtprawpacket.h"
#include <string.h>

namespace qrtplib
{

void RTPPacket::Clear()
{
    hasextension = false;
    hasmarker = false;
    numcsrcs = 0;
    payloadtype = 0;
    extseqnr = 0;
    timestamp = 0;
    ssrc = 0;
    packet = 0;
    payload = 0;
    packetlength = 0;
    payloadlength = 0;
    extid = 0;
    extension = 0;
    extensionlength = 0;
    error = 0;
    externalbuffer = false;
}

RTPPacket::RTPPacket(RTPRawPacket &rawpack) :
        receivetime(rawpack.GetReceiveTime())
{
    Clear();
    error = ParseRawPacket(rawpack);
}

RTPPacket::RTPPacket(
        uint8_t payloadtype,
        const void *payloaddata,
        unsigned int payloadlen,
        uint16_t seqnr,
        uint32_t timestamp,
        uint32_t ssrc,
        bool gotmarker,
        uint8_t numcsrcs,
        const uint32_t *csrcs,
        bool gotextension,
        uint16_t extensionid,
        uint16_t extensionlen_numwords,
        const void *extensiondata,
        unsigned int maxpacksize) :
    receivetime(0, 0)
{
    Clear();
    error = BuildPacket(
                payloadtype,
                payloaddata,
                payloadlen,
                seqnr,
                timestamp,
                ssrc,
                gotmarker,
                numcsrcs,
                csrcs,
                gotextension,
                extensionid,
                extensionlen_numwords,
                extensiondata,
                0,
                maxpacksize);
}

RTPPacket::RTPPacket(
        uint8_t payloadtype,
        const void *payloaddata,
        unsigned int payloadlen,
        uint16_t seqnr,
        uint32_t timestamp,
        uint32_t ssrc,
        bool gotmarker,
        uint8_t numcsrcs,
        const uint32_t *csrcs,
        bool gotextension,
        uint16_t extensionid,
        uint16_t extensionlen_numwords,
        const void *extensiondata,
        void *buffer,
        unsigned int buffersize) :
    receivetime(0, 0)
{
    Clear();
    if (buffer == 0)
        error = ERR_RTP_PACKET_EXTERNALBUFFERNULL;
    else if (buffersize <= 0)
        error = ERR_RTP_PACKET_ILLEGALBUFFERSIZE;
    else
        error = BuildPacket(
                    payloadtype,
                    payloaddata,
                    payloadlen,
                    seqnr,
                    timestamp,
                    ssrc,
                    gotmarker,
                    numcsrcs,
                    csrcs,
                    gotextension,
                    extensionid,
                    extensionlen_numwords,
                    extensiondata,
                    buffer,
                    buffersize);
}

int RTPPacket::ParseRawPacket(RTPRawPacket &rawpack)
{
    uint8_t *packetbytes;
    unsigned int packetlen;
    uint8_t payloadtype;
    RTPHeader *rtpheader;
    bool marker;
    int csrccount;
    bool hasextension;
    int payloadoffset, payloadlength;
    int numpadbytes;
    RTPExtensionHeader *rtpextheader;

    if (!rawpack.IsRTP()) // If we didn't receive it on the RTP port, we'll ignore it
        return ERR_RTP_PACKET_INVALIDPACKET;

    // The length should be at least the size of the RTP header
    packetlen = rawpack.GetDataLength();
    if (packetlen < sizeof(RTPHeader))
        return ERR_RTP_PACKET_INVALIDPACKET;

    packetbytes = (uint8_t *) rawpack.GetData();
    rtpheader = (RTPHeader *) packetbytes;

    // The version number should be correct
    if (rtpheader->version != RTP_VERSION)
        return ERR_RTP_PACKET_INVALIDPACKET;

    // We'll check if this is possibly a RTCP packet. For this to be possible
    // the marker bit and payload type combined should be either an SR or RR
    // identifier
    marker = (rtpheader->marker == 0) ? false : true;
    payloadtype = rtpheader->payloadtype;
    if (marker)
    {
        if (payloadtype == (RTP_RTCPTYPE_SR & 127)) // don't check high bit (this was the marker!!)
            return ERR_RTP_PACKET_INVALIDPACKET;
        if (payloadtype == (RTP_RTCPTYPE_RR & 127))
            return ERR_RTP_PACKET_INVALIDPACKET;
    }

    csrccount = rtpheader->csrccount;
    payloadoffset = sizeof(RTPHeader) + (int) (csrccount * sizeof(uint32_t));

    if (rtpheader->padding) // adjust payload length to take padding into account
    {
        numpadbytes = (int) packetbytes[packetlen - 1]; // last byte contains number of padding bytes
        if (numpadbytes <= 0)
            return ERR_RTP_PACKET_INVALIDPACKET;
    }
    else
        numpadbytes = 0;

    hasextension = (rtpheader->extension == 0) ? false : true;
    if (hasextension) // got header extension
    {
        rtpextheader = (RTPExtensionHeader *) (packetbytes + payloadoffset);
        payloadoffset += sizeof(RTPExtensionHeader);

        uint16_t exthdrlen = m_endian.qToHost(rtpextheader->length);
        payloadoffset += ((int) exthdrlen) * sizeof(uint32_t);
    }
    else
    {
        rtpextheader = 0;
    }

    payloadlength = packetlen - numpadbytes - payloadoffset;
    if (payloadlength < 0)
        return ERR_RTP_PACKET_INVALIDPACKET;

    // Now, we've got a valid packet, so we can create a new instance of RTPPacket
    // and fill in the members

    RTPPacket::hasextension = hasextension;
    if (hasextension)
    {
        RTPPacket::extid = m_endian.qToHost(rtpextheader->extid);
        RTPPacket::extensionlength = ((int) m_endian.qToHost(rtpextheader->length)) * sizeof(uint32_t);
        RTPPacket::extension = ((uint8_t *) rtpextheader) + sizeof(RTPExtensionHeader);
    }

    RTPPacket::hasmarker = marker;
    RTPPacket::numcsrcs = csrccount;
    RTPPacket::payloadtype = payloadtype;

    // Note: we don't fill in the EXTENDED sequence number here, since we
    // don't have information about the source here. We just fill in the low
    // 16 bits
    RTPPacket::extseqnr = (uint32_t) m_endian.qToHost(rtpheader->sequencenumber);

    RTPPacket::timestamp = m_endian.qToHost(rtpheader->timestamp);
    RTPPacket::ssrc = m_endian.qToHost(rtpheader->ssrc);
    RTPPacket::packet = packetbytes;
    RTPPacket::payload = packetbytes + payloadoffset;
    RTPPacket::packetlength = packetlen;
    RTPPacket::payloadlength = payloadlength;

    return 0;
}

uint32_t RTPPacket::GetCSRC(int num) const
{
    if (num >= numcsrcs)
        return 0;

    uint8_t *csrcpos;
    uint32_t *csrcval_nbo;
    uint32_t csrcval_hbo;

    csrcpos = packet + sizeof(RTPHeader) + num * sizeof(uint32_t);
    csrcval_nbo = (uint32_t *) csrcpos;
    csrcval_hbo = m_endian.qToHost(*csrcval_nbo);
    return csrcval_hbo;
}

int RTPPacket::BuildPacket(
        uint8_t payloadtype,
        const void *payloaddata,
        unsigned int payloadlen,
        uint16_t seqnr,
        uint32_t timestamp,
        uint32_t ssrc,
        bool gotmarker,
        uint8_t numcsrcs,
        const uint32_t *csrcs,
        bool gotextension,
        uint16_t extensionid,
        uint16_t extensionlen_numwords,
        const void *extensiondata,
        void *buffer,
        unsigned int maxsize)
{
    if (numcsrcs > RTP_MAXCSRCS)
        return ERR_RTP_PACKET_TOOMANYCSRCS;

    if (payloadtype > 127) // high bit should not be used
        return ERR_RTP_PACKET_BADPAYLOADTYPE;
    if (payloadtype == 72 || payloadtype == 73) // could cause confusion with rtcp types
        return ERR_RTP_PACKET_BADPAYLOADTYPE;

    packetlength = sizeof(RTPHeader);
    packetlength += sizeof(uint32_t) * ((unsigned int) numcsrcs);
    if (gotextension)
    {
        packetlength += sizeof(RTPExtensionHeader);
        packetlength += sizeof(uint32_t) * ((unsigned int) extensionlen_numwords);
    }
    packetlength += payloadlen;

    if (maxsize > 0 && packetlength > maxsize)
    {
        packetlength = 0;
        return ERR_RTP_PACKET_DATAEXCEEDSMAXSIZE;
    }

    // Ok, now we'll just fill in...

    RTPHeader *rtphdr;

    if (buffer == 0)
    {
        packet = new uint8_t[packetlength];
        externalbuffer = false;
    }
    else
    {
        packet = (uint8_t *) buffer;
        externalbuffer = true;
    }

    RTPPacket::hasmarker = gotmarker;
    RTPPacket::hasextension = gotextension;
    RTPPacket::numcsrcs = numcsrcs;
    RTPPacket::payloadtype = payloadtype;
    RTPPacket::extseqnr = (uint32_t) seqnr;
    RTPPacket::timestamp = timestamp;
    RTPPacket::ssrc = ssrc;
    RTPPacket::payloadlength = payloadlen;
    RTPPacket::extid = extensionid;
    RTPPacket::extensionlength = ((unsigned int) extensionlen_numwords) * sizeof(uint32_t);

    rtphdr = (RTPHeader *) packet;
    rtphdr->version = RTP_VERSION;
    rtphdr->padding = 0;
    if (gotmarker)
        rtphdr->marker = 1;
    else
        rtphdr->marker = 0;
    if (gotextension)
        rtphdr->extension = 1;
    else
        rtphdr->extension = 0;
    rtphdr->csrccount = numcsrcs;
    rtphdr->payloadtype = payloadtype & 127; // make sure high bit isn't set
    rtphdr->sequencenumber = qToBigEndian(seqnr);
    rtphdr->timestamp = qToBigEndian(timestamp);
    rtphdr->ssrc = qToBigEndian(ssrc);

    uint32_t *curcsrc;
    int i;

    curcsrc = (uint32_t *) (packet + sizeof(RTPHeader));
    for (i = 0; i < numcsrcs; i++, curcsrc++)
        *curcsrc = qToBigEndian(csrcs[i]);

    payload = packet + sizeof(RTPHeader) + ((unsigned int) numcsrcs) * sizeof(uint32_t);
    if (gotextension)
    {
        RTPExtensionHeader *rtpexthdr = (RTPExtensionHeader *) payload;

        rtpexthdr->extid = qToBigEndian(extensionid);
        rtpexthdr->length = qToBigEndian((uint16_t) extensionlen_numwords);

        payload += sizeof(RTPExtensionHeader);
        memcpy(payload, extensiondata, RTPPacket::extensionlength);

        payload += RTPPacket::extensionlength;
    }
    memcpy(payload, payloaddata, payloadlen);
    return 0;
}

} // end namespace

