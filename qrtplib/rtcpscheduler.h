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
 * \file rtcpscheduler.h
 */

#ifndef RTCPSCHEDULER_H

#define RTCPSCHEDULER_H

#include "rtpconfig.h"
#include "rtptimeutilities.h"
#include "rtprandom.h"
#include <cstddef>

#include "export.h"

namespace qrtplib
{

class RTCPCompoundPacket;
class RTPPacket;
class RTPSources;

/** Describes parameters used by the RTCPScheduler class. */
class QRTPLIB_API RTCPSchedulerParams
{
public:
    RTCPSchedulerParams();
    ~RTCPSchedulerParams();

    /** Sets the RTCP bandwidth to be used to \c bw (in bytes per second). */
    int SetRTCPBandwidth(double bw);

    /** Returns the used RTCP bandwidth in bytes per second (default is 1000). */
    double GetRTCPBandwidth() const
    {
        return bandwidth;
    }

    /** Sets the fraction of the RTCP bandwidth reserved for senders to \c fraction. */
    int SetSenderBandwidthFraction(double fraction);

    /** Returns the fraction of the RTCP bandwidth reserved for senders (default is 25%). */
    double GetSenderBandwidthFraction() const
    {
        return senderfraction;
    }

    /** Sets the minimum (deterministic) interval between RTCP compound packets to \c t. */
    int SetMinimumTransmissionInterval(const RTPTime &t);

    /** Returns the minimum RTCP transmission interval (default is 5 seconds). */
    RTPTime GetMinimumTransmissionInterval() const
    {
        return mininterval;
    }

    /** If \c usehalf is \c true, only use half the minimum interval before sending the first RTCP compound packet. */
    void SetUseHalfAtStartup(bool usehalf)
    {
        usehalfatstartup = usehalf;
    }

    /** Returns \c true if only half the minimum interval should be used before sending the first RTCP compound packet
     *  (defualt is \c true).
     */
    bool GetUseHalfAtStartup() const
    {
        return usehalfatstartup;
    }

    /** If \c v is \c true, the scheduler will schedule a BYE packet to be sent immediately if allowed. */
    void SetRequestImmediateBYE(bool v)
    {
        immediatebye = v;
    }

    /** Returns if the scheduler will schedule a BYE packet to be sent immediately if allowed
     *  (default is \c true).
     */
    bool GetRequestImmediateBYE() const
    {
        return immediatebye;
    }
private:
    double bandwidth;
    double senderfraction;
    RTPTime mininterval;
    bool usehalfatstartup;
    bool immediatebye;
};

/** This class determines when RTCP compound packets should be sent. */
class RTCPScheduler
{
public:
    /** Creates an instance which will use the source table RTPSources to determine when RTCP compound
     *  packets should be scheduled.
     *  Creates an instance which will use the source table RTPSources to determine when RTCP compound
     *  packets should be scheduled. Note that for correct operation the \c sources instance should have information
     *  about the own SSRC (added by RTPSources::CreateOwnSSRC). You must also supply a random number
     *  generator \c rtprand which will be used for adding randomness to the RTCP intervals.
     */
    RTCPScheduler(RTPSources &sources, RTPRandom &rtprand);
    ~RTCPScheduler();

    /** Resets the scheduler. */
    void Reset();

    /** Sets the scheduler parameters to be used to \c params. */
    void SetParameters(const RTCPSchedulerParams &params)
    {
        schedparams = params;
    }

    /** Returns the currently used scheduler parameters. */
    RTCPSchedulerParams GetParameters() const
    {
        return schedparams;
    }

    /** Sets the header overhead from underlying protocols (for example UDP and IP) to \c numbytes. */
    void SetHeaderOverhead(std::size_t numbytes)
    {
        headeroverhead = numbytes;
    }

    /** Returns the currently used header overhead. */
    std::size_t GetHeaderOverhead() const
    {
        return headeroverhead;
    }

    /** For each incoming RTCP compound packet, this function has to be called for the scheduler to work correctly. */
    void AnalyseIncoming(RTCPCompoundPacket &rtcpcomppack);

    /** For each outgoing RTCP compound packet, this function has to be called for the scheduler to work correctly. */
    void AnalyseOutgoing(RTCPCompoundPacket &rtcpcomppack);

    /** This function has to be called each time a member times out or sends a BYE packet. */
    void ActiveMemberDecrease();

    /** Asks the scheduler to schedule an RTCP compound packet containing a BYE packetl; the compound packet
     *  has size \c packetsize.
     */
    void ScheduleBYEPacket(std::size_t packetsize);

    /**	Returns the delay after which an RTCP compound will possibly have to be sent.
     *  Returns the delay after which an RTCP compound will possibly have to be sent. The IsTime member function
     *  should be called afterwards to make sure that it actually is time to send an RTCP compound packet.
     */
    RTPTime GetTransmissionDelay();

    /** This function returns \c true if it's time to send an RTCP compound packet and \c false otherwise.
     *  This function returns \c true if it's time to send an RTCP compound packet and \c false otherwise.
     *  If the function returns \c true, it will also have calculated the next time at which a packet should
     *  be sent, so if it is called again right away, it will return \c false.
     */
    bool IsTime();

    /** Calculates the deterministic interval at this time.
     *  Calculates the deterministic interval at this time. This is used - in combination with a certain multiplier -
     *  to time out members, senders etc.
     */
    RTPTime CalculateDeterministicInterval(bool sender = false);
private:
    void CalculateNextRTCPTime();
    void PerformReverseReconsideration();
    RTPTime CalculateBYETransmissionInterval();
    RTPTime CalculateTransmissionInterval(bool sender);

    RTPSources &sources;
    RTCPSchedulerParams schedparams;
    std::size_t headeroverhead;
    std::size_t avgrtcppacksize;
    bool hassentrtcp;
    bool firstcall;
    RTPTime nextrtcptime;
    RTPTime prevrtcptime;
    int pmembers;

    // for BYE packet scheduling
    bool byescheduled;
    int byemembers, pbyemembers;
    std::size_t avgbyepacketsize;
    bool sendbyenow;

    RTPRandom &rtprand;
};

} // end namespace

#endif // RTCPSCHEDULER_H

