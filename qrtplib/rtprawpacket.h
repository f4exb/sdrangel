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
 * \file rtprawpacket.h
 */

#ifndef RTPRAWPACKET_H

#define RTPRAWPACKET_H

#include "rtpconfig.h"
#include "rtptimeutilities.h"
#include "rtpaddress.h"
#include "rtptypes.h"

namespace qrtplib
{

/** This class is used by the transmission component to store the incoming RTP and RTCP data in. */
class RTPRawPacket
{
public:
    /** Creates an instance which stores data from \c data with length \c datalen.
     *  Creates an instance which stores data from \c data with length \c datalen. Only the pointer
     *  to the data is stored, no actual copy is made! The address from which this packet originated
     *  is set to \c address and the time at which the packet was received is set to \c recvtime.
     *  The flag which indicates whether this data is RTP or RTCP data is set to \c rtp. A memory
     *  manager can be installed as well.
     */
    RTPRawPacket(const uint8_t *data, std::size_t datalen, const RTPAddress& address, RTPTime &recvtime, bool rtp);
    ~RTPRawPacket();

    /** Returns the pointer to the data which is contained in this packet. */
    uint8_t *GetData()
    {
        return packetdata;
    }

    /** Returns the length of the packet described by this instance. */
    std::size_t GetDataLength() const
    {
        return packetdatalength;
    }

    /** Returns the time at which this packet was received. */
    RTPTime GetReceiveTime() const
    {
        return receivetime;
    }

    /** Returns the address stored in this packet. */
    const RTPAddress& GetSenderAddress() const
    {
        return senderaddress;
    }

    /** Returns \c true if this data is RTP data, \c false if it is RTCP data. */
    bool IsRTP() const
    {
        return isrtp;
    }

    /** Deallocates the previously stored data and replaces it with the data that's
     *  specified, can be useful when e.g. decrypting data in RTPSession::OnChangeIncomingData */
    void SetData(const uint8_t *data, std::size_t datalen);

    /** Deallocates the currently stored RTPAddress instance and replaces it
     *  with the one that's specified (you probably don't need this function). */
    void SetSenderAddress(const RTPAddress& address);
private:
    void DeleteData();

    uint8_t *packetdata;
    std::size_t packetdatalength;
    RTPTime receivetime;
    RTPAddress senderaddress;
    bool isrtp;
};

inline RTPRawPacket::RTPRawPacket(const uint8_t *data, std::size_t datalen, const RTPAddress& address, RTPTime &recvtime, bool rtp) :
        receivetime(recvtime)
{
    packetdata = new uint8_t[datalen];
    memcpy(packetdata, data, datalen);
    packetdatalength = datalen;
    senderaddress = address;
    isrtp = rtp;
}

inline RTPRawPacket::~RTPRawPacket()
{
    DeleteData();
}

inline void RTPRawPacket::DeleteData()
{
    if (packetdata)
    {
        delete[] packetdata;
        packetdata = 0;
    }
}

inline void RTPRawPacket::SetData(const uint8_t *data, std::size_t datalen)
{
    if (packetdata) {
        delete[] packetdata;
    }

    packetdata = new uint8_t[datalen];
    memcpy(packetdata, data, datalen);
    packetdatalength = datalen;
}

inline void RTPRawPacket::SetSenderAddress(const RTPAddress& address)
{
    senderaddress = address;
}

} // end namespace

#endif // RTPRAWPACKET_H

