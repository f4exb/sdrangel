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
 * \file rtpsourcedata.h
 */

#ifndef RTPSOURCEDATA_H

#define RTPSOURCEDATA_H

#include "rtpconfig.h"
#include "rtptimeutilities.h"
#include "rtppacket.h"
#include "rtcpsdesinfo.h"
#include "rtptypes.h"
#include "rtpsources.h"
#include <list>

#include "export.h"

namespace qrtplib
{

class RTPAddress;

class QRTPLIB_API RTCPSenderReportInfo
{
public:
    RTCPSenderReportInfo() :
            ntptimestamp(0, 0), receivetime(0, 0)
    {
        hasinfo = false;
        rtptimestamp = 0;
        packetcount = 0;
        bytecount = 0;
    }
    void Set(const RTPNTPTime &ntptime, uint32_t rtptime, uint32_t pcount, uint32_t bcount, const RTPTime &rcvtime)
    {
        ntptimestamp = ntptime;
        rtptimestamp = rtptime;
        packetcount = pcount;
        bytecount = bcount;
        receivetime = rcvtime;
        hasinfo = true;
    }

    bool HasInfo() const
    {
        return hasinfo;
    }
    RTPNTPTime GetNTPTimestamp() const
    {
        return ntptimestamp;
    }
    uint32_t GetRTPTimestamp() const
    {
        return rtptimestamp;
    }
    uint32_t GetPacketCount() const
    {
        return packetcount;
    }
    uint32_t GetByteCount() const
    {
        return bytecount;
    }
    RTPTime GetReceiveTime() const
    {
        return receivetime;
    }
private:
    bool hasinfo;
    RTPNTPTime ntptimestamp;
    uint32_t rtptimestamp;
    uint32_t packetcount;
    uint32_t bytecount;
    RTPTime receivetime;
};

class RTCPReceiverReportInfo
{
public:
    RTCPReceiverReportInfo() :
            receivetime(0, 0)
    {
        hasinfo = false;
        fractionlost = 0;
        packetslost = 0;
        exthighseqnr = 0;
        jitter = 0;
        lsr = 0;
        dlsr = 0;
    }
    void Set(uint8_t fraclost, int32_t plost, uint32_t exthigh, uint32_t jit, uint32_t l, uint32_t dl, const RTPTime &rcvtime)
    {
        fractionlost = ((double) fraclost) / 256.0;
        packetslost = plost;
        exthighseqnr = exthigh;
        jitter = jit;
        lsr = l;
        dlsr = dl;
        receivetime = rcvtime;
        hasinfo = true;
    }

    bool HasInfo() const
    {
        return hasinfo;
    }
    double GetFractionLost() const
    {
        return fractionlost;
    }
    int32_t GetPacketsLost() const
    {
        return packetslost;
    }
    uint32_t GetExtendedHighestSequenceNumber() const
    {
        return exthighseqnr;
    }
    uint32_t GetJitter() const
    {
        return jitter;
    }
    uint32_t GetLastSRTimestamp() const
    {
        return lsr;
    }
    uint32_t GetDelaySinceLastSR() const
    {
        return dlsr;
    }
    RTPTime GetReceiveTime() const
    {
        return receivetime;
    }
private:
    bool hasinfo;
    double fractionlost;
    int32_t packetslost;
    uint32_t exthighseqnr;
    uint32_t jitter;
    uint32_t lsr;
    uint32_t dlsr;
    RTPTime receivetime;
};

class RTPSourceStats
{
public:
    RTPSourceStats();
    void ProcessPacket(
            RTPPacket *pack,
            const RTPTime &receivetime,
            double tsunit,
            bool ownpacket,
            bool *accept);

    bool HasSentData() const
    {
        return sentdata;
    }
    uint32_t GetNumPacketsReceived() const
    {
        return packetsreceived;
    }
    uint32_t GetBaseSequenceNumber() const
    {
        return baseseqnr;
    }
    uint32_t GetExtendedHighestSequenceNumber() const
    {
        return exthighseqnr;
    }
    uint32_t GetJitter() const
    {
        return jitter;
    }

    int32_t GetNumPacketsReceivedInInterval() const
    {
        return numnewpackets;
    }
    uint32_t GetSavedExtendedSequenceNumber() const
    {
        return savedextseqnr;
    }
    void StartNewInterval()
    {
        numnewpackets = 0;
        savedextseqnr = exthighseqnr;
    }

    void SetLastMessageTime(const RTPTime &t)
    {
        lastmsgtime = t;
    }
    RTPTime GetLastMessageTime() const
    {
        return lastmsgtime;
    }
    void SetLastRTPPacketTime(const RTPTime &t)
    {
        lastrtptime = t;
    }
    RTPTime GetLastRTPPacketTime() const
    {
        return lastrtptime;
    }

    void SetLastNoteTime(const RTPTime &t)
    {
        lastnotetime = t;
    }
    RTPTime GetLastNoteTime() const
    {
        return lastnotetime;
    }
private:
    bool sentdata;
    uint32_t packetsreceived;
    uint32_t numcycles; // shifted left 16 bits
    uint32_t baseseqnr;
    uint32_t exthighseqnr, prevexthighseqnr;
    uint32_t jitter, prevtimestamp;
    double djitter;
    RTPTime prevpacktime;
    RTPTime lastmsgtime;
    RTPTime lastrtptime;
    RTPTime lastnotetime;
    uint32_t numnewpackets;
    uint32_t savedextseqnr;
};

inline RTPSourceStats::RTPSourceStats() :
        prevpacktime(0, 0), lastmsgtime(0, 0), lastrtptime(0, 0), lastnotetime(0, 0)
{
    sentdata = false;
    packetsreceived = 0;
    baseseqnr = 0;
    exthighseqnr = 0;
    prevexthighseqnr = 0;
    jitter = 0;
    numcycles = 0;
    numnewpackets = 0;
    prevtimestamp = 0;
    djitter = 0;
    savedextseqnr = 0;
}

/** Describes an entry in the RTPSources source table. */
class RTPSourceData
{
protected:
    RTPSourceData(uint32_t ssrc);
    virtual ~RTPSourceData();
public:
    /** Extracts the first packet of this participants RTP packet queue. */
    RTPPacket *GetNextPacket();

    /** Clears the participant's RTP packet list. */
    void FlushPackets();

    /** Returns \c true if there are RTP packets which can be extracted. */
    bool HasData() const
    {
        if (!validated)
            return false;
        return packetlist.empty() ? false : true;
    }

    /** Returns the SSRC identifier for this member. */
    uint32_t GetSSRC() const
    {
        return ssrc;
    }

    /** Returns \c true if the participant was added using the RTPSources member function CreateOwnSSRC and
     *  returns \c false otherwise.
     */
    bool IsOwnSSRC() const
    {
        return ownssrc;
    }

    /** Returns \c true if the source identifier is actually a CSRC from an RTP packet. */
    bool IsCSRC() const
    {
        return iscsrc;
    }

    /** Returns \c true if this member is marked as a sender and \c false if not. */
    bool IsSender() const
    {
        return issender;
    }

    /** Returns \c true if the participant is validated, which is the case if a number of
     *  consecutive RTP packets have been received or if a CNAME item has been received for
     *  this participant.
     */
    bool IsValidated() const
    {
        return validated;
    }

    /** Returns \c true if the source was validated and had not yet sent a BYE packet. */
    bool IsActive() const
    {
        if (!validated)
            return false;
        if (receivedbye)
            return false;
        return true;
    }

    /** This function is used by the RTCPPacketBuilder class to mark whether this participant's
     *  information has been processed in a report block or not.
     */
    void SetProcessedInRTCP(bool v)
    {
        processedinrtcp = v;
    }

    /** This function is used by the RTCPPacketBuilder class and returns whether this participant
     *  has been processed in a report block or not.
     */
    bool IsProcessedInRTCP() const
    {
        return processedinrtcp;
    }

    /** Returns \c true if the address from which this participant's RTP packets originate has
     *  already been set.
     */
    bool IsRTPAddressSet() const
    {
        return isrtpaddrset;
    }

    /** Returns \c true if the address from which this participant's RTCP packets originate has
     * already been set.
     */
    bool IsRTCPAddressSet() const
    {
        return isrtcpaddrset;
    }

    /** Returns the address from which this participant's RTP packets originate.
     *  Returns the address from which this participant's RTP packets originate. If the address has
     *  been set and the returned value is NULL, this indicates that it originated from the local
     *  participant.
     */
    const RTPAddress *GetRTPDataAddress() const
    {
        return rtpaddr;
    }

    /** Returns the address from which this participant's RTCP packets originate.
     *  Returns the address from which this participant's RTCP packets originate. If the address has
     *  been set and the returned value is NULL, this indicates that it originated from the local
     *  participant.
     */
    const RTPAddress *GetRTCPDataAddress() const
    {
        return rtcpaddr;
    }

    /** Returns \c true if we received a BYE message for this participant and \c false otherwise. */
    bool ReceivedBYE() const
    {
        return receivedbye;
    }

    /** Returns the reason for leaving contained in the BYE packet of this participant.
     *  Returns the reason for leaving contained in the BYE packet of this participant. The length of
     *  the reason is stored in \c len.
     */
    uint8_t *GetBYEReason(std::size_t *len) const
    {
        *len = byereasonlen;
        return byereason;
    }

    /** Returns the time at which the BYE packet was received. */
    RTPTime GetBYETime() const
    {
        return byetime;
    }

    /** Sets the value for the timestamp unit to be used in jitter calculations for data received from this participant.
     *  Sets the value for the timestamp unit to be used in jitter calculations for data received from this participant.
     *  If not set, the library uses an approximation for the timestamp unit which is calculated from two consecutive
     *  RTCP sender reports. The timestamp unit is defined as a time interval divided by the corresponding timestamp
     *  interval. For 8000 Hz audio this would be 1/8000. For video, often a timestamp unit of 1/90000 is used.
     */
    void SetTimestampUnit(double tsu)
    {
        timestampunit = tsu;
    }

    /** Returns the timestamp unit used for this participant. */
    double GetTimestampUnit() const
    {
        return timestampunit;
    }

    /** Returns \c true if an RTCP sender report has been received from this participant. */
    bool SR_HasInfo() const
    {
        return SRinf.HasInfo();
    }

    /** Returns the NTP timestamp contained in the last sender report. */
    RTPNTPTime SR_GetNTPTimestamp() const
    {
        return SRinf.GetNTPTimestamp();
    }

    /** Returns the RTP timestamp contained in the last sender report. */
    uint32_t SR_GetRTPTimestamp() const
    {
        return SRinf.GetRTPTimestamp();
    }

    /** Returns the packet count contained in the last sender report. */
    uint32_t SR_GetPacketCount() const
    {
        return SRinf.GetPacketCount();
    }

    /** Returns the octet count contained in the last sender report. */
    uint32_t SR_GetByteCount() const
    {
        return SRinf.GetByteCount();
    }

    /** Returns the time at which the last sender report was received. */
    RTPTime SR_GetReceiveTime() const
    {
        return SRinf.GetReceiveTime();
    }

    /** Returns \c true if more than one RTCP sender report has been received. */
    bool SR_Prev_HasInfo() const
    {
        return SRprevinf.HasInfo();
    }

    /** Returns the NTP timestamp contained in the second to last sender report. */
    RTPNTPTime SR_Prev_GetNTPTimestamp() const
    {
        return SRprevinf.GetNTPTimestamp();
    }

    /** Returns the RTP timestamp contained in the second to last sender report. */
    uint32_t SR_Prev_GetRTPTimestamp() const
    {
        return SRprevinf.GetRTPTimestamp();
    }

    /** Returns the packet count contained in the second to last sender report. */
    uint32_t SR_Prev_GetPacketCount() const
    {
        return SRprevinf.GetPacketCount();
    }

    /**  Returns the octet count contained in the second to last sender report. */
    uint32_t SR_Prev_GetByteCount() const
    {
        return SRprevinf.GetByteCount();
    }

    /** Returns the time at which the second to last sender report was received. */
    RTPTime SR_Prev_GetReceiveTime() const
    {
        return SRprevinf.GetReceiveTime();
    }

    /** Returns \c true if this participant sent a receiver report with information about the reception of our data. */
    bool RR_HasInfo() const
    {
        return RRinf.HasInfo();
    }

    /** Returns the fraction lost value from the last report. */
    double RR_GetFractionLost() const
    {
        return RRinf.GetFractionLost();
    }

    /** Returns the number of lost packets contained in the last report. */
    int32_t RR_GetPacketsLost() const
    {
        return RRinf.GetPacketsLost();
    }

    /** Returns the extended highest sequence number contained in the last report. */
    uint32_t RR_GetExtendedHighestSequenceNumber() const
    {
        return RRinf.GetExtendedHighestSequenceNumber();
    }

    /** Returns the jitter value from the last report. */
    uint32_t RR_GetJitter() const
    {
        return RRinf.GetJitter();
    }

    /** Returns the LSR value from the last report. */
    uint32_t RR_GetLastSRTimestamp() const
    {
        return RRinf.GetLastSRTimestamp();
    }

    /** Returns the DLSR value from the last report. */
    uint32_t RR_GetDelaySinceLastSR() const
    {
        return RRinf.GetDelaySinceLastSR();
    }

    /** Returns the time at which the last report was received. */
    RTPTime RR_GetReceiveTime() const
    {
        return RRinf.GetReceiveTime();
    }

    /** Returns \c true if this participant sent more than one receiver report with information
     *  about the reception of our data.
     */
    bool RR_Prev_HasInfo() const
    {
        return RRprevinf.HasInfo();
    }

    /** Returns the fraction lost value from the second to last report. */
    double RR_Prev_GetFractionLost() const
    {
        return RRprevinf.GetFractionLost();
    }

    /** Returns the number of lost packets contained in the second to last report. */
    int32_t RR_Prev_GetPacketsLost() const
    {
        return RRprevinf.GetPacketsLost();
    }

    /** Returns the extended highest sequence number contained in the second to last report. */
    uint32_t RR_Prev_GetExtendedHighestSequenceNumber() const
    {
        return RRprevinf.GetExtendedHighestSequenceNumber();
    }

    /** Returns the jitter value from the second to last report. */
    uint32_t RR_Prev_GetJitter() const
    {
        return RRprevinf.GetJitter();
    }

    /** Returns the LSR value from the second to last report. */
    uint32_t RR_Prev_GetLastSRTimestamp() const
    {
        return RRprevinf.GetLastSRTimestamp();
    }

    /** Returns the DLSR value from the second to last report. */
    uint32_t RR_Prev_GetDelaySinceLastSR() const
    {
        return RRprevinf.GetDelaySinceLastSR();
    }

    /** Returns the time at which the second to last report was received. */
    RTPTime RR_Prev_GetReceiveTime() const
    {
        return RRprevinf.GetReceiveTime();
    }

    /** Returns \c true if validated RTP packets have been received from this participant. */
    bool INF_HasSentData() const
    {
        return stats.HasSentData();
    }

    /** Returns the total number of received packets from this participant. */
    int32_t INF_GetNumPacketsReceived() const
    {
        return stats.GetNumPacketsReceived();
    }

    /** Returns the base sequence number of this participant. */
    uint32_t INF_GetBaseSequenceNumber() const
    {
        return stats.GetBaseSequenceNumber();
    }

    /** Returns the extended highest sequence number received from this participant. */
    uint32_t INF_GetExtendedHighestSequenceNumber() const
    {
        return stats.GetExtendedHighestSequenceNumber();
    }

    /** Returns the current jitter value for this participant. */
    uint32_t INF_GetJitter() const
    {
        return stats.GetJitter();
    }

    /** Returns the time at which something was last heard from this member. */
    RTPTime INF_GetLastMessageTime() const
    {
        return stats.GetLastMessageTime();
    }

    /** Returns the time at which the last RTP packet was received. */
    RTPTime INF_GetLastRTPPacketTime() const
    {
        return stats.GetLastRTPPacketTime();
    }

    /** Returns the estimated timestamp unit, calculated from two consecutive sender reports. */
    double INF_GetEstimatedTimestampUnit() const;

    /** Returns the number of packets received since a new interval was started with INF_StartNewInterval. */
    uint32_t INF_GetNumPacketsReceivedInInterval() const
    {
        return stats.GetNumPacketsReceivedInInterval();
    }

    /** Returns the extended sequence number which was stored by the INF_StartNewInterval call. */
    uint32_t INF_GetSavedExtendedSequenceNumber() const
    {
        return stats.GetSavedExtendedSequenceNumber();
    }

    /** Starts a new interval to count received packets in; this also stores the current extended highest sequence
     *  number to be able to calculate the packet loss during the interval.
     */
    void INF_StartNewInterval()
    {
        stats.StartNewInterval();
    }

    /** Estimates the round trip time by using the LSR and DLSR info from the last receiver report. */
    RTPTime INF_GetRoundtripTime() const;

    /** Returns the time at which the last SDES NOTE item was received. */
    RTPTime INF_GetLastSDESNoteTime() const
    {
        return stats.GetLastNoteTime();
    }

    /** Returns a pointer to the SDES CNAME item of this participant and stores its length in \c len. */
    uint8_t *SDES_GetCNAME(std::size_t *len) const
    {
        return SDESinf.GetCNAME(len);
    }

    /** Returns a pointer to the SDES name item of this participant and stores its length in \c len. */
    uint8_t *SDES_GetName(std::size_t *len) const
    {
        return SDESinf.GetName(len);
    }

    /** Returns a pointer to the SDES e-mail item of this participant and stores its length in \c len. */
    uint8_t *SDES_GetEMail(std::size_t *len) const
    {
        return SDESinf.GetEMail(len);
    }

    /** Returns a pointer to the SDES phone item of this participant and stores its length in \c len. */
    uint8_t *SDES_GetPhone(std::size_t *len) const
    {
        return SDESinf.GetPhone(len);
    }

    /** Returns a pointer to the SDES location item of this participant and stores its length in \c len. */
    uint8_t *SDES_GetLocation(std::size_t *len) const
    {
        return SDESinf.GetLocation(len);
    }

    /** Returns a pointer to the SDES tool item of this participant and stores its length in \c len. */
    uint8_t *SDES_GetTool(std::size_t *len) const
    {
        return SDESinf.GetTool(len);
    }

    /** Returns a pointer to the SDES note item of this participant and stores its length in \c len. */
    uint8_t *SDES_GetNote(std::size_t *len) const
    {
        return SDESinf.GetNote(len);
    }

#ifdef RTP_SUPPORT_SDESPRIV
    /** Starts the iteration over the stored SDES private item prefixes and their associated values. */
    void SDES_GotoFirstPrivateValue()
    {
        SDESinf.GotoFirstPrivateValue();
    }

    /** If available, returns \c true and stores the next SDES private item prefix in \c prefix and its length in
     *  \c prefixlen; the associated value and its length are then stored in \c value and \c valuelen.
     */
    bool SDES_GetNextPrivateValue(uint8_t **prefix, std::size_t *prefixlen, uint8_t **value, std::size_t *valuelen)
    {
        return SDESinf.GetNextPrivateValue(prefix, prefixlen, value, valuelen);
    }

    /**	Looks for the entry which corresponds to the SDES private item prefix \c prefix with length
     *  \c prefixlen; if found, the function returns \c true and stores the associated value and
     *  its length in \c value and \c valuelen respectively.
     */
    bool SDES_GetPrivateValue(uint8_t *prefix, std::size_t prefixlen, uint8_t **value, std::size_t *valuelen) const
    {
        return SDESinf.GetPrivateValue(prefix, prefixlen, value, valuelen);
    }
#endif // RTP_SUPPORT_SDESPRIV

protected:
    std::list<RTPPacket *> packetlist;

    uint32_t ssrc;
    bool ownssrc;
    bool iscsrc;
    double timestampunit;
    bool receivedbye;
    bool validated;
    bool processedinrtcp;
    bool issender;

    RTCPSenderReportInfo SRinf, SRprevinf;
    RTCPReceiverReportInfo RRinf, RRprevinf;
    RTPSourceStats stats;
    RTCPSDESInfo SDESinf;

    bool isrtpaddrset, isrtcpaddrset;
    RTPAddress *rtpaddr, *rtcpaddr;

    RTPTime byetime;
    uint8_t *byereason;
    std::size_t byereasonlen;
};

inline RTPPacket *RTPSourceData::GetNextPacket()
{
    if (!validated)
        return 0;

    RTPPacket *p;

    if (packetlist.empty())
        return 0;
    p = *(packetlist.begin());
    packetlist.pop_front();
    return p;
}

inline void RTPSourceData::FlushPackets()
{
    std::list<RTPPacket *>::const_iterator it;

    for (it = packetlist.begin(); it != packetlist.end(); ++it)
        delete *it;
    packetlist.clear();
}

} // end namespace

#endif // RTPSOURCEDATA_H

