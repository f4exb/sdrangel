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
 * \file rtcpapppacket.h
 */

#ifndef RTCPAPPPACKET_H

#define RTCPAPPPACKET_H

#include "rtpconfig.h"
#include "rtcppacket.h"
#include "rtpstructs.h"
#include "rtpendian.h"

#include "export.h"

namespace qrtplib
{

class RTCPCompoundPacket;

/** Describes an RTCP APP packet. */
class QRTPLIB_API RTCPAPPPacket: public RTCPPacket
{
public:
    /** Creates an instance based on the data in \c data with length \c datalen.
     *  Creates an instance based on the data in \c data with length \c datalen. Since the \c data pointer
     *  is referenced inside the class (no copy of the data is made) one must make sure that the memory it
     *  points to is valid as long as the class instance exists.
     */
    RTCPAPPPacket(uint8_t *data, std::size_t datalen);
    ~RTCPAPPPacket()
    {
    }

    /** Returns the subtype contained in the APP packet. */
    uint8_t GetSubType() const;

    /** Returns the SSRC of the source which sent this packet. */
    uint32_t GetSSRC() const;

    /** Returns the name contained in the APP packet.
     *  Returns the name contained in the APP packet. This alway consists of four bytes and is not NULL-terminated.
     */
    uint8_t *GetName();

    /** Returns a pointer to the actual data. */
    uint8_t *GetAPPData();

    /** Returns the length of the actual data. */
    std::size_t GetAPPDataLength() const;
private:
    RTPEndian m_endian;
    std::size_t appdatalen;
};

inline uint8_t RTCPAPPPacket::GetSubType() const
{
    if (!knownformat)
        return 0;
    RTCPCommonHeader *hdr = (RTCPCommonHeader *) data;
    return hdr->count;
}

inline uint32_t RTCPAPPPacket::GetSSRC() const
{
    if (!knownformat)
        return 0;

    uint32_t *ssrc = (uint32_t *) (data + sizeof(RTCPCommonHeader));
    return m_endian.qToHost(*ssrc);
}

inline uint8_t *RTCPAPPPacket::GetName()
{
    if (!knownformat)
        return 0;

    return (data + sizeof(RTCPCommonHeader) + sizeof(uint32_t));
}

inline uint8_t *RTCPAPPPacket::GetAPPData()
{
    if (!knownformat)
        return 0;
    if (appdatalen == 0)
        return 0;
    return (data + sizeof(RTCPCommonHeader) + sizeof(uint32_t) * 2);
}

inline std::size_t RTCPAPPPacket::GetAPPDataLength() const
{
    if (!knownformat)
        return 0;
    return appdatalen;
}

} // end namespace

#endif // RTCPAPPPACKET_H

