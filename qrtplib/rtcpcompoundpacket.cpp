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

#include "rtcpcompoundpacket.h"
#include "rtprawpacket.h"
#include "rtperrors.h"
#include "rtpstructs.h"
#include "rtpdefines.h"
#include "rtcpsrpacket.h"
#include "rtcprrpacket.h"
#include "rtcpsdespacket.h"
#include "rtcpbyepacket.h"
#include "rtcpapppacket.h"
#include "rtcpunknownpacket.h"

namespace qrtplib
{

RTCPCompoundPacket::RTCPCompoundPacket(RTPRawPacket &rawpack)
{
    compoundpacket = 0;
    compoundpacketlength = 0;
    error = 0;

    if (rawpack.IsRTP())
    {
        error = ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
        return;
    }

    uint8_t *data = rawpack.GetData();
    std::size_t datalen = rawpack.GetDataLength();

    error = ParseData(data, datalen);

    if (error < 0) {
        return;
    }

    compoundpacket = rawpack.GetData();
    compoundpacketlength = rawpack.GetDataLength();

    rtcppackit = rtcppacklist.begin();
}

RTCPCompoundPacket::RTCPCompoundPacket(uint8_t *packet, std::size_t packetlen)
{
    compoundpacket = 0;
    compoundpacketlength = 0;

    error = ParseData(packet, packetlen);

    if (error < 0) {
        return;
    }

    compoundpacket = packet;
    compoundpacketlength = packetlen;

    rtcppackit = rtcppacklist.begin();
}

RTCPCompoundPacket::RTCPCompoundPacket()
{
    compoundpacket = 0;
    compoundpacketlength = 0;
    error = 0;
}

int RTCPCompoundPacket::ParseData(uint8_t *data, std::size_t datalen)
{
    bool first;

    if (datalen < sizeof(RTCPCommonHeader))
        return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;

    first = true;

    do
    {
        RTCPCommonHeader *rtcphdr;
        std::size_t length;

        rtcphdr = (RTCPCommonHeader *) data;
        if (rtcphdr->version != RTP_VERSION) // check version
        {
            ClearPacketList();
            return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
        }
        if (first)
        {
            // Check if first packet is SR or RR

            first = false;
            if (!(rtcphdr->packettype == RTP_RTCPTYPE_SR || rtcphdr->packettype == RTP_RTCPTYPE_RR))
            {
                ClearPacketList();
                return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
            }
        }

        length = (std::size_t) m_endian.qToHost(rtcphdr->length);
        length++;
        length *= sizeof(uint32_t);

        if (length > datalen) // invalid length field
        {
            ClearPacketList();
            return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
        }

        if (rtcphdr->padding)
        {
            // check if it's the last packet
            if (length != datalen)
            {
                ClearPacketList();
                return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
            }
        }

        RTCPPacket *p;

        switch (rtcphdr->packettype)
        {
        case RTP_RTCPTYPE_SR:
            p = new RTCPSRPacket(data, length);
            break;
        case RTP_RTCPTYPE_RR:
            p = new RTCPRRPacket(data, length);
            break;
        case RTP_RTCPTYPE_SDES:
            p = new RTCPSDESPacket(data, length);
            break;
        case RTP_RTCPTYPE_BYE:
            p = new RTCPBYEPacket(data, length);
            break;
        case RTP_RTCPTYPE_APP:
            p = new RTCPAPPPacket(data, length);
            break;
        default:
            p = new RTCPUnknownPacket(data, length);
        }

        rtcppacklist.push_back(p);

        datalen -= length;
        data += length;
    } while (datalen >= (std::size_t) sizeof(RTCPCommonHeader));

    if (datalen != 0) // some remaining bytes
    {
        ClearPacketList();
        return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
    }
    return 0;
}

RTCPCompoundPacket::~RTCPCompoundPacket()
{
    ClearPacketList();
}

void RTCPCompoundPacket::ClearPacketList()
{
    std::list<RTCPPacket *>::const_iterator it;

    for (it = rtcppacklist.begin(); it != rtcppacklist.end(); it++) {
        delete *it;
    }

    rtcppacklist.clear();
    rtcppackit = rtcppacklist.begin();
}

} // end namespace

