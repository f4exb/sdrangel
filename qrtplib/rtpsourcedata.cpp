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

#include "rtpsourcedata.h"
#include "rtpdefines.h"
#include "rtpaddress.h"

#define ACCEPTPACKETCODE									\
		*accept = true;									\
												\
		sentdata = true;								\
		packetsreceived++;								\
		numnewpackets++;								\
												\
		if (pack->GetExtendedSequenceNumber() == 0)					\
		{										\
			baseseqnr = 0x0000FFFF;							\
			numcycles = 0x00010000;							\
		}										\
		else										\
			baseseqnr = pack->GetExtendedSequenceNumber() - 1;			\
												\
		exthighseqnr = baseseqnr + 1;							\
		prevpacktime = receivetime;							\
		prevexthighseqnr = baseseqnr;							\
		savedextseqnr = baseseqnr;							\
												\
		pack->SetExtendedSequenceNumber(exthighseqnr);					\
												\
		prevtimestamp = pack->GetTimestamp();						\
		lastmsgtime = prevpacktime;							\
		if (!ownpacket) /* for own packet, this value is set on an outgoing packet */	\
			lastrtptime = prevpacktime;

namespace qrtplib
{

void RTPSourceStats::ProcessPacket(
        RTPPacket *pack,
        const RTPTime &receivetime,
        double tsunit,
        bool ownpacket,
        bool *accept)
{
    // Note that the sequence number in the RTP packet is still just the
    // 16 bit number contained in the RTP header

    if (!sentdata) // no valid packets received yet
    {
        ACCEPTPACKETCODE
    }
    else // already got packets
    {
        uint16_t maxseq16;
        uint32_t extseqnr;

        // Adjust max extended sequence number and set extende seq nr of packet

        *accept = true;
        packetsreceived++;
        numnewpackets++;

        maxseq16 = (uint16_t) (exthighseqnr & 0x0000FFFF);
        if (pack->GetExtendedSequenceNumber() >= maxseq16)
        {
            extseqnr = numcycles + pack->GetExtendedSequenceNumber();
            exthighseqnr = extseqnr;
        }
        else
        {
            uint16_t dif1, dif2;

            dif1 = ((uint16_t) pack->GetExtendedSequenceNumber());
            dif1 -= maxseq16;
            dif2 = maxseq16;
            dif2 -= ((uint16_t) pack->GetExtendedSequenceNumber());
            if (dif1 < dif2)
            {
                numcycles += 0x00010000;
                extseqnr = numcycles + pack->GetExtendedSequenceNumber();
                exthighseqnr = extseqnr;
            }
            else
                extseqnr = numcycles + pack->GetExtendedSequenceNumber();
        }

        pack->SetExtendedSequenceNumber(extseqnr);

        // Calculate jitter

        if (tsunit > 0)
        {
#if 0
            RTPTime curtime = receivetime;
            double diffts1,diffts2,diff;

            curtime -= prevpacktime;
            diffts1 = curtime.GetDouble()/tsunit;
            diffts2 = (double)pack->GetTimestamp() - (double)prevtimestamp;
            diff = diffts1 - diffts2;
            if (diff < 0)
            diff = -diff;
            diff -= djitter;
            diff /= 16.0;
            djitter += diff;
            jitter = (uint32_t)djitter;
#else
            RTPTime curtime = receivetime;
            double diffts1, diffts2, diff;
            uint32_t curts = pack->GetTimestamp();

            curtime -= prevpacktime;
            diffts1 = curtime.GetDouble() / tsunit;

            if (curts > prevtimestamp)
            {
                uint32_t unsigneddiff = curts - prevtimestamp;

                if (unsigneddiff < 0x10000000) // okay, curts realy is larger than prevtimestamp
                    diffts2 = (double) unsigneddiff;
                else
                {
                    // wraparound occurred and curts is actually smaller than prevtimestamp

                    unsigneddiff = -unsigneddiff; // to get the actual difference (in absolute value)
                    diffts2 = -((double) unsigneddiff);
                }
            }
            else if (curts < prevtimestamp)
            {
                uint32_t unsigneddiff = prevtimestamp - curts;

                if (unsigneddiff < 0x10000000) // okay, curts really is smaller than prevtimestamp
                    diffts2 = -((double) unsigneddiff); // negative since we actually need curts-prevtimestamp
                else
                {
                    // wraparound occurred and curts is actually larger than prevtimestamp

                    unsigneddiff = -unsigneddiff; // to get the actual difference (in absolute value)
                    diffts2 = (double) unsigneddiff;
                }
            }
            else
                diffts2 = 0;

            diff = diffts1 - diffts2;
            if (diff < 0)
                diff = -diff;
            diff -= djitter;
            diff /= 16.0;
            djitter += diff;
            jitter = (uint32_t) djitter;
#endif
        }
        else
        {
            djitter = 0;
            jitter = 0;
        }

        prevpacktime = receivetime;
        prevtimestamp = pack->GetTimestamp();
        lastmsgtime = prevpacktime;
        if (!ownpacket) // for own packet, this value is set on an outgoing packet
            lastrtptime = prevpacktime;
    }
}

RTPSourceData::RTPSourceData(uint32_t s) :
        byetime(0, 0)
{
    ssrc = s;
    issender = false;
    iscsrc = false;
    timestampunit = -1;
    receivedbye = false;
    byereason = 0;
    byereasonlen = 0;
    rtpaddr = 0;
    rtcpaddr = 0;
    ownssrc = false;
    validated = false;
    processedinrtcp = false;
    isrtpaddrset = false;
    isrtcpaddrset = false;
}

RTPSourceData::~RTPSourceData()
{
    FlushPackets();
    if (byereason)
        delete[] byereason;
    if (rtpaddr)
        delete rtpaddr;
    if (rtcpaddr)
        delete rtcpaddr;
}

double RTPSourceData::INF_GetEstimatedTimestampUnit() const
{
    if (!SRprevinf.HasInfo())
        return -1.0;

    RTPTime t1 = RTPTime(SRinf.GetNTPTimestamp());
    RTPTime t2 = RTPTime(SRprevinf.GetNTPTimestamp());
    if (t1.IsZero() || t2.IsZero()) // one of the times couldn't be calculated
        return -1.0;

    if (t1 <= t2)
        return -1.0;

    t1 -= t2; // get the time difference

    uint32_t tsdiff = SRinf.GetRTPTimestamp() - SRprevinf.GetRTPTimestamp();

    return (t1.GetDouble() / ((double) tsdiff));
}

RTPTime RTPSourceData::INF_GetRoundtripTime() const
{
    if (!RRinf.HasInfo())
        return RTPTime(0, 0);
    if (RRinf.GetDelaySinceLastSR() == 0 && RRinf.GetLastSRTimestamp() == 0)
        return RTPTime(0, 0);

    RTPNTPTime recvtime = RRinf.GetReceiveTime().GetNTPTime();
    uint32_t rtt = ((recvtime.GetMSW() & 0xFFFF) << 16) | ((recvtime.GetLSW() >> 16) & 0xFFFF);
    rtt -= RRinf.GetLastSRTimestamp();
    rtt -= RRinf.GetDelaySinceLastSR();

    double drtt = (((double) rtt) / 65536.0);
    return RTPTime(drtt);
}

} // end namespace

