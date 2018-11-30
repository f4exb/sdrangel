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
 * \file rtppacketbuilder.h
 */

#ifndef RTPPACKETBUILDER_H

#define RTPPACKETBUILDER_H

#include "rtpconfig.h"
#include "rtperrors.h"
#include "rtpdefines.h"
#include "rtprandom.h"
#include "rtptimeutilities.h"
#include "rtptypes.h"

#include "export.h"

namespace qrtplib
{

class RTPSources;

/** This class can be used to build RTP packets and is a bit more high-level than the RTPPacket
 *  class: it generates an SSRC identifier, keeps track of timestamp and sequence number etc.
 */
class QRTPLIB_API RTPPacketBuilder
{
public:
    /** Constructs an instance which will use \c rtprand for generating random numbers
     *  (used to initialize the SSRC value and sequence number), optionally installing a memory manager.
     **/
    RTPPacketBuilder(RTPRandom &rtprand);
    ~RTPPacketBuilder();

    /** Initializes the builder to only allow packets with a size below \c maxpacksize. */
    int Init(unsigned int maxpacksize);

    /** Cleans up the builder. */
    void Destroy();

    /** Returns the number of packets which have been created with the current SSRC identifier. */
    uint32_t GetPacketCount()
    {
        if (!init)
            return 0;
        return numpackets;
    }

    /** Returns the number of payload octets which have been generated with this SSRC identifier. */
    uint32_t GetPayloadOctetCount()
    {
        if (!init)
            return 0;
        return numpayloadbytes;
    }

    /** Sets the maximum allowed packet size to \c maxpacksize. */
    int SetMaximumPacketSize(unsigned int maxpacksize);

    /** Adds a CSRC to the CSRC list which will be stored in the RTP packets. */
    int AddCSRC(uint32_t csrc);

    /** Deletes a CSRC from the list which will be stored in the RTP packets. */
    int DeleteCSRC(uint32_t csrc);

    /** Clears the CSRC list. */
    void ClearCSRCList();

    /** Builds a packet with payload \c data and payload length \c len.
     *  Builds a packet with payload \c data and payload length \c len. The payload type, marker
     *  and timestamp increment used will be those that have been set using the \c SetDefault
     *  functions below.
     */
    int BuildPacket(const void *data, unsigned int len);

    /** Builds a packet with payload \c data and payload length \c len.
     *  Builds a packet with payload \c data and payload length \c len. The payload type will be
     *  set to \c pt, the marker bit to \c mark and after building this packet, the timestamp will
     *  be incremented with \c timestamp.
     */
    int BuildPacket(const void *data, unsigned int len, uint8_t pt, bool mark, uint32_t timestampinc);

    /** Builds a packet with payload \c data and payload length \c len.
     *  Builds a packet with payload \c data and payload length \c len. The payload type, marker
     *  and timestamp increment used will be those that have been set using the \c SetDefault
     *  functions below. This packet will also contain an RTP header extension with identifier
     *  \c hdrextID and data \c hdrextdata. The length of the header extension data is given by
     *  \c numhdrextwords which expresses the length in a number of 32-bit words.
     */
    int BuildPacketEx(const void *data, unsigned int len, uint16_t hdrextID, const void *hdrextdata, unsigned int numhdrextwords);

    /** Builds a packet with payload \c data and payload length \c len.
     *  Builds a packet with payload \c data and payload length \c len. The payload type will be set
     *  to \c pt, the marker bit to \c mark and after building this packet, the timestamp will
     *  be incremented with \c timestamp. This packet will also contain an RTP header extension
     *  with identifier \c hdrextID and data \c hdrextdata. The length of the header extension
     *  data is given by \c numhdrextwords which expresses the length in a number of 32-bit words.
     */
    int BuildPacketEx(const void *data, unsigned int len, uint8_t pt, bool mark, uint32_t timestampinc, uint16_t hdrextID, const void *hdrextdata, unsigned int numhdrextwords);

    /** Returns a pointer to the last built RTP packet data. */
    uint8_t *GetPacket()
    {
        if (!init)
            return 0;
        return buffer;
    }

    /** Returns the size of the last built RTP packet. */
    unsigned int GetPacketLength()
    {
        if (!init)
            return 0;
        return packetlength;
    }

    /** Sets the default payload type to \c pt. */
    int SetDefaultPayloadType(uint8_t pt);

    /** Sets the default marker bit to \c m. */
    int SetDefaultMark(bool m);

    /** Sets the default timestamp increment to \c timestampinc. */
    int SetDefaultTimestampIncrement(uint32_t timestampinc);

    /** This function increments the timestamp with the amount given by \c inc.
     *  This function increments the timestamp with the amount given by \c inc. This can be useful
     *  if, for example, a packet was not sent because it contained only silence. Then, this function
     *  should be called to increment the timestamp with the appropriate amount so that the next packets
     *  will still be played at the correct time at other hosts.
     */
    int IncrementTimestamp(uint32_t inc);

    /** This function increments the timestamp with the amount given set by the SetDefaultTimestampIncrement
     *  member function.
     *  This function increments the timestamp with the amount given set by the SetDefaultTimestampIncrement
     *  member function. This can be useful if, for example, a packet was not sent because it contained only silence.
     *  Then, this function should be called to increment the timestamp with the appropriate amount so that the next
     *  packets will still be played at the correct time at other hosts.
     */
    int IncrementTimestampDefault();

    /** Creates a new SSRC to be used in generated packets.
     *  Creates a new SSRC to be used in generated packets. This will also generate new timestamp and
     *  sequence number offsets.
     */
    uint32_t CreateNewSSRC();

    /** Creates a new SSRC to be used in generated packets.
     *  Creates a new SSRC to be used in generated packets. This will also generate new timestamp and
     *  sequence number offsets. The source table \c sources is used to make sure that the chosen SSRC
     *  isn't used by another participant yet.
     */
    uint32_t CreateNewSSRC(RTPSources &sources);

    /** Returns the current SSRC identifier. */
    uint32_t GetSSRC() const
    {
        if (!init)
            return 0;
        return ssrc;
    }

    /** Returns the current RTP timestamp. */
    uint32_t GetTimestamp() const
    {
        if (!init)
            return 0;
        return timestamp;
    }

    /** Returns the current sequence number. */
    uint16_t GetSequenceNumber() const
    {
        if (!init)
            return 0;
        return seqnr;
    }

    /** Returns the time at which a packet was generated.
     *  Returns the time at which a packet was generated. This is not necessarily the time at which
     *  the last RTP packet was generated: if the timestamp increment was zero, the time is not updated.
     */
    RTPTime GetPacketTime() const
    {
        if (!init)
            return RTPTime(0, 0);
        return lastwallclocktime;
    }

    /** Returns the RTP timestamp which corresponds to the time returned by the previous function. */
    uint32_t GetPacketTimestamp() const
    {
        if (!init)
            return 0;
        return lastrtptimestamp;
    }

    /** Sets a specific SSRC to be used.
     *  Sets a specific SSRC to be used. Does not create a new timestamp offset or sequence number
     *  offset. Does not reset the packet count or byte count. Think twice before using this!
     */
    void AdjustSSRC(uint32_t s)
    {
        ssrc = s;
    }
private:
    int PrivateBuildPacket(
            const void *data,
            unsigned int len, uint8_t pt,
            bool mark,
            uint32_t timestampinc,
            bool gotextension, uint16_t hdrextID = 0,
            const void *hdrextdata = 0,
            unsigned int numhdrextwords = 0);

    RTPRandom &rtprnd;
    unsigned int maxpacksize;
    uint8_t *buffer;
    unsigned int packetlength;

    uint32_t numpayloadbytes;
    uint32_t numpackets;
    bool init;

    uint32_t ssrc;
    uint32_t timestamp;
    uint16_t seqnr;

    uint32_t defaulttimestampinc;
    uint8_t defaultpayloadtype;
    bool defaultmark;

    bool deftsset, defptset, defmarkset;

    uint32_t csrcs[RTP_MAXCSRCS];
    int numcsrcs;

    RTPTime lastwallclocktime;
    uint32_t lastrtptimestamp;
    uint32_t prevrtptimestamp;
};

inline int RTPPacketBuilder::SetDefaultPayloadType(uint8_t pt)
{
    if (!init)
        return ERR_RTP_PACKBUILD_NOTINIT;
    defptset = true;
    defaultpayloadtype = pt;
    return 0;
}

inline int RTPPacketBuilder::SetDefaultMark(bool m)
{
    if (!init)
        return ERR_RTP_PACKBUILD_NOTINIT;
    defmarkset = true;
    defaultmark = m;
    return 0;
}

inline int RTPPacketBuilder::SetDefaultTimestampIncrement(uint32_t timestampinc)
{
    if (!init)
        return ERR_RTP_PACKBUILD_NOTINIT;
    deftsset = true;
    defaulttimestampinc = timestampinc;
    return 0;
}

inline int RTPPacketBuilder::IncrementTimestamp(uint32_t inc)
{
    if (!init)
        return ERR_RTP_PACKBUILD_NOTINIT;
    timestamp += inc;
    return 0;
}

inline int RTPPacketBuilder::IncrementTimestampDefault()
{
    if (!init)
        return ERR_RTP_PACKBUILD_NOTINIT;
    if (!deftsset)
        return ERR_RTP_PACKBUILD_DEFAULTTSINCNOTSET;
    timestamp += defaulttimestampinc;
    return 0;
}

} // end namespace

#endif // RTPPACKETBUILDER_H

