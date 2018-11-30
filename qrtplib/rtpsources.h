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
 * \file rtpsources.h
 */

#ifndef RTPSOURCES_H

#define RTPSOURCES_H

#include "rtpconfig.h"
#include "rtpkeyhashtable.h"
#include "rtcpsdespacket.h"
#include "rtptypes.h"

#include "export.h"

#define RTPSOURCES_HASHSIZE							8317

namespace qrtplib
{

class QRTPLIB_API RTPSources_GetHashIndex
{
public:
    static int GetIndex(const uint32_t &ssrc)
    {
        return ssrc % RTPSOURCES_HASHSIZE;
    }
};

class RTPNTPTime;
class RTPTransmitter;
class RTCPAPPPacket;
class RTPInternalSourceData;
class RTPRawPacket;
class RTPPacket;
class RTPTime;
class RTPAddress;
class RTPSourceData;

/** Represents a table in which information about the participating sources is kept.
 *  Represents a table in which information about the participating sources is kept. The class has member
 *  functions to process RTP and RTCP data and to iterate over the participants. Note that a NULL address
 *  is used to identify packets from our own session. The class also provides some overridable functions
 *  which can be used to catch certain events (new SSRC, SSRC collision, ...).
 */
class QRTPLIB_API RTPSources
{
public:
    /** Type of probation to use for new sources. */
    enum ProbationType
    {
        NoProbation, /**< Don't use the probation algorithm; accept RTP packets immediately. */
        ProbationDiscard, /**< Discard incoming RTP packets originating from a source that's on probation. */
        ProbationStore /**< Store incoming RTP packet from a source that's on probation for later retrieval. */
    };

    /** In the constructor you can select the probation type you'd like to use and also a memory manager. */
    RTPSources();
    virtual ~RTPSources();

    /** Clears the source table. */
    void Clear();

    /** Creates an entry for our own SSRC identifier. */
    int CreateOwnSSRC(uint32_t ssrc);

    /** Deletes the entry for our own SSRC identifier. */
    int DeleteOwnSSRC();

    /** This function should be called if our own session has sent an RTP packet.
     *  This function should be called if our own session has sent an RTP packet.
     *  For our own SSRC entry, the sender flag is updated based upon outgoing packets instead of incoming packets.
     */
    void SentRTPPacket();

    /** Processes a raw packet \c rawpack.
     *  Processes a raw packet \c rawpack. The instance \c trans will be used to check if this
     *  packet is one of our own packets. The flag \c acceptownpackets indicates whether own packets should be
     *  accepted or ignored.
     */
    int ProcessRawPacket(RTPRawPacket *rawpack, RTPTransmitter *trans, bool acceptownpackets);

    /** Processes a raw packet \c rawpack.
     *  Processes a raw packet \c rawpack. Every transmitter in the array \c trans of length \c numtrans
     *  is used to check if the packet is from our own session. The flag \c acceptownpackets indicates
     *  whether own packets should be accepted or ignored.
     */
    int ProcessRawPacket(RTPRawPacket *rawpack, RTPTransmitter *trans[], int numtrans, bool acceptownpackets);

    /** Processes an RTPPacket instance \c rtppack which was received at time \c receivetime and
     *  which originated from \c senderaddres.
     *  Processes an RTPPacket instance \c rtppack which was received at time \c receivetime and
     *  which originated from \c senderaddres. The \c senderaddress parameter must be NULL if
     *  the packet was sent by the local participant. The flag \c stored indicates whether the packet
     *  was stored in the table or not.  If so, the \c rtppack instance may not be deleted.
     */
    int ProcessRTPPacket(RTPPacket *rtppack, const RTPTime &receivetime, const RTPAddress *senderaddress, bool *stored);

    /** Processes the RTCP compound packet \c rtcpcomppack which was received at time \c receivetime from \c senderaddress.
     *  Processes the RTCP compound packet \c rtcpcomppack which was received at time \c receivetime from \c senderaddress.
     *  The \c senderaddress parameter must be NULL if the packet was sent by the local participant.
     */
    int ProcessRTCPCompoundPacket(RTCPCompoundPacket *rtcpcomppack, const RTPTime &receivetime, const RTPAddress *senderaddress);

    /** Process the sender information of SSRC \c ssrc into the source table.
     *  Process the sender information of SSRC \c ssrc into the source table. The information was received
     *  at time \c receivetime from address \c senderaddress. The \c senderaddress} parameter must be NULL
     *  if the packet was sent by the local participant.
     */
    int ProcessRTCPSenderInfo(uint32_t ssrc, const RTPNTPTime &ntptime, uint32_t rtptime, uint32_t packetcount, uint32_t octetcount, const RTPTime &receivetime,
            const RTPAddress *senderaddress);

    /** Processes the report block information which was sent by participant \c ssrc into the source table.
     *  Processes the report block information which was sent by participant \c ssrc into the source table.
     *  The information was received at time \c receivetime from address \c senderaddress The \c senderaddress
     *  parameter must be NULL if the packet was sent by the local participant.
     */
    int ProcessRTCPReportBlock(uint32_t ssrc, uint8_t fractionlost, int32_t lostpackets, uint32_t exthighseqnr, uint32_t jitter, uint32_t lsr, uint32_t dlsr,
            const RTPTime &receivetime, const RTPAddress *senderaddress);

    /** Processes the non-private SDES item from source \c ssrc into the source table.
     *  Processes the non-private SDES item from source \c ssrc into the source table. The information was
     *  received at time \c receivetime from address \c senderaddress. The \c senderaddress parameter must
     *  be NULL if the packet was sent by the local participant.
     */
    int ProcessSDESNormalItem(uint32_t ssrc, RTCPSDESPacket::ItemType t, std::size_t itemlength, const void *itemdata, const RTPTime &receivetime, const RTPAddress *senderaddress);
#ifdef RTP_SUPPORT_SDESPRIV
    /** Processes the SDES private item from source \c ssrc into the source table.
     *  Processes the SDES private item from source \c ssrc into the source table. The information was
     *  received at time \c receivetime from address \c senderaddress. The \c senderaddress
     *  parameter must be NULL if the packet was sent by the local participant.
     */
    int ProcessSDESPrivateItem(uint32_t ssrc, std::size_t prefixlen, const void *prefixdata, std::size_t valuelen, const void *valuedata, const RTPTime &receivetime,
            const RTPAddress *senderaddress);
#endif //RTP_SUPPORT_SDESPRIV
    /** Processes the BYE message for SSRC \c ssrc.
     *  Processes the BYE message for SSRC \c ssrc. The information was received at time \c receivetime from
     *  address \c senderaddress. The \c senderaddress parameter must be NULL if the packet was sent by the
     *  local participant.
     */
    int ProcessBYE(uint32_t ssrc, std::size_t reasonlength, const void *reasondata, const RTPTime &receivetime, const RTPAddress *senderaddress);

    /** If we heard from source \c ssrc, but no actual data was added to the source table (for example, if
     *  no report block was meant for us), this function can e used to indicate that something was received from
     *  this source.
     *  If we heard from source \c ssrc, but no actual data was added to the source table (for example, if
     *  no report block was meant for us), this function can e used to indicate that something was received from
     *  this source. This will prevent a premature timeout for this participant. The message was received at time
     *  \c receivetime from address \c senderaddress. The \c senderaddress parameter must be NULL if the
     *  packet was sent by the local participant.
     */
    int UpdateReceiveTime(uint32_t ssrc, const RTPTime &receivetime, const RTPAddress *senderaddress);

    /** Starts the iteration over the participants by going to the first member in the table.
     *  Starts the iteration over the participants by going to the first member in the table.
     *  If a member was found, the function returns \c true, otherwise it returns \c false.
     */
    bool GotoFirstSource();

    /** Sets the current source to be the next source in the table.
     *  Sets the current source to be the next source in the table. If we're already at the last source,
     *  the function returns \c false, otherwise it returns \c true.
     */
    bool GotoNextSource();

    /** Sets the current source to be the previous source in the table.
     *  Sets the current source to be the previous source in the table. If we're at the first source,
     *  the function returns \c false, otherwise it returns \c true.
     */
    bool GotoPreviousSource();

    /** Sets the current source to be the first source in the table which has RTPPacket instances
     *  that we haven't extracted yet.
     *  Sets the current source to be the first source in the table which has RTPPacket instances
     *  that we haven't extracted yet. If no such member was found, the function returns \c false,
     *  otherwise it returns \c true.
     */
    bool GotoFirstSourceWithData();

    /** Sets the current source to be the next source in the table which has RTPPacket instances that
     *  we haven't extracted yet.
     *  Sets the current source to be the next source in the table which has RTPPacket instances that
     *  we haven't extracted yet. If no such member was found, the function returns \c false,
     *  otherwise it returns \c true.
     */
    bool GotoNextSourceWithData();

    /** Sets the current source to be the previous source in the table which has RTPPacket instances
     *  that we haven't extracted yet.
     *  Sets the current source to be the previous source in the table which has RTPPacket instances
     *  that we haven't extracted yet. If no such member was found, the function returns \c false,
     *  otherwise it returns \c true.
     */
    bool GotoPreviousSourceWithData();

    /** Returns the RTPSourceData instance for the currently selected participant. */
    RTPSourceData *GetCurrentSourceInfo();

    /** Returns the RTPSourceData instance for the participant identified by \c ssrc, or
     *  NULL if no such entry exists.
     */
    RTPSourceData *GetSourceInfo(uint32_t ssrc);

    /** Extracts the next packet from the received packets queue of the current participant. */
    RTPPacket *GetNextPacket();

    /** Returns \c true if an entry for participant \c ssrc exists and \c false otherwise. */
    bool GotEntry(uint32_t ssrc);

    /** If present, it returns the RTPSourceData instance of the entry which was created by CreateOwnSSRC. */
    RTPSourceData *GetOwnSourceInfo()
    {
        return (RTPSourceData *) owndata;
    }

    /** Assuming that the current time is \c curtime, time out the members from whom we haven't heard
     *  during the previous time  interval \c timeoutdelay.
     */
    void Timeout(const RTPTime &curtime, const RTPTime &timeoutdelay);

    /** Assuming that the current time is \c curtime, remove the sender flag for senders from whom we haven't
     *  received any RTP packets during the previous time interval \c timeoutdelay.
     */
    void SenderTimeout(const RTPTime &curtime, const RTPTime &timeoutdelay);

    /** Assuming that the current time is \c curtime, remove the members who sent a BYE packet more than
     *  the time interval \c timeoutdelay ago.
     */
    void BYETimeout(const RTPTime &curtime, const RTPTime &timeoutdelay);

    /** Assuming that the current time is \c curtime, clear the SDES NOTE items which haven't been updated
     *  during the previous time interval \c timeoutdelay.
     */
    void NoteTimeout(const RTPTime &curtime, const RTPTime &timeoutdelay);

    /** Combines the functions SenderTimeout, BYETimeout, Timeout and NoteTimeout.
     *  Combines the functions SenderTimeout, BYETimeout, Timeout and NoteTimeout. This is more efficient
     *  than calling all four functions since only one iteration is needed in this function.
     */
    void MultipleTimeouts(const RTPTime &curtime, const RTPTime &sendertimeout, const RTPTime &byetimeout, const RTPTime &generaltimeout, const RTPTime &notetimeout);

    /** Returns the number of participants which are marked as a sender. */
    int GetSenderCount() const
    {
        return sendercount;
    }

    /** Returns the total number of entries in the source table. */
    int GetTotalCount() const
    {
        return totalcount;
    }

    /** Returns the number of members which have been validated and which haven't sent a BYE packet yet. */
    int GetActiveMemberCount() const
    {
        return activecount;
    }

protected:
    /** Is called when an RTP packet is about to be processed. */
    virtual void OnRTPPacket(RTPPacket *pack, const RTPTime &receivetime, const RTPAddress *senderaddress);

    /** Is called when an RTCP compound packet is about to be processed. */
    virtual void OnRTCPCompoundPacket(RTCPCompoundPacket *pack, const RTPTime &receivetime, const RTPAddress *senderaddress);

    /** Is called when an SSRC collision was detected.
     *  Is called when an SSRC collision was detected. The instance \c srcdat is the one present in
     *  the table, the address \c senderaddress is the one that collided with one of the addresses
     *  and \c isrtp indicates against which address of \c srcdat the check failed.
     */
    virtual void OnSSRCCollision(RTPSourceData *srcdat, const RTPAddress *senderaddress, bool isrtp);

    /** Is called when another CNAME was received than the one already present for source \c srcdat. */
    virtual void OnCNAMECollision(RTPSourceData *srcdat, const RTPAddress *senderaddress, const uint8_t *cname, std::size_t cnamelength);

    /** Is called when a new entry \c srcdat is added to the source table. */
    virtual void OnNewSource(RTPSourceData *srcdat);

    /** Is called when the entry \c srcdat is about to be deleted from the source table. */
    virtual void OnRemoveSource(RTPSourceData *srcdat);

    /** Is called when participant \c srcdat is timed out. */
    virtual void OnTimeout(RTPSourceData *srcdat);

    /** Is called when participant \c srcdat is timed after having sent a BYE packet. */
    virtual void OnBYETimeout(RTPSourceData *srcdat);

    /** Is called when a BYE packet has been processed for source \c srcdat. */
    virtual void OnBYEPacket(RTPSourceData *srcdat);

    /** Is called when an RTCP sender report has been processed for this source. */
    virtual void OnRTCPSenderReport(RTPSourceData *srcdat);

    /** Is called when an RTCP receiver report has been processed for this source. */
    virtual void OnRTCPReceiverReport(RTPSourceData *srcdat);

    /** Is called when a specific SDES item was received for this source. */
    virtual void OnRTCPSDESItem(RTPSourceData *srcdat, RTCPSDESPacket::ItemType t, const void *itemdata, std::size_t itemlength);
#ifdef RTP_SUPPORT_SDESPRIV
    /** Is called when a specific SDES item of 'private' type was received for this source. */
    virtual void OnRTCPSDESPrivateItem(RTPSourceData *srcdat, const void *prefixdata, std::size_t prefixlen, const void *valuedata, std::size_t valuelen);
#endif // RTP_SUPPORT_SDESPRIV

    /** Is called when an RTCP APP packet \c apppacket has been received at time \c receivetime
     *  from address \c senderaddress.
     */
    virtual void OnAPPPacket(RTCPAPPPacket *apppacket, const RTPTime &receivetime, const RTPAddress *senderaddress);

    /** Is called when an unknown RTCP packet type was detected. */
    virtual void OnUnknownPacketType(RTCPPacket *rtcppack, const RTPTime &receivetime, const RTPAddress *senderaddress);

    /** Is called when an unknown packet format for a known packet type was detected. */
    virtual void OnUnknownPacketFormat(RTCPPacket *rtcppack, const RTPTime &receivetime, const RTPAddress *senderaddress);

    /** Is called when the SDES NOTE item for source \c srcdat has been timed out. */
    virtual void OnNoteTimeout(RTPSourceData *srcdat);

    /** Allows you to use an RTP packet from the specified source directly.
     *  Allows you to use an RTP packet from the specified source directly. If
     *  `ispackethandled` is set to `true`, the packet will no longer be stored in this
     *  source's packet list. */
    virtual void OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack, bool isonprobation, bool *ispackethandled);
private:
    void ClearSourceList();
    int ObtainSourceDataInstance(uint32_t ssrc, RTPInternalSourceData **srcdat, bool *created);
    int GetRTCPSourceData(uint32_t ssrc, const RTPAddress *senderaddress, RTPInternalSourceData **srcdat, bool *newsource);
    bool CheckCollision(RTPInternalSourceData *srcdat, const RTPAddress *senderaddress, bool isrtp);

    RTPKeyHashTable<const uint32_t, RTPInternalSourceData*, RTPSources_GetHashIndex, RTPSOURCES_HASHSIZE> sourcelist;

    int sendercount;
    int totalcount;
    int activecount;

    RTPInternalSourceData *owndata;

    friend class RTPInternalSourceData;
};

// Inlining the default implementations to avoid unused-parameter errors.
inline void RTPSources::OnRTPPacket(RTPPacket *, const RTPTime &, const RTPAddress *)
{
}
inline void RTPSources::OnRTCPCompoundPacket(RTCPCompoundPacket *, const RTPTime &, const RTPAddress *)
{
}
inline void RTPSources::OnSSRCCollision(RTPSourceData *, const RTPAddress *, bool)
{
}
inline void RTPSources::OnCNAMECollision(RTPSourceData *, const RTPAddress *, const uint8_t *, std::size_t)
{
}
inline void RTPSources::OnNewSource(RTPSourceData *)
{
}
inline void RTPSources::OnRemoveSource(RTPSourceData *)
{
}
inline void RTPSources::OnTimeout(RTPSourceData *)
{
}
inline void RTPSources::OnBYETimeout(RTPSourceData *)
{
}
inline void RTPSources::OnBYEPacket(RTPSourceData *)
{
}
inline void RTPSources::OnRTCPSenderReport(RTPSourceData *)
{
}
inline void RTPSources::OnRTCPReceiverReport(RTPSourceData *)
{
}
inline void RTPSources::OnRTCPSDESItem(RTPSourceData *, RTCPSDESPacket::ItemType, const void *, std::size_t)
{
}
#ifdef RTP_SUPPORT_SDESPRIV
inline void RTPSources::OnRTCPSDESPrivateItem(RTPSourceData *, const void *, std::size_t, const void *, std::size_t)
{
}
#endif // RTP_SUPPORT_SDESPRIV
inline void RTPSources::OnAPPPacket(RTCPAPPPacket *, const RTPTime &, const RTPAddress *)
{
}
inline void RTPSources::OnUnknownPacketType(RTCPPacket *, const RTPTime &, const RTPAddress *)
{
}
inline void RTPSources::OnUnknownPacketFormat(RTCPPacket *, const RTPTime &, const RTPAddress *)
{
}
inline void RTPSources::OnNoteTimeout(RTPSourceData *)
{
}
inline void RTPSources::OnValidatedRTPPacket(RTPSourceData *, RTPPacket *, bool, bool *)
{
}

} // end namespace

#endif // RTPSOURCES_H

