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
 * \file rtpsessionparams.h
 */

#ifndef RTPSESSIONPARAMS_H

#define RTPSESSIONPARAMS_H

#include "rtpconfig.h"
#include "rtptypes.h"
#include "rtptransmitter.h"
#include "rtptimeutilities.h"
#include "rtpsources.h"

#include "export.h"

namespace qrtplib
{

/** Describes the parameters for to be used by an RTPSession instance.
 *  Describes the parameters for to be used by an RTPSession instance. Note that the own timestamp
 *  unit must be set to a valid number, otherwise the session can't be created.
 */
class QRTPLIB_API RTPSessionParams
{
public:
    RTPSessionParams();

    /** If \c usethread is \c true, the session will use a poll thread to automatically process incoming
     *  data and to send RTCP packets when necessary.
     */
    int SetUsePollThread(bool usethread);

    /** if `s` is `true`, the session will use mutexes in case multiple threads
     *  are at work. */
    int SetNeedThreadSafety(bool s);

    /** Returns whether the session should use a poll thread or not (default is \c true). */
    bool IsUsingPollThread() const
    {
        return usepollthread;
    }

    /** Sets the maximum allowed packet size for the session. */
    void SetMaximumPacketSize(std::size_t max)
    {
        maxpacksize = max;
    }

    /** Returns the maximum allowed packet size (default is 1400 bytes). */
    std::size_t GetMaximumPacketSize() const
    {
        return maxpacksize;
    }

    /** If the argument is \c true, the session should accept its own packets and store
     *  them accordingly in the source table.
     */
    void SetAcceptOwnPackets(bool accept)
    {
        acceptown = accept;
    }

    /** Returns \c true if the session should accept its own packets (default is \c false). */
    bool AcceptOwnPackets() const
    {
        return acceptown;
    }

    /** Sets the receive mode to be used by the session. */
    void SetReceiveMode(RTPTransmitter::ReceiveMode recvmode)
    {
        receivemode = recvmode;
    }

    /** Sets the receive mode to be used by the session (default is: accept all packets). */
    RTPTransmitter::ReceiveMode GetReceiveMode() const
    {
        return receivemode;
    }

    /** Sets the timestamp unit for our own data.
     *  Sets the timestamp unit for our own data. The timestamp unit is defined as a time interval in
     *  seconds divided by the corresponding timestamp interval. For example, for 8000 Hz audio, the
     *  timestamp unit would typically be 1/8000. Since this value is initially set to an illegal value,
     *  the user must set this to an allowed value to be able to create a session.
     */
    void SetOwnTimestampUnit(double tsunit)
    {
        owntsunit = tsunit;
    }

    /** Returns the currently set timestamp unit. */
    double GetOwnTimestampUnit() const
    {
        return owntsunit;
    }

    /** Sets a flag indicating if a DNS lookup should be done to determine our hostname (to construct a CNAME item).
     *  If \c v is set to \c true, the session will ask the transmitter to find a host name based upon the IP
     *  addresses in its list of local IP addresses. If set to \c false, a call to \c gethostname or something
     *  similar will be used to find the local hostname. Note that the first method might take some time.
     */
    void SetResolveLocalHostname(bool v)
    {
        resolvehostname = v;
    }

    /** Returns whether the local hostname should be determined from the transmitter's list of local IP addresses
     *  or not (default is \c false).
     */
    bool GetResolveLocalHostname() const
    {
        return resolvehostname;
    }

    /** Sets the session bandwidth in bytes per second. */
    void SetSessionBandwidth(double sessbw)
    {
        sessionbandwidth = sessbw;
    }

    /** Returns the session bandwidth in bytes per second (default is 10000 bytes per second). */
    double GetSessionBandwidth() const
    {
        return sessionbandwidth;
    }

    /** Sets the fraction of the session bandwidth to be used for control traffic. */
    void SetControlTrafficFraction(double frac)
    {
        controlfrac = frac;
    }

    /** Returns the fraction of the session bandwidth that will be used for control traffic (default is 5%). */
    double GetControlTrafficFraction() const
    {
        return controlfrac;
    }

    /** Sets the minimum fraction of the control traffic that will be used by senders. */
    void SetSenderControlBandwidthFraction(double frac)
    {
        senderfrac = frac;
    }

    /** Returns the minimum fraction of the control traffic that will be used by senders (default is 25%). */
    double GetSenderControlBandwidthFraction() const
    {
        return senderfrac;
    }

    /** Set the minimal time interval between sending RTCP packets. */
    void SetMinimumRTCPTransmissionInterval(const RTPTime &t)
    {
        mininterval = t;
    }

    /** Returns the minimal time interval between sending RTCP packets (default is 5 seconds). */
    RTPTime GetMinimumRTCPTransmissionInterval() const
    {
        return mininterval;
    }

    /** If \c usehalf is set to \c true, the session will only wait half of the calculated RTCP
     *  interval before sending its first RTCP packet.
     */
    void SetUseHalfRTCPIntervalAtStartup(bool usehalf)
    {
        usehalfatstartup = usehalf;
    }

    /** Returns whether the session will only wait half of the calculated RTCP interval before sending its
     *  first RTCP packet or not (default is \c true).
     */
    bool GetUseHalfRTCPIntervalAtStartup() const
    {
        return usehalfatstartup;
    }

    /** If \c v is \c true, the session will send a BYE packet immediately if this is allowed. */
    void SetRequestImmediateBYE(bool v)
    {
        immediatebye = v;
    }

    /** Returns whether the session should send a BYE packet immediately (if allowed) or not (default is \c true). */
    bool GetRequestImmediateBYE() const
    {
        return immediatebye;
    }

    /** When sending a BYE packet, this indicates whether it will be part of an RTCP compound packet
     *  that begins with a sender report (if allowed) or a receiver report.
     */
    void SetSenderReportForBYE(bool v)
    {
        SR_BYE = v;
    }

    /** Returns \c true if a BYE packet will be sent in an RTCP compound packet which starts with a
     *  sender report; if a receiver report will be used, the function returns \c false (default is \c true).
     */
    bool GetSenderReportForBYE() const
    {
        return SR_BYE;
    }

    /** Sets the multiplier to be used when timing out senders. */
    void SetSenderTimeoutMultiplier(double m)
    {
        sendermultiplier = m;
    }

    /** Returns the multiplier to be used when timing out senders (default is 2). */
    double GetSenderTimeoutMultiplier() const
    {
        return sendermultiplier;
    }

    /** Sets the multiplier to be used when timing out members. */
    void SetSourceTimeoutMultiplier(double m)
    {
        generaltimeoutmultiplier = m;
    }

    /** Returns the multiplier to be used when timing out members (default is 5). */
    double GetSourceTimeoutMultiplier() const
    {
        return generaltimeoutmultiplier;
    }

    /** Sets the multiplier to be used when timing out a member after it has sent a BYE packet. */
    void SetBYETimeoutMultiplier(double m)
    {
        byetimeoutmultiplier = m;
    }

    /** Returns the multiplier to be used when timing out a member after it has sent a BYE packet (default is 1). */
    double GetBYETimeoutMultiplier() const
    {
        return byetimeoutmultiplier;
    }

    /** Sets the multiplier to be used when timing out entries in the collision table. */
    void SetCollisionTimeoutMultiplier(double m)
    {
        collisionmultiplier = m;
    }

    /** Returns the multiplier to be used when timing out entries in the collision table (default is 10). */
    double GetCollisionTimeoutMultiplier() const
    {
        return collisionmultiplier;
    }

    /** Sets the multiplier to be used when timing out SDES NOTE information. */
    void SetNoteTimeoutMultiplier(double m)
    {
        notemultiplier = m;
    }

    /** Returns the multiplier to be used when timing out SDES NOTE information (default is 25). */
    double GetNoteTimeoutMultiplier() const
    {
        return notemultiplier;
    }

    /** Sets a flag which indicates if a predefined SSRC identifier should be used. */
    void SetUsePredefinedSSRC(bool f)
    {
        usepredefinedssrc = f;
    }

    /** Returns a flag indicating if a predefined SSRC should be used. */
    bool GetUsePredefinedSSRC() const
    {
        return usepredefinedssrc;
    }

    /** Sets the SSRC which will be used if RTPSessionParams::GetUsePredefinedSSRC returns true. */
    void SetPredefinedSSRC(uint32_t ssrc)
    {
        predefinedssrc = ssrc;
    }

    /** Returns the SSRC which will be used if RTPSessionParams::GetUsePredefinedSSRC returns true. */
    uint32_t GetPredefinedSSRC() const
    {
        return predefinedssrc;
    }

    /** Forces this string to be used as the CNAME identifier. */
    void SetCNAME(const std::string &s)
    {
        cname = s;
    }

    /** Returns the currently set CNAME, is blank when this will be generated automatically (the default). */
    std::string GetCNAME() const
    {
        return cname;
    }

    /** Returns `true` if thread safety was requested using RTPSessionParams::SetNeedThreadSafety. */
    bool NeedThreadSafety() const
    {
        return m_needThreadSafety;
    }
private:
    bool acceptown;
    bool usepollthread;
    std::size_t maxpacksize;
    double owntsunit;
    RTPTransmitter::ReceiveMode receivemode;
    bool resolvehostname;

    double sessionbandwidth;
    double controlfrac;
    double senderfrac;
    RTPTime mininterval;
    bool usehalfatstartup;
    bool immediatebye;
    bool SR_BYE;

    double sendermultiplier;
    double generaltimeoutmultiplier;
    double byetimeoutmultiplier;
    double collisionmultiplier;
    double notemultiplier;

    bool usepredefinedssrc;
    uint32_t predefinedssrc;

    std::string cname;
    bool m_needThreadSafety;
};

} // end namespace

#endif // RTPSESSIONPARAMS_H

