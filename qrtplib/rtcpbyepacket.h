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
 * \file rtcpbyepacket.h
 */

#ifndef RTCPBYEPACKET_H

#define RTCPBYEPACKET_H

#include "rtpconfig.h"
#include "rtcppacket.h"
#include "rtpstructs.h"
#include "rtpendian.h"

#include "export.h"

namespace qrtplib
{

class RTCPCompoundPacket;

/** Describes an RTCP BYE packet. */
class QRTPLIB_API RTCPBYEPacket: public RTCPPacket
{
public:
    /** Creates an instance based on the data in \c data with length \c datalen.
     *  Creates an instance based on the data in \c data with length \c datalen. Since the \c data pointer
     *  is referenced inside the class (no copy of the data is made) one must make sure that the memory it
     *  points to is valid as long as the class instance exists.
     */
    RTCPBYEPacket(uint8_t *data, std::size_t datalen);
    ~RTCPBYEPacket()
    {
    }

    /** Returns the number of SSRC identifiers present in this BYE packet. */
    int GetSSRCCount() const;

    /** Returns the SSRC described by \c index which may have a value from 0 to GetSSRCCount()-1
     *  (note that no check is performed to see if \c index is valid).
     */
    uint32_t GetSSRC(int index) const; // note: no check is performed to see if index is valid!

    /** Returns true if the BYE packet contains a reason for leaving. */
    bool HasReasonForLeaving() const;

    /** Returns the length of the string which describes why the source(s) left. */
    std::size_t GetReasonLength() const;

    /** Returns the actual reason for leaving data. */
    uint8_t *GetReasonData();

private:
    RTPEndian m_endian;
    std::size_t reasonoffset;
};

inline int RTCPBYEPacket::GetSSRCCount() const
{
    if (!knownformat)
        return 0;

    RTCPCommonHeader *hdr = (RTCPCommonHeader *) data;
    return (int) (hdr->count);
}

inline uint32_t RTCPBYEPacket::GetSSRC(int index) const
{
    if (!knownformat)
        return 0;
    uint32_t *ssrc = (uint32_t *) (data + sizeof(RTCPCommonHeader) + sizeof(uint32_t) * index);
    return m_endian.qToHost(*ssrc);
}

inline bool RTCPBYEPacket::HasReasonForLeaving() const
{
    if (!knownformat)
        return false;
    if (reasonoffset == 0)
        return false;
    return true;
}

inline std::size_t RTCPBYEPacket::GetReasonLength() const
{
    if (!knownformat)
        return 0;
    if (reasonoffset == 0)
        return 0;
    uint8_t *reasonlen = (data + reasonoffset);
    return (std::size_t)(*reasonlen);
}

inline uint8_t *RTCPBYEPacket::GetReasonData()
{
    if (!knownformat)
        return 0;
    if (reasonoffset == 0)
        return 0;
    uint8_t *reasonlen = (data + reasonoffset);
    if ((*reasonlen) == 0)
        return 0;
    return (data + reasonoffset + 1);
}

} // end namespace

#endif // RTCPBYEPACKET_H

