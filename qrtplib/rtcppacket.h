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
 * \file rtcppacket.h
 */

#ifndef RTCPPACKET_H

#define RTCPPACKET_H

#include "rtpconfig.h"
#include "rtptypes.h"
#include <cstddef>

namespace qrtplib
{

class RTCPCompoundPacket;

/** Base class for specific types of RTCP packets. */
class RTCPPacket
{
public:
    /** Identifies the specific kind of RTCP packet. */
    enum PacketType
    {
        SR, /**< An RTCP sender report. */
        RR, /**< An RTCP receiver report. */
        SDES, /**< An RTCP source description packet. */
        BYE, /**< An RTCP bye packet. */
        APP, /**< An RTCP packet containing application specific data. */
        Unknown /**< The type of RTCP packet was not recognized. */
    };
protected:
    RTCPPacket(PacketType t, uint8_t *d, std::size_t dlen) :
            data(d), datalen(dlen), packettype(t)
    {
        knownformat = false;
    }
public:
    virtual ~RTCPPacket()
    {
    }

    /** Returns \c true if the subclass was able to interpret the data and \c false otherwise. */
    bool IsKnownFormat() const
    {
        return knownformat;
    }

    /** Returns the actual packet type which the subclass implements. */
    PacketType GetPacketType() const
    {
        return packettype;
    }

    /** Returns a pointer to the data of this RTCP packet. */
    uint8_t *GetPacketData()
    {
        return data;
    }

    /** Returns the length of this RTCP packet. */
    std::size_t GetPacketLength() const
    {
        return datalen;
    }

protected:
    uint8_t *data;
    std::size_t datalen;
    bool knownformat;
private:
    const PacketType packettype;
};

} // end namespace

#endif // RTCPPACKET_H

