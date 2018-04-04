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
 * \file rtpsession.h
 */

#ifndef RTPSESSION_H

#define RTPSESSION_H

#include "rtpconfig.h"
#include "rtppacketbuilder.h"
#include "rtpsessionsources.h"
#include "rtptransmitter.h"
#include "rtpcollisionlist.h"
#include "rtcpscheduler.h"
#include "rtcppacketbuilder.h"
#include "rtptimeutilities.h"
#include "rtcpcompoundpacketbuilder.h"
#include <list>

#include "export.h"

namespace qrtplib
{

class RTPTransmitter;
class RTPSessionParams;
class RTPTransmissionParams;
class RTPAddress;
class RTPSourceData;
class RTPPacket;
class RTPPollThread;
class RTPTransmissionInfo;
class RTCPCompoundPacket;
class RTCPPacket;
class RTCPAPPPacket;

/** High level class for using RTP.
 *  For most RTP based applications, the RTPSession class will probably be the one to use. It handles
 *  the RTCP part completely internally, so the user can focus on sending and receiving the actual data.
 *  In case you want to use SRTP, you should create an RTPSecureSession derived class.
 *  \note The RTPSession class is not meant to be thread safe. The user should use some kind of locking
 *        mechanism to prevent different threads from using the same RTPSession instance.
 */
class QRTPLIB_API RTPSession
{
public:
    /** Constructs an RTPSession instance, optionally using a specific instance of a random
     *  number generator, and optionally installing a memory manager.
     *  Constructs an RTPSession instance, optionally using a specific instance of a random
     *  number generator, and optionally installing a memory manager. If no random number generator
     *  is specified, the RTPSession object will try to use either a RTPRandomURandom or
     *  RTPRandomRandS instance. If neither is available on the current platform, a RTPRandomRand48
     *  instance will be used instead. By specifying a random number generator yourself, it is
     *  possible to use the same generator in several RTPSession instances.
     */
    RTPSession(RTPRandom *rnd = 0);
    virtual ~RTPSession();

    /** Creates an RTP session.
     *  This function creates an RTP session with parameters \c sessparams, which will use a transmitter
     *  corresponding to \c proto. Parameters for this transmitter can be specified as well. If \c
     *  proto is of type RTPTransmitter::UserDefinedProto, the NewUserDefinedTransmitter function must
     *  be implemented.
     */
    //int Create(const RTPSessionParams &sessparams, const RTPTransmissionParams *transparams = 0, RTPTransmitter::TransmissionProtocol proto = RTPTransmitter::IPv4UDPProto);

    /** Creates an RTP session using \c transmitter as transmission component.
     *  This function creates an RTP session with parameters \c sessparams, which will use the
     *  transmission component \c transmitter. Initialization and destruction of the transmitter
     *  will not be done by the RTPSession class if this Create function is used. This function
     *  can be useful if you which to reuse the transmission component in another RTPSession
     *  instance, once the original RTPSession isn't using the transmitter anymore.
     */
    int Create(const RTPSessionParams &sessparams, RTPTransmitter *transmitter);

    /** Leaves the session without sending a BYE packet. */
    void Destroy();

    /** Sends a BYE packet and leaves the session.
     *  Sends a BYE packet and leaves the session. At most a time \c maxwaittime will be waited to
     *  send the BYE packet. If this time expires, the session will be left without sending a BYE packet.
     *  The BYE packet will contain as reason for leaving \c reason with length \c reasonlength.
     */
    void BYEDestroy(const RTPTime &maxwaittime, const void *reason, std::size_t reasonlength);

    /** Returns whether the session has been created or not. */
    bool IsActive();

    /** Returns our own SSRC. */
    uint32_t GetLocalSSRC();

    /** Adds \c addr to the list of destinations. */
    int AddDestination(const RTPAddress &addr);

    /** Deletes \c addr from the list of destinations. */
    int DeleteDestination(const RTPAddress &addr);

    /** Clears the list of destinations. */
    void ClearDestinations();

    /** Returns \c true if multicasting is supported. */
    bool SupportsMulticasting();

    /** Joins the multicast group specified by \c addr. */
    int JoinMulticastGroup(const RTPAddress &addr);

    /** Leaves the multicast group specified by \c addr. */
    int LeaveMulticastGroup(const RTPAddress &addr);

    /** Sends the RTP packet with payload \c data which has length \c len.
     *  Sends the RTP packet with payload \c data which has length \c len.
     *  The used payload type, marker and timestamp increment will be those that have been set
     *  using the \c SetDefault member functions.
     */
    int SendPacket(const void *data, std::size_t len);

    /** Sends the RTP packet with payload \c data which has length \c len.
     *  It will use payload type \c pt, marker \c mark and after the packet has been built, the
     *  timestamp will be incremented by \c timestampinc.
     */
    int SendPacket(const void *data, std::size_t len, uint8_t pt, bool mark, uint32_t timestampinc);

    /** Sends the RTP packet with payload \c data which has length \c len.
     *  The packet will contain a header extension with identifier \c hdrextID and containing data
     *  \c hdrextdata. The length of this data is given by \c numhdrextwords and is specified in a
     *  number of 32-bit words. The used payload type, marker and timestamp increment will be those that
     *  have been set using the \c SetDefault member functions.
     */
    int SendPacketEx(const void *data, std::size_t len, uint16_t hdrextID, const void *hdrextdata, std::size_t numhdrextwords);

    /** Sends the RTP packet with payload \c data which has length \c len.
     *  It will use payload type \c pt, marker \c mark and after the packet has been built, the
     *  timestamp will be incremented by \c timestampinc. The packet will contain a header
     *  extension with identifier \c hdrextID and containing data \c hdrextdata. The length
     *  of this data is given by \c numhdrextwords and is specified in a number of 32-bit words.
     */
    int SendPacketEx(const void *data, std::size_t len, uint8_t pt, bool mark, uint32_t timestampinc, uint16_t hdrextID, const void *hdrextdata, std::size_t numhdrextwords);
#ifdef RTP_SUPPORT_SENDAPP
    /** If sending of RTCP APP packets was enabled at compile time, this function creates a compound packet
     *  containing an RTCP APP packet and sends it immediately.
     *  If sending of RTCP APP packets was enabled at compile time, this function creates a compound packet
     *  containing an RTCP APP packet and sends it immediately. If successful, the function returns the number
     *  of bytes in the RTCP compound packet. Note that this immediate sending is not compliant with the RTP
     *  specification, so use with care.
     */
    int SendRTCPAPPPacket(uint8_t subtype, const uint8_t name[4], const void *appdata, std::size_t appdatalen);
#endif // RTP_SUPPORT_SENDAPP

    /** With this function raw data can be sent directly over the RTP or
     *  RTCP channel (if they are different); the data is **not** passed through the
     *  RTPSession::OnChangeRTPOrRTCPData function. */
    int SendRawData(const void *data, std::size_t len, bool usertpchannel);

    /** Sets the default payload type for RTP packets to \c pt. */
    int SetDefaultPayloadType(uint8_t pt);

    /** Sets the default marker for RTP packets to \c m. */
    int SetDefaultMark(bool m);

    /** Sets the default value to increment the timestamp with to \c timestampinc. */
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

    /** This function allows you to inform the library about the delay between sampling the first
     *  sample of a packet and sending the packet.
     *  This function allows you to inform the library about the delay between sampling the first
     *  sample of a packet and sending the packet. This delay is taken into account when calculating the
     *  relation between RTP timestamp and wallclock time, used for inter-media synchronization.
     */
    int SetPreTransmissionDelay(const RTPTime &delay);

    /** This function returns an instance of a subclass of RTPTransmissionInfo which will give some
     *  additional information about the transmitter (a list of local IP addresses for example).
     *  This function returns an instance of a subclass of RTPTransmissionInfo which will give some
     *  additional information about the transmitter (a list of local IP addresses for example). The user
     *  has to free the returned instance when it is no longer needed, preferably using the DeleteTransmissionInfo
     *  function.
     */
    RTPTransmissionInfo *GetTransmissionInfo();

    /** Frees the memory used by the transmission information \c inf. */
    void DeleteTransmissionInfo(RTPTransmissionInfo *inf);

    /** Returns the time interval after which an RTCP compound packet may have to be sent (only works when
     *  you're not using the poll thread.
     */
    RTPTime GetRTCPDelay();

    /** The following member functions (till EndDataAccess}) need to be accessed between a call
     *  to BeginDataAccess and EndDataAccess.
     *  The BeginDataAccess function makes sure that the poll thread won't access the source table
     *  at the same time that you're using it. When the EndDataAccess is called, the lock on the
     *  source table is freed again.
     */
    int BeginDataAccess();

    /** Starts the iteration over the participants by going to the first member in the table.
     *  Starts the iteration over the participants by going to the first member in the table.
     *  If a member was found, the function returns \c true, otherwise it returns \c false.
     */
    bool GotoFirstSource();

    /** Sets the current source to be the next source in the table.
     *  Sets the current source to be the next source in the table. If we're already at the last
     *  source, the function returns \c false, otherwise it returns \c true.
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

    /** Sets the current source to be the next source in the table which has RTPPacket instances
     *  that we haven't extracted yet.
     *  Sets the current source to be the next source in the table which has RTPPacket instances
     *  that we haven't extracted yet. If no such member was found, the function returns \c false,
     *  otherwise it returns \c true.
     */
    bool GotoNextSourceWithData();

    /** Sets the current source to be the previous source in the table which has RTPPacket
     *  instances that we haven't extracted yet.
     *  Sets the current source to be the previous source in the table which has RTPPacket
     *  instances that we haven't extracted yet. If no such member was found, the function returns \c false,
     *  otherwise it returns \c true.
     */
    bool GotoPreviousSourceWithData();

    /** Returns the \c RTPSourceData instance for the currently selected participant. */
    RTPSourceData *GetCurrentSourceInfo();

    /** Returns the \c RTPSourceData instance for the participant identified by \c ssrc,
     *  or NULL if no such entry exists.
     */
    RTPSourceData *GetSourceInfo(uint32_t ssrc);

    /** Extracts the next packet from the received packets queue of the current participant,
     *  or NULL if no more packets are available.
     *  Extracts the next packet from the received packets queue of the current participant,
     *  or NULL if no more packets are available. When the packet is no longer needed, its
     *  memory should be freed using the DeletePacket member function.
     */
    RTPPacket *GetNextPacket();

    /** Returns the Sequence Number that will be used in the next SendPacket function call. */
    uint16_t GetNextSequenceNumber() const;

    /** Frees the memory used by \c p. */
    void DeletePacket(RTPPacket *p);

    /** See BeginDataAccess. */
    int EndDataAccess();

    /** Sets the receive mode to \c m.
     *  Sets the receive mode to \c m. Note that when the receive mode is changed, the list of
     *  addresses to be ignored ot accepted will be cleared.
     */
    int SetReceiveMode(RTPTransmitter::ReceiveMode m);

    /** Adds \c addr to the list of addresses to ignore. */
    int AddToIgnoreList(const RTPAddress &addr);

    /** Deletes \c addr from the list of addresses to ignore. */
    int DeleteFromIgnoreList(const RTPAddress &addr);

    /** Clears the list of addresses to ignore. */
    void ClearIgnoreList();

    /** Adds \c addr to the list of addresses to accept. */
    int AddToAcceptList(const RTPAddress &addr);

    /** Deletes \c addr from the list of addresses to accept. */
    int DeleteFromAcceptList(const RTPAddress &addr);

    /** Clears the list of addresses to accept. */
    void ClearAcceptList();

    /** Sets the maximum allowed packet size to \c s. */
    int SetMaximumPacketSize(std::size_t s);

    /** Sets the session bandwidth to \c bw, which is specified in bytes per second. */
    int SetSessionBandwidth(double bw);

    /** Sets the timestamp unit for our own data.
     *  Sets the timestamp unit for our own data. The timestamp unit is defined as a time interval in
     *  seconds divided by the corresponding timestamp interval. For example, for 8000 Hz audio, the
     *  timestamp unit would typically be 1/8000. Since this value is initially set to an illegal value,
     *  the user must set this to an allowed value to be able to create a session.
     */
    int SetTimestampUnit(double u);

    /** Sets the RTCP interval for the SDES name item.
     *  After all possible sources in the source table have been processed, the class will check if other
     *  SDES items need to be sent. If \c count is zero or negative, nothing will happen. If \c count
     *  is positive, an SDES name item will be added after the sources in the source table have been
     *  processed \c count times.
     */
    void SetNameInterval(int count);

    /** Sets the RTCP interval for the SDES e-mail item.
     *  After all possible sources in the source table have been processed, the class will check if other
     *  SDES items need to be sent. If \c count is zero or negative, nothing will happen. If \c count
     *  is positive, an SDES e-mail item will be added after the sources in the source table have been
     *  processed \c count times.
     */
    void SetEMailInterval(int count);

    /** Sets the RTCP interval for the SDES location item.
     *  After all possible sources in the source table have been processed, the class will check if other
     *  SDES items need to be sent. If \c count is zero or negative, nothing will happen. If \c count
     *  is positive, an SDES location item will be added after the sources in the source table have been
     *  processed \c count times.
     */
    void SetLocationInterval(int count);

    /** Sets the RTCP interval for the SDES phone item.
     *  After all possible sources in the source table have been processed, the class will check if other
     *  SDES items need to be sent. If \c count is zero or negative, nothing will happen. If \c count
     *  is positive, an SDES phone item will be added after the sources in the source table have been
     *  processed \c count times.
     */
    void SetPhoneInterval(int count);

    /** Sets the RTCP interval for the SDES tool item.
     *  After all possible sources in the source table have been processed, the class will check if other
     *  SDES items need to be sent. If \c count is zero or negative, nothing will happen. If \c count
     *  is positive, an SDES tool item will be added after the sources in the source table have been
     *  processed \c count times.
     */
    void SetToolInterval(int count);

    /** Sets the RTCP interval for the SDES note item.
     *  After all possible sources in the source table have been processed, the class will check if other
     *  SDES items need to be sent. If \c count is zero or negative, nothing will happen. If \c count
     *  is positive, an SDES note item will be added after the sources in the source table have been
     *  processed \c count times.
     */
    void SetNoteInterval(int count);

    /** Sets the SDES name item for the local participant to the value \c s with length \c len. */
    int SetLocalName(const void *s, std::size_t len);

    /** Sets the SDES e-mail item for the local participant to the value \c s with length \c len. */
    int SetLocalEMail(const void *s, std::size_t len);

    /** Sets the SDES location item for the local participant to the value \c s with length \c len. */
    int SetLocalLocation(const void *s, std::size_t len);

    /** Sets the SDES phone item for the local participant to the value \c s with length \c len. */
    int SetLocalPhone(const void *s, std::size_t len);

    /** Sets the SDES tool item for the local participant to the value \c s with length \c len. */
    int SetLocalTool(const void *s, std::size_t len);

    /** Sets the SDES note item for the local participant to the value \c s with length \c len. */
    int SetLocalNote(const void *s, std::size_t len);

protected:
    /** Allocate a user defined transmitter.
     *  In case you specified in the Create function that you want to use a
     *  user defined transmitter, you should override this function. The RTPTransmitter
     *  instance returned by this function will then be used to send and receive RTP and
     *  RTCP packets. Note that when the session is destroyed, this RTPTransmitter
     *  instance will be destroyed as well.
     */
    virtual RTPTransmitter *NewUserDefinedTransmitter();

    /** Is called when an incoming RTP packet is about to be processed.
     *  Is called when an incoming RTP packet is about to be processed. This is _not_
     *  a good function to process an RTP packet in, in case you want to avoid iterating
     *  over the sources using the GotoFirst/GotoNext functions. In that case, the
     *  RTPSession::OnValidatedRTPPacket function should be used.
     */
    virtual void OnRTPPacket(RTPPacket *pack, const RTPTime &receivetime, const RTPAddress *senderaddress);

    /** Is called when an incoming RTCP packet is about to be processed. */
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

    /** Is called when a BYE packet has been processed for source \c srcdat. */
    virtual void OnBYEPacket(RTPSourceData *srcdat);

    /** Is called when an RTCP compound packet has just been sent (useful to inspect outgoing RTCP data). */
    virtual void OnSendRTCPCompoundPacket(RTCPCompoundPacket *pack);

    /** If this is set to true, outgoing data will be passed through RTPSession::OnChangeRTPOrRTCPData
     *  and RTPSession::OnSentRTPOrRTCPData, allowing you to modify the data (e.g. to encrypt it). */
    void SetChangeOutgoingData(bool change)
    {
        m_changeOutgoingData = change;
    }

    /** If this is set to true, incoming data will be passed through RTPSession::OnChangeIncomingData,
     *  allowing you to modify the data (e.g. to decrypt it). */
    void SetChangeIncomingData(bool change)
    {
        m_changeIncomingData = change;
    }

    /** If RTPSession::SetChangeOutgoingData was sent to true, overriding this you can change the
     *  data packet that will actually be sent, for example adding encryption.
     *  If RTPSession::SetChangeOutgoingData was sent to true, overriding this you can change the
     *  data packet that will actually be sent, for example adding encryption.
     *  Note that no memory management will be performed on the `senddata` pointer you fill in,
     *  so if it needs to be deleted at some point you need to take care of this in some way
     *  yourself, a good way may be to do this in RTPSession::OnSentRTPOrRTCPData. If `senddata` is
     *  set to 0, no packet will be sent out. This also provides a way to turn off sending RTCP
     *  packets if desired. */
    virtual int OnChangeRTPOrRTCPData(const void *origdata, std::size_t origlen, bool isrtp, void **senddata, std::size_t *sendlen);

    /** This function is called when an RTP or RTCP packet was sent, it can be helpful
     *  when data was allocated in RTPSession::OnChangeRTPOrRTCPData to deallocate it
     *  here. */
    virtual void OnSentRTPOrRTCPData(void *senddata, std::size_t sendlen, bool isrtp);

    /** By overriding this function, the raw incoming data can be inspected
     *  and modified (e.g. for encryption).
     *  By overriding this function, the raw incoming data can be inspected
     *  and modified (e.g. for encryption). If the function returns `false`,
     *  the packet is discarded.
     */
    virtual bool OnChangeIncomingData(RTPRawPacket *rawpack);

    /** Allows you to use an RTP packet from the specified source directly.
     *  Allows you to use an RTP packet from the specified source directly. If
     *  `ispackethandled` is set to `true`, the packet will no longer be stored in this
     *  source's packet list. Note that if you do set this flag, you'll need to
     *  deallocate the packet yourself at an appropriate time.
     *
     *  The difference with RTPSession::OnRTPPacket is that that
     *  function will always process the RTP packet further and is therefore not
     *  really suited to actually do something with the data.
     */
    virtual void OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack, bool isonprobation, bool *ispackethandled);
private:
    int InternalCreate(const RTPSessionParams &sessparams);
    int CreateCNAME(uint8_t *buffer, std::size_t *bufferlength, bool resolve);
    int ProcessPolledData();
    int ProcessRTCPCompoundPacket(RTCPCompoundPacket &rtcpcomppack, RTPRawPacket *pack);
    RTPRandom *GetRandomNumberGenerator(RTPRandom *r);
    int SendRTPData(const void *data, std::size_t len);
    int SendRTCPData(const void *data, std::size_t len);

    RTPRandom *rtprnd;
    bool deletertprnd;

    RTPTransmitter *rtptrans;
    bool created;
    bool deletetransmitter;
    bool usingpollthread;
    bool acceptownpackets;
    bool useSR_BYEifpossible;
    std::size_t maxpacksize;
    double sessionbandwidth;
    double controlfragment;
    double sendermultiplier;
    double byemultiplier;
    double membermultiplier;
    double collisionmultiplier;
    double notemultiplier;
    bool sentpackets;

    bool m_changeIncomingData, m_changeOutgoingData;

    RTPSessionSources sources;
    RTPPacketBuilder packetbuilder;
    RTCPScheduler rtcpsched;
    RTCPPacketBuilder rtcpbuilder;
    RTPCollisionList collisionlist;

    std::list<RTCPCompoundPacket *> byepackets;

    friend class RTPSessionSources;
    friend class RTCPSessionPacketBuilder;
};

inline RTPTransmitter *RTPSession::NewUserDefinedTransmitter()
{
    return 0;
}
inline void RTPSession::OnRTPPacket(RTPPacket *, const RTPTime &, const RTPAddress *)
{
}
inline void RTPSession::OnRTCPCompoundPacket(RTCPCompoundPacket *, const RTPTime &, const RTPAddress *)
{
}
inline void RTPSession::OnSSRCCollision(RTPSourceData *, const RTPAddress *, bool)
{
}
inline void RTPSession::OnCNAMECollision(RTPSourceData *, const RTPAddress *, const uint8_t *, std::size_t)
{
}
inline void RTPSession::OnNewSource(RTPSourceData *)
{
}
inline void RTPSession::OnRemoveSource(RTPSourceData *)
{
}
inline void RTPSession::OnTimeout(RTPSourceData *)
{
}
inline void RTPSession::OnBYETimeout(RTPSourceData *)
{
}
inline void RTPSession::OnAPPPacket(RTCPAPPPacket *, const RTPTime &, const RTPAddress *)
{
}
inline void RTPSession::OnUnknownPacketType(RTCPPacket *, const RTPTime &, const RTPAddress *)
{
}
inline void RTPSession::OnUnknownPacketFormat(RTCPPacket *, const RTPTime &, const RTPAddress *)
{
}
inline void RTPSession::OnNoteTimeout(RTPSourceData *)
{
}
inline void RTPSession::OnRTCPSenderReport(RTPSourceData *)
{
}
inline void RTPSession::OnRTCPReceiverReport(RTPSourceData *)
{
}
inline void RTPSession::OnRTCPSDESItem(RTPSourceData *, RTCPSDESPacket::ItemType, const void *, std::size_t)
{
}

#ifdef RTP_SUPPORT_SDESPRIV
inline void RTPSession::OnRTCPSDESPrivateItem(RTPSourceData *, const void *, std::size_t, const void *, std::size_t)
{
}
#endif // RTP_SUPPORT_SDESPRIV

inline void RTPSession::OnBYEPacket(RTPSourceData *)
{
}
inline void RTPSession::OnSendRTCPCompoundPacket(RTCPCompoundPacket *)
{
}

inline int RTPSession::OnChangeRTPOrRTCPData(const void *, std::size_t, bool, void **, std::size_t *)
{
    return ERR_RTP_RTPSESSION_CHANGEREQUESTEDBUTNOTIMPLEMENTED;
}
inline void RTPSession::OnSentRTPOrRTCPData(void *, std::size_t, bool)
{
}
inline bool RTPSession::OnChangeIncomingData(RTPRawPacket *)
{
    return true;
}
inline void RTPSession::OnValidatedRTPPacket(RTPSourceData *, RTPPacket *, bool, bool *)
{
}

} // end namespace

#endif // RTPSESSION_H

