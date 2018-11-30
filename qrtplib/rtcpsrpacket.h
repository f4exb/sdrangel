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
 * \file rtcpsrpacket.h
 */

#ifndef RTCPSRPACKET_H

#define RTCPSRPACKET_H

#include "rtpconfig.h"
#include "rtcppacket.h"
#include "rtptimeutilities.h"
#include "rtpstructs.h"
#include "rtpendian.h"

#include "export.h"

namespace qrtplib
{

class RTCPCompoundPacket;

/** Describes an RTCP sender report packet. */
class QRTPLIB_API RTCPSRPacket: public RTCPPacket
{
public:
    /** Creates an instance based on the data in \c data with length \c datalen.
     *  Creates an instance based on the data in \c data with length \c datalen. Since the \c data pointer
     *  is referenced inside the class (no copy of the data is made) one must make sure that the memory it
     *  points to is valid as long as the class instance exists.
     */
    RTCPSRPacket(uint8_t *data, std::size_t datalength);
    ~RTCPSRPacket()
    {
    }

    /** Returns the SSRC of the participant who sent this packet. */
    uint32_t GetSenderSSRC() const;

    /** Returns the NTP timestamp contained in the sender report. */
    RTPNTPTime GetNTPTimestamp() const;

    /** Returns the RTP timestamp contained in the sender report. */
    uint32_t GetRTPTimestamp() const;

    /** Returns the sender's packet count contained in the sender report. */
    uint32_t GetSenderPacketCount() const;

    /** Returns the sender's octet count contained in the sender report. */
    uint32_t GetSenderOctetCount() const;

    /** Returns the number of reception report blocks present in this packet. */
    int GetReceptionReportCount() const;

    /** Returns the SSRC of the reception report block described by \c index which may have a value
     *  from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if \c index is
     *  valid).
     */
    uint32_t GetSSRC(int index) const;

    /** Returns the `fraction lost' field of the reception report described by \c index which may have
     *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if \c index is
     *  valid).
     */
    uint8_t GetFractionLost(int index) const;

    /** Returns the number of lost packets in the reception report block described by \c index which may have
     *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if \c index is
     *  valid).
     */
    int32_t GetLostPacketCount(int index) const;

    /** Returns the extended highest sequence number of the reception report block described by \c index which may have
     *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if \c index is
     *  valid).
     */
    uint32_t GetExtendedHighestSequenceNumber(int index) const;

    /** Returns the jitter field of the reception report block described by \c index which may have
     *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if \c index is
     *  valid).
     */
    uint32_t GetJitter(int index) const;

    /** Returns the LSR field of the reception report block described by \c index which may have
     *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if \c index is
     *  valid).
     */
    uint32_t GetLSR(int index) const;

    /** Returns the DLSR field of the reception report block described by \c index which may have
     *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if \c index is
     *  valid).
     */
    uint32_t GetDLSR(int index) const;

private:
    RTCPReceiverReport *GotoReport(int index) const;

    RTPEndian m_endian;
};

inline uint32_t RTCPSRPacket::GetSenderSSRC() const
{
    if (!knownformat)
        return 0;

    uint32_t *ssrcptr = (uint32_t *) (data + sizeof(RTCPCommonHeader));
    return m_endian.qToHost(*ssrcptr);
}

inline RTPNTPTime RTCPSRPacket::GetNTPTimestamp() const
{
    if (!knownformat)
        return RTPNTPTime(0, 0);

    RTCPSenderReport *sr = (RTCPSenderReport *) (data + sizeof(RTCPCommonHeader) + sizeof(uint32_t));
    return RTPNTPTime(m_endian.qToHost(sr->ntptime_msw), m_endian.qToHost(sr->ntptime_lsw));
}

inline uint32_t RTCPSRPacket::GetRTPTimestamp() const
{
    if (!knownformat)
        return 0;
    RTCPSenderReport *sr = (RTCPSenderReport *) (data + sizeof(RTCPCommonHeader) + sizeof(uint32_t));
    return m_endian.qToHost(sr->rtptimestamp);
}

inline uint32_t RTCPSRPacket::GetSenderPacketCount() const
{
    if (!knownformat)
        return 0;
    RTCPSenderReport *sr = (RTCPSenderReport *) (data + sizeof(RTCPCommonHeader) + sizeof(uint32_t));
    return m_endian.qToHost(sr->packetcount);
}

inline uint32_t RTCPSRPacket::GetSenderOctetCount() const
{
    if (!knownformat)
        return 0;
    RTCPSenderReport *sr = (RTCPSenderReport *) (data + sizeof(RTCPCommonHeader) + sizeof(uint32_t));
    return m_endian.qToHost(sr->octetcount);
}

inline int RTCPSRPacket::GetReceptionReportCount() const
{
    if (!knownformat)
        return 0;
    RTCPCommonHeader *hdr = (RTCPCommonHeader *) data;
    return ((int) hdr->count);
}

inline RTCPReceiverReport *RTCPSRPacket::GotoReport(int index) const
{
    RTCPReceiverReport *r = (RTCPReceiverReport *) (data + sizeof(RTCPCommonHeader) + sizeof(uint32_t) + sizeof(RTCPSenderReport) + index * sizeof(RTCPReceiverReport));
    return r;
}

inline uint32_t RTCPSRPacket::GetSSRC(int index) const
{
    if (!knownformat)
        return 0;
    RTCPReceiverReport *r = GotoReport(index);
    return m_endian.qToHost(r->ssrc);
}

inline uint8_t RTCPSRPacket::GetFractionLost(int index) const
{
    if (!knownformat)
        return 0;
    RTCPReceiverReport *r = GotoReport(index);
    return r->fractionlost;
}

inline int32_t RTCPSRPacket::GetLostPacketCount(int index) const
{
    if (!knownformat)
        return 0;
    RTCPReceiverReport *r = GotoReport(index);
    uint32_t count = ((uint32_t) r->packetslost[2]) | (((uint32_t) r->packetslost[1]) << 8) | (((uint32_t) r->packetslost[0]) << 16);
    if ((count & 0x00800000) != 0) // test for negative number
        count |= 0xFF000000;
    int32_t *count2 = (int32_t *) (&count);
    return (*count2);
}

inline uint32_t RTCPSRPacket::GetExtendedHighestSequenceNumber(int index) const
{
    if (!knownformat)
        return 0;
    RTCPReceiverReport *r = GotoReport(index);
    return m_endian.qToHost(r->exthighseqnr);
}

inline uint32_t RTCPSRPacket::GetJitter(int index) const
{
    if (!knownformat)
        return 0;
    RTCPReceiverReport *r = GotoReport(index);
    return m_endian.qToHost(r->jitter);
}

inline uint32_t RTCPSRPacket::GetLSR(int index) const
{
    if (!knownformat)
        return 0;
    RTCPReceiverReport *r = GotoReport(index);
    return m_endian.qToHost(r->lsr);
}

inline uint32_t RTCPSRPacket::GetDLSR(int index) const
{
    if (!knownformat)
        return 0;
    RTCPReceiverReport *r = GotoReport(index);
    return m_endian.qToHost(r->dlsr);
}

} // end namespace

#endif // RTCPSRPACKET_H

