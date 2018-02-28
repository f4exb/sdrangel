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
 * \file rtcpunknownpacket.h
 */

#ifndef RTCPUNKNOWNPACKET_H

#define RTCPUNKNOWNPACKET_H

#include "rtpconfig.h"
#include "rtcppacket.h"

namespace qrtplib
{

class RTCPCompoundPacket;

/** Describes an RTCP packet of unknown type.
 *  Describes an RTCP packet of unknown type. This class doesn't have any extra member functions besides
 *  the ones it inherited. Note that since an unknown packet type doesn't have any format to check
 *  against, the IsKnownFormat function will trivially return \c true.
 */
class RTCPUnknownPacket: public RTCPPacket
{
public:
    /** Creates an instance based on the data in \c data with length \c datalen.
     *  Creates an instance based on the data in \c data with length \c datalen. Since the \c data pointer
     *  is referenced inside the class (no copy of the data is made) one must make sure that the memory it
     *  points to is valid as long as the class instance exists.
     */
    RTCPUnknownPacket(uint8_t *data, std::size_t datalen) :
            RTCPPacket(Unknown, data, datalen)
    {
        // Since we don't expect a format, we'll trivially put knownformat = true
        knownformat = true;
    }
    ~RTCPUnknownPacket()
    {
    }
};

} // end namespace

#endif // RTCPUNKNOWNPACKET_H

