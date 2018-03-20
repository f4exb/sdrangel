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
 * \file rtcpcompoundpacket.h
 */

#ifndef RTCPCOMPOUNDPACKET_H

#define RTCPCOMPOUNDPACKET_H

#include "rtpconfig.h"
#include "rtptypes.h"
#include "rtpendian.h"
#include <list>

#include "export.h"

namespace qrtplib
{

class RTPRawPacket;
class RTCPPacket;

/** Represents an RTCP compound packet. */
class QRTPLIB_API RTCPCompoundPacket
{
public:
    /** Creates an RTCPCompoundPacket instance from the data in \c rawpack, installing a memory manager if specified. */
    RTCPCompoundPacket(RTPRawPacket &rawpack);

    /** Creates an RTCPCompoundPacket instance from the data in \c packet}, with size \c len. */
    RTCPCompoundPacket(uint8_t *packet, std::size_t len);
protected:
    RTCPCompoundPacket(); // this is for the compoundpacket builder
public:
    virtual ~RTCPCompoundPacket();

    /** Checks if the RTCP compound packet was created successfully.
     *  If the raw packet data in the constructor could not be parsed, this function returns the error code of
     *  what went wrong. If the packet had an invalid format, the return value is \c ERR_RTP_RTCPCOMPOUND_INVALIDPACKET.
     */
    int GetCreationError()
    {
        return error;
    }

    /** Returns a pointer to the data of the entire RTCP compound packet. */
    uint8_t *GetCompoundPacketData()
    {
        return compoundpacket;
    }

    /** Returns the size of the entire RTCP compound packet. */
    std::size_t GetCompoundPacketLength()
    {
        return compoundpacketlength;
    }

    /** Starts the iteration over the individual RTCP packets in the RTCP compound packet. */
    void GotoFirstPacket()
    {
        rtcppackit = rtcppacklist.begin();
    }

    /** Returns a pointer to the next individual RTCP packet.
     *  Returns a pointer to the next individual RTCP packet. Note that no \c delete call may be done
     *  on the RTCPPacket instance which is returned.
     */
    RTCPPacket *GetNextPacket()
    {
        if (rtcppackit == rtcppacklist.end())
            return 0;
        RTCPPacket *p = *rtcppackit;
        rtcppackit++;
        return p;
    }

protected:
    void ClearPacketList();
    int ParseData(uint8_t *packet, std::size_t len);

    RTPEndian m_endian;
    int error;

    uint8_t *compoundpacket;
    std::size_t compoundpacketlength;

    std::list<RTCPPacket *> rtcppacklist;
    std::list<RTCPPacket *>::const_iterator rtcppackit;
};

} // end namespace

#endif // RTCPCOMPOUNDPACKET_H

